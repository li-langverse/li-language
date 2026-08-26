# Changelog

All notable changes to this package will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Deterministic `Prng` (`prng_seed`, `prng_next`, `prng_fill4`) with bench-aligned LCG golden vectors.
- OsRng uniform contract documented; `prob_ensures` oracles in `li-tests/rng/`.
- HTTP config profile gates for Prng-on-TLS (`scripts/httpd_rng.py`).

### Added (scaffold)

- Initial scaffold via `scripts/li-new-package` (PKG-li-rng).

## [0.1.0] - 2026-05-22

### Added

- Package skeleton.
