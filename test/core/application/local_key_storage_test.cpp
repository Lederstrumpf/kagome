/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "application/impl/local_key_storage.hpp"

#include <gtest/gtest.h>
#include <libp2p/crypto/key_generator/key_generator_impl.hpp>
#include <libp2p/crypto/key_validator/key_validator_impl.hpp>
#include <libp2p/crypto/random_generator/boost_generator.hpp>
#include <testutil/outcome.hpp>
#include "application/impl/key_storage_error.hpp"

using kagome::application::KeyStorageError;
using kagome::application::LocalKeyStorage;
using libp2p::crypto::KeyGeneratorImpl;
using libp2p::crypto::random::BoostRandomGenerator;
using libp2p::crypto::validator::KeyValidatorImpl;

class LocalKeyStorageTest : public testing::Test {
 public:
  void SetUp() {
    generator = std::make_shared<KeyGeneratorImpl>(random_generator);
    validator = std::make_shared<KeyValidatorImpl>(generator);
    auto path = boost::filesystem::path(__FILE__).parent_path().string();
    config.ed25519_keypair_location = path + "/ed25519key.pem";
    config.sr25519_keypair_location = path + "/sr25519key.txt";
    config.p2p_keypair_location = path + "/ed25519key.pem";
    config.p2p_keypair_type = libp2p::crypto::Key::Type::Ed25519;
  }

  LocalKeyStorage::Config config;
  BoostRandomGenerator random_generator;
  std::shared_ptr<KeyGeneratorImpl> generator;
  std::shared_ptr<KeyValidatorImpl> validator;
};

TEST_F(LocalKeyStorageTest, CreateWithEd25519) {
  auto s = LocalKeyStorage::create(config, generator, validator);
  EXPECT_OUTCOME_TRUE_1(s);
}

TEST_F(LocalKeyStorageTest, FileNotFound) {
  config.p2p_keypair_location = "aaa";
  auto s = LocalKeyStorage::create(config, generator, validator);
  EXPECT_OUTCOME_FALSE(e, s);
  ASSERT_EQ(e, KeyStorageError::FILE_READ_ERROR);
}

TEST_F(LocalKeyStorageTest, UnsupportedKeyType) {
  config.p2p_keypair_type = libp2p::crypto::Key::Type::UNSPECIFIED;
  auto s = LocalKeyStorage::create(config, generator, validator);
  EXPECT_OUTCOME_FALSE(e, s);
  ASSERT_EQ(e, KeyStorageError::UNSUPPORTED_KEY_TYPE);
}

TEST_F(LocalKeyStorageTest, InvalidKey) {
  config.ed25519_keypair_location =
      boost::filesystem::path(__FILE__).parent_path().string()
      + "/wrong_ed25519key.pem";
  auto s = LocalKeyStorage::create(config, generator, validator);
  EXPECT_OUTCOME_FALSE(e, s);
  ASSERT_EQ(e, KeyStorageError::PRIVATE_KEY_READ_ERROR);
}