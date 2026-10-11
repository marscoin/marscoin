// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include <crypto/pq_sphincs.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>
#include <test/util/logging.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

namespace wallet {

BOOST_AUTO_TEST_SUITE(walletload_tests)

class DummyDescriptor final : public Descriptor {
private:
    std::string desc;
public:
    explicit DummyDescriptor(const std::string& descriptor) : desc(descriptor) {};
    ~DummyDescriptor() = default;

    std::string ToString(bool compat_format) const override { return desc; }
    std::optional<OutputType> GetOutputType() const override { return OutputType::UNKNOWN; }

    bool IsRange() const override { return false; }
    bool IsSolvable() const override { return false; }
    bool IsSingleType() const override { return true; }
    bool ToPrivateString(const SigningProvider& provider, std::string& out) const override { return false; }
    bool ToNormalizedString(const SigningProvider& provider, std::string& out, const DescriptorCache* cache = nullptr) const override { return false; }
    bool Expand(int pos, const SigningProvider& provider, std::vector<CScript>& output_scripts, FlatSigningProvider& out, DescriptorCache* write_cache = nullptr) const override { return false; };
    bool ExpandFromCache(int pos, const DescriptorCache& read_cache, std::vector<CScript>& output_scripts, FlatSigningProvider& out) const override { return false; }
    void ExpandPrivate(int pos, const SigningProvider& provider, FlatSigningProvider& out) const override {}
    std::optional<int64_t> ScriptSize() const override { return {}; }
    std::optional<int64_t> MaxSatisfactionWeight(bool) const override { return {}; }
    std::optional<int64_t> MaxSatisfactionElems() const override { return {}; }
    void GetPubKeys(std::set<CPubKey>& pubkeys, std::set<CExtPubKey>& ext_pubs) const override {}
};

BOOST_FIXTURE_TEST_CASE(wallet_load_descriptors, TestingSetup)
{
    std::unique_ptr<WalletDatabase> database = CreateMockableWalletDatabase();
    {
        // Write unknown active descriptor
        WalletBatch batch(*database, false);
        std::string unknown_desc = "trx(tpubD6NzVbkrYhZ4Y4S7m6Y5s9GD8FqEMBy56AGphZXuagajudVZEnYyBahZMgHNCTJc2at82YX6s8JiL1Lohu5A3v1Ur76qguNH4QVQ7qYrBQx/86'/1'/0'/0/*)#8pn8tzdt";
        WalletDescriptor wallet_descriptor(std::make_shared<DummyDescriptor>(unknown_desc), 0, 0, 0, 0);
        BOOST_CHECK(batch.WriteDescriptor(uint256(), wallet_descriptor));
        BOOST_CHECK(batch.WriteActiveScriptPubKeyMan(static_cast<uint8_t>(OutputType::UNKNOWN), uint256(), false));
    }

    {
        // Now try to load the wallet and verify the error.
        const std::shared_ptr<CWallet> wallet(new CWallet(m_node.chain.get(), "", std::move(database)));
        BOOST_CHECK_EQUAL(wallet->LoadWallet(), DBErrors::UNKNOWN_DESCRIPTOR);
    }

    // Test 2
    // Now write a valid descriptor with an invalid ID.
    // As the software produces another ID for the descriptor, the loading process must be aborted.
    database = CreateMockableWalletDatabase();

    // Verify the error
    bool found = false;
    DebugLogHelper logHelper("The descriptor ID calculated by the wallet differs from the one in DB", [&](const std::string* s) {
        found = true;
        return false;
    });

    {
        // Write valid descriptor with invalid ID
        WalletBatch batch(*database, false);
        std::string desc = "wpkh([d34db33f/84h/0h/0h]xpub6DJ2dNUysrn5Vt36jH2KLBT2i1auw1tTSSomg8PhqNiUtx8QX2SvC9nrHu81fT41fvDUnhMjEzQgXnQjKEu3oaqMSzhSrHMxyyoEAmUHQbY/0/*)#cjjspncu";
        WalletDescriptor wallet_descriptor(std::make_shared<DummyDescriptor>(desc), 0, 0, 0, 0);
        BOOST_CHECK(batch.WriteDescriptor(uint256::ONE, wallet_descriptor));
    }

    {
        // Now try to load the wallet and verify the error.
        const std::shared_ptr<CWallet> wallet(new CWallet(m_node.chain.get(), "", std::move(database)));
        BOOST_CHECK_EQUAL(wallet->LoadWallet(), DBErrors::CORRUPT);
        BOOST_CHECK(found); // The error must be logged
    }
}

bool HasAnyRecordOfType(WalletDatabase& db, const std::string& key)
{
    std::unique_ptr<DatabaseBatch> batch = db.MakeBatch(false);
    BOOST_CHECK(batch);
    std::unique_ptr<DatabaseCursor> cursor = batch->GetNewCursor();
    BOOST_CHECK(cursor);
    while (true) {
        DataStream ssKey{};
        DataStream ssValue{};
        DatabaseCursor::Status status = cursor->Next(ssKey, ssValue);
        assert(status != DatabaseCursor::Status::FAIL);
        if (status == DatabaseCursor::Status::DONE) break;
        std::string type;
        ssKey >> type;
        if (type == key) return true;
    }
    return false;
}

template<typename... Args>
SerializeData MakeSerializeData(const Args&... args)
{
    DataStream s{};
    SerializeMany(s, args...);
    return {s.begin(), s.end()};
}


BOOST_FIXTURE_TEST_CASE(wallet_load_ckey, TestingSetup)
{
    SerializeData ckey_record_key;
    SerializeData ckey_record_value;
    MockableData records;

    {
        // Context setup.
        // Create and encrypt legacy wallet
        std::shared_ptr<CWallet> wallet(new CWallet(m_node.chain.get(), "", CreateMockableWalletDatabase()));
        LOCK(wallet->cs_wallet);
        auto legacy_spkm = wallet->GetOrCreateLegacyScriptPubKeyMan();
        BOOST_CHECK(legacy_spkm->SetupGeneration(true));

        // Retrieve a key
        CTxDestination dest = *Assert(legacy_spkm->GetNewDestination(OutputType::LEGACY));
        CKeyID key_id = GetKeyForDestination(*legacy_spkm, dest);
        CKey first_key;
        BOOST_CHECK(legacy_spkm->GetKey(key_id, first_key));

        // Encrypt the wallet
        BOOST_CHECK(wallet->EncryptWallet("encrypt"));
        wallet->Flush();

        // Store a copy of all the records
        records = GetMockableDatabase(*wallet).m_records;

        // Get the record for the retrieved key
        ckey_record_key = MakeSerializeData(DBKeys::CRYPTED_KEY, first_key.GetPubKey());
        ckey_record_value = records.at(ckey_record_key);
    }

    {
        // First test case:
        // Erase all the crypted keys from db and unlock the wallet.
        // The wallet will only re-write the crypted keys to db if any checksum is missing at load time.
        // So, if any 'ckey' record re-appears on db, then the checksums were not properly calculated, and we are re-writing
        // the records every time that 'CWallet::Unlock' gets called, which is not good.

        // Load the wallet and check that is encrypted
        std::shared_ptr<CWallet> wallet(new CWallet(m_node.chain.get(), "", CreateMockableWalletDatabase(records)));
        BOOST_CHECK_EQUAL(wallet->LoadWallet(), DBErrors::LOAD_OK);
        BOOST_CHECK(wallet->IsCrypted());
        BOOST_CHECK(HasAnyRecordOfType(wallet->GetDatabase(), DBKeys::CRYPTED_KEY));

        // Now delete all records and check that the 'Unlock' function doesn't re-write them
        BOOST_CHECK(wallet->GetLegacyScriptPubKeyMan()->DeleteRecords());
        BOOST_CHECK(!HasAnyRecordOfType(wallet->GetDatabase(), DBKeys::CRYPTED_KEY));
        BOOST_CHECK(wallet->Unlock("encrypt"));
        BOOST_CHECK(!HasAnyRecordOfType(wallet->GetDatabase(), DBKeys::CRYPTED_KEY));
    }

    {
        // Second test case:
        // Verify that loading up a 'ckey' with no checksum triggers a complete re-write of the crypted keys.

        // Cut off the 32 byte checksum from a ckey record
        records[ckey_record_key].resize(ckey_record_value.size() - 32);

        // Load the wallet and check that is encrypted
        std::shared_ptr<CWallet> wallet(new CWallet(m_node.chain.get(), "", CreateMockableWalletDatabase(records)));
        BOOST_CHECK_EQUAL(wallet->LoadWallet(), DBErrors::LOAD_OK);
        BOOST_CHECK(wallet->IsCrypted());
        BOOST_CHECK(HasAnyRecordOfType(wallet->GetDatabase(), DBKeys::CRYPTED_KEY));

        // Now delete all ckey records and check that the 'Unlock' function re-writes them
        // (this is because the wallet, at load time, found a ckey record with no checksum)
        BOOST_CHECK(wallet->GetLegacyScriptPubKeyMan()->DeleteRecords());
        BOOST_CHECK(!HasAnyRecordOfType(wallet->GetDatabase(), DBKeys::CRYPTED_KEY));
        BOOST_CHECK(wallet->Unlock("encrypt"));
        BOOST_CHECK(HasAnyRecordOfType(wallet->GetDatabase(), DBKeys::CRYPTED_KEY));
    }

    {
        // Third test case:
        // Verify that loading up a 'ckey' with an invalid checksum throws an error.

        // Cut off the 32 byte checksum from a ckey record
        records[ckey_record_key].resize(ckey_record_value.size() - 32);
        // Fill in the checksum space with 0s
        records[ckey_record_key].resize(ckey_record_value.size());

        std::shared_ptr<CWallet> wallet(new CWallet(m_node.chain.get(), "", CreateMockableWalletDatabase(records)));
        BOOST_CHECK_EQUAL(wallet->LoadWallet(), DBErrors::CORRUPT);
    }

    {
        // Fourth test case:
        // Verify that loading up a 'ckey' with an invalid pubkey throws an error
        CPubKey invalid_key;
        BOOST_CHECK(!invalid_key.IsValid());
        SerializeData key = MakeSerializeData(DBKeys::CRYPTED_KEY, invalid_key);
        records[key] = ckey_record_value;

        std::shared_ptr<CWallet> wallet(new CWallet(m_node.chain.get(), "", CreateMockableWalletDatabase(records)));
        BOOST_CHECK_EQUAL(wallet->LoadWallet(), DBErrors::CORRUPT);
    }
}

//! Whether any record value in the wallet database contains the given bytes.
static bool AnyRecordContains(CWallet& wallet, const std::vector<unsigned char>& needle)
{
    for (const auto& [key, value] : GetMockableDatabase(wallet).m_records) {
        const auto* begin = reinterpret_cast<const unsigned char*>(value.data());
        if (std::search(begin, begin + value.size(), needle.begin(), needle.end()) != begin + value.size()) return true;
    }
    return false;
}

BOOST_FIXTURE_TEST_CASE(wallet_pq_key_encryption, TestingSetup)
{
    // Fake SPHINCS+ keypair with the real layout: the 64-byte secret key is
    // SK.seed || SK.prf || PK.seed || PK.root and the public key is PK.seed || PK.root.
    std::vector<unsigned char> pubkey(pq::sphincs::SPHINCS_PUBLIC_KEY_SIZE_SHA2_128S);
    std::vector<unsigned char> privkey(pq::sphincs::SPHINCS_SECRET_KEY_SIZE_SHA2_128S);
    for (size_t i = 0; i < pubkey.size(); ++i) pubkey[i] = static_cast<unsigned char>(0xa0 + i);
    for (size_t i = 0; i < 32; ++i) privkey[i] = static_cast<unsigned char>(0x10 + i);
    std::copy(pubkey.begin(), pubkey.end(), privkey.begin() + 32);
    const std::vector<unsigned char> secret_part(privkey.begin(), privkey.begin() + 32);
    const uint256 program_a{uint256::ONE};
    const uint256 program_b{uint256S("02")};
    const uint256 program_c{uint256S("03")};

    std::shared_ptr<CWallet> wallet(new CWallet(m_node.chain.get(), "", CreateMockableWalletDatabase()));
    {
        LOCK(wallet->cs_wallet);
        wallet->SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
        wallet->SetupDescriptorScriptPubKeyMans();

        // A secret key that does not end with its public key is rejected
        std::vector<unsigned char> mismatched{privkey};
        mismatched.back() ^= 1;
        BOOST_CHECK(!wallet->AddPQKey(program_c, 0, pubkey, mismatched));

        // An unencrypted wallet stores the key in a plaintext record
        BOOST_CHECK(wallet->AddPQKey(program_a, 0, pubkey, privkey));
    }
    BOOST_CHECK(HasAnyRecordOfType(wallet->GetDatabase(), DBKeys::PQKEY));
    BOOST_CHECK(AnyRecordContains(*wallet, secret_part));

    // Encrypting the wallet encrypts existing PQ keys
    BOOST_CHECK(wallet->EncryptWallet("pass"));
    BOOST_CHECK(!HasAnyRecordOfType(wallet->GetDatabase(), DBKeys::PQKEY));
    BOOST_CHECK(HasAnyRecordOfType(wallet->GetDatabase(), DBKeys::CRYPTED_PQKEY));
    BOOST_CHECK(!AnyRecordContains(*wallet, secret_part));

    // Locked: the key is known but cannot be read, and new keys cannot be stored
    BOOST_CHECK(wallet->IsLocked());
    BOOST_CHECK(wallet->HavePQKey(program_a));
    PQKeyData key;
    BOOST_CHECK(!wallet->GetPQKey(program_a, key));
    {
        LOCK(wallet->cs_wallet);
        BOOST_CHECK(!wallet->AddPQKey(program_b, 0, pubkey, privkey));
    }

    // Unlocked: the key decrypts to the original
    BOOST_CHECK(wallet->Unlock("pass"));
    BOOST_CHECK(wallet->GetPQKey(program_a, key));
    BOOST_CHECK(key.pubkey == pubkey);
    BOOST_CHECK(key.privkey == privkey);
    {
        // Keys added while unlocked are stored encrypted
        LOCK(wallet->cs_wallet);
        BOOST_CHECK(wallet->AddPQKey(program_b, 0, pubkey, privkey));
    }
    BOOST_CHECK(!HasAnyRecordOfType(wallet->GetDatabase(), DBKeys::PQKEY));
    BOOST_CHECK(!AnyRecordContains(*wallet, secret_part));
    BOOST_CHECK(wallet->Lock());

    // An encrypted wallet that still holds a plaintext PQ key (written by an
    // earlier build) loads, and the key is encrypted on the next unlock
    {
        WalletBatch batch(wallet->GetDatabase());
        BOOST_CHECK(batch.WritePQKey(program_c, 0, pubkey, privkey));
    }
    const MockableData records{GetMockableDatabase(*wallet).m_records};
    std::shared_ptr<CWallet> reloaded(new CWallet(m_node.chain.get(), "", CreateMockableWalletDatabase(records)));
    BOOST_CHECK_EQUAL(reloaded->LoadWallet(), DBErrors::LOAD_OK);
    BOOST_CHECK(reloaded->IsCrypted());
    BOOST_CHECK(reloaded->HavePQKey(program_a));
    BOOST_CHECK(reloaded->HavePQKey(program_c));
    BOOST_CHECK(HasAnyRecordOfType(reloaded->GetDatabase(), DBKeys::PQKEY));
    BOOST_CHECK(reloaded->Unlock("pass"));
    BOOST_CHECK(!HasAnyRecordOfType(reloaded->GetDatabase(), DBKeys::PQKEY));
    BOOST_CHECK(!AnyRecordContains(*reloaded, secret_part));
    BOOST_CHECK(reloaded->GetPQKey(program_c, key));
    BOOST_CHECK(key.privkey == privkey);
    BOOST_CHECK(reloaded->GetPQKey(program_a, key));
    BOOST_CHECK(key.privkey == privkey);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet
