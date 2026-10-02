# Contributing

1. Read `README.md` for the project scope and build instructions.
2. Build and test using CMake presets.
3. Prefer mock/replay camera tests for reproducibility.
4. Keep platform/vendor dependencies behind interfaces.
5. Add tests for behavior and regression fixtures where appropriate.
6. Update documentation when architecture/public behavior changes.
7. Use an ADR for costly cross-cutting decisions.
8. Never claim hardware support without physical validation evidence.

Pull requests should explain:

- problem being solved;
- architecture impact;
- test strategy;
- platform impact;
- hardware used for validation if relevant;
- screenshots/video for UI changes where useful.
