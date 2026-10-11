# Marsqnet Operator Onboarding

This guide is for developers/operators who want to join `marsqnet`, the
Marscoin post-quantum test network: RandomX proof of work, P2WPQH
(SLH-DSA, FIPS 205) outputs and the adaptive block weight limit.

Marsqnet v2 starts from a new genesis block (dated 2026-10-09) and uses its
own message start and ports. Nodes and data from the earlier marsqnet don't
connect or sync with it. Its data lives in
the `marsqnet` subdirectory of the data directory. Default ports: P2P 29348,
RPC 29347.

## 1) Build requirements (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential automake libtool autotools-dev pkg-config python3 \
  libevent-dev libboost-dev libsqlite3-dev libssl-dev \
  libminiupnpc-dev libnatpmp-dev git cmake
```

## 2) Clone and build

RandomX is required for proof of work: a node built without
`--enable-randomx-vendor` refuses to start on marsqnet. Post-quantum (P2WPQH)
signature checks need no option; SLH-DSA is built into every node.

```bash
git clone --branch feature/quantum-upgrade https://github.com/marscoin/marscoin.git
cd marscoin
./autogen.sh
./configure --without-gui --disable-tests --disable-bench --enable-randomx-vendor
make -j"$(nproc)" src/marscoind src/marscoin-cli
```

## 3) Minimal config

Create `marsqnet.conf`:

```ini
server=1
listen=1
daemon=0
txindex=1
dnsseed=0
fixedseeds=0
discover=0
fallbackfee=0.0002

addnode=161.35.136.251:29348
addnode=137.184.66.189:29348
addnode=159.203.79.101:29348
addnode=104.236.58.205:49338
```

## 4) Start node

```bash
./src/marscoind -chain=marsqnet -conf=/path/to/marsqnet.conf -datadir=/path/to/marsqnet-data
```

## 5) Verify sync and connectivity

```bash
./src/marscoin-cli -chain=marsqnet -datadir=/path/to/marsqnet-data getblockcount
./src/marscoin-cli -chain=marsqnet -datadir=/path/to/marsqnet-data getbestblockhash
./src/marscoin-cli -chain=marsqnet -datadir=/path/to/marsqnet-data getconnectioncount
```

Genesis block (marsqnet v2): `61174bcc4b3b9face187ef053b97aad26a07ab227a6647cc6ef38f21d0249a08`
(`getblockhash 0`).

## 6) Optional local mining smoke

```bash
./src/marscoin-cli -chain=marsqnet -datadir=/path/to/marsqnet-data createwallet dev
ADDR=$(./src/marscoin-cli -chain=marsqnet -datadir=/path/to/marsqnet-data -rpcwallet=dev getnewaddress)
./src/marscoin-cli -chain=marsqnet -datadir=/path/to/marsqnet-data generatetoaddress 1 "$ADDR"
```

## 7) Report back

When joining, share in issue threads:

- hostname/region,
- block height,
- best hash,
- peer count,
- any errors from `journalctl` or debug log.

## Notes

- `qdevnet` remains a compatibility alias, but new deployments should use
  `marsqnet` naming.
- This is a development network. Do not treat balances as production assets.
