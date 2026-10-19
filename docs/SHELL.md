# Game-free iOS shell

`ACNHMobile-shell-build-input.zip` contains only the compatibility runtime. No game executable, translated game shaders, assets, keys, saves or fonts are included. It is not directly playable or installable. Renaming it to IPA or importing `.data` alone does not supply the missing program.

In GitHub Actions, run **Build game-free iOS shell** on `main`. Download its artifact and unpack the outer ZIP. The inner shell ZIP, audit and checksums are the release assets. Pushing a `v*` tag automatically publishes a **pre-release** with the complete assets after checks pass. Publishing a release through GitHub also triggers the build. For a manual run, enter an existing tag in `release_tag` to publish it automatically; leave it empty for an artifact only. No game inputs, Apple certificates or signing secrets are needed by CI.

To prepare a private installable IPA, use the matching source release on an Apple Silicon Mac. Install the dependencies in [BUILD.md](BUILD.md), then run `setup.sh rewrite` and `setup.sh shaders` against your own decrypted files. Run:

```sh
python tools/pack_shell.py --shell /path/to/ACNHMobile-shell-build-input.zip \
  --patched /path/to/private/patched --shaders /path/to/private/shaders/ios \
  --font /path/to/standard.ttf --output /path/to/private/ACNHMobile-unsigned.ipa
```

Sign the whole IPA, including Guest.framework, before installing. This resulting private IPA contains game-derived code/shaders and must not be published as a shell. Resource `.data` files remain external in `Documents/ACNHMobile` and are read without extraction. Full preparation on Windows/Linux is not currently provided.

To build the public shell locally without game inputs: `python tools/build_shell.py --work /path/to/new-work --output /path/to/shell-output`.

Local iOS compilation and package structure/content checks passed. This shell path still needs phone signing and gameplay verification. The hosted workflow needs its first run after the source is uploaded.
