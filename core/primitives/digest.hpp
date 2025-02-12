/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/variant.hpp>

#include "common/buffer.hpp"
#include "common/tagged.hpp"
#include "common/unused.hpp"
#include "consensus/babe/types/babe_block_header.hpp"
#include "consensus/babe/types/babe_configuration.hpp"
#include "consensus/babe/types/epoch_data.hpp"
#include "consensus/babe/types/scheduled_change.hpp"
#include "consensus/constants.hpp"
#include "consensus/grandpa/types/scheduled_change.hpp"
#include "scale/scale.hpp"
#include "scale/tie.hpp"

namespace kagome::primitives {
  /// Consensus engine unique ID.
  using ConsensusEngineId = common::Blob<4>;

  inline const auto kBabeEngineId =
      ConsensusEngineId::fromString("BABE").value();

  inline const auto kGrandpaEngineId =
      ConsensusEngineId::fromString("FRNK").value();

  inline const auto kUnsupportedEngineId_POL1 =
      ConsensusEngineId::fromString("POL1").value();

  inline const auto kBeefyEngineId =
      ConsensusEngineId::fromString("BEEF").value();

  struct Other : public common::Buffer {};

  namespace detail {
    struct DigestItemCommon {
      SCALE_TIE(2);

      ConsensusEngineId consensus_engine_id;

      common::SLBuffer<consensus::kMaxValidatorsNumber * 1024> data;
    };
  }  // namespace detail

  /// A pre-runtime digest.
  ///
  /// These are messages from the consensus engine to the runtime, although
  /// the consensus engine can (and should) read them itself to avoid
  /// code and state duplication. It is erroneous for a runtime to produce
  /// these, but this is not (yet) checked.
  struct PreRuntime : public detail::DigestItemCommon {};

  /// https://github.com/paritytech/substrate/blob/polkadot-v0.9.8/primitives/consensus/babe/src/lib.rs#L130
  using BabeDigest =
      /// Note: order of types in variant matters
      std::variant<Unused<0>,
                   consensus::babe::EpochData,        // 1: (Auth[]; Rand)
                   consensus::babe::OnDisabled,       // 2: AuthIndex
                   consensus::babe::NextConfigData>;  // 3: c, S2nd

  /// https://github.com/paritytech/substrate/blob/polkadot-v0.9.8/primitives/finality-grandpa/src/lib.rs#L92
  using GrandpaDigest =
      /// Note: order of types in variant matters
      std::variant<Unused<0>,
                   consensus::grandpa::ScheduledChange,  // 1: (Auth[]; Delay)
                   consensus::grandpa::ForcedChange,     // 2: (Auth[]; Delay)
                   consensus::grandpa::OnDisabled,       // 3: AuthIndex
                   consensus::grandpa::Pause,            // 4: Delay
                   consensus::grandpa::Resume>;          // 5: Delay

  using UnsupportedDigest_POL1 = Tagged<Empty, struct POL1>;
  using UnsupportedDigest_BEEF = Tagged<Empty, struct BEEF>;

  struct DecodedConsensusMessage {
    static outcome::result<DecodedConsensusMessage> create(
        ConsensusEngineId engine_id, const common::Buffer &data) {
      DecodedConsensusMessage msg;
      msg.consensus_engine_id = engine_id;
      if (engine_id == primitives::kBabeEngineId) {
        OUTCOME_TRY(payload, scale::decode<BabeDigest>(data));
        msg.digest = std::move(payload);
      } else if (engine_id == primitives::kGrandpaEngineId) {
        OUTCOME_TRY(payload, scale::decode<GrandpaDigest>(data));
        msg.digest = std::move(payload);
      } else if (engine_id == primitives::kUnsupportedEngineId_POL1) {
        OUTCOME_TRY(payload, scale::decode<UnsupportedDigest_POL1>(data));
        msg.digest = payload;
      } else if (engine_id == primitives::kBeefyEngineId) {
        OUTCOME_TRY(payload, scale::decode<UnsupportedDigest_BEEF>(data));
        msg.digest = payload;
      } else {
        BOOST_ASSERT_MSG(false, "Invalid consensus engine id");
      }
      return msg;
    }

    const GrandpaDigest &asGrandpaDigest() const {
      BOOST_ASSERT(consensus_engine_id == primitives::kGrandpaEngineId);
      return std::get<GrandpaDigest>(digest);
    }

    ConsensusEngineId consensus_engine_id;
    std::variant<BabeDigest,
                 GrandpaDigest,
                 UnsupportedDigest_POL1,
                 UnsupportedDigest_BEEF>
        digest{};

   private:
    DecodedConsensusMessage() = default;
  };

  /// A message from the runtime to the consensus engine. This should *never*
  /// be generated by the native code of any consensus engine, but this is not
  /// checked (yet).
  struct Consensus : public detail::DigestItemCommon {
    Consensus() = default;

    Consensus(ConsensusEngineId id, const auto &v)
        : DigestItemCommon{id, common::Buffer{scale::encode(v).value()}} {}

    // Note: this ctor is needed only for tests
    template <class A>
    Consensus(const A &a) {
      // clang-format off
      if constexpr (std::is_same_v<A, consensus::babe::EpochData>
                 or std::is_same_v<A, consensus::babe::NextConfigData>) {
        consensus_engine_id = primitives::kBabeEngineId;
        data = common::Buffer(scale::encode(BabeDigest(a)).value());
      } else if constexpr (std::is_same_v<A, consensus::grandpa::ScheduledChange>
                        or std::is_same_v<A, consensus::grandpa::ForcedChange>
                        or std::is_same_v<A, consensus::grandpa::OnDisabled>
                        or std::is_same_v<A, consensus::grandpa::Pause>
                        or std::is_same_v<A, consensus::grandpa::Resume>) {
        consensus_engine_id = primitives::kGrandpaEngineId;
        data = common::Buffer(scale::encode(GrandpaDigest(a)).value());
      } else {
        BOOST_UNREACHABLE_RETURN();
      }
      // clang-format on
    }

    outcome::result<DecodedConsensusMessage> decode() const {
      return DecodedConsensusMessage::create(consensus_engine_id, data);
    }
  };

  /// Put a Seal on it.
  /// This is only used by native code, and is never seen by runtimes.
  struct Seal : public detail::DigestItemCommon {};

  /// Runtime code or heap pages updated.
  struct RuntimeEnvironmentUpdated : public Empty {};

  /// Digest item that is able to encode/decode 'system' digest items and
  /// provide opaque access to other items.
  /// Note: order of types in variant matters. Should match type ids from here:
  /// https://github.com/paritytech/substrate/blob/polkadot-v0.9.12/primitives/runtime/src/generic/digest.rs#L272
  using DigestItem = std::variant<Other,                       // 0
                                  Unused<1>,                   // 1
                                  Unused<2>,                   // 2
                                  Unused<3>,                   // 3
                                  Consensus,                   // 4
                                  Seal,                        // 5
                                  PreRuntime,                  // 6
                                  Unused<7>,                   // 7
                                  RuntimeEnvironmentUpdated>;  // 8

  namespace {
    // This value is enough to disable each of validators in each of two
    // consensus engines
    constexpr auto kMaxItemsInDigest = consensus::kMaxValidatorsNumber * 4;
  }  // namespace

  /**
   * Digest is an implementation- and usage-defined entity, for example,
   * information, needed to verify the block
   */
  using Digest = common::SLVector<DigestItem, kMaxItemsInDigest>;

}  // namespace kagome::primitives
