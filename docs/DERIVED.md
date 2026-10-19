# Source release boundary

Included: compatibility-layer code, offline tool source, version IDs, API/interface metadata, shader identifiers and interface facts, pipeline-state records, author-written tests, third-party source and its licenses.

Excluded: original or rewritten game modules; ROMs, keys and saves; RomFS or `.data`; game artwork (except the bundled app-identification icon), audio, text tables, fonts or videos; original/translated shader code and libraries; private test IPAs; generated embedding assembly, build caches, signing credentials and personal paths.

A private IPA with external RomFS can still contain game code and translated shaders. This release is a **source-only archive**, not such an IPA. The included audit checks common forbidden paths, formats, module signatures, key patterns and personal paths; it does not claim exhaustive detection of arbitrarily encoded content.

The single pinned icon at `dm/ios-acnh/Resources/AppIcon.png` is included for app identification. It is not covered by the compatibility-layer MIT license. Other image assets remain excluded.
