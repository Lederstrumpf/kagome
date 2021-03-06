/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_CORE_PRIMITIVES_DIGEST_HPP
#define KAGOME_CORE_PRIMITIVES_DIGEST_HPP

#include <boost/variant.hpp>
#include "common/buffer.hpp"

namespace kagome::primitives {
  // from
  // https://github.com/paritytech/substrate/blob/39094c764a0bc12134d2a2ed8ab494a9ebfeba88/core/sr-primitives/src/generic/digest.rs#L77-L102

  /// Consensus engine unique ID.
  using ConsensusEngineId = common::Blob<4>;

  /// System digest item that contains the root of changes trie at given
  /// block. It is created for every block iff runtime supports changes
  /// trie creation.
  using ChangesTrieRoot = common::Hash256;

  namespace detail {
    struct DigestItemCommon {
      ConsensusEngineId consensus_engine_id;
      common::Buffer data;

      bool operator==(const DigestItemCommon &rhs) const {
        return consensus_engine_id == rhs.consensus_engine_id
               and data == rhs.data;
      }

      bool operator!=(const DigestItemCommon &rhs) const {
        return !operator==(rhs);
      }
    };
  }  // namespace detail

  /// A pre-runtime digest.
  ///
  /// These are messages from the consensus engine to the runtime, although
  /// the consensus engine can (and should) read them itself to avoid
  /// code and state duplication. It is erroneous for a runtime to produce
  /// these, but this is not (yet) checked.
  struct PreRuntime : public detail::DigestItemCommon {};

  /// A message from the runtime to the consensus engine. This should *never*
  /// be generated by the native code of any consensus engine, but this is not
  /// checked (yet).
  struct Consensus : public detail::DigestItemCommon {};

  /// Put a Seal on it. This is only used by native code, and is never seen
  /// by runtimes.
  struct Seal : public detail::DigestItemCommon {};

  template <class Stream,
            typename = std::enable_if_t<Stream::is_encoder_stream>>
  Stream &operator<<(Stream &s, const detail::DigestItemCommon &dic) {
    return s << dic.consensus_engine_id << dic.data;
  }

  template <class Stream,
            typename = std::enable_if_t<Stream::is_decoder_stream>>
  Stream &operator>>(Stream &s, detail::DigestItemCommon &dic) {
    return s >> dic.consensus_engine_id >> dic.data;
  }

  /// Some other thing. Unsupported and experimental.
  using Other = common::Buffer;
  /// Digest item that is able to encode/decode 'system' digest items and
  /// provide opaque access to other items.
  /// Note: order of types in variant matters. Should match type ids from here:
  /// https://github.com/paritytech/substrate/blob/39094c764a0bc12134d2a2ed8ab494a9ebfeba88/core/sr-primitives/src/generic/digest.rs#L155-L161
  using DigestItem = boost::variant<
      Other,            // = 0
      uint32_t,         // = 1 (fake type, should never be used in digest)
      ChangesTrieRoot,  // = 2
      std::string,      // = 3 (fake type, should never be used in digest)
      Consensus,        // = 4
      Seal,             // = 5
      PreRuntime>;      // = 6

  /**
   * Digest is an implementation- and usage-defined entity, for example,
   * information, needed to verify the block
   */
  using Digest = std::vector<DigestItem>;
}  // namespace kagome::primitives

#endif  // KAGOME_CORE_PRIMITIVES_DIGEST_HPP
