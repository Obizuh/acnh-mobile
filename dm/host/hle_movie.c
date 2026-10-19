// SDK 的影片库（subsdk0 = NintendoSDK_movie，Engage）：Extractor 拆封装，Decoder 用 NVIDIA 的硬件解码器（NVOMX → NvRmChannelOpen 开 nvdrv 设备）。
// 宿主没有 nvdrv：解码线程开设备时 SDK 中止。Extractor::SetDataSource 回 0x1000（SDK 自己在没有实现对象时回的同一个码）；Engage 的 Unity 自己拆 WebM，不走它。
// Decoder 由宿主接管（下面，VP9 用 libvpx 解）：Engage 的影片是 RomFS/Data/StreamingAssets/Movie 下的 103 个 VP9 WebM（2.7 GB，无音轨），开场动画 S01F 不放完游戏不往下走。
// The SDK's movie library (subsdk0 = NintendoSDK_movie, Engage): Extractor demuxes, Decoder uses NVIDIA's hardware decoder (NVOMX → NvRmChannelOpen opens the nvdrv device).
// The host has no nvdrv: the SDK aborts when the decoder thread opens the device. Extractor::SetDataSource returns 0x1000 (the same code the SDK returns when it has no implementation object); Engage's Unity demuxes WebM itself and does not use it.
// The host takes over Decoder (below; VP9 is decoded with libvpx): Engage's movies are 103 VP9 WebM files under RomFS/Data/StreamingAssets/Movie (2.7 GB, no audio track), and the game does not go on until the opening movie S01F has played.
#include "hle.h"
#include <TargetConditionals.h>
#include <arm_neon.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/qos.h>
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

HLE_FORCE("_ZN5movie9Extractor13SetDataSourceEPKc", "影片读取：动森使用 SDK WebM 读取器，其他游戏保持原策略", int, h_movie_SetSource, void *self, const char *path) {
    if(getenv("ACNH_MOVIE_NATIVE") && !strcmp(dm_main_build(),"ff1d1c05670db6021c85b624a710b963")) {
        int (*orig)(void *,const char *)=guest_sym("_ZN5movie9Extractor13SetDataSourceEPKc");
        LOG("ACNH movie: opening %s",path?path:"(null)");
        int result=orig?orig(self,path):0x1000;LOG("ACNH movie: SetDataSource result=%#x",result);return result;
    }
    (void)self; static int red; if (!red++) LOG("【红】影片：宿主不拆封装，开片报失败（首个 %s）", path ? path : "(null)");
    return 0x1000;
}


// ---------- 解码器 | Decoder ----------
// SDK 的 Decoder 是 Android MediaCodec 的薄封装（DecoderImpl 包 android::MediaCodec）：
//   · 客户端注册 4 个 nn::os 事件 DecoderEvents {输入有空、输出就绪、格式变了、出错}，有事就 SignalEvent；
//   · 输入：AcquireInputBufferIndex 取空输入号（没有回 0x1011）→ GetInputBuffer 把数据区交给客户端的 movie::Buffer → SendInputBufferForDecode(号, 偏移, 长, 时间 µs, 标志)；
//   · 输出：AcquireOutputBufferIndex(号, 时间, 标志, 余数)（没有回 0x100e）→ GetOutputBufferFormat / GetOutputBuffer 把一帧 NV12 拷进客户端自己分配的缓冲 → ReleaseOutputBufferIndex。
//   Unity（mode 1、输出格式 4 = NV12）按 GetOutputBufferSize 分配输出缓冲，从 MediaData 读 width / height / crop / nv12-y-stride / nv12-y-offset / nv12-uv-offset / slice-height。
// 宿主实现同一协议，VP9 用 libvpx 解（dm/third_party/libvpx）。输入包按到达顺序排队；有空输出槽就取队头解一包、还输入号，出了画面就进就绪输出队列，时间与标志照抄
// （流结束标志随最后一帧传回）。输出槽都被客户端占着时包留在队里、输入号不还（同 MediaCodec：解码器只在有空输出时消耗输入），客户端还一个输出号就再解一包。
// 尺寸取 Configure 的 MediaData，没有时取首个关键帧的 VP9 帧头。Unity 按宽 × 高 × 1.5 自己分配输出缓冲（不调 GetOutputBufferSize），所以行距 = 宽、平面高 = 高，不对齐。
// 解码在每个解码器自己的宿主线程上做，送包 / 还输出号的客体线程等它做完：客体线程跑在 SDK 给的客体栈上，libvpx 的栈帧大；事件仍由客体线程发（SignalEvent 是客体代码）。
// 对 Buffer / MediaData / 事件的操作都调 SDK 自己的函数（guest_sym），对象布局不用宿主知道。
// The SDK's Decoder is a thin wrapper over Android's MediaCodec (DecoderImpl wraps android::MediaCodec):
//   · the client registers 4 nn::os events DecoderEvents {input available, output ready, format changed, error}, signalled with SignalEvent;
//   · input: AcquireInputBufferIndex takes a free input index (none: 0x1011) → GetInputBuffer hands its data area to the client's movie::Buffer → SendInputBufferForDecode(index, offset, size, time µs, flags);
//   · output: AcquireOutputBufferIndex(index, time, flags, remaining) (none: 0x100e) → GetOutputBufferFormat / GetOutputBuffer copy one NV12 frame into a buffer the client allocated → ReleaseOutputBufferIndex.
//   Unity (mode 1, output format 4 = NV12) allocates its output buffers by GetOutputBufferSize and reads width / height / crop / nv12-y-stride / nv12-y-offset / nv12-uv-offset / slice-height from the MediaData.
// The host implements the same protocol and decodes VP9 with libvpx (dm/third_party/libvpx). Input packets queue in arrival order; whenever an output slot is free, the head packet is decoded and its input index returned, and a picture goes to the ready-output queue with its time and flags copied
// (the end-of-stream flag comes back with the last frame). While the client holds every output slot, packets stay queued and their input indices are not returned (as with MediaCodec: a decoder only consumes input when it has a free output); each output index the client returns lets one more packet be decoded.
// The size comes from Configure's MediaData, or else from the VP9 header of the first key frame. Unity allocates its output buffers itself as width × height × 1.5 (it does not call GetOutputBufferSize), so the stride is the width and the plane height the height, unaligned.
// Decoding runs on each decoder's own host thread, and the guest thread that sends a packet or returns an output index waits for it: guest threads run on guest stacks given by the SDK, and libvpx's stack frames are large. Events are still signalled by guest threads (SignalEvent is guest code).
// Buffer / MediaData / event operations call the SDK's own functions (guest_sym), so the host needs no knowledge of their layouts.

#define MD_NIN 4                 // 输入缓冲数 | input buffers
#define MD_INCAP (4u << 20)      // 每个输入缓冲 4 MB（1080p VP9 一包远小于此） | 4 MB per input buffer (a 1080p VP9 packet is far smaller)
#define MD_NOUT 8                // 输出槽数 | output slots
#define MD_NO_INPUT 0x1011       // SDK：没有空输入 | SDK: no free input
#define MD_NO_OUTPUT 0x100e      // SDK：没有就绪输出 | SDK: no output ready
#define MD_FMT_NV12 4
#define MD_TYPE_VP9 3

typedef struct {
    pthread_mutex_t mu;                          // 护下面的队列与槽 | guards the queues and slots below
    int type, fmt, mode;
    void **ev;                                   // DecoderEvents*：{输入, 输出, 格式, 出错} 的 EventType* | {input, output, format, error} EventType*
    int w, h, fmt_sent, started, input_eos;
    int channels, rate, vorbis_on;
    _Alignas(16) uint8_t vi[0x38], vc[0x20]; void *vdsp;
    uint8_t *in[MD_NIN]; int infree[MD_NIN], ninfree;                     // 空输入号队列 | queue of free input indices
    struct { int used, pic; int64_t pts; uint32_t flags; uint8_t *frm; size_t cap, bytes; } out[MD_NOUT];   // pic：frm 里有解出的画面；cap：frm 的字节数 | pic: frm holds a decoded picture; cap: bytes in frm
    int outq[MD_NOUT], noutq;                                             // 就绪输出号队列 | queue of ready output indices
    struct { int in, off, size; int64_t pts; uint32_t flags; } pend[MD_NIN]; int npend;   // 等解码的输入包（按到达顺序） | input packets waiting to be decoded (in arrival order)
    // 解码：dmu 让同一时刻只有一个客体线程在取包解码（包按顺序进 libvpx）；wmu / wcv 把一件活交给宿主线程并等它做完
    // Decoding: dmu lets only one guest thread take and decode packets at a time (packets enter libvpx in order); wmu / wcv hand one job to the host thread and wait for it
    pthread_mutex_t dmu, wmu; pthread_cond_t wcv; pthread_t wk;
    int job; const uint8_t *jp; int jn, jslot, jgot;
    vpx_codec_ctx_t vpx; int vpx_on, red_dec, red_img;
    uint64_t dec_n, dec_ns, dec_max;            // 解码计时（宿主线程上，每包） | decode timing (on the host thread, per packet)
    unsigned long ncall[24];
} mdec_t;

static struct {
    void (*signal)(void *ev);
    void (*set_data)(void *buf, void *data, size_t cap);
    void (*set_range)(void *buf, int off, int size);
    void *(*data)(const void *buf);
    size_t (*cap)(const void *buf);
    void (*set_i32)(void *md, const char *k, int v);
    void (*set_rect)(void *md, const char *k, int l, int t, int r, int b);
    int (*find_i32)(const void *md, const char *k, int *v);
    int (*find_buf)(const void *,const char *,void **);
    size_t (*size)(const void *);
    void (*set_string)(void *,const char *,const char *);
} G;

static void *msym(const char *n) { void *p = guest_sym(n); if (!p) die("影片：缺 SDK 符号 %s", n); return p; }
static void md_syms(void) {
    if (G.signal) return;
    G.set_data = msym("_ZN5movie6Buffer18SetDataAndCapacityEPvm");
    G.set_range = msym("_ZN5movie6Buffer8SetRangeEii");
    G.data = msym("_ZNK5movie6Buffer4DataEv");
    G.cap = msym("_ZNK5movie6Buffer8CapacityEv");
    G.set_i32 = msym("_ZN5movie9MediaData8SetInt32EPKci");
    G.set_rect = msym("_ZN5movie9MediaData7SetRectEPKciiii");
    G.find_i32 = msym("_ZNK5movie9MediaData9FindInt32EPKcPi");
    G.find_buf=msym("_ZNK5movie9MediaData10FindBufferEPKcPPNS_6BufferE");
    G.size=msym("_ZNK5movie6Buffer4SizeEv");
    G.set_string=msym("_ZN5movie9MediaData9SetStringEPKcS2_");
    G.signal = msym("_ZN2nn2os11SignalEventEPNS0_9EventTypeE");
}
// 使用同一游戏 SDK 自带的软件 Vorbis；这些调用保留在客体线程上，沿用其分配器和线程环境。
// Use this game's SDK software Vorbis on the guest thread, preserving its allocator and thread context.
typedef struct {const uint8_t *data;int64_t size;int references;void *owner;} mv_ogg_buffer;
typedef struct mv_ogg_ref {mv_ogg_buffer *buffer;int64_t begin,length;struct mv_ogg_ref *next;} mv_ogg_ref;
typedef struct {mv_ogg_ref *packet;int64_t bytes,bos,eos,granule,number;} mv_ogg_packet;
typedef struct {mv_ogg_buffer buffer;mv_ogg_ref ref;mv_ogg_packet packet;} mv_packet_storage;
static void mv_packet_init(mv_packet_storage *s,const uint8_t *data,int64_t bytes,int number){
 memset(s,0,sizeof *s);s->buffer.data=data;s->buffer.size=bytes;s->ref.buffer=&s->buffer;s->ref.length=bytes;
 s->packet=(mv_ogg_packet){.packet=&s->ref,.bytes=bytes,.bos=number==0,.granule=-1,.number=number};
}
static struct {
 void (*info_init)(void *),(*comment_init)(void *),(*info_clear)(void *),(*comment_clear)(void *),(*destroy)(void *);
 int (*header)(void *,void *,mv_ogg_packet *),(*synthesis)(void *,mv_ogg_packet *,int),(*pcmout)(void *,int16_t *,int),(*read)(void *,int),(*restart)(void *);
 void *(*create)(void *);
} V;
static void mv_vorbis_syms(void){
 if(V.info_init)return;
 V.info_init=msym("_ZN11mm_internal16vorbis_info_initEPNS_11vorbis_infoE");
 V.comment_init=msym("_ZN11mm_internal19vorbis_comment_initEPNS_14vorbis_commentE");
 V.info_clear=msym("_ZN11mm_internal17vorbis_info_clearEPNS_11vorbis_infoE");
 V.comment_clear=msym("_ZN11mm_internal20vorbis_comment_clearEPNS_14vorbis_commentE");
 V.header=msym("_ZN11mm_internal19vorbis_dsp_headerinEPNS_11vorbis_infoEPNS_14vorbis_commentEPNS_10ogg_packetE");
 V.create=msym("_ZN11mm_internal17vorbis_dsp_createEPNS_11vorbis_infoE");
 V.destroy=msym("_ZN11mm_internal18vorbis_dsp_destroyEPNS_16vorbis_dsp_stateE");
 V.synthesis=msym("_ZN11mm_internal20vorbis_dsp_synthesisEPNS_16vorbis_dsp_stateEPNS_10ogg_packetEi");
 V.pcmout=msym("_ZN11mm_internal17vorbis_dsp_pcmoutEPNS_16vorbis_dsp_stateEPsi");
 V.read=msym("_ZN11mm_internal15vorbis_dsp_readEPNS_16vorbis_dsp_stateEi");
 V.restart=msym("_ZN11mm_internal18vorbis_dsp_restartEPNS_16vorbis_dsp_stateE");
}
static void mv_vorbis_clear(mdec_t *d){
 if(d->vdsp){V.destroy(d->vdsp);d->vdsp=NULL;}
 if(d->vorbis_on){V.info_clear(d->vi);V.comment_clear(d->vc);d->vorbis_on=0;}
}
static int mv_vorbis_config(mdec_t *d,void *md){
 mv_vorbis_syms();mv_vorbis_clear(d);V.info_init(d->vi);V.comment_init(d->vc);d->vorbis_on=1;
 void *info=NULL,*books=NULL;G.find_buf(md,"csd-0",&info);G.find_buf(md,"csd-1",&books);
 if(!info)G.find_buf(md,"vorbis-info",&info);if(!books)G.find_buf(md,"vorbis-books",&books);
 LOG("[影片] Vorbis 配置：info=%zu books=%zu",info?G.size(info):0,books?G.size(books):0);
 if(!info||!books)return 0x100b;
 static const uint8_t comment[]={3,'v','o','r','b','i','s',0,0,0,0,0,0,0,0,1};
 mv_packet_storage packet;mv_packet_init(&packet,G.data(info),G.size(info),0);
 int r=V.header(d->vi,d->vc,&packet.packet);if(r)return 0x100b;
 mv_packet_init(&packet,comment,sizeof comment,1);
 if(V.header(d->vi,d->vc,&packet.packet))return 0x100b;
 mv_packet_init(&packet,G.data(books),G.size(books),2);
 if(V.header(d->vi,d->vc,&packet.packet))return 0x100b;
 G.find_i32(md,"channel-count",&d->channels);G.find_i32(md,"sample-rate",&d->rate);
 if(d->channels<1||d->channels>8||d->rate<8000||d->rate>192000)return 0x100b;
 d->vdsp=V.create(d->vi);LOG("[影片] Vorbis 软件解码：%d Hz %d 声道 dsp=%p",d->rate,d->channels,d->vdsp);return d->vdsp?0:0x1000;
}
static int mv_vorbis_decode(mdec_t *d,const uint8_t *p,int n,int slot){
 if(!d->vdsp)return 0;
 mv_packet_storage packet;mv_packet_init(&packet,p,n,3);
 int r=V.synthesis(d->vdsp,&packet.packet,1);if(r){LOG("【红】影片 Vorbis 包解码失败 %d",r);return 0;}
 size_t capacity=(size_t)8192*d->channels*2;
 if(d->out[slot].cap<capacity){free(d->out[slot].frm);d->out[slot].frm=malloc(capacity);d->out[slot].cap=capacity;}
 int samples=V.pcmout(d->vdsp,(int16_t *)d->out[slot].frm,8192);if(samples<0||samples>8192){LOG("【红】影片 Vorbis PCM 数量异常 %d",samples);return 0;}
 if(samples>0)V.read(d->vdsp,samples);
 d->out[slot].bytes=(size_t)samples*d->channels*2;return samples>0;
}
static mdec_t *md_of(void **self) { return self ? (mdec_t *)self[1] : NULL; }
static void md_event(mdec_t *d, int i) { if (d->ev && d->ev[i]) G.signal(d->ev[i]); }   // 0 输入 1 输出 2 格式 3 出错 | 0 input 1 output 2 format 3 error
// 前 12 次逐条记，之后每 2^k 次记一次 | log the first 12 calls one by one, then every 2^k-th
#define MD_TRACE(d, slot, ...) do { unsigned long n_ = ++(d)->ncall[slot]; if (n_ <= 12 || !(n_ & (n_ - 1))) LOG("[影片] " __VA_ARGS__); } while (0)

// VP9 关键帧的未压缩帧头 → 宽高（VP9 规范 6.2：frame_marker、profile、show_existing_frame、frame_type、show_frame、error_resilient_mode、同步码、颜色配置、frame_size）
// VP9 key frame uncompressed header → width and height (VP9 spec 6.2: frame_marker, profile, show_existing_frame, frame_type, show_frame, error_resilient_mode, sync code, color config, frame_size)
static int vp9_size(const uint8_t *p, int n, int *w, int *h) {
    int bit = 0;
    #define RB(k) ({ unsigned v_ = 0; for (int i_ = 0; i_ < (k); i_++, bit++) { if (bit >> 3 >= n) return 0; v_ = v_ << 1 | (p[bit >> 3] >> (7 - (bit & 7)) & 1); } v_; })
    if (RB(2) != 2) return 0;
    unsigned lo = RB(1), hi = RB(1), prof = hi << 1 | lo;
    if (prof == 3) RB(1);
    if (RB(1)) return 0;                       // show_existing_frame
    if (RB(1) != 0) return 0;                  // 不是关键帧 | not a key frame
    RB(2);                                     // show_frame, error_resilient_mode
    if (RB(24) != 0x498342) return 0;
    if (prof >= 2) RB(1);
    if (RB(3) != 7) { RB(1); if (prof == 1 || prof == 3) RB(3); }
    else if (prof == 1 || prof == 3) RB(1);
    *w = (int)RB(16) + 1; *h = (int)RB(16) + 1;
    #undef RB
    return 1;
}
static void md_dims(const mdec_t *d, int *w, int *h, int *stride, int *slice) {
    *w = d->w > 0 ? d->w : 1920; *h = d->h > 0 ? d->h : 1080;
    *stride = *w; *slice = *h;
}

// ---------- 宿主解码线程 | Host decoding thread ----------
enum { MDJ_NONE, MDJ_DECODE, MDJ_CLOSE, MDJ_QUIT };
static int md_threads(void) { long n = sysconf(_SC_NPROCESSORS_ONLN); return n >= 8 ? 4 : 2; }
// I420 → NV12（行距 = 宽、平面高 = 高）；图比槽小的部分补黑，大的裁掉 | I420 → NV12 (stride = width, plane height = height); a smaller picture is padded with black, a larger one cropped
static void md_nv12(const vpx_image_t *im, uint8_t *dst, int w, int h) {
    int cw = (int)im->d_w < w ? (int)im->d_w : w, ch = (int)im->d_h < h ? (int)im->d_h : h;
    if (cw < w || ch < h) { memset(dst, 16, (size_t)w * h); memset(dst + (size_t)w * h, 128, (size_t)w * h / 2); }
    for (int y = 0; y < ch; y++) memcpy(dst + (size_t)y * w, im->planes[0] + (size_t)y * im->stride[0], cw);
    int cw2 = (cw + 1) / 2, ch2 = (ch + 1) / 2;
    for (int y = 0; y < ch2 && y < h / 2; y++) {
        const uint8_t *u = im->planes[1] + (size_t)y * im->stride[1], *v = im->planes[2] + (size_t)y * im->stride[2];
        uint8_t *o = dst + (size_t)w * h + (size_t)y * w;
        int x = 0;
        for (; x + 16 <= cw2; x += 16) { uint8x16x2_t t = { { vld1q_u8(u + x), vld1q_u8(v + x) } }; vst2q_u8(o + 2 * x, t); }
        for (; x < cw2 && 2 * x + 1 < w; x++) { o[2 * x] = u[x]; o[2 * x + 1] = v[x]; }
    }
}
// 解一包进输出槽 slot：1 = 出了一帧，0 = 没出（隐藏帧或出错） | decode one packet into output slot slot: 1 = a picture came out, 0 = none (hidden frame or error)
static int md_vp9(mdec_t *d, const uint8_t *p, int n, int slot) {
    if (!d->vpx_on) {
        vpx_codec_dec_cfg_t cfg = { .threads = (unsigned)md_threads(), .w = (unsigned)d->w, .h = (unsigned)d->h };
        vpx_codec_err_t e = vpx_codec_dec_init(&d->vpx, vpx_codec_vp9_dx(), &cfg, 0);
        if (e) { LOG("【红】影片：libvpx 建 VP9 解码器失败：%s", vpx_codec_err_to_string(e)); return 0; }
        vpx_codec_control(&d->vpx, VP9D_SET_ROW_MT, 1);
        d->vpx_on = 1;
        LOG("[影片] libvpx %s：VP9 解码器 %d 线程", vpx_codec_version_str(), cfg.threads);
    }
    if (vpx_codec_decode(&d->vpx, p, (unsigned)n, NULL, 0)) {
        if (!d->red_dec++) LOG("【红】影片：VP9 解码出错（首例，%d 字节）：%s %s", n, vpx_codec_error(&d->vpx), vpx_codec_error_detail(&d->vpx) ? vpx_codec_error_detail(&d->vpx) : "");
        return 0;
    }
    vpx_codec_iter_t it = NULL; vpx_image_t *im, *last = NULL;
    while ((im = vpx_codec_get_frame(&d->vpx, &it))) last = im;
    if (!last) return 0;
    if (last->fmt != VPX_IMG_FMT_I420) { if (!d->red_img++) LOG("【红】影片：VP9 画面格式 %#x 不是 I420（4:2:0 8 位），出黑帧", last->fmt); return 0; }
    if ((int)last->d_w != d->w || (int)last->d_h != d->h) { if (!d->red_img++) LOG("【红】影片：VP9 画面 %u×%u 与输出 %d×%d 不符（首例），裁剪 / 补黑", last->d_w, last->d_h, d->w, d->h); }
    size_t need = (size_t)d->w * d->h * 3 / 2;
    if (d->out[slot].cap < need) { free(d->out[slot].frm); d->out[slot].frm = malloc(need); d->out[slot].cap = need; }   // Reset 后再 Configure 可能换了尺寸 | a Configure after Reset may change the size
    md_nv12(last, d->out[slot].frm, d->w, d->h);
    return 1;
}
static void md_close(mdec_t *d) { if (d->vpx_on) { vpx_codec_destroy(&d->vpx); d->vpx_on = 0; } }
static void *md_worker(void *arg) {
    mdec_t *d = arg;
    pthread_setname_np("dm 影片解码 | movie decoder");
    // Decoding is on the video presentation path; keep it at the same QoS
    // as shader warm-up instead of inheriting a guest worker's default QoS.
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
    pthread_mutex_lock(&d->wmu);
    for (;;) {
        while (d->job == MDJ_NONE) pthread_cond_wait(&d->wcv, &d->wmu);
        int job = d->job;
        pthread_mutex_unlock(&d->wmu);
        if (job == MDJ_DECODE) {
            uint64_t t0 = now_ns();
            d->jgot = md_vp9(d, d->jp, d->jn, d->jslot);
            uint64_t dt = now_ns() - t0; d->dec_n++; d->dec_ns += dt; if (dt > d->dec_max) d->dec_max = dt;
        }
        else md_close(d);
        pthread_mutex_lock(&d->wmu);
        d->job = MDJ_NONE; pthread_cond_broadcast(&d->wcv);
        if (job == MDJ_QUIT) break;
    }
    pthread_mutex_unlock(&d->wmu);
    return NULL;
}
// 把一件活交给宿主线程并等它做完（调用方持 dmu） | hand one job to the host thread and wait for it (the caller holds dmu)
static void md_job(mdec_t *d, int job) {
    pthread_mutex_lock(&d->wmu);
    d->job = job; pthread_cond_broadcast(&d->wcv);
    while (d->job != MDJ_NONE) pthread_cond_wait(&d->wcv, &d->wmu);
    pthread_mutex_unlock(&d->wmu);
}
// 有包排队又有空输出槽就解队头，直到队空或槽满；按结果发输入 / 输出事件 | while packets are queued and an output slot is free, decode the head packet, until the queue is empty or every slot is taken; signal the input / output events by the result
static void md_pump(mdec_t *d) {
    pthread_mutex_lock(&d->dmu);
    for (;;) {
        pthread_mutex_lock(&d->mu);
        int slot = -1;
        if (d->started && d->npend) for (int k = 0; k < MD_NOUT; k++) if (!d->out[k].used) { slot = k; break; }
        if (slot < 0) { pthread_mutex_unlock(&d->mu); break; }
        __typeof__(d->pend[0]) pk = d->pend[0];
        memmove(d->pend, d->pend + 1, --d->npend * sizeof *d->pend);
        d->out[slot].used = 1;d->out[slot].bytes=0;
        pthread_mutex_unlock(&d->mu);
        int got = 0;
        if (pk.size > 0 && d->type == MD_TYPE_VP9) { d->jp = d->in[pk.in] + pk.off; d->jn = pk.size; d->jslot = slot; md_job(d, MDJ_DECODE); got = d->jgot; }
        if(pk.size>0 && d->type==5)got=mv_vorbis_decode(d,d->in[pk.in]+pk.off,pk.size,slot);
        int eos = (pk.flags & 4) != 0;   // MediaCodec BUFFER_FLAG_END_OF_STREAM
        pthread_mutex_lock(&d->mu);
        d->infree[d->ninfree++] = pk.in;
        int ready = got || eos || (d->type != MD_TYPE_VP9 && d->type != 5);   // 没解出画面的流结束包也要出一个输出，把标志带回去；AVC / VP8 没有解码器，出黑帧 | an end-of-stream packet without a picture still yields an output to carry the flag back; AVC / VP8 have no decoder and give black frames
        if (ready) { d->out[slot].pic = got; d->out[slot].pts = pk.pts; d->out[slot].flags = pk.flags; d->outq[d->noutq++] = slot; }
        else d->out[slot].used = 0;
        pthread_mutex_unlock(&d->mu);
        md_event(d, 0);
        if (ready) md_event(d, 1);
    }
    pthread_mutex_unlock(&d->dmu);
}

HLE_FORCE("_ZN5movie7DecoderC1ENS_11DecoderTypeENS_19DecoderOutputFormatENS_11DecoderModeEi", "影片解码器：宿主实现（不建 NVIDIA 硬件解码器，VP9 用 libvpx）",
          void, h_movie_Decoder, void **self, int type, int fmt, int mode, int prio) {
    static void *vt; if (!vt) { uint8_t *t = guest_sym("_ZTVN5movie7DecoderE"); if (!t) die("影片：缺 movie::Decoder 的虚表"); vt = t + 0x10; }
    md_syms();
    self[0] = vt; self[1] = NULL;
    // 视频输出 NV12；当前动森版本另支持 Vorbis → PCM16，其他音频类型仍返回失败。
    // Video outputs NV12; this ACNH build also supports Vorbis to PCM16. Other audio codecs still fail.
    if (!((type>=1 && type<=3 && fmt==MD_FMT_NV12) || (type==5 && fmt==1 && getenv("ACNH_MOVIE_NATIVE") && !strcmp(dm_main_build(),"ff1d1c05670db6021c85b624a710b963")))) { LOG("【红】影片：解码器类型 %d 输出 %d 模式 %d 宿主不支持，各方法回失败", type, fmt, mode); return; }
    if (type != MD_TYPE_VP9 && type!=5) LOG("【红】影片：解码器类型 %d（AVC / VP8）宿主不解码，出黑帧", type);
    mdec_t *d = calloc(1, sizeof *d);
    pthread_mutex_init(&d->mu, NULL); pthread_mutex_init(&d->dmu, NULL); pthread_mutex_init(&d->wmu, NULL); pthread_cond_init(&d->wcv, NULL);
    d->type = type; d->fmt = fmt; d->mode = mode;
    for (int i = 0; i < MD_NIN; i++) { d->in[i] = malloc(MD_INCAP); d->infree[i] = i; }
    d->ninfree = MD_NIN;
    pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, 1 << 20);
    if (pthread_create(&d->wk, &a, md_worker, d)) die("影片：建解码线程失败");
    pthread_attr_destroy(&a);
    self[1] = d;
    LOG("[影片] 解码器 %p：类型 %d 输出 %d 模式 %d 优先级 %d", (void *)self, type, fmt, mode, prio);
}
static void md_free(void **self) {
    mdec_t *d = md_of(self); if (!d) return;
    self[1] = NULL;
    LOG("[影片] 解码器 %p 析构：送入 %lu 包，取出 %lu 帧；VP9 解 %llu 包，每包均 %.2f ms、最长 %.2f ms", (void *)self, d->ncall[7], d->ncall[8],
        (unsigned long long)d->dec_n, d->dec_n ? d->dec_ns / 1e6 / d->dec_n : 0.0, d->dec_max / 1e6);
    pthread_mutex_lock(&d->dmu); md_job(d, MDJ_QUIT); pthread_mutex_unlock(&d->dmu);
    pthread_join(d->wk, NULL);
    if(d->type==5)mv_vorbis_clear(d);
    for (int i = 0; i < MD_NIN; i++) free(d->in[i]);
    for (int k = 0; k < MD_NOUT; k++) free(d->out[k].frm);
    pthread_mutex_destroy(&d->mu); pthread_mutex_destroy(&d->dmu); pthread_mutex_destroy(&d->wmu); pthread_cond_destroy(&d->wcv); free(d);
}
HLE_FORCE("_ZN5movie7DecoderD2Ev", "影片解码器析构：释放宿主对象", void, h_movie_DecD2, void **self) { md_free(self); }
// 删除式析构：Unity 用自己的分配器建对象、先调 D2 再自己释放，这里不释放对象本身 | deleting destructor: Unity allocates the object with its own allocator, calls D2 and frees it itself; the object itself is not freed here
HLE_FORCE("_ZN5movie7DecoderD0Ev", "影片解码器删除式析构：释放宿主对象", void, h_movie_DecD0, void **self) { LOG("【红】影片：删除式析构（对象本身未释放）"); md_free(self); }

HLE_FORCE("_ZN5movie7Decoder9ConfigureEPNS_9MediaDataE", "影片解码器配置", int, h_movie_Configure, void **self, void *md) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if(d->type==5)return md?mv_vorbis_config(d,md):0x100b;
    int w = 0, h = 0; if (md) { G.find_i32(md, "width", &w); G.find_i32(md, "height", &h); }
    pthread_mutex_lock(&d->mu); if (w > 0 && h > 0 && !d->fmt_sent) { d->w = w; d->h = h; } pthread_mutex_unlock(&d->mu);
    MD_TRACE(d, 0, "Configure：MediaData 宽 %d 高 %d", w, h);
    return 0;
}
HLE_FORCE("_ZN5movie7Decoder20RegisterClientEventsEPNS_13DecoderEventsE", "影片解码器：登记客户端事件", int, h_movie_RegEv, void **self, void **ev) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    d->ev = ev; MD_TRACE(d, 1, "RegisterClientEvents：%p %p %p %p", ev ? ev[0] : 0, ev ? ev[1] : 0, ev ? ev[2] : 0, ev ? ev[3] : 0);
    return 0;
}
HLE_FORCE("_ZN5movie7Decoder5StartEv", "影片解码器开始：输入全空，通知客户端", int, h_movie_Start, void **self) {
    mdec_t *d = md_of(self); if (!d) return 0x2002;
    pthread_mutex_lock(&d->dmu);
    pthread_mutex_lock(&d->mu); d->started = 1; pthread_mutex_unlock(&d->mu);
    pthread_mutex_unlock(&d->dmu);
    MD_TRACE(d, 2, "Start");
    md_event(d, 0);
    return 0;
}
// 清空队列与槽；close 时连 libvpx 解码器也关掉（下一包重建） | clear the queues and slots; with close, the libvpx decoder is closed too (rebuilt at the next packet)
static void md_reset(mdec_t *d, int close) {
    pthread_mutex_lock(&d->dmu);   // 等正在解的那包做完 | wait for the packet being decoded
    // Publish the stopped state before closing libvpx. A late sender must not
    // recreate it from an inter-frame packet after the skip button was pressed.
    if (close) { pthread_mutex_lock(&d->mu); d->started = 0; pthread_mutex_unlock(&d->mu); md_job(d, MDJ_CLOSE); }
    if(d->type==5 && d->vdsp)V.restart(d->vdsp);
    pthread_mutex_lock(&d->mu);
    for (int i = 0; i < MD_NIN; i++) d->infree[i] = i;
    d->ninfree = MD_NIN; d->noutq = 0; d->npend = 0;d->input_eos=0;
    for (int i = 0; i < MD_NOUT; i++) d->out[i].used = d->out[i].pic = 0;
    pthread_mutex_unlock(&d->mu);
    pthread_mutex_unlock(&d->dmu);
}
HLE_FORCE("_ZN5movie7Decoder4StopEv", "影片解码器停止", int, h_movie_Stop, void **self) {
    mdec_t *d = md_of(self); if (!d) return 0x2003;
    MD_TRACE(d, 3, "Stop"); md_reset(d, 1); return 0;
}
HLE_FORCE("_ZN5movie7Decoder5ResetEv", "影片解码器复位", int, h_movie_Reset, void **self) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    MD_TRACE(d, 4, "Reset"); md_reset(d, 1); d->fmt_sent = 0; return 0;
}
// 清空：输入输出都还回去，libvpx 保留（跳转后的下一包是关键帧） | flush: inputs and outputs all go back; libvpx is kept (the next packet after a seek is a key frame)
HLE_FORCE("_ZN5movie7Decoder5FlushEv", "影片解码器清空：输入输出都还回去", int, h_movie_Flush, void **self) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    MD_TRACE(d, 5, "Flush"); md_reset(d, 0); md_event(d, 0); return 0;
}
HLE_FORCE("_ZN5movie7Decoder23AcquireInputBufferIndexEPiS1_", "影片解码器：取空输入号", int, h_movie_AcqIn, void **self, int *idx, int *left) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if (!idx || !left) return 0x100b;
    pthread_mutex_lock(&d->mu);
    int r = MD_NO_INPUT; *idx = -1; *left = 0;
    if (d->started && !d->input_eos && d->ninfree) { *idx = d->infree[0]; memmove(d->infree, d->infree + 1, --d->ninfree * sizeof *d->infree); *left = d->ninfree; r = 0; }
    pthread_mutex_unlock(&d->mu);
    MD_TRACE(d, 6, "AcquireInputBufferIndex → %#x 号 %d 余 %d", r, *idx, *left);
    return r;
}
HLE_FORCE("_ZN5movie7Decoder14GetInputBufferEiPNS_6BufferE", "影片解码器：交出输入缓冲", int, h_movie_GetIn, void **self, int i, void *buf) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if (!buf || i < 0 || i >= MD_NIN) return 0x100b;
    G.set_data(buf, d->in[i], MD_INCAP); G.set_range(buf, 0, (int)MD_INCAP);
    return 0;
}
HLE_FORCE("_ZN5movie7Decoder24SendInputBufferForDecodeEiiilj", "影片解码器：送一包解码（VP9 用 libvpx）", int, h_movie_SendIn,
          void **self, int i, int off, int size, int64_t pts, uint32_t flags) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if (i < 0 || i >= MD_NIN || off < 0 || size < 0 || (uint32_t)off + (uint32_t)size > MD_INCAP) return 0x100b;
    pthread_mutex_lock(&d->mu);
    // EOS 后不再交出输入槽，防止读到文件尾的线程反复空送包占满 CPU。
    // Stop issuing input slots after EOS, so the source thread cannot spin on empty end-of-file packets.
    if(!d->started || d->npend >= MD_NIN){pthread_mutex_unlock(&d->mu);return 0x2003;}
    if(d->input_eos){pthread_mutex_unlock(&d->mu);return (flags&4)?0:0x2002;}
    if(flags&4)d->input_eos=1;
    int fmt_now = 0;
    if (!d->w && size > 0 && vp9_size(d->in[i] + off, size, &d->w, &d->h)) LOG("[影片] VP9 关键帧：%d×%d", d->w, d->h);
    if (!d->fmt_sent) { d->fmt_sent = 1; fmt_now = 1; if (!d->w) { d->w = 1920; d->h = 1080; } }   // 尺寸从此定下（解码按它分配槽） | the size is fixed from here on (slots are allocated by it)
    d->pend[d->npend].in = i; d->pend[d->npend].off = off; d->pend[d->npend].size = size; d->pend[d->npend].pts = pts; d->pend[d->npend].flags = flags; d->npend++;
    int queued = d->npend;
    pthread_mutex_unlock(&d->mu);
    MD_TRACE(d, 7, "SendInputBufferForDecode 号 %d 偏移 %d 长 %d 时间 %lld µs 标志 %#x（队里 %d 包）", i, off, size, (long long)pts, flags, queued);
    if (fmt_now) md_event(d, 2);
    md_pump(d);
    return 0;
}

HLE_FORCE("_ZN5movie7Decoder24AcquireOutputBufferIndexEPiPlPjS1_", "影片解码器：取就绪输出号", int, h_movie_AcqOut,
          void **self, int *idx, int64_t *pts, uint32_t *flags, int *left) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if (!idx || !pts || !flags || !left) return 0x100b;
    pthread_mutex_lock(&d->mu);
    int r = MD_NO_OUTPUT; *idx = -1; *left = 0;
    if (d->noutq) {
        int s = d->outq[0]; memmove(d->outq, d->outq + 1, --d->noutq * sizeof *d->outq);
        *idx = s; *pts = d->out[s].pts; *flags = d->out[s].flags; *left = d->noutq; r = 0;
    }
    pthread_mutex_unlock(&d->mu);
    if (!r) MD_TRACE(d, 8, "AcquireOutputBufferIndex → 号 %d 时间 %lld 标志 %#x 余 %d", *idx, (long long)*pts, *flags, *left);
    return r;
}
static void md_format(mdec_t *d, void *md) {
    if(d->type==5){G.set_i32(md,"sample-rate",d->rate);G.set_i32(md,"channel-count",d->channels);G.set_i32(md,"pcm-encoding",2);G.set_string(md,"mime","audio/raw");return;}
    int w, h, stride, slice; md_dims(d, &w, &h, &stride, &slice);
    G.set_i32(md, "width", w); G.set_i32(md, "height", h);
    G.set_rect(md, "crop", 0, 0, w - 1, h - 1);
    G.set_i32(md, "stride", stride); G.set_i32(md, "slice-height", slice);
    G.set_i32(md, "nv12-y-stride", stride); G.set_i32(md, "nv12-y-offset", 0); G.set_i32(md, "nv12-uv-offset", stride * slice);
}
HLE_FORCE("_ZN5movie7Decoder15GetOutputFormatEPNS_9MediaDataE", "影片解码器：输出格式（NV12）", int, h_movie_OutFmt, void **self, void *md) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if (!md) return 0x100b;
    md_format(d, md); MD_TRACE(d, 9, "GetOutputFormat");
    return 0;
}
HLE_FORCE("_ZN5movie7Decoder21GetOutputBufferFormatEiPNS_9MediaDataE", "影片解码器：输出帧格式（NV12）", int, h_movie_BufFmt, void **self, int i, void *md) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if (!md) return 0x100b;
    md_format(d, md); MD_TRACE(d, 10, "GetOutputBufferFormat 号 %d", i);
    return 0;
}
HLE_FORCE("_ZN5movie7Decoder19GetOutputBufferSizeEPiS1_", "影片解码器：输出缓冲大小（NV12 一帧）", int, h_movie_OutSize, void **self, int *size, int *mode) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if (!size || !mode) return 0x100b;
    int w, h, stride, slice; md_dims(d, &w, &h, &stride, &slice);
    *size = d->type==5?8192*8*2:stride * slice * 3 / 2; *mode = d->mode;   // SDK 第二个出参填解码器模式 | the SDK fills the second output with the decoder mode
    MD_TRACE(d, 11, "GetOutputBufferSize → %d（%d×%d）", *size, w, h);
    return 0;
}
// 把一帧写进客户端缓冲：解出的画面，没有画面（流结束包、AVC / VP8）时是黑帧（视频范围 Y 16、UV 128）
// Write one frame into the client buffer: the decoded picture, or a black frame (video range Y 16, UV 128) when there is none (end-of-stream packet, AVC / VP8)
HLE_FORCE("_ZN5movie7Decoder15GetOutputBufferEiPNS_6BufferE", "影片解码器：取一帧", int, h_movie_GetOut, void **self, int i, void *buf) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if (!buf || i < 0 || i >= MD_NOUT) return 0x100b;
    pthread_mutex_lock(&d->mu);
    if (!d->started || !d->out[i].used) { pthread_mutex_unlock(&d->mu); return 0x2003; }
    int w, h, stride, slice; md_dims(d, &w, &h, &stride, &slice);
    size_t need = d->type==5?d->out[i].bytes:(size_t)stride * slice * 3 / 2, cap = G.cap(buf);
    uint8_t *p = G.data(buf);
    if (!p || cap < need) { LOG("【红】影片：客户端输出缓冲 %zu 字节，一帧要 %zu", cap, need); G.set_range(buf, 0, 0); pthread_mutex_unlock(&d->mu); return 0x100b; }
    if (d->type==5 || (d->out[i].pic && d->out[i].frm)) {if(need)memcpy(p, d->out[i].frm, need);}
    else { memset(p, 16, (size_t)stride * slice); memset(p + (size_t)stride * slice, 128, need - (size_t)stride * slice); }
    G.set_range(buf, 0, (int)need);
    MD_TRACE(d, 12, "GetOutputBuffer 号 %d → %zu 字节%s", i, need, d->out[i].pic ? "" : "（黑帧）");
    pthread_mutex_unlock(&d->mu);
    return 0;
}
HLE_FORCE("_ZN5movie7Decoder15GetOutputBufferEiPNS_6BufferEPNS_9MediaDataE", "影片解码器：取一帧连格式", int, h_movie_GetOutFmt, void **self, int i, void *buf, void *md) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if (md) md_format(d, md);
    return h_movie_GetOut(self, i, buf);
}
HLE_FORCE("_ZN5movie7Decoder24ReleaseOutputBufferIndexEi", "影片解码器：还输出号（再解排队的包）", int, h_movie_RelOut, void **self, int i) {
    mdec_t *d = md_of(self); if (!d) return 0x1000;
    if (i < 0 || i >= MD_NOUT) return 0x100b;
    pthread_mutex_lock(&d->mu);
    d->out[i].used = 0; d->out[i].pic = 0;
    int queued = d->npend;
    pthread_mutex_unlock(&d->mu);
    MD_TRACE(d, 13, "ReleaseOutputBufferIndex 号 %d（队里 %d 包）", i, queued);
    if (queued) md_pump(d);
    return 0;
}
HLE_FORCE("_ZN5movie7Decoder12GetLastErrorEv", "影片解码器：最近错误（无）", int, h_movie_LastErr, void **self) { (void)self; return 0; }

#if !TARGET_OS_IPHONE
// 本机集成测试：用独立分配器读取真实影片，解出音轨前几秒；不进入手机包。
// Mac integration probe: read the actual movie with an isolated allocator and decode its audio; excluded from iOS.
static void *mv_probe_alloc(size_t n,size_t align,void *u){(void)u;void *p=NULL;if(align<sizeof(void *))align=sizeof(void *);return posix_memalign(&p,align,n?n:1)?NULL:p;}
static void mv_probe_free(void *p,void *u){(void)u;free(p);}
static void *mv_probe_realloc(void *p,size_t n,void *u){(void)u;return realloc(p,n);}
void dm_acnh_movie_probe(void){
 static int done;if(done||!getenv("ACNH_MOVIE_PROBE"))return;done=1;
 void (*allocator)(void *,void *,void *,void *)=msym("_ZN5movie12SetAllocatorEPFPvmmS0_EPFvS0_S0_EPFS0_S0_mS0_ES0_");
 void (*heap_init)(void)=msym("_ZN2nv2mm16InitializeMMHeapEv");heap_init();
 allocator(mv_probe_alloc,mv_probe_free,mv_probe_realloc,NULL);
 mv_vorbis_syms();mdec_t *d=calloc(1,sizeof *d);d->type=5;d->channels=2;d->rate=48000;V.info_init(d->vi);V.comment_init(d->vc);d->vorbis_on=1;
 FILE *f=fopen("work/acnh-vorbis-probe.packets","rb"),*pcm=fopen("work/acnh-vorbis-probe.pcm","wb");if(!f)return;
 uint8_t packet[65536];uint32_t n;int k=0,frames=0,peak=0;
 while(fread(&n,4,1,f)==1 && n<=sizeof packet && fread(packet,1,n,f)==n){
  if(k<3){mv_packet_storage p;mv_packet_init(&p,packet,n,k);int r=V.header(d->vi,d->vc,&p.packet);LOG("ACNH Vorbis probe header %d result=%d",k,r);if(r)break;if(k==2){d->vdsp=V.create(d->vi);if(!d->vdsp)break;}}
  else if(mv_vorbis_decode(d,packet,n,0)){int16_t *samples=(int16_t *)d->out[0].frm;for(size_t j=0;j<d->out[0].bytes/2;j++){int v=abs(samples[j]);if(v>peak)peak=v;}if(pcm)fwrite(samples,1,d->out[0].bytes,pcm);frames++;}
  k++;
 }
 fclose(f);if(pcm)fclose(pcm);LOG("ACNH Vorbis probe: packets=%d frames=%d peak=%d",k-3,frames,peak);mv_vorbis_clear(d);free(d->out[0].frm);free(d);
 void *decoder[2]={0};h_movie_Decoder(decoder,3,4,1,20);h_movie_Start(decoder);int index,left;int r=h_movie_AcqIn(decoder,&index,&left);
 if(!r)r=h_movie_SendIn(decoder,index,0,0,0,4);int64_t pts;uint32_t flags=0;
 if(!r)r=h_movie_AcqOut(decoder,&index,&pts,&flags,&left);
 if(!r)h_movie_RelOut(decoder,index);
 int in=h_movie_AcqIn(decoder,&index,&left),out=h_movie_AcqOut(decoder,&index,&pts,&flags,&left);
 LOG("ACNH EOS probe: first=%#x flags=%#x next-input=%#x next-output=%#x",r,flags,in,out);md_free(decoder);

}
#endif
