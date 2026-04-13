# Marsqnet Operator Onboarding

This guide is for developers/operators who want to join the Marscoin RandomX
development network (`marsqnet`).

## 1) Build requirements (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential automake libtool autotools-dev pkg-config python3 \
  libevent-dev libboost-dev libsqlite3-dev libssl-dev \
  libminiupnpc-dev libnatpmp-dev git
```

## 2) Clone and build

```bash
git clone --branch feat/pow-randomx https://github.com/marscoin/marscoin.git
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

addnode=161.35.136.251:29338
addnode=137.184.66.189:29338
addnode=159.203.79.101:29338
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

Current known reference best hash (initial rollout):

`5a03638e8bee12da16975369afd379520a5621172fc87b955a6aaaef41a65bf5`

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
