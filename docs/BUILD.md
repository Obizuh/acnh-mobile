# Private local build

Use an Apple Silicon Mac, Xcode 26 with Metal tools, XcodeGen and Python 3.10+ with FontTools. All game inputs must be your own. Neither the repository nor GitHub Actions obtains game files or keys.

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
./setup.sh check
./setup.sh build \
  --patched /path/to/owned/patched \
  --romfs /path/to/owned/RomFS \
  --shaders /path/to/owned/shaders/ios \
  --font /path/to/owned/font.ttf \
  --work /path/to/private/work \
  --output /path/to/private/ACNHMobile.ipa
```

`--patched` contains official rewritten `main`, `subsdk0`, `sdk` images and their fixups/gates/cuts/obs tables. Supported 3.0.3 build IDs are recorded in `dm/games/01006F8002326000/game.tsv`. Use rewritten inputs matching this official source, not outputs from older project releases.

`--shaders` is the parent of the complete matching `b-<chain>` cache, including reflection JSON and shadow-input variants. The v0.3.8 geometry integration uses `b-ade9bce36598` with the pinned upstream tools. Rebuild older shader caches; external RomFS `.data` remains compatible. Do not mix old project shader patches, chain metadata or translation binaries with the official runtime. For fresh extraction/rewriting/translation, follow the official source procedure in the upstream [README](https://github.com/seedds/nxnative/tree/e72a06510e6dfff4a77c49fd205a82e21cad0786); per-game tools are retained under `dm/`. Translating with a different toolchain requires regenerating the chain header, Mac/iOS shaders, c1/constant-buffer tables together and rebuilding the runtime. The checked-in chain lock is provenance, not a redistributable translator binary.

RomFS is checked as a local input but stays external. Existing 3.0.3 `.data` files work. To create a new pack:

```sh
./setup.sh data --romfs /path/to/owned/RomFS --output /path/to/private/data
```

Copy all `.data` files, including `ACNH-index.data`, to `Documents/ACNHMobile`. No extraction occurs on the phone. The JSON manifest generated alongside the pack is a tooling record; the phone reads the `.data` index.

The full IPA is private and contains game-derived modules and shaders. Do not publish it. Sign it yourself; this project does not sign or install on devices.
