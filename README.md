# ACNH Mobile

Experimental native compatibility layer for Animal Crossing: New Horizons **3.0.3** on iPhone / iPad, with an Apple Silicon Mac development host. Based on the open-source D2R Mobile compatibility layer, with ACNH-specific graphics, input, storage and system-service support.

**Source and tools only. No game program, game assets, translated shaders, keys or saves are distributed.** Users prepare all game-derived runtime files locally from their own files. A locally generated IPA embeds the user's game modules and translated shaders; it is not part of the source-only release.

[简体中文（完整说明）](README.zh-CN.md) · [Build](docs/BUILD.md) · [Game files](docs/GAME_FILES.md) · [Release contents](docs/DERIVED.md)

Maintenance build 12, 2026-10-19. The iOS project currently targets iOS 26+ using the Metal 3 backend. This is experimental; complete gameplay and all device configurations have not been verified.

Current changes include direct `.data` access, save compatibility, local mailbox support, map alpha tests, vertex position invariance, native text input, touch controls, 30 fps/720p defaults, host timezone handling and bounded iPhone haptic feedback. Phone haptic feel still needs device verification.

On an Apple Silicon Mac, install Xcode with Metal tools, XcodeGen, .NET 10, spirv-cross, zstd and Python 3.10+, then:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
./setup.sh check
```

Follow [BUILD](docs/BUILD.md) to prepare your own decrypted ExeFS/RomFS, translate shaders and build a private unsigned IPA. The tools do not obtain game files or keys. Publish only the audited source archive, not locally generated game-containing builds.

Credits: [D2R Mobile](https://github.com/jhaizhou-ops/d2r-mobile), the MIT upstream compatibility layer, and the projects credited in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES.md). Preserve [LICENSE](LICENSE) and all third-party licenses. This project is not affiliated with Nintendo.
