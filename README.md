# ACNH Mobile

Current release: **v0.1.6 (build 21)** adds only the map/UI alpha-test shader fix and packaging checks on top of v0.1.5 build 17. It does not include the separate CPU/GPU/memory optimization branch.

v0.1.4 was reissued with vertex position-invariance verification for leaf flicker. Regenerate older shader packs before building.

Latest update: **v0.1.4** fixes missing houses/bridges and blank NPC dialogue on developed islands by growing the Metal 3 buffer registry beyond 512 entries. Verified with a two-resident save on the Mac host and a 1537-buffer regression test; phone verification remains pending.

Experimental native compatibility layer for Animal Crossing: New Horizons **3.0.3** on iPhone / iPad, with an Apple Silicon Mac development host. Based on the open-source D2R Mobile compatibility layer, with ACNH-specific graphics, input, storage and system-service support.

****Public downloads contain source/tools and a game-free runtime shell. No game program, playable game assets, translated game shaders, keys or saves are distributed. The bundled app-identification icon is the sole artwork exception.**** Users prepare all game-derived runtime files locally from their own files. A locally generated IPA embeds the user's game modules and translated shaders; it is not part of the source-only release.

[简体中文（完整说明）](README.zh-CN.md) · [Build](docs/BUILD.md) · [Game files](docs/GAME_FILES.md) · [Release contents](docs/DERIVED.md)

Maintenance build 12, 2026-10-19. The iOS project currently targets iOS 26+ using the Metal 3 backend. This is experimental; complete gameplay and all device configurations have not been verified.

Current changes include direct `.data` access, save compatibility, local mailbox support, map alpha tests, vertex position invariance, native text input, touch controls, 30 fps/720p defaults, host timezone handling and bounded iPhone haptic feedback. Phone haptic feel still needs device verification.

Latest fixes (2026-10-20): local screenshot service for shutter crashes, and a targeted workaround for yellow shoreline splash prepasses. Mac shoreline regression captured 300 frames with no yellow artifacts versus 34 affected frames in the baseline. iOS Release compilation passed; device verification is still required. See the [maintenance log](docs/CHANGELOG.zh-CN.md).

GitHub shell builds and local IPA builds use the bundled project icon automatically; no `DM_ICON` setting is required.

## Download or build a game-free shell

Open **Actions → Build game-free iOS shell → Run workflow**. After it finishes, download the artifact and unpack it to obtain `ACNHMobile-shell-build-input.zip`. Version tags (`v*`) automatically publish a pre-release with the shell attached. To publish an existing tag manually, enter it in the workflow’s `release_tag` field; leaving that field empty creates an artifact only.

The shell is **not directly playable or installable**. Use your own game files on an Apple Silicon Mac to prepare the program and shaders, then assemble and sign a private IPA. Existing `.data` resources stay external and are read without extraction. See [Shell guide](docs/SHELL.md) for the exact steps and current verification limits.

## Build locally

On an Apple Silicon Mac, install Xcode with Metal tools, XcodeGen, .NET 10, spirv-cross, zstd and Python 3.10+, then:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
./setup.sh check
```

Follow [BUILD](docs/BUILD.md) to prepare your own decrypted ExeFS/RomFS, translate shaders and build a private unsigned IPA. The tools do not obtain game files or keys. Publish only audited sources and game-free shells, not locally generated game-containing builds.

Credits: [D2R Mobile](https://github.com/jhaizhou-ops/d2r-mobile), the MIT upstream compatibility layer, and the projects credited in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES.md). Preserve [LICENSE](LICENSE) and all third-party licenses. This project is not affiliated with Nintendo.
