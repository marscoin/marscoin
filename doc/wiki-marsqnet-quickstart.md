# Marsqnet Quickstart (Wiki Draft)

Marsqnet is the Marscoin post-quantum test network: RandomX proof of work,
P2WPQH (SLH-DSA, FIPS 205) outputs and the adaptive block weight limit.

## Current network

- Marsqnet v2: a new genesis (dated 2026-10-09), its own message start
  (`4d5132fa`) and ports. Nodes and data from the earlier marsqnet don't
  connect or sync with it.
- Genesis block: `61174bcc4b3b9face187ef053b97aad26a07ab227a6647cc6ef38f21d0249a08`
- Default ports: P2P 29348, RPC 29347. Data subdirectory: `marsqnet`.

## Build

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential automake libtool autotools-dev pkg-config python3 \
  libevent-dev libboost-dev libsqlite3-dev libssl-dev \
  libminiupnpc-dev libnatpmp-dev git cmake

git clone --branch feature/quantum-upgrade https://github.com/marscoin/marscoin.git
cd marscoin
./autogen.sh
./configure --without-gui --disable-tests --disable-bench --enable-randomx-vendor
make -j"$(nproc)" src/marscoind src/marscoin-cli
```

## Minimal config

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

## Run and verify

```bash
./src/marscoind -chain=marsqnet -conf=/path/to/marsqnet.conf -datadir=/path/to/marsqnet-data

./src/marscoin-cli -chain=marsqnet -datadir=/path/to/marsqnet-data getblockcount
./src/marscoin-cli -chain=marsqnet -datadir=/path/to/marsqnet-data getbestblockhash
./src/marscoin-cli -chain=marsqnet -datadir=/path/to/marsqnet-data getconnectioncount
```

## Ops docs

- Soak checklist: `doc/marsqnet-soak-checklist.md`
- Onboarding guide: `doc/marsqnet-onboarding.md`
