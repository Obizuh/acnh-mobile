# Game files

Provide your own decrypted ACNH 3.0.3 ExeFS and RomFS. No game files, keys or saves are included.

```sh
python3 tools/pack_data.py /path/to/RomFS /path/to/private-work/ACNHMobile
```

The output directory must be empty. Copy `ACNH-index.data` and all numbered `ACNH-*.data` parts to the app's `Documents/ACNHMobile/`. Parts are range-read directly; no RomFS expansion takes place at runtime. Keep every part from the same generated set. Existing build 7–12 data sets remain compatible.

Keep saves in the app's save directory, separate from resource packs and public sources.
