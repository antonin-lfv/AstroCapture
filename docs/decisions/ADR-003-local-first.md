# ADR-003 — Local-first desktop acquisition

## Status
Accepted.

## Context

Camera acquisition is USB/native-hardware intensive, bandwidth sensitive and commonly performed in field environments with unreliable/no Internet connectivity.

## Decision

Core acquisition, recording, preview and essential processing execute locally. Cloud functionality is optional and additive.

## Consequences

Positive:

- low latency;
- works offline;
- raw data remains local by default;
- no server bandwidth cost for high-rate capture;
- direct hardware access.

Negative:

- cross-platform packaging complexity;
- updates must reach installed clients;
- local hardware performance varies;
- licensing/telemetry/cloud sync need separate architecture.
