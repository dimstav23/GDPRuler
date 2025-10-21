#include <gtest/gtest.h>
#include "Crypto.hpp"
#include <string>
#include <vector>
#include <algorithm>

class CryptoTest : public ::testing::Test
{
protected:
    Crypto crypto;

    // Helper method to create a random key of proper size
    std::vector<uint8_t> createRandomKey()
    {
        std::vector<uint8_t> key(Crypto::KEY_SIZE);
        for (size_t i = 0; i < key.size(); ++i)
        {
            key[i] = static_cast<uint8_t>(rand() % 256);
        }
        return key;
    }

    // Helper method to create a dummy IV
    std::vector<uint8_t> createDummyIV()
    {
        return std::vector<uint8_t>(Crypto::GCM_IV_SIZE, 0x24);
    }

    // Helper method to convert string to byte vector
    std::vector<uint8_t> stringToBytes(const std::string &str)
    {
        return std::vector<uint8_t>(str.begin(), str.end());
    }

    // Helper method to convert byte vector to string
    std::string bytesToString(const std::vector<uint8_t> &bytes)
    {
        return std::string(bytes.begin(), bytes.end());
    }

    void SetUp() override
    {
        // Seed random number generator for consistent test results
        srand(42);
    }
};

// Test empty data encryption and decryption
TEST_F(CryptoTest, EmptyData)
{
    std::vector<uint8_t> emptyData;
    std::vector<uint8_t> key = createRandomKey();
    std::vector<uint8_t> iv = createDummyIV();

    // Encrypt empty data
    std::vector<uint8_t> encrypted = crypto.encrypt(std::move(emptyData), key, iv);
    EXPECT_TRUE(encrypted.empty());

    // Decrypt empty data
    std::vector<uint8_t> decrypted = crypto.decrypt(encrypted, key, iv);
    EXPECT_TRUE(decrypted.empty());
}

// Test basic encryption and decryption
TEST_F(CryptoTest, BasicEncryptDecrypt)
{
    std::string testMessage = "This is a test message for encryption";
    std::vector<uint8_t> data = stringToBytes(testMessage);
    std::vector<uint8_t> key = createRandomKey();
    std::vector<uint8_t> iv = createDummyIV();

    // Encrypt the data
    std::vector<uint8_t> encrypted = crypto.encrypt(std::move(data), key, iv);
    EXPECT_FALSE(encrypted.empty());

    // The encrypted data should be different from the original
    EXPECT_NE(data, encrypted);

    // Decrypt the data
    std::vector<uint8_t> decrypted = crypto.decrypt(encrypted, key, iv);

    // The decrypted data should match the original
    EXPECT_EQ(data, decrypted);
    EXPECT_EQ(testMessage, bytesToString(decrypted));
}

// Test encryption with various data sizes
TEST_F(CryptoTest, VariousDataSizes)
{
    std::vector<size_t> sizes = {10, 100, 1000, 10000};
    std::vector<uint8_t> key = createRandomKey();
    std::vector<uint8_t> iv = createDummyIV();

    for (size_t size : sizes)
    {
        // Create data of specified size
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i)
        {
            data[i] = static_cast<uint8_t>(i % 256);
        }

        // Encrypt the data
        std::vector<uint8_t> encrypted = crypto.encrypt(std::move(data), key, iv);
        EXPECT_FALSE(encrypted.empty());

        // Decrypt the data
        std::vector<uint8_t> decrypted = crypto.decrypt(encrypted, key, iv);

        // The decrypted data should match the original
        EXPECT_EQ(data, decrypted);
    }
}

// Test encryption with invalid key size
TEST_F(CryptoTest, InvalidKeySize)
{
    std::string testMessage = "Testing invalid key size";
    std::vector<uint8_t> data = stringToBytes(testMessage);
    std::vector<uint8_t> iv = createDummyIV();

    // Create keys with invalid sizes
    std::vector<uint8_t> shortKey(16); // Too short
    std::vector<uint8_t> longKey(64);  // Too long

    // Encryption with short key should throw
    EXPECT_THROW(crypto.encrypt(std::move(data), shortKey, iv), std::runtime_error);

    // Encryption with long key should throw
    EXPECT_THROW(crypto.encrypt(std::move(data), longKey, iv), std::runtime_error);
}

// Test encryption with invalid IV size
TEST_F(CryptoTest, InvalidIVSize)
{
    std::string testMessage = "Testing invalid IV size";
    std::vector<uint8_t> data = stringToBytes(testMessage);
    std::vector<uint8_t> key = createRandomKey();

    // Create IVs with invalid sizes
    std::vector<uint8_t> shortIV(8); // Too short
    std::vector<uint8_t> longIV(16); // Too long

    // Encryption with short IV should throw
    EXPECT_THROW(crypto.encrypt(std::move(data), key, shortIV), std::runtime_error);

    // Encryption with long IV should throw
    EXPECT_THROW(crypto.encrypt(std::move(data), key, longIV), std::runtime_error);
}

// Test decryption with wrong key
TEST_F(CryptoTest, WrongKey)
{
    std::string testMessage = "This should not decrypt correctly with wrong key";
    std::vector<uint8_t> data = stringToBytes(testMessage);
    std::vector<uint8_t> iv = createDummyIV();

    // Create two different keys
    std::vector<uint8_t> correctKey = createRandomKey();
    std::vector<uint8_t> wrongKey = createRandomKey();

    // Make sure the keys are different
    ASSERT_NE(correctKey, wrongKey);

    // Encrypt with the correct key
    std::vector<uint8_t> encrypted = crypto.encrypt(std::move(data), correctKey, iv);

    // Attempt to decrypt with the wrong key
    std::vector<uint8_t> decrypted = crypto.decrypt(encrypted, wrongKey, iv);

    // The decryption should fail (return empty vector) or the result should be different
    // from the original data
    EXPECT_TRUE(decrypted.empty() || decrypted != data);
}

// Test decryption with wrong IV
TEST_F(CryptoTest, WrongIV)
{
    std::string testMessage = "This should not decrypt correctly with wrong IV";
    std::vector<uint8_t> data = stringToBytes(testMessage);
    std::vector<uint8_t> key = createRandomKey();

    // Create two different IVs
    std::vector<uint8_t> correctIV = createDummyIV();
    std::vector<uint8_t> wrongIV(Crypto::GCM_IV_SIZE, 0x42); // Different value

    // Make sure the IVs are different
    ASSERT_NE(correctIV, wrongIV);

    // Encrypt with the correct IV
    std::vector<uint8_t> encrypted = crypto.encrypt(std::move(data), key, correctIV);

    // Attempt to decrypt with the wrong IV
    std::vector<uint8_t> decrypted = crypto.decrypt(encrypted, key, wrongIV);

    // The decryption should fail (return empty vector) or the result should be different
    // from the original data
    EXPECT_TRUE(decrypted.empty() || decrypted != data);
}

// Test tampering detection
TEST_F(CryptoTest, TamperingDetection)
{
    std::string testMessage = "This message should be protected against tampering";
    std::vector<uint8_t> data = stringToBytes(testMessage);
    std::vector<uint8_t> key = createRandomKey();
    std::vector<uint8_t> iv = createDummyIV();

    // Encrypt the data
    std::vector<uint8_t> encrypted = crypto.encrypt(std::move(data), key, iv);
    ASSERT_FALSE(encrypted.empty());

    // Tamper with the encrypted data (modify a byte in the middle)
    if (encrypted.size() > 20)
    {
        encrypted[encrypted.size() / 2] ^= 0xFF; // Flip all bits in one byte

        // Decryption should now fail or produce incorrect results
        std::vector<uint8_t> decrypted = crypto.decrypt(encrypted, key, iv);
        EXPECT_TRUE(decrypted.empty() || decrypted != data);
    }
}

// Test binary data encryption and decryption
TEST_F(CryptoTest, BinaryData)
{
    // Create binary data with all possible byte values
    std::vector<uint8_t> binaryData(256);
    for (int i = 0; i < 256; ++i)
    {
        binaryData[i] = static_cast<uint8_t>(i);
    }

    std::vector<uint8_t> key = createRandomKey();
    std::vector<uint8_t> iv = createDummyIV();

    // Encrypt the binary data
    std::vector<uint8_t> encrypted = crypto.encrypt(std::move(binaryData), key, iv);
    EXPECT_FALSE(encrypted.empty());

    // Decrypt the data
    std::vector<uint8_t> decrypted = crypto.decrypt(encrypted, key, iv);

    // The decrypted data should match the original
    EXPECT_EQ(binaryData, decrypted);
}

// Test large data encryption and decryption
TEST_F(CryptoTest, LargeData)
{
    // Create a large data set (1MB)
    const size_t size = 1024 * 1024;
    std::vector<uint8_t> largeData(size);
    for (size_t i = 0; i < size; ++i)
    {
        largeData[i] = static_cast<uint8_t>(i % 256);
    }

    std::vector<uint8_t> key = createRandomKey();
    std::vector<uint8_t> iv = createDummyIV();

    // Encrypt the large data
    std::vector<uint8_t> encrypted = crypto.encrypt(std::move(largeData), key, iv);
    EXPECT_FALSE(encrypted.empty());

    // Decrypt the data
    std::vector<uint8_t> decrypted = crypto.decrypt(encrypted, key, iv);

    // The decrypted data should match the original
    EXPECT_EQ(largeData, decrypted);
}

// Test encryption and decryption with a fixed key and IV (for reproducibility)
TEST_F(CryptoTest, FixedKeyAndIV)
{
    std::string testMessage = "Testing with fixed key and IV";
    std::vector<uint8_t> data = stringToBytes(testMessage);

    // Create a fixed key and IV
    std::vector<uint8_t> fixedKey(Crypto::KEY_SIZE, 0x42);   // Fill with the value 0x42
    std::vector<uint8_t> fixedIV(Crypto::GCM_IV_SIZE, 0x24); // Fill with the value 0x24

    // Encrypt with the fixed key and IV
    std::vector<uint8_t> encrypted1 = crypto.encrypt(std::move(data), fixedKey, fixedIV);
    EXPECT_FALSE(encrypted1.empty());

    // Decrypt with the same key and IV
    std::vector<uint8_t> decrypted = crypto.decrypt(encrypted1, fixedKey, fixedIV);
    EXPECT_EQ(data, decrypted);

    // The same data encrypted with the same key and IV should produce the same ciphertexts
    // unlike the previous version with random IVs
    std::vector<uint8_t> encrypted2 = crypto.encrypt(std::move(data), fixedKey, fixedIV);
    EXPECT_EQ(encrypted1, encrypted2); // This test should now PASS with fixed IV
}

// Test that different IVs produce different ciphertexts
TEST_F(CryptoTest, DifferentIVs)
{
    std::string testMessage = "Testing with different IVs";
    std::vector<uint8_t> data = stringToBytes(testMessage);
    std::vector<uint8_t> key = createRandomKey();

    // Create two different IVs
    std::vector<uint8_t> iv1(Crypto::GCM_IV_SIZE, 0x24);
    std::vector<uint8_t> iv2(Crypto::GCM_IV_SIZE, 0x42);

    // Encrypt with different IVs
    std::vector<uint8_t> encrypted1 = crypto.encrypt(std::move(data), key, iv1);
    std::vector<uint8_t> encrypted2 = crypto.encrypt(std::move(data), key, iv2);

    // The ciphertexts should be different
    EXPECT_NE(encrypted1, encrypted2);

    // But both should decrypt correctly with their respective IVs
    std::vector<uint8_t> decrypted1 = crypto.decrypt(encrypted1, key, iv1);
    std::vector<uint8_t> decrypted2 = crypto.decrypt(encrypted2, key, iv2);

    EXPECT_EQ(data, decrypted1);
    EXPECT_EQ(data, decrypted2);
}

// Main function that runs all the tests
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}