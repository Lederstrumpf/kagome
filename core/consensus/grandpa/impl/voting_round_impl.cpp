/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "consensus/grandpa/impl/voting_round_impl.hpp"

#include <boost/range/algorithm/find_if.hpp>
#include "common/visitor.hpp"
#include "scale/scale.hpp"

namespace kagome::consensus::grandpa {

  VotingRoundImpl::VotingRoundImpl(
      std::shared_ptr<VoterSet> voters,
      RoundNumber round_number,
      Duration duration,
      TimePoint start_time,
      MembershipCounter counter,
      RoundState last_round_state,
      crypto::ED25519Keypair keypair,
      size_t threshold,
      std::unique_ptr<VoteTracker> tracker,
      std::unique_ptr<VoteGraph> graph,
      std::shared_ptr<Gossiper> gossiper,
      std::shared_ptr<crypto::ED25519Provider> ed_provider,
      std::shared_ptr<Clock> clock,
      std::shared_ptr<blockchain::BlockTree> block_tree,
      Timer timer,
      common::Logger logger)
      : voters_{std::move(voters)},
        round_number_{round_number},
        duration_{duration},
        start_time_{start_time},
        counter_{counter},
        last_round_state_{std::move(last_round_state)},
        keypair_{keypair},
        id_{keypair_.public_key},
        state_{State::START},
        threshold_{threshold},
        tracker_{std::move(tracker)},
        graph_{std::move(graph)},
        gossiper_{std::move(gossiper)},
        ed_provider_{std::move(ed_provider)},
        clock_{std::move(clock)},
        block_tree_{std::move(block_tree)},
        timer_{std::move(timer)},
        logger_{std::move(logger)} {
    BOOST_ASSERT(voters_ != nullptr);
    BOOST_ASSERT(tracker_ != nullptr);
    BOOST_ASSERT(graph_ != nullptr);
    BOOST_ASSERT(gossiper_ != nullptr);
    BOOST_ASSERT(ed_provider_ != nullptr);
    BOOST_ASSERT(clock_ != nullptr);
    BOOST_ASSERT(block_tree_ != nullptr);

    threshold_ = getThreshold(voters_);
  }

  bool VotingRoundImpl::isPrimary() const {
    auto index = round_number_ % voters_->size();
    return voters_->at(index) == keypair_.public_key;
  }

  size_t VotingRoundImpl::getThreshold(
      const std::shared_ptr<VoterSet> &voters) {
    return (voters->size() / 2) * 3;
  }

  void VotingRoundImpl::onFin(const Fin &f) {
    // validate message
    switch (state_) {
      case State::PROPOSED:
      case State::START:
      case State::PREVOTED:
        break;
      case State::PRECOMMITTED: {
        tryFinalize();
      }
    }
  }

  void VotingRoundImpl::tryFinalize() {
    auto l = block_tree_->getLastFinalized();

    auto e = cur_round_state_.estimate;

    if (not e) {
      return;
    }

    if (e->block_number > l.block_number) {
      auto justification = tracker_->getJustification(*e);

      primitives::Justification kagome_just{
          .data = common::Buffer{scale::encode(justification).value()}};

      auto finalized = block_tree_->finalize(e->block_hash, kagome_just);
      if (not finalized) {
        logger_->warn("Could not finalize message during round {}. Error: {}",
                      round_number_,
                      finalized.error().message());
      }

      // TODO (kamilsa): do not broadcast if fin message already received
      gossiper_->fin(Fin{.vote = e.value(),
                         .round_number = round_number_,
                         .justification = justification});
    }
  }

  void VotingRoundImpl::onVoteMessage(const VoteMessage &vote_message) {
    // validate message
    switch (state_) {
      case State::PROPOSED:
      case State::START: {
        // as state is start it should be prevote message
        auto opt_prevote = getFromVariant<SignedPrevote>(vote_message.vote);
        if (not opt_prevote) {
          logger_->warn(
              "Received unexpected message after start. Expected: "
              "Prevote. Received: Precommit");
          return;
        }
        const auto &prevote = opt_prevote.value();

        onVote<SignedPrevote>(prevote);

        update();
        break;
      }

      case State::PREVOTED: {
        auto opt_precommit = getFromVariant<SignedPrecommit>(vote_message.vote);
        if (not opt_precommit) {
          logger_->warn(
              "Received unexpected message after start. Expected: "
              "Precommit. Received: Prevote");
          return;
        }
        const auto &precommit = opt_precommit.value();

        onVote<SignedPrecommit>(precommit);

        update();
        break;
      }

      case State::PRECOMMITTED: {
        break;
      }
    }
  }

  template <typename VoteType>
  void VotingRoundImpl::onVote(const VoteType &vote) {
    switch (tracker_->push(vote)) {
      case VoteTracker::PushResult::SUCCESS: {
        auto it = std::find(voters_->begin(), voters_->end(), vote.id);
        if (it != voters_->end()) {
          return;
        }

        // prepare VoteWeight which contains index of who has voted and what
        // kind of vote it was
        VoteWeight v;
        auto index = std::distance(voters_->begin(), it);
        if constexpr (std::is_same_v<VoteType, SignedPrevote>) {
          v.prevotes[index] = true;
        }
        if constexpr (std::is_same_v<VoteType, SignedPrecommit>) {
          v.precommits[index] = true;
        }

        if (auto inserted = graph_->insert(
                BlockInfo{.block_hash = vote.message.block_hash,
                          .block_number = vote.message.block_number},
                v);
            not inserted) {
          logger_->warn("Vote {} was not inserted with error: {}",
                        vote.message.block_hash.toHex(),
                        inserted.error().message());
        }
        break;
      }
      case VoteTracker::PushResult::DUPLICATE: {
        break;
      }
      case VoteTracker::PushResult::EQUIVOCATED: {
        // TODO (kamilsa): handle equivocated case
        break;
      }
    }

    if (completable() and clock_->now() < timer_.expires_at()) {
      timer_.cancel();
    }
  }

  template void VotingRoundImpl::onVote<SignedPrevote>(const SignedPrevote &);
  template void VotingRoundImpl::onVote<SignedPrecommit>(
      const SignedPrecommit &);

  bool VotingRoundImpl::completable() const {
    return completable_;
  }

  void VotingRoundImpl::playGrandpaRound(const RoundState &last_round_state) {
    primaryPropose(last_round_state);
    prevote(last_round_state);
    precommit(last_round_state);
  }

  void VotingRoundImpl::primaryPropose(const RoundState &last_round_state) {
    switch (state_) {
      case State::START: {
        auto maybe_estimate = last_round_state.estimate;

        if (maybe_estimate and isPrimary()) {
          auto maybe_finalized = last_round_state.finalized;

          // we should send primary if last round's state contains finalized
          // block and last round estimate is better than finalized block
          bool should_send_primary =
              maybe_finalized
                  .map([&maybe_estimate](const BlockInfo &finalized) {
                    return maybe_estimate->block_number
                           > finalized.block_number;
                  })
                  .value_or(false);

          if (should_send_primary) {
            logger_->debug("Sending primary block hint for round {}",
                           round_number_);
            primaty_vote_ = maybe_estimate;

            gossiper_->fin(Fin{.vote = primaty_vote_.value(),
                               .round_number = round_number_ - 1,
                               .justification = tracker_->getJustification(
                                   maybe_estimate.value())});
            state_ = State::PROPOSED;
          }
        } else {
          logger_->debug(
              "Last round estimate does not exist, not sending primary block "
              "hint during round {}",
              round_number_);
        }
        break;
      }
      case State::PROPOSED:
      case State::PREVOTED:
      case State::PRECOMMITTED:
        break;
    }
  }

  void VotingRoundImpl::prevote(const RoundState &last_round_state) {
    auto handle_prevote = [this, last_round_state](auto &&ec) {
      // Return if error is not caused by timer cancellation
      if (ec and ec != boost::asio::error::operation_aborted) {
        logger_->error("Error happened during precommit timer: {}",
                       ec.message());
        return;
      }
      switch (state_) {
        case State::START:
        case State::PROPOSED: {
          auto prevote = constructPrevote(last_round_state);
          if (prevote) {
            gossipPrevote(prevote.value());
          }
          state_ = State::PREVOTED;
          break;
        }
        case State::PREVOTED:
        case State::PRECOMMITTED: {
          break;
        }
      }
    };
    timer_.expires_at(start_time_ + duration_ * 2);
    timer_.async_wait(handle_prevote);
  };

  void VotingRoundImpl::precommit(const RoundState &last_round_state) {
    auto handle_precommit = [this, last_round_state](
                                const boost::system::error_code &ec) {
      // Return if error is not caused by timer cancellation
      if (ec and ec != boost::asio::error::operation_aborted) {
        logger_->error("Error happened during precommit timer: {}",
                       ec.message());
        return;
      }

      switch (state_) {
        case State::PREVOTED: {
          if (not last_round_state.estimate) {
            logger_->warn("Rounds only started when prior round completable");
            return;
          }
          auto last_round_estimate = last_round_state.estimate.value();

          // We should precommit if current state contains prevote and it is
          // either equal to the last round estimate of is descendant of it
          bool should_precommit =
              cur_round_state_.prevote_ghost
                  .map([&](const Prevote &p_g) {
                    return p_g.block_hash == last_round_estimate.block_hash
                           or chain_->isEqualOrDescendOf(
                               last_round_estimate.block_hash, p_g.block_hash);
                  })
                  .value_or(false);

          if (should_precommit) {
            logger_->debug("Casting precommit for round {}", round_number_);

            auto precommit = constructPrecommit(last_round_state);

            if (precommit) {
              gossipPrecommit(precommit.value());
            }
            state_ = State::PRECOMMITTED;
          }
          break;
        }
        case State::START:
        case State::PROPOSED:
        case State::PRECOMMITTED:
          break;
      }
    };
    timer_.expires_at(start_time_ + duration_ * 4);
    timer_.async_wait(handle_precommit);
  }

  outcome::result<SignedPrevote> VotingRoundImpl::constructPrevote(
      const RoundState &last_round_state) const {
    if (not last_round_state.estimate) {
      logger_->warn("Rounds only started when prior round completable");
      return outcome::failure(boost::system::error_code());
    }
    auto last_round_estimate = last_round_state.estimate.value();

    // find the block that we should take descendent from
    auto find_descendent_of =
        primaty_vote_
            .map([&](const BlockInfo &primary) {
              // if we have primary_vote then:
              // if last prevote is the same as primary vote, then return it
              // else if primary vote is better than last prevote return
              auto last_prevote_g = last_round_state.prevote_ghost.value();

              if (std::tie(primary.block_number, primary.block_hash)
                  == std::tie(last_prevote_g.block_number,
                              last_prevote_g.block_hash)) {
                return primary;
              }
              if (primary.block_number >= last_prevote_g.block_number) {
                return last_round_estimate;
              }

              // from this point onwards, the number of the primary-broadcasted
              // block is less than the last prevote-GHOST's number.
              // if the primary block is in the ancestry of p-G we vote for the
              // best chain containing it.
              if (auto ancestry =
                      chain_->ancestry(last_round_estimate.block_hash,
                                       last_prevote_g.block_hash);
                  ancestry) {
                auto to_sub = primary.block_number + 1;

                auto offset = 0;
                if (last_prevote_g.block_number >= to_sub) {
                  offset = last_prevote_g.block_number - to_sub;
                }

                if (ancestry.value().at(offset) == primary.block_hash) {
                  return primary;
                }
                return last_round_estimate;
              }
              return last_round_estimate;
            })
            .value_or_eval(
                [last_round_estimate] { return last_round_estimate; });

    auto best_chain =
        chain_->bestChainContaining(find_descendent_of.block_hash);

    if (not best_chain) {
      logger_->error(
          "Could not cast prevote: previousle known block {} has disappeared",
          find_descendent_of.block_hash.toHex());
      return outcome::failure(boost::system::error_code());
    }

    return signPrevote(Prevote{.block_hash = best_chain->block_hash,
                               .block_number = best_chain->block_number});
  }

  outcome::result<SignedPrecommit> VotingRoundImpl::constructPrecommit(
      const RoundState &last_round_state) const {
    const auto &base = graph_->getBase();
    const auto &target = cur_round_state_.prevote_ghost.value_or(Prevote{
        .block_number = base.block_number, .block_hash = base.block_hash});

    return signPrecommit(Precommit{.block_hash = target.block_hash,
                                   .block_number = target.block_number});
  }

  void VotingRoundImpl::update() {
    if (tracker_->prevoteWeight() < threshold_) {
      return;
    }

    auto total_weight = voters_->size();

    if (not cur_round_state_.prevote_ghost) {
      return;
    }

    auto prevote_ghost = cur_round_state_.prevote_ghost.value();

    // anything new finalized? finalized blocks are those which have both
    // 2/3+ prevote and precommit weight.
    auto current_precommits = tracker_->precommitWeight();

    if (current_precommits > threshold_) {
      cur_round_state_.finalized = graph_->findAncestor(
          BlockInfo{.block_hash = prevote_ghost.block_hash,
                    .block_number = prevote_ghost.block_number},
          [this](const VoteWeight &vote_weight) {
            return vote_weight.totalWeight().precommit > threshold_;
          });
    }

    if (tracker_->precommitWeight() >= threshold_) {
      cur_round_state_.estimate = graph_->findAncestor(
          BlockInfo{.block_hash = prevote_ghost.block_hash,
                    .block_number = prevote_ghost.block_number},
          [this](const VoteWeight &v) {
            return v.totalWeight().precommit >= threshold_;
          });
    } else {
      cur_round_state_.estimate =
          BlockInfo{prevote_ghost.block_number, prevote_ghost.block_hash};
      return;
    }

    completable_ =
        cur_round_state_.estimate
            .map([&prevote_ghost, this](const BlockInfo &block) -> bool {
              return block.block_hash == prevote_ghost.block_hash
                     and graph_->findGhost(block, [this](const VoteWeight &v) {
                           return v.totalWeight().precommit >= threshold_;
                         });
            })
            .value_or(false);
  }

  void VotingRoundImpl::gossipPrevote(const SignedPrevote &prevote) {
    VoteMessage message{.vote = prevote,
                        .round_number = round_number_,
                        .counter = counter_,
                        .id = id_};
    gossiper_->vote(message);
  }

  void VotingRoundImpl::gossipPrecommit(const SignedPrecommit &precommit) {
    VoteMessage message{.vote = precommit,
                        .round_number = round_number_,
                        .counter = counter_,
                        .id = id_};
    gossiper_->vote(message);
  }

  template <typename VoteType>
  crypto::ED25519Signature VotingRoundImpl::voteSignature(
      uint8_t stage, const VoteType &vote_type) const {
    auto payload =
        scale::encode(stage, vote_type, round_number_, counter_).value();
    return ed_provider_->sign(keypair_, payload).value();
  }

  template crypto::ED25519Signature VotingRoundImpl::voteSignature<Prevote>(
      uint8_t, const Prevote &) const;
  template crypto::ED25519Signature VotingRoundImpl::voteSignature<Precommit>(
      uint8_t, const Precommit &) const;

  SignedPrevote VotingRoundImpl::signPrevote(const Prevote &prevote) const {
    const static uint8_t prevote_stage = 0;

    return SignedPrevote{.message = prevote,
                         .id = id_,
                         .signature = voteSignature(prevote_stage, prevote)};
  }

  SignedPrecommit VotingRoundImpl::signPrecommit(
      const Precommit &precommit) const {
    const static uint8_t precommit_stage = 1;

    return SignedPrecommit{
        .message = precommit,
        .id = id_,
        .signature = voteSignature(precommit_stage, precommit)};
  }

}  // namespace kagome::consensus::grandpa
