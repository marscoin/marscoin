#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MARSCOIND="${MARSCOIND:-$ROOT_DIR/src/marscoind}"
MARSCOINCLI="${MARSCOINCLI:-$ROOT_DIR/src/marscoin-cli}"
CHAIN="${CHAIN:-marsqnet}"
DATADIR_BASE="${DATADIR_BASE:-$(mktemp -d /tmp/marscoin-marsqnet-XXXXXX)}"
BLOCKS="${BLOCKS:-3}"
TIMEOUT_SEC="${TIMEOUT_SEC:-60}"

NODE1_RPC_PORT="29543"
NODE1_P2P_PORT="29544"
NODE2_RPC_PORT="39543"
NODE2_P2P_PORT="39544"

NODE1_DIR="$DATADIR_BASE/node1"
NODE2_DIR="$DATADIR_BASE/node2"

cleanup() {
  set +e
  "$MARSCOINCLI" -datadir="$NODE1_DIR" -chain="$CHAIN" -rpcport="$NODE1_RPC_PORT" stop >/dev/null 2>&1
  "$MARSCOINCLI" -datadir="$NODE2_DIR" -chain="$CHAIN" -rpcport="$NODE2_RPC_PORT" stop >/dev/null 2>&1
}
trap cleanup EXIT

mkdir -p "$NODE1_DIR" "$NODE2_DIR"

echo "[marsqnet-smoke] chain: $CHAIN"
echo "[marsqnet-smoke] datadir: $DATADIR_BASE"

ulimit -S -n 1024

"$MARSCOIND" -daemonwait -datadir="$NODE1_DIR" -chain="$CHAIN" -listen=1 -dnsseed=0 -fixedseeds=0 -discover=0 -port="$NODE1_P2P_PORT" -rpcport="$NODE1_RPC_PORT" -fallbackfee=0.0002
"$MARSCOIND" -daemonwait -datadir="$NODE2_DIR" -chain="$CHAIN" -listen=1 -dnsseed=0 -fixedseeds=0 -discover=0 -port="$NODE2_P2P_PORT" -rpcport="$NODE2_RPC_PORT" -addnode="127.0.0.1:$NODE1_P2P_PORT" -fallbackfee=0.0002

"$MARSCOINCLI" -datadir="$NODE1_DIR" -chain="$CHAIN" -rpcport="$NODE1_RPC_PORT" createwallet "dev" >/dev/null
MINER_ADDR="$($MARSCOINCLI -datadir="$NODE1_DIR" -chain="$CHAIN" -rpcport="$NODE1_RPC_PORT" -rpcwallet="dev" getnewaddress)"

echo "[marsqnet-smoke] mining $BLOCKS blocks on node1"
"$MARSCOINCLI" -datadir="$NODE1_DIR" -chain="$CHAIN" -rpcport="$NODE1_RPC_PORT" generatetoaddress "$BLOCKS" "$MINER_ADDR" >/dev/null

NODE1_HEIGHT="$($MARSCOINCLI -datadir="$NODE1_DIR" -chain="$CHAIN" -rpcport="$NODE1_RPC_PORT" getblockcount)"
echo "[marsqnet-smoke] node1 height: $NODE1_HEIGHT"

DEADLINE=$((SECONDS + TIMEOUT_SEC))
NODE2_HEIGHT=0
while [[ $SECONDS -lt $DEADLINE ]]; do
  NODE2_HEIGHT="$($MARSCOINCLI -datadir="$NODE2_DIR" -chain="$CHAIN" -rpcport="$NODE2_RPC_PORT" getblockcount 2>/dev/null || printf '0')"
  if [[ "$NODE2_HEIGHT" -ge "$BLOCKS" ]]; then
    break
  fi
  sleep 1
done

echo "[marsqnet-smoke] node2 height: $NODE2_HEIGHT"

if [[ "$NODE2_HEIGHT" -lt "$BLOCKS" ]]; then
  echo "[marsqnet-smoke] ERROR: node2 did not sync to expected height within timeout" >&2
  exit 1
fi

echo "[marsqnet-smoke] success"
