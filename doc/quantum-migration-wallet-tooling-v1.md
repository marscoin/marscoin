# Quantum Migration Wallet Tooling v1 (Scaffold)

This document defines the first read-only wallet tooling scaffold for quantum
migration planning.

## Scope

- Add status reporting RPC for migration readiness.
- Expose wallet balance/transaction context needed for operator planning.
- Keep wallet state immutable (no fund movement, no key mutation).

## RPC: getquantummigrationstatus

Current scaffold behavior:

- `migration_enabled` is `false`
- `phase` is `"scaffold"`
- `migrated_balance` is always `0`
- `remaining_legacy_balance` is computed from current wallet balances

This RPC is intended as a stable contract for monitoring integrations and
operator playbooks before migration transactions are implemented.

## RPC: estimatequantummigration

Current scaffold behavior:

- Inspects wallet UTXOs in read-only mode.
- Classifies outputs into `eligible`, `pending_or_unsafe`, and `locked` groups.
- Produces rough transaction/vsize and fee estimates (`low`, `medium`, `high`).
- Returns warnings for operator attention (for example no eligible UTXOs).

This endpoint is a dry-run planner and does not create migration transactions.

## Non-goals (v1)

- No creation of migration transactions
- No automatic key/address conversion
- No consensus activation logic
- No irreversible wallet actions

## Follow-up work

1. Add dry-run migration planning endpoint with fee estimates.
2. Add per-UTXO migration classification outputs.
3. Add resumable migration execution flow gated by activation rules.
