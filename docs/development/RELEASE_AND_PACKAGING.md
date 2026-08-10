# Production Builds, Release & Packaging

## 1. Principle

Users download and install AstroCapture like a normal desktop product. They do not install Python, CMake, Qt, OpenCV or camera SDK development packages manually unless a specific hardware vendor legally/technically requires a separate runtime driver.

## 2. Build types

### Development

- debug or RelWithDebInfo as appropriate;
- local executable;
- diagnostics and mock camera available;
- no signing/package needed.

### Release candidate

- optimized;
- production-like dependencies/resources;
- optional additional symbols stored separately;
- signed/notarized when validating distribution flow;
- distributed only to testers.

### Stable production

- optimized and reproducible from release tag;
- approved dependency versions;
- signed artifacts;
- platform package;
- update metadata;
- changelog/release notes;
- archived debug symbols where supported.

## 3. CI matrix

Every meaningful PR should compile/test on representative runners for:

- macOS;
- Windows;
- Linux.

Hardware-independent tests use mock/replay inputs.

Release jobs may be separate per OS because signing tools and packaging differ.

## 4. macOS distribution

Expected direct-distribution pipeline:

1. build `.app` bundle;
2. deploy Qt/runtime frameworks correctly;
3. embed permitted vendor SDK libraries/resources;
4. set entitlements/permissions;
5. sign nested code and final app with Developer ID identity;
6. submit for Apple notarization;
7. staple/verify notarization where applicable;
8. create DMG/package;
9. verify Gatekeeper behavior on a clean test account/machine.

Do not treat `codesign` success alone as release completion.

Test Apple Silicon natively. Decide Intel/universal support based on product requirements and current user base, not assumption.

## 5. Windows distribution

Expected pipeline:

1. release build;
2. deploy Qt DLLs/plugins;
3. include permitted native runtime libraries;
4. create chosen installer/package;
5. code-sign binaries/installer using current Microsoft-compatible signing approach;
6. timestamp signatures;
7. verify on clean Windows installation;
8. test install/uninstall/upgrade.

MSIX vs MSI/EXE installer should be an ADR after testing camera SDK/driver constraints and update strategy.

## 6. Linux distribution

Initial strategy should minimize maintenance burden.

Candidates:

- AppImage for simple portable distribution;
- Flatpak for sandboxed ecosystem distribution if hardware/USB access and SDK licensing work cleanly;
- native distro packages later if demand exists.

Do not advertise unsupported distributions broadly.

Validate camera permissions/udev rules and USB access on target distributions.

## 7. Auto-update

Do not implement an updater casually; it is security-sensitive.

Requirements before shipping:

- signed update metadata/artifacts;
- HTTPS;
- channel awareness (stable/beta);
- rollback/recovery story;
- package integrity verification;
- no elevation beyond necessity;
- safe behavior if update is interrupted.

A mature external update framework may be preferable to custom cryptographic/update code.

## 8. Versioning

Use semantic-ish product versioning, e.g. `MAJOR.MINOR.PATCH`, while acknowledging that product compatibility policy must be documented.

Also embed:

- git commit/build id;
- build timestamp if reproducibility policy allows;
- channel;
- backend SDK versions in diagnostics.

## 9. Release checklist

Before stable release:

- CI green on all supported targets;
- changelog complete;
- dependency/license review complete;
- hardware matrix executed for supported devices;
- format compatibility tests executed;
- install/upgrade/uninstall smoke tests;
- clean-machine first launch;
- camera permission flow checked;
- crash-free representative capture session;
- prolonged recording soak test;
- disk-full/slow-disk behavior checked;
- package signatures verified;
- macOS notarization verified;
- update metadata checked if updater exists;
- release artifacts hashed/archived;
- symbols archived.

## 10. Production configuration

Production app uses a stable application identifier and user data paths.

Development builds should use separate configuration namespace when possible to prevent schema experiments from corrupting stable settings.

## 11. Crash reporting

Potential future production crash reporting must be privacy-conscious and opt-in/appropriately disclosed according to product/legal requirements.

Store symbol files securely for stack symbolication.

## 12. Artifact retention

For each release retain:

- source tag;
- build configuration;
- dependency lock/manifest;
- installers;
- hashes;
- debug symbols;
- release notes;
- hardware compatibility matrix snapshot;
- AI model artifacts/model cards.
