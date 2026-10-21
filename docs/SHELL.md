# Game-free iOS shell

Public downloads are unsigned, game-free IPA shells: `ACNHMobile-iOS26` (minimum iOS26) and `ACNHMobile-iOS18-compat` (minimum iOS18, Metal3). They contain the compatibility runtime only. Signing a shell or importing `.data` alone does not supply the missing game program, translated shaders, fonts or other owned runtime files.

In GitHub Actions, run **Build game-free iOS shell** on `main`. Its artifact contains both IPA shells, an audit report and checksums. A matching `v*` tag publishes one formal Release after both builds and audits pass; the first main push at 0.1.21/build34 also starts that release automatically. Release attachments are exactly the two shells and `SHA256SUMS.txt`. Existing releases are never replaced. Manual runs with both tag fields empty produce artifacts only. The compatibility source must already exist on `ios18-compat` with the same version/build. No game inputs or Apple signing secrets are used by CI. Publishing a Release manually does not trigger a competing build.

To prepare a private installable IPA, use the matching source release on an Apple Silicon Mac. Install the dependencies in [BUILD.md](BUILD.md), then run `setup.sh rewrite` and `setup.sh shaders` against your own decrypted files. Run:

```sh
python tools/pack_shell.py --shell /path/to/ACNHMobile-iOS26-v0.1.21-build34-shell-unsigned.ipa \
  --patched /path/to/private/patched --shaders /path/to/private/shaders/ios \
  --font /path/to/standard.ttf --output /path/to/private/ACNHMobile-unsigned.ipa
```

Sign the whole IPA, including Guest.framework, before installing. This resulting private IPA contains game-derived code/shaders and must not be published as a shell. Resource `.data` files remain external in `Documents/ACNHMobile` and are read without extraction. Full preparation on Windows/Linux is not currently provided.

Use the source branch matching your shell edition; mixing fingerprints is rejected. Local shell builds still emit `ACNHMobile-shell-build-input.zip`, which is also accepted by the packer.

To build the public shell locally without game inputs: `python tools/build_shell.py --work /path/to/new-work --output /path/to/shell-output`.

Local iOS compilation and package structure/content checks passed. This shell path still needs phone signing and gameplay verification. The hosted workflow needs its first run after the source is uploaded.
