# Technical Governance

## 1. Ownership

As the team grows, assign maintainers by subsystem rather than every developer editing everything without review:

- camera/backends;
- acquisition/core;
- recording/formats;
- UI/design system;
- processing;
- AI/research;
- release/platform engineering.

## 2. Architectural change

Changes requiring ADR include examples such as:

- replacing Qt;
- replacing CMake;
- shipping Python runtime;
- public plugin ABI;
- changing raw frame semantics;
- adopting cloud-required acquisition;
- adding an auto-update framework;
- changing Qt license strategy;
- changing package format with important operational consequences.

## 3. API stability

Internal interfaces are free to evolve before stable 1.0, but changes should still be deliberate.

Public plugin/API compatibility is not promised until explicitly versioned.

## 4. Dependencies

New major dependencies require a short rationale in PR and license review.

Avoid dependencies for trivial utilities if standard library/Qt already provides the capability sufficiently.

## 5. Hardware support decisions

Support priority should consider:

- user demand;
- quality/stability of vendor SDK;
- redistribution rights;
- macOS/Windows/Linux availability;
- documentation quality;
- device access for testing;
- support burden.

## 6. Release authority

Stable releases require explicit approval from maintainers responsible for:

- core/acquisition;
- release engineering;
- any backend whose support status changes;
- product/UX for major workflow changes.

## 7. Documentation as code

Documentation changes are reviewed in pull requests. Broken/stale architecture docs are defects.

When implementation intentionally diverges from architecture, update the document or create an ADR in the same change.
