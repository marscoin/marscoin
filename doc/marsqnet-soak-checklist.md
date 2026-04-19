# Marsqnet 7-Day Soak Checklist

Use this checklist to validate marsqnet stability before wider external
onboarding.

## Baseline

- Network: `marsqnet`
- Baseline tag: `marsqnet-baseline-2026-04-13`
- Expected reference best hash at launch: `5a03638e8bee12da16975369afd379520a5621172fc87b955a6aaaef41a65bf5`

## Daily checks (all nodes)

Run at least twice per day on each node:

```bash
marscoin-cli -chain=marsqnet -datadir=<datadir> getblockcount
marscoin-cli -chain=marsqnet -datadir=<datadir> getbestblockhash
marscoin-cli -chain=marsqnet -datadir=<datadir> getconnectioncount
marscoin-cli -chain=marsqnet -datadir=<datadir> getblockchaininfo
```

Record values and ensure all nodes converge on same best hash.

## Event checks

- Restart each node service at least once during soak.
- Verify clean recovery after restart:
  - service active,
  - same best hash as peers,
  - normal peer count restored.
- Verify at least one new mined block per day and propagation to all peers.

## Failure conditions (stop and investigate)

- Any sustained chain split (different best hash for >15 min).
- Repeated crash loops (service restart count rising).
- Block generation accepted on one node but rejected/ignored by peers.
- Node stuck with zero peers for >30 min.

## Minimum acceptance criteria after 7 days

- No unresolved chain split incidents.
- No unresolved node crash loops.
- Consistent tip convergence across all participating nodes.
- Successful block propagation observed repeatedly.
- Explorer and pool telemetry aligned with node RPC values.

## Suggested log commands

```bash
systemctl status marscoin-marsqnet --no-pager
journalctl -u marscoin-marsqnet -n 200 --no-pager
journalctl -u marscoin-marsqnet --since "24 hours ago" --no-pager
```
