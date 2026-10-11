// Copyright (c) 2026 The Marscoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <crypto/pq_sphincs.h>
#include <random.h>
#include <uint256.h>

#include <cassert>
#include <string>
#include <vector>

using pq::sphincs::ParameterSet;

static constexpr ParameterSet PARAM_SET{ParameterSet::SLH_DSA_SHA2_128S};

static void SLHDSAKeygen(benchmark::Bench& bench)
{
    std::vector<unsigned char> pubkey, privkey;
    std::string error;
    bench.unit("keypair").run([&] {
        const bool ok{pq::sphincs::GenerateKeypair(PARAM_SET, pubkey, privkey, error)};
        assert(ok);
    });
}

static void SLHDSASign(benchmark::Bench& bench)
{
    std::vector<unsigned char> pubkey, privkey, payload;
    std::string error;
    assert(pq::sphincs::GenerateKeypair(PARAM_SET, pubkey, privkey, error));
    const uint256 msg{GetRandHash()};
    bench.unit("signature").run([&] {
        const bool ok{pq::sphincs::SignMessage(PARAM_SET, privkey, Span<const unsigned char>{msg.data(), msg.size()}, payload, error)};
        assert(ok);
    });
}

static void SLHDSAVerify(benchmark::Bench& bench)
{
    std::vector<unsigned char> pubkey, privkey, payload;
    std::string error;
    assert(pq::sphincs::GenerateKeypair(PARAM_SET, pubkey, privkey, error));
    const uint256 msg{GetRandHash()};
    assert(pq::sphincs::SignMessage(PARAM_SET, privkey, Span<const unsigned char>{msg.data(), msg.size()}, payload, error));
    bench.unit("signature").run([&] {
        const bool ok{pq::sphincs::VerifyMessage(PARAM_SET, pubkey, Span<const unsigned char>{msg.data(), msg.size()}, payload, error)};
        assert(ok);
    });
}

BENCHMARK(SLHDSAKeygen, benchmark::PriorityLevel::HIGH);
BENCHMARK(SLHDSASign, benchmark::PriorityLevel::HIGH);
BENCHMARK(SLHDSAVerify, benchmark::PriorityLevel::HIGH);
