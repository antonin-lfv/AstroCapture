# ADR-002 — Python for AI R&D, native runtime for production inference

## Status
Accepted as default strategy.

## Context

The team wants Python/PyTorch flexibility while users should download a normal desktop application without configuring Python environments.

## Decision

Use Python for research/training/prototyping. Export compatible models, preferentially ONNX, and run them in production through a native inference runtime such as ONNX Runtime C++.

A Python sidecar may be used experimentally in development builds but is not the default production architecture.

## Consequences

Positive:

- excellent ML development ecosystem;
- simple end-user installation;
- predictable dependency/runtime control;
- local/offline inference;
- easier native performance management.

Negative:

- not every research operator exports cleanly to ONNX;
- export parity must be tested;
- two environments/toolchains must be maintained;
- model features must fit supported runtime operations.
