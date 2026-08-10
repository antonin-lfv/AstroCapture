# AI & Automation Strategy

## 1. Role of AI

AI is a means to reduce acquisition friction and improve image quality, not a product identity substitute.

A user should benefit from AstroCapture even if every AI feature is disabled.

## 2. Architecture

Development:

```text
AstroCapture capture/export
        ↓
Dataset + metadata
        ↓
Python research
  NumPy/OpenCV/PyTorch
        ↓
Evaluation against deterministic baseline
        ↓
Candidate model
```

Production:

```text
Versioned model artifact
        ↓
ONNX / compatible native representation
        ↓
ONNX Runtime C++ inference service
        ↓
Feature-specific application service
        ↓
recommendation / score / automation
```

Python is not a normal user installation requirement.

## 3. Candidate feature backlog

### 3.1 Planet/object detection

Purpose:

- detect target location in preview;
- estimate bounding box/diameter;
- assist ROI and centering;
- potentially identify object class.

Baseline first:

- thresholding/connected components;
- Hough/circle methods where appropriate;
- brightness/shape heuristics.

ML only if robustness across moons, clouds, varying gain, overexposure etc. justifies it.

### 3.2 Frame quality scoring

Goal: rank frames according to expected stacking value.

Baselines:

- Laplacian variance;
- gradient energy;
- high-frequency Fourier energy;
- local contrast;
- entropy;
- object-specific edge sharpness.

ML target could be trained using:

- expert ranking;
- synthetic degradation levels;
- correlation with resulting stack quality;
- pairwise ranking rather than arbitrary absolute labels.

### 3.3 Seeing estimation

Treat as an estimation task with uncertainty, not a scientifically universal scalar unless definition is explicit.

Potential inputs:

- temporal deformation of planetary edge/features;
- local motion statistics;
- sharpness distribution across short sequences;
- metadata/exposure.

UI should phrase estimates appropriately.

### 3.4 Focus assistance

Start deterministic. ML may later identify optimum focus or distinguish seeing blur from defocus.

Potential workflow:

- define target ROI;
- compute robust focus metric over time;
- smooth turbulence noise;
- show trend and best observed point;
- integrate motor focuser only later.

### 3.5 Exposure/gain recommendation

Likely hybrid control problem rather than pure neural network.

Inputs:

- histogram/clipping;
- object size/brightness;
- exposure;
- gain;
- FPS/readout constraints;
- target type;
- desired capture strategy.

Use rules/optimization first. ML may model outcome quality later.

### 3.6 Bad-frame/event detection

Detect:

- large telescope shake;
- clouds/obstruction;
- severe blur;
- sudden brightness jump;
- target lost from ROI.

This may save disk and warn users.

## 4. Data strategy

Every future AI feature depends on good data provenance.

Dataset manifest includes:

- source session id;
- camera/model;
- telescope/optics when known;
- target;
- exposure/gain;
- frame format;
- transformations;
- labels/labeler;
- split assignment;
- rights/license/consent.

Avoid leakage: frames from the same capture sequence must not be split naively across train and validation if temporal similarity would inflate performance.

## 5. Model lifecycle

Statuses:

- Research;
- Candidate;
- Preview;
- Production;
- Deprecated.

Promoting to Production requires:

- reproducible training configuration;
- model card;
- evaluation on held-out sessions/cameras;
- native inference parity test;
- runtime benchmark;
- failure/fallback design;
- artifact checksum/versioning.

## 6. Inference API

Keep feature-oriented interfaces.

Bad generic API:

```cpp
Tensor runModel(Tensor input);
```

Useful internal adapter may use tensors, but application code should call semantics such as:

```cpp
FrameQualityResult evaluateFrameQuality(const FrameView&);
TargetDetectionResult detectTarget(const FrameView&);
```

This decouples model architecture from product logic.

## 7. Scheduling AI

Do not infer on every camera frame by default.

Strategies:

- sample every N frames;
- target a max analysis Hz;
- adapt based on GPU/CPU load;
- skip stale analysis jobs;
- use smaller ROI/resolution;
- batch offline during post-processing.

Acquisition and recording have priority.

## 8. Hardware acceleration

ONNX Runtime execution providers may allow platform acceleration. Enable only after cross-platform validation. Always provide a reliable CPU fallback where model complexity permits.

Avoid building core functionality that requires a discrete GPU unless product requirements explicitly move there.

## 9. Explainability

For recommendations, show the underlying reason when possible:

> Highlights are clipping. Reduce exposure or gain.

is more useful than:

> AI recommends 6.2 ms.

AI may produce recommendation values, but supporting observable metrics build trust.

## 10. Privacy

Default AI inference is local.

If cloud training/data contribution is introduced:

- explicit opt-in;
- clear scope of uploaded data;
- anonymization/minimization;
- deletion policy;
- rights to use training data;
- separate consent from essential application operation.
