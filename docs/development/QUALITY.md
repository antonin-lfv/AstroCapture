# Engineering Quality Standard

## 1. Quality philosophy

AstroCapture handles high-throughput hardware input and user data. Correctness, observability and recovery are first-class features.

## 2. Coding standards

Prefer:

- small cohesive types;
- explicit ownership;
- RAII;
- strong types/enum classes;
- const correctness;
- value semantics where practical;
- standard library before bespoke utilities;
- deterministic resource release;
- clear error/result APIs;
- comments explaining why, not restating code.

Avoid:

- global mutable state;
- hidden thread ownership;
- raw owning pointers;
- magic numeric device codes scattered through UI;
- giant God objects;
- vendor SDK headers leaking into unrelated modules;
- silent exception swallowing;
- catch-all “fixes” that hide frame corruption.

This project intentionally avoids arbitrary universal limits such as “all functions <= 40 lines”. Complexity is reviewed semantically, not by ritual metrics. Refactor when cohesion/readability suffers.

## 3. Formatting

Use automated formatters:

- clang-format for C++;
- QML formatter/linter supported by current Qt tooling;
- Python formatter/linter selected for research tooling.

Formatting is CI-enforced once configuration stabilizes.

## 4. Compiler warnings

Enable strong warning levels across compilers and treat project-owned warnings as errors in CI when practical. Vendor headers may require isolated suppression.

## 5. Tests required by subsystem

### Camera backend

- enumeration mapping;
- capability conversion;
- stream mode conversion;
- error mapping;
- lifecycle with mock/fake SDK layer where feasible.

### Frame pipeline

- payload size validation;
- pixel format conversion;
- Bayer correctness;
- queue overflow policy;
- timestamps/order;
- cancellation/shutdown.

### Recorder

- correct headers;
- frame count finalization;
- metadata;
- interrupted recording behavior;
- disk error propagation.

### UI/application

- command/state transitions;
- disabled action correctness;
- failure states;
- settings migration.

## 6. Regression fixtures

Keep small legally distributable frame fixtures in repository or test assets:

- synthetic Bayer patterns;
- monochrome gradient;
- high-bit-depth ramp;
- malformed payload;
- known astronomy frame samples if license permits.

Large datasets should live in versioned external storage with manifests, not Git history.

## 7. Performance gates

Establish benchmark baselines before imposing strict CI thresholds.

Track:

- per-frame conversion time;
- memory allocation count/bytes;
- recorder throughput;
- queue behavior;
- startup time;
- model inference latency.

Flag statistically meaningful regressions.

## 8. Soak tests

Before major releases run prolonged streaming/recording on representative cameras to expose:

- memory leaks;
- handles not released;
- timestamp drift;
- queue growth;
- file finalization bugs;
- thermal/performance degradation.

## 9. Code review checklist

Reviewer asks:

- Does this belong in this layer?
- Is hardware behavior assumed or evidenced?
- Is memory ownership clear?
- Could this block acquisition/UI?
- What happens on disconnect/error/cancel?
- Does it preserve raw data correctly?
- Is cross-platform impact isolated?
- Are tests representative?
- Does UI expose actionable state?
- Does dependency addition have licensing consequences?

## 10. Hardware support labels

Use explicit maturity:

- Experimental — developer testing, no compatibility promise;
- Beta — expected to work, limited matrix;
- Supported — defined tested combinations and support policy.

Do not call a camera “supported” because one developer got one frame once.

## 11. Local audit — 2026-10-02

Scope: macOS/Apple Silicon, with the NexImage 5 as the user's current
camera. Reviewed architecture, camera drivers, Bayer conversion, preview,
recording, file naming, build scripts and existing tests. This is a local
engineering review, not certification of every backend or target OS.
Pre-existing packaging and roadmap edits were preserved.

### Corrections and evidence

- Recording shutdown detaches and seals the queue before draining, so
  acquisition cannot keep extending the recording during stop. Writer
  failures end the session with an error; automatic limits stop admission
  immediately. A separate timer handles duration limits without requiring
  a new frame. Delayed completion signals cannot stop a newer recording.
- Capture filenames are reserved with exclusive creation and a numeric
  suffix on collision. Multiple captures within one second no longer
  overwrite earlier files.
- USB transfer cancellation, retirement and resubmission are synchronized.
  Partial startup failure cancels and drains previously submitted transfers;
  buffers remain alive until all callbacks finish. Built successfully, but
  physical USB error/shutdown testing is still required. The ownership rule
  follows [libusb's asynchronous I/O documentation](https://libusb.sourceforge.io/api-1.0/group__libusb__asyncio.html).
- SER strips input row padding, checks frame format consistency and bit
  depth, and uses the de facto little-endian flag (0), as documented by
  [Siril](https://siril.readthedocs.io/en/latest/file-formats/SER.html).
  Tests independently inspect padded 16-bit payloads, header fields and
  file size; an external-reader interoperability run is still outstanding.
- Bayer8 conversion uses direct interpolation for interior pixels, with
  the existing edge behavior preserved. Worker count scales with image
  size. Invalid geometry, short strides and unknown Bayer enums are rejected.
  Deterministic random-pixel comparisons cover all four Bayer patterns,
  padded rows, odd sizes, borders and the parallel execution path.
- GUI statistics refresh at preview cadence. ROI changes invalidate both
  image caches. The macOS development helper launches the bundled executable.

Validation: all **13 tests passed in Debug, Release and UBSan**
(`halt_on_error=1`; ZWO disabled in the UBSan build). The new controller
integration test exercises mock acquisition, a five-frame SER capture,
manual stop while streaming, timed stop, stale completion signals, ROI,
disconnect and filename collisions. Shell syntax and `git diff --check`
also passed. App launch, discovery and the simulated color preview were
checked through the native UI; discovery found no physical camera.

AddressSanitizer could not be evaluated: its runtime deadlocked during
initialization, before the first test entered `main`. A process sample
showed reentrant ASan allocator initialization under dyld on this Mac;
the stuck test and runner were stopped. This is not an ASan pass.

Performance spot-check: Apple Silicon, clang++ C++20 `-O3`, a constant
Bayer8 input, three warmups and 30 conversions. Full 2592x1944 conversion
averaged **16.66 ms before / 3.49 ms after**; 640x480 averaged
1.63 / 1.40 ms. This measures conversion only, not camera FPS, USB
throughput, end-to-end preview latency or sustained acquisition.

### Outstanding priorities

1. **Validate NexImage color against a known target.** GBRG remains a
   hardcoded assumption in `EuvcCameraDevice`, as the hardware notes already
   acknowledge. Add a per-camera Bayer override and verify red/blue/green
   patches, white balance, preview and exported SER together.
2. **Report acquisition failures and losses.** The frame-sink interface
   has no stream-error event; USB disconnects can leave the UI looking
   connected. The recording drop counter does not include incomplete USB
   frames discarded before delivery. Corrupted packet assembly and
   oversized payload handling also need explicit regression fixtures.
3. **Complete advertised format coverage before claiming wider support.**
   ZWO selects Bayer16/Mono16 for many sensors, but preview conversion only
   implements Bayer8/BGRA32 and the histogram lacks 16-bit input support.
   Even Mono8 preview is currently black. TIFF16 is an expanded 8-bit RGB
   export, not preservation of native 16-bit sensor precision.
4. **Finish hardware and distribution validation.** Real camera ROI/mode
   selection remains limited; test sustained recordings, unplug/replug,
   disk failures and cold starts. Packaging uses Bash associative arrays
   (Bash 4+), needs a clean-Mac test, and is still ad-hoc signed rather than
   Developer ID signed/notarized. The DMG was not regenerated in this audit.
5. **Harden secondary paths.** RawSequenceReader still needs version,
   enum and allocation-size validation for malformed input files. Slow-disk
   finalization and full-resolution loupe processing still run/block on the
   GUI thread; benchmark them with real acquisition before further tuning.
