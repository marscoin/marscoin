// Copyright (c) 2026 The Marscoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config/bitcoin-config.h> // IWYU pragma: keep

#include <coins.h>
#include <consensus/amount.h>
#include <core_io.h>
#include <crypto/pq_sphincs.h>
#include <crypto/sha256.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <test/data/p2wpqh_sighash_vectors.json.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <univalue.h>
#include <util/strencodings.h>
#include <util/translation.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>

using pq::sphincs::ParameterSet;

BOOST_FIXTURE_TEST_SUITE(pq_sighash_tests, BasicTestingSetup)

namespace {

constexpr uint8_t PARAM_SET_ID{static_cast<uint8_t>(ParameterSet::SLH_DSA_SHA2_128S)};

struct VectorData {
    CMutableTransaction tx;
    std::vector<CTxOut> spent;
    UniValue vectors;
};

VectorData LoadVectors()
{
    UniValue data;
    BOOST_REQUIRE(data.read(json_tests::p2wpqh_sighash_vectors));
    VectorData out;
    BOOST_REQUIRE(DecodeHexTx(out.tx, data["tx"].get_str()));
    for (const UniValue& s : data["spent_outputs"].getValues()) {
        const std::vector<unsigned char> script{ParseHex(s["scriptPubKey"].get_str())};
        out.spent.emplace_back(s["amount"].getInt<int64_t>(), CScript(script.begin(), script.end()));
    }
    out.vectors = data["vectors"];
    return out;
}

} // namespace

BOOST_AUTO_TEST_CASE(p2wpqh_sighash_vectors)
{
    // The vectors come from an independent Python implementation of the spec
    // (contrib/devtools/gen-p2wpqh-sighash-vectors.py).
    const VectorData data{LoadVectors()};
    PrecomputedTransactionData txdata;
    txdata.Init(data.tx, std::vector<CTxOut>{data.spent}, /*force=*/true);

    size_t defined{0};
    for (const UniValue& v : data.vectors.getValues()) {
        const std::string& comment{v["comment"].get_str()};
        const auto in_pos{v["input_index"].getInt<uint32_t>()};
        const auto hash_type{static_cast<uint8_t>(v["hash_type"].getInt<int>())};
        const auto param_set_id{static_cast<uint8_t>(v["param_set_id"].getInt<int>())};
        const std::vector<unsigned char> pubkey{ParseHex(v["pubkey"].get_str())};

        uint256 sighash;
        const bool ok{SignatureHashPQ(sighash, data.tx, in_pos, hash_type, param_set_id, pubkey, txdata, MissingDataBehavior::FAIL)};
        if (v["sighash"].isNull()) {
            BOOST_CHECK_MESSAGE(!ok, comment);
        } else {
            BOOST_REQUIRE_MESSAGE(ok, comment);
            BOOST_CHECK_MESSAGE(HexStr(sighash) == v["sighash"].get_str(), comment);
            ++defined;
        }
        BOOST_CHECK_EQUAL(IsValidPQHashType(hash_type), hash_type != 0x04 && hash_type != 0x84);
    }
    BOOST_CHECK_EQUAL(defined, 9U);

    // Without spent outputs the signature hash is unavailable.
    PrecomputedTransactionData no_spent;
    no_spent.Init(data.tx, {}, /*force=*/true);
    uint256 sighash;
    BOOST_CHECK(!SignatureHashPQ(sighash, data.tx, 0, SIGHASH_DEFAULT, PARAM_SET_ID, std::vector<unsigned char>(32, 0), no_spent, MissingDataBehavior::FAIL));
}

BOOST_AUTO_TEST_CASE(p2wpqh_precomputation_detects_spends)
{
    const VectorData data{LoadVectors()};

    // A witness-bearing input that spends a P2WPQH output needs the BIP341-style hashes.
    CMutableTransaction pq_spend{data.tx};
    pq_spend.vin[0].scriptWitness.stack = {{0x01}};
    PrecomputedTransactionData pq_txdata;
    pq_txdata.Init(pq_spend, std::vector<CTxOut>{data.spent});
    BOOST_CHECK(pq_txdata.m_bip341_taproot_ready);
    BOOST_CHECK(pq_txdata.m_spent_outputs_ready);

    // A witness-bearing input that spends P2WPKH does not.
    CMutableTransaction v0_spend{data.tx};
    v0_spend.vin[1].scriptWitness.stack = {{0x01}};
    PrecomputedTransactionData v0_txdata;
    v0_txdata.Init(v0_spend, std::vector<CTxOut>{data.spent});
    BOOST_CHECK(!v0_txdata.m_bip341_taproot_ready);
    BOOST_CHECK(v0_txdata.m_bip143_segwit_ready);
}

#ifdef ENABLE_PQ_OQS_VENDOR
namespace {

constexpr unsigned int PQ_FLAGS{SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_WITNESS_V2};

uint256 Program(const std::vector<unsigned char>& pubkey)
{
    uint256 program;
    CSHA256().Write(&PARAM_SET_ID, 1).Write(pubkey.data(), pubkey.size()).Finalize(program.begin());
    return program;
}

CScript P2WPQHScript(const std::vector<unsigned char>& pubkey)
{
    const uint256 program{Program(pubkey)};
    return CScript() << OP_2 << std::vector<unsigned char>(program.begin(), program.end());
}

std::vector<unsigned char> SignInput0(const CMutableTransaction& tx, const std::vector<CTxOut>& spent, uint8_t hash_type,
                                      const std::vector<unsigned char>& pubkey, const std::vector<unsigned char>& privkey)
{
    PrecomputedTransactionData txdata;
    txdata.Init(tx, std::vector<CTxOut>{spent}, /*force=*/true);
    uint256 sighash;
    BOOST_REQUIRE(SignatureHashPQ(sighash, tx, 0, hash_type, PARAM_SET_ID, pubkey, txdata, MissingDataBehavior::FAIL));
    std::vector<unsigned char> payload;
    std::string error;
    BOOST_REQUIRE_MESSAGE(pq::sphincs::SignMessage(ParameterSet::SLH_DSA_SHA2_128S, privkey,
                                                   Span<const unsigned char>(sighash.begin(), sighash.size()), payload, error),
                          error);
    if (hash_type != SIGHASH_DEFAULT) payload.push_back(hash_type);
    return payload;
}

void SetWitness(CMutableTransaction& tx, const std::vector<unsigned char>& payload, const std::vector<unsigned char>& pubkey)
{
    tx.vin[0].scriptWitness.stack = {payload, {PARAM_SET_ID}, pubkey};
}

bool VerifyInput0(const CMutableTransaction& tx, const std::vector<CTxOut>& spent, ScriptError& error, bool with_spent_outputs = true)
{
    PrecomputedTransactionData txdata;
    txdata.Init(tx, with_spent_outputs ? std::vector<CTxOut>{spent} : std::vector<CTxOut>{});
    const MutableTransactionSignatureChecker checker{&tx, 0, spent[0].nValue, txdata, MissingDataBehavior::FAIL};
    return VerifyScript(tx.vin[0].scriptSig, spent[0].scriptPubKey, &tx.vin[0].scriptWitness, PQ_FLAGS, checker, &error);
}

struct Spend {
    CMutableTransaction tx;
    std::vector<CTxOut> spent;
};

Spend MakeSpend(const std::vector<unsigned char>& pubkey)
{
    Spend s;
    s.tx.version = 2;
    s.tx.nLockTime = 0;
    s.tx.vin = {CTxIn{COutPoint{Txid::FromUint256(uint256{1}), 0}}, CTxIn{COutPoint{Txid::FromUint256(uint256{2}), 3}}};
    s.tx.vout = {CTxOut{1 * COIN, CScript() << OP_TRUE}, CTxOut{2 * COIN, CScript() << OP_TRUE << OP_TRUE}};
    s.spent = {CTxOut{3 * COIN, P2WPQHScript(pubkey)}, CTxOut{1 * COIN, CScript() << OP_0 << std::vector<unsigned char>(20, 0x11)}};
    return s;
}

} // namespace

BOOST_AUTO_TEST_CASE(p2wpqh_spend_hash_types)
{
    std::string error;
    std::vector<unsigned char> pubkey, privkey;
    BOOST_REQUIRE(pq::sphincs::GenerateKeypair(ParameterSet::SLH_DSA_SHA2_128S, pubkey, privkey, error));
    const Spend base{MakeSpend(pubkey)};

    const uint8_t DEFAULT{SIGHASH_DEFAULT}, ALL{SIGHASH_ALL}, NONE{SIGHASH_NONE}, SINGLE{SIGHASH_SINGLE};
    const uint8_t ACP{SIGHASH_ANYONECANPAY};
    const std::vector<uint8_t> hash_types{DEFAULT, ALL, NONE, SINGLE, uint8_t(ALL | ACP), uint8_t(NONE | ACP), uint8_t(SINGLE | ACP)};

    // Each mutation lists the hash types under which the signature stays valid.
    struct Mutation {
        std::string name;
        std::function<void(CMutableTransaction&, std::vector<CTxOut>&)> apply;
        std::vector<uint8_t> still_valid;
    };
    const std::vector<Mutation> mutations{
        {"other output value", [](auto& tx, auto&) { tx.vout[1].nValue += 1; }, {NONE, SINGLE, uint8_t(NONE | ACP), uint8_t(SINGLE | ACP)}},
        {"same-index output value", [](auto& tx, auto&) { tx.vout[0].nValue += 1; }, {NONE, uint8_t(NONE | ACP)}},
        {"other input amount", [](auto&, auto& spent) { spent[1].nValue += 1; }, {uint8_t(ALL | ACP), uint8_t(NONE | ACP), uint8_t(SINGLE | ACP)}},
        {"other input scriptPubKey", [](auto&, auto& spent) { spent[1].scriptPubKey = CScript() << OP_1; }, {uint8_t(ALL | ACP), uint8_t(NONE | ACP), uint8_t(SINGLE | ACP)}},
        {"other input sequence", [](auto& tx, auto&) { tx.vin[1].nSequence -= 1; }, {uint8_t(ALL | ACP), uint8_t(NONE | ACP), uint8_t(SINGLE | ACP)}},
        {"own input amount", [](auto&, auto& spent) { spent[0].nValue += 1; }, {}},
        {"locktime", [](auto& tx, auto&) { tx.nLockTime += 1; }, {}},
    };

    for (const uint8_t hash_type : hash_types) {
        Spend signed_spend{base};
        SetWitness(signed_spend.tx, SignInput0(base.tx, base.spent, hash_type, pubkey, privkey), pubkey);

        ScriptError err;
        BOOST_CHECK_MESSAGE(VerifyInput0(signed_spend.tx, signed_spend.spent, err), "hash type " << int{hash_type} << ": " << ScriptErrorString(err));

        for (const Mutation& m : mutations) {
            Spend mutated{signed_spend};
            m.apply(mutated.tx, mutated.spent);
            const bool expect_valid{std::find(m.still_valid.begin(), m.still_valid.end(), hash_type) != m.still_valid.end()};
            const bool valid{VerifyInput0(mutated.tx, mutated.spent, err)};
            BOOST_CHECK_MESSAGE(valid == expect_valid, "hash type " << int{hash_type} << ", " << m.name << ": expected " << expect_valid);
            if (!expect_valid && !valid) BOOST_CHECK_EQUAL(err, SCRIPT_ERR_PQ_SIG_VERIFY);
        }
    }
}

BOOST_AUTO_TEST_CASE(p2wpqh_payload_rules)
{
    std::string error;
    std::vector<unsigned char> pubkey, privkey;
    BOOST_REQUIRE(pq::sphincs::GenerateKeypair(ParameterSet::SLH_DSA_SHA2_128S, pubkey, privkey, error));
    const Spend base{MakeSpend(pubkey)};
    const std::vector<unsigned char> default_payload{SignInput0(base.tx, base.spent, SIGHASH_DEFAULT, pubkey, privkey)};
    const std::vector<unsigned char> all_payload{SignInput0(base.tx, base.spent, SIGHASH_ALL, pubkey, privkey)};
    BOOST_CHECK_EQUAL(default_payload.size(), 1 + pq::sphincs::SPHINCS_SIGNATURE_SIZE_SHA2_128S);
    BOOST_CHECK_EQUAL(all_payload.size(), 2 + pq::sphincs::SPHINCS_SIGNATURE_SIZE_SHA2_128S);

    const auto check = [&](std::vector<unsigned char> payload, bool expect_valid, ScriptError expect_error, const std::string& what) {
        Spend s{base};
        SetWitness(s.tx, payload, pubkey);
        ScriptError err;
        const bool valid{VerifyInput0(s.tx, s.spent, err)};
        BOOST_CHECK_MESSAGE(valid == expect_valid, what);
        if (!expect_valid) BOOST_CHECK_MESSAGE(err == expect_error, what << ": got " << ScriptErrorString(err));
    };

    check(default_payload, true, SCRIPT_ERR_OK, "SIGHASH_DEFAULT payload");
    check(all_payload, true, SCRIPT_ERR_OK, "SIGHASH_ALL payload");

    // An explicit SIGHASH_DEFAULT byte is invalid.
    auto explicit_default{default_payload};
    explicit_default.push_back(SIGHASH_DEFAULT);
    check(explicit_default, false, SCRIPT_ERR_PQ_SIG_HASHTYPE, "explicit 0x00 hash type");

    // Undefined hash types are invalid.
    for (const uint8_t bad : {uint8_t{0x04}, uint8_t{0x80}, uint8_t{0x84}, uint8_t{0xff}}) {
        auto payload{default_payload};
        payload.push_back(bad);
        check(payload, false, SCRIPT_ERR_PQ_SIG_HASHTYPE, "hash type " + std::to_string(bad));
    }

    // The hash type is committed: relabeling a signature breaks it, and
    // SIGHASH_DEFAULT and SIGHASH_ALL signatures are not interchangeable.
    auto relabeled{all_payload};
    relabeled.back() = SIGHASH_NONE;
    check(relabeled, false, SCRIPT_ERR_PQ_SIG_VERIFY, "SIGHASH_ALL signature relabeled as SIGHASH_NONE");
    auto default_as_all{default_payload};
    default_as_all.push_back(SIGHASH_ALL);
    check(default_as_all, false, SCRIPT_ERR_PQ_SIG_VERIFY, "SIGHASH_DEFAULT signature labeled SIGHASH_ALL");
    check(std::vector<unsigned char>(all_payload.begin(), all_payload.end() - 1), false, SCRIPT_ERR_PQ_SIG_VERIFY, "SIGHASH_ALL signature without its byte");

    // Wrong lengths are format errors.
    check(std::vector<unsigned char>(default_payload.begin(), default_payload.end() - 1), false, SCRIPT_ERR_PQ_SIG_FORMAT, "truncated payload");
    auto too_long{all_payload};
    too_long.push_back(SIGHASH_ALL);
    check(too_long, false, SCRIPT_ERR_PQ_SIG_FORMAT, "payload with two hash type bytes");

    // Verification fails cleanly when spent outputs are unknown.
    {
        Spend s{base};
        SetWitness(s.tx, default_payload, pubkey);
        ScriptError err;
        BOOST_CHECK(!VerifyInput0(s.tx, s.spent, err, /*with_spent_outputs=*/false));
    }

    // SIGHASH_SINGLE without a matching output is undefined: it can't be signed or verified.
    {
        Spend no_outputs{base};
        no_outputs.tx.vout.clear();
        PrecomputedTransactionData txdata;
        txdata.Init(no_outputs.tx, std::vector<CTxOut>{no_outputs.spent}, /*force=*/true);
        uint256 sighash;
        BOOST_CHECK(!SignatureHashPQ(sighash, no_outputs.tx, 0, SIGHASH_SINGLE, PARAM_SET_ID, pubkey, txdata, MissingDataBehavior::FAIL));
        auto payload{SignInput0(no_outputs.tx, no_outputs.spent, SIGHASH_DEFAULT, pubkey, privkey)};
        payload.push_back(SIGHASH_SINGLE);
        SetWitness(no_outputs.tx, payload, pubkey);
        ScriptError err;
        BOOST_CHECK(!VerifyInput0(no_outputs.tx, no_outputs.spent, err));
        BOOST_CHECK_EQUAL(err, SCRIPT_ERR_PQ_SIG_HASHTYPE);
    }
}

BOOST_AUTO_TEST_CASE(p2wpqh_sign_transaction_hash_types)
{
    std::string error;
    std::vector<unsigned char> pubkey, privkey;
    BOOST_REQUIRE(pq::sphincs::GenerateKeypair(ParameterSet::SLH_DSA_SHA2_128S, pubkey, privkey, error));
    const Spend base{MakeSpend(pubkey)};

    FlatSigningProvider provider;
    provider.pq_keys[Program(pubkey)] = PQKeyData{PARAM_SET_ID, pubkey, privkey};
    std::map<COutPoint, Coin> coins;
    coins[base.tx.vin[0].prevout] = Coin{base.spent[0], /*nHeightIn=*/1, /*fCoinBaseIn=*/false};
    coins[base.tx.vin[1].prevout] = Coin{base.spent[1], /*nHeightIn=*/1, /*fCoinBaseIn=*/false};

    for (const int hash_type : {int{SIGHASH_DEFAULT}, int{SIGHASH_ALL}, int{SIGHASH_SINGLE | SIGHASH_ANYONECANPAY}}) {
        CMutableTransaction tx{base.tx};
        std::map<int, bilingual_str> input_errors;
        // Input 1 has no key in the provider, so the call reports it; input 0 is signed.
        ::SignTransaction(tx, &provider, coins, hash_type, input_errors);
        BOOST_CHECK(!input_errors.count(0));
        BOOST_REQUIRE_EQUAL(tx.vin[0].scriptWitness.stack.size(), 3U);
        const auto& payload{tx.vin[0].scriptWitness.stack[0]};
        if (hash_type == SIGHASH_DEFAULT) {
            BOOST_CHECK_EQUAL(payload.size(), 1 + pq::sphincs::SPHINCS_SIGNATURE_SIZE_SHA2_128S);
        } else {
            BOOST_CHECK_EQUAL(payload.size(), 2 + pq::sphincs::SPHINCS_SIGNATURE_SIZE_SHA2_128S);
            BOOST_CHECK_EQUAL(int{payload.back()}, hash_type);
        }
        ScriptError err;
        BOOST_CHECK_MESSAGE(VerifyInput0(tx, base.spent, err), "hash type " << hash_type << ": " << ScriptErrorString(err));
    }

    // An invalid hash type is refused at signing time.
    CMutableTransaction tx{base.tx};
    std::map<int, bilingual_str> input_errors;
    ::SignTransaction(tx, &provider, coins, 0x04, input_errors);
    BOOST_CHECK(tx.vin[0].scriptWitness.IsNull());
}
#endif // ENABLE_PQ_OQS_VENDOR

BOOST_AUTO_TEST_SUITE_END()
