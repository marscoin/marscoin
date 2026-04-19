# Marsqnet Quickstart (Wiki Draft)

Marsqnet is the Marscoin RandomX development network.

## Current baseline

- Baseline tag: `marsqnet-baseline-2026-04-13`
- Example reference best hash: `5a03638e8bee12da16975369afd379520a5621172fc87b955a6aaaef41a65bf5`

## Build

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential automake libtool autotools-dev pkg-config python3 \
  libevent-dev libboost-dev libsqlite3-dev libssl-dev \
  libminiupnpc-dev libnatpmp-dev git

git clone --branch feat/pow-randomx https://github.com/marscoin/marscoin.git
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

addnode=161.35.136.251:29338
addnode=137.184.66.189:29338
addnode=159.203.79.101:29338
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
