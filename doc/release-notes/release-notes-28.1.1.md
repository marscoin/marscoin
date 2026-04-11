Marscoin Core version 28.1.1 is now available from:

  <https://github.com/marscoin/marscoin/releases/tag/v28.1.1>

This is a maintenance and build-portability release focused on improving
cross-platform build reliability and release distribution.

How to Upgrade
==============

If you are running an older version, shut it down cleanly and wait for it to
fully exit before replacing binaries.

- macOS: replace `marscoin-qt` or CLI binaries, then restart.
- Linux: replace `marscoind`, `marscoin-cli`, `marscoin-tx`, `marscoin-util`.
- Windows: replace `.exe` binaries and restart the node/wallet.

Notable changes
===============

Apple Silicon build compatibility
---------------------------------

- Removed x86-only includes from `src/crypto/scrypt.cpp` that were not used by
  the active implementation path. This fixes compilation on macOS arm64
  toolchains.

macOS build documentation update
--------------------------------

- Updated `doc/build-osx.md` to include `openssl@3` in dependencies.
- Added Homebrew `CPPFLAGS`/`LDFLAGS` examples for OpenSSL header/library
  discovery on modern macOS setups.

CI and release automation
-------------------------

- Added GitHub Actions CI workflow for Linux CLI and macOS arm64 GUI builds.
- Added tag-driven release workflow that builds and publishes release artifacts
  for Linux x86_64, macOS x86_64, macOS arm64, and Windows x86_64.
- Added generated `SHA256SUMS` publication in release assets.

Verification notes
------------------

- macOS arm64 local validation included build, daemon startup, CLI RPC checks,
  snapshot-assisted synchronization, and wallet loading tests.
- GitHub-hosted CI validated Linux and macOS build pipeline execution.

Credits
=======

Thanks to everyone testing Marscoin Core on modern hardware, especially macOS
Intel and Apple Silicon users reporting compatibility and sync issues.
