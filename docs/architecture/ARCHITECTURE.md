# Software Architecture

## 1. Architectural goals

The architecture must support four tensions simultaneously:

- very low-friction UI development;
- high-throughput camera acquisition;
- vendor-specific native SDKs;
- long-term cross-platform maintainability.

The design therefore uses explicit layers with dependency direction toward domain abstractions.

## 2. Layer model

### 2.1 Presentation — QML

Responsibilities:

- visual composition;
- animations and interaction states;
- keyboard/mouse input routing;
- data binding to presentation models;
- accessible labels and shortcuts.

Must not:

- call camera SDKs;
- own frame acquisition threads;
- write SER files;
- implement ML inference;
- contain business rules that cannot be unit tested outside QML.

### 2.2 Presentation models / application layer

C++ QObject-facing layer connecting domain state to QML.

Responsibilities:

- map domain types to UI-friendly models;
- expose commands such as Connect, StartStreaming, StartRecording;
- orchestrate workflows;
- convert errors into user-facing state;
- manage screen/session-level state.

Avoid putting high-throughput frame copies through generic QVariant-heavy APIs. Rendering requires a purpose-built bridge.

### 2.3 Domain/core

Framework-light business contracts and state machines.

Contains concepts such as:

- CameraId;
- CameraDescriptor;
- CameraCapability;
- StreamMode;
- PixelFormat;
- BayerPattern;
- FrameDescriptor;
- Frame;
- CaptureSession;
- RecordingRequest;
- RecordingStats;
- CaptureState.

Qt dependencies should be minimized in deep core where practical, especially in driver-independent algorithms. Pragmatism is allowed; purity is not a goal by itself.

### 2.4 Infrastructure

Implements abstractions:

- camera SDK adapters;
- platform device APIs;
- file/format writers;
- model runtimes;
- settings persistence;
- update service;
- network/cloud clients.

### 2.5 Platform layer

All unavoidable OS-specific integration lives here or inside backend-specific implementation modules.

Examples:

- macOS permissions/entitlements;
- Windows device/API integration;
- Linux V4L2/UVC/udev concerns;
- secure credential storage;
- native file/reveal dialogs where Qt abstractions are insufficient;
- OS power-management inhibition during critical captures if needed.

## 3. Dependency direction

Preferred:

```text
QML
 ↓
Presentation models
 ↓
Application services
 ↓
Domain interfaces
 ↑
Infrastructure adapters
```

Drivers depend on domain contracts, not the reverse.

## 4. Camera plugin/backends

Start with linked modules in the same application. Do not begin with dynamically loaded third-party binary plugins unless there is a demonstrated need.

Internally, structure camera support as backends implementing shared contracts. A future plugin ABI can wrap those contracts after versioning requirements are understood.

Reasons to avoid a public binary plugin ABI too early:

- C++ ABI compatibility concerns;
- Qt version compatibility;
- security/update complexity;
- unclear vendor needs;
- API freezes product experimentation.

## 5. State machines

Camera and recording lifecycle should be explicit state machines.

Example camera states:

```text
Unavailable
Discovered
Opening
Ready
StartingStream
Streaming
StoppingStream
ErrorRecoverable
ErrorFatal
Closing
```

Transitions are commands/events, not arbitrary boolean combinations.

Do not represent lifecycle as a collection such as:

```text
isConnected, isStreaming, isRecording, hasError
```

without enforcing legal combinations.

Recording is a related but separate state machine:

```text
Idle -> Preparing -> Recording -> Finalizing -> Completed
                      ↓
                    Error
```

Finalization matters because astronomy formats may require header counts/metadata completion.

## 6. Events and commands

Differentiate:

- command: user/system intent (`StartRecording`);
- event: something that happened (`RecordingStarted`, `CameraDisconnected`);
- telemetry/statistic: continuously sampled status (`142.3 FPS`).

This separation improves tests and avoids UI code guessing hardware state.

## 7. Error taxonomy

Use structured errors with machine-readable category and human-readable context.

Categories may include:

- DeviceNotFound;
- DeviceBusy;
- PermissionDenied;
- UnsupportedFormat;
- UnsupportedControl;
- StreamStartFailed;
- DeviceDisconnected;
- InvalidFrame;
- RecordingOpenFailed;
- DiskWriteFailed;
- DiskTooSlow;
- ModelLoadFailed;
- InternalInvariantViolation.

Vendor codes may be attached as diagnostic context, not used as cross-project semantics.

## 8. Memory ownership

High-rate image applications are sensitive to copies and allocations.

Preferred design:

- reusable frame buffer pool;
- clear ownership token / smart pointer semantics;
- driver either fills project-owned memory or wraps SDK-owned memory with a documented lifetime;
- recording and preview consumers hold references only for bounded periods;
- if SDK buffers cannot outlive callbacks, copy once into pool memory and release SDK buffer immediately.

Measure before implementing exotic lock-free structures.

## 9. Scheduling/concurrency

Initial safe design can use:

- acquisition thread per active camera/backend if required;
- recording worker thread;
- processing worker pool;
- Qt UI thread;
- optional inference worker.

Queues must declare:

- capacity;
- producer;
- consumer(s);
- overflow behavior;
- shutdown behavior;
- latency expectations.

## 10. Rendering

The live preview path should minimize CPU copies and UI-thread work.

Implementation can evolve:

- initial: straightforward frame conversion and Qt-compatible texture/image upload;
- later: GPU-backed debayer/color transform if CPU becomes a bottleneck;
- advanced: platform graphics acceleration behind Qt's rendering abstractions where appropriate.

Do not optimize to Metal/Vulkan/Direct3D before profiling demonstrates need. Preserve an abstraction boundary that allows later GPU work.

## 11. Processing graph evolution

MVP may have fixed processing stages, but define them as composable operations conceptually:

```text
RawFrame
  -> unpack/normalize
  -> optional debayer
  -> display stretch/white balance
  -> preview
```

Later:

```text
RawFrame
  ├─> raw recorder
  └─> analysis branch
       -> quality metric
       -> tracking
       -> alignment
       -> stack
       -> display processing
```

A general node-graph framework is not required early. Avoid overengineering. Maintain stage boundaries so a graph can emerge later if needed.

## 12. Configuration architecture

Use typed configuration structures and schema migration.

Potential domains:

- app preferences;
- rendering preferences;
- camera profiles;
- capture profiles;
- workspace layout;
- AI preferences.

Persist stable semantic keys, not object memory layouts or QML-specific internal names.

## 13. Versioning

Version independently where useful:

- application version;
- settings schema version;
- capture profile schema version;
- model version;
- plugin/API version if introduced;
- backend SDK version reported in diagnostics.

## 14. Extensibility strategy

Extension points should be interfaces, not promises of public APIs.

Examples:

- ICameraBackend;
- IRecorder;
- IFrameMetric;
- IInferenceModel;
- IProcessingStage.

Keep them internal until requirements stabilize.
