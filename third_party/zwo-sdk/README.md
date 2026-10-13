# ZWO ASICamera2 SDK (vendored)

Vendored for `drivers/zwo` — see `docs/decisions/ADR-007-zwo-asi-backend.md`.

- `include/ASICamera2.h` — SDK version 1.41 (per the source repo's `CMakeLists.txt` comment: "Using ASI Camera SDK Version 1.34 updated on 2025-11-18" for the bundled binaries, header itself is the general "second version of release").
- `lib/mac_arm64/libASICamera2.bin` — Mach-O arm64 dylib (Apple Silicon).
- `lib/mac_x64/libASICamera2.bin` — Mach-O universal dylib (i386 + x86_64, Intel).
- `lib/linux_x64/libASICamera2.bin` — Linux x86-64 ELF shared library, used as `libASICamera2.so` in the build.
- `LICENSE.txt` — MIT, copyright ZWO Company. Must be kept alongside any redistribution of these files per its own terms.

Source: mirrored from [`indilib/indi-3rdparty/libasi`](https://github.com/indilib/indi-3rdparty/tree/master/libasi) (also MIT-licensed as a whole), not from ZWO's own download site directly — that domain (`download.astronomy-imaging-camera.com`) wasn't reachable from the environment these were fetched in. If updating, prefer pulling fresh files from ZWO's own site when possible; the INDI mirror is a reliable fallback and is what INDI's own `indi-asi` driver ships.

`libASICamera2.bin` depends on `@rpath/libusb-1.0.0.dylib` at runtime (confirmed via `otool -L`) — Homebrew's `libusb` installs as `libusb-1.0.dylib` (no trailing `.0`), so this needs a path fix at build time; see `drivers/zwo/CMakeLists.txt`.

Linux x86-64 source: `indilib/indi-3rdparty` commit `f5e7166564d1afea311faf622d368afbe14bd841`, path `libasi/x64/libASICamera2.bin`. SHA-256: `d1de4a5ab85c8cafbddfad9c593bbba515890d3adf20c1ca44dafcf15f2775ce`. The build verifies this hash. USB access uses the local desktop user’s udev permissions; actual Linux hardware capture still needs physical validation.
