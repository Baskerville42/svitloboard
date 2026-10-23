# SvitloBoard 2

Version 2 is being built as a new firmware in [`firmware/`](firmware/), while the legacy Arduino
sketch remains intact in the repository root. The behavior contract, build targets, limitations,
and test gates are documented in:

- [`firmware/README.md`](firmware/README.md) — build and first-boot behavior;
- [`docs/architecture.md`](docs/architecture.md) — storage, Wi-Fi, TLS, API, and OTA design;
- [`docs/hardware-power-input.md`](docs/hardware-power-input.md) — expert OFF input and safety;
- [`docs/test-plan.md`](docs/test-plan.md) — per-board release acceptance checklist;
- [`docs/validation.md`](docs/validation.md) — current automated evidence and remaining gaps;
- [`docs/roadmap.md`](docs/roadmap.md) — implemented foundation and remaining release gates;
- [`docs/index.html`](docs/index.html) — GitHub Pages browser installer.

The new GitHub Actions workflow builds every declared board on pull requests, publishes checksummed
binaries on `v2.*` tags, and deploys the installer from `main`.

Before merging this work into the current remote `main`, retire the legacy
`.github/workflows/release.yml`; it creates v1 releases on every main push and is superseded by
`.github/workflows/firmware-v2.yml`. The local checkout is behind the remote commit that introduced
that workflow, so it was intentionally not overwritten in a dirty worktree. The v2 release
contract fails while the legacy workflow is present, making this migration requirement explicit.
