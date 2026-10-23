# Game-free shell assembly

The release IPA contains the game-free runtime and is not playable by itself. Importing external `.data` cannot supply the missing executable or shader cache.

Use source from the exact release, Python with FontTools, and your own official rewritten 3.0.3 modules, complete matching iOS shader cache and font:

```sh
./setup.sh build \
  --shell /path/to/ACNHMobile-iOS26-v0.3.0-build2-shell-unsigned.ipa \
  --patched /path/to/owned/patched \
  --romfs /path/to/owned/RomFS \
  --shaders /path/to/owned/shaders/ios \
  --font /path/to/owned/font.ttf \
  --work /path/to/private/work \
  --output /path/to/private/ACNHMobile.ipa
```

`--shell` is optional. Omit it to compile an embedded private IPA using Xcode, preserving the existing user build interface. With it, the source fingerprint is checked and owned executable images are packed into Guest.framework; no Xcode compilation is required at assembly time. Sign the entire output, including Guest.framework. Matching `.data` remains external and directly read. Never upload the assembled IPA.

The release shell has no program images, shaders, game resources, keys, saves or font. The application icon is retained. See BUILD.md for official shader chain requirements and device validation limits.
