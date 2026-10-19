# Local build

Use your own decrypted ACNH 3.0.3 ExeFS (`main`, `subsdk0`, `sdk`) and RomFS. No keys are included or acquired. Install the dependencies in the root README; run `./setup.sh check` first. Public NuGet package restoration may need network access.

```sh
./setup.sh rewrite --exefs /path/to/ExeFS --output /path/to/private-work/patched
./setup.sh shaders --exefs /path/to/ExeFS --romfs /path/to/RomFS --output /path/to/private-work/shaders
./setup.sh build --patched /path/to/private-work/patched --romfs /path/to/RomFS \
  --shaders /path/to/private-work/shaders/ios --font /path/to/your-standard-font.ttf \
  --work /path/to/private-work/app --output /path/to/private-work/ACNHMobile.ipa
```

The shader step rebuilds the public translator source, refreshes local tool fingerprints and the shader chain, and applies the position-invariance and map alpha-test fixes. Never mix chain versions. Supply your own standard font.

RomFS stays external, but the private IPA still embeds original game modules and translated shaders. It is not a content-free shell and must not be included in a source release. All generated files belong outside the public archive. Sign/install locally using your own method.

See [the detailed Chinese build guide](BUILD.zh-CN.md), [game-data preparation](GAME_FILES.md), and the release audit tool `python3 tools/audit_release.py .`.
