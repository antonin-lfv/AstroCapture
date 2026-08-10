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
