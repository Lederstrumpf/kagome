/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_CORE_CONSENSUS_GRANDPA_IMPL_VOTING_ROUND_IMPL_HPP
#define KAGOME_CORE_CONSENSUS_GRANDPA_IMPL_VOTING_ROUND_IMPL_HPP

#include "consensus/grandpa/voting_round.hpp"

#include <boost/asio/basic_waitable_timer.hpp>
#include "blockchain/block_tree.hpp"
#include "common/logger.hpp"
#include "consensus/grandpa/gossiper.hpp"
#include "consensus/grandpa/vote_graph.hpp"
#include "consensus/grandpa/vote_tracker.hpp"

namespace kagome::consensus::grandpa {

  class VotingRoundImpl : public VotingRound {
    using Timer = boost::asio::basic_waitable_timer<std::chrono::system_clock>;

   public:
    ~VotingRoundImpl() override = default;

    VotingRoundImpl(std::shared_ptr<VoterSet> voters,
                    RoundNumber round_number,
                    Duration duration,
                    TimePoint start_time,
                    MembershipCounter counter,
                    Id id,
                    VoteGraph::CumulativeVote threshold,
                    std::unique_ptr<VoteTracker> tracker,
                    std::unique_ptr<VoteGraph> graph,
                    std::shared_ptr<Gossiper> gossiper,
                    std::shared_ptr<Clock> clock,
                    std::shared_ptr<blockchain::BlockTree> block_tree,
                    Timer timer,
                    common::Logger logger = common::createLogger("Grandpa"));

    static VoteGraph::CumulativeVote getThreshold(
        const std::shared_ptr<VoterSet> &voters);

    void onFin(const Fin &f) override;

    void tryFinalize() override;

    void onVoteMessage(const VoteMessage &vote_message) override;

    void playGrandpaRound() override;

    Id getPrimary() const override {
      auto index = round_number_ % voters_->size();
      return voters_->at(index);
    }

    BlockInfo bestFinalCandidate(RoundNumber round) override {
      return {};  // TODO(warchant): implement
    };

   private:
    /**
     * @tparam VoteType – either SignedPrevote or SignedPrecommit
     * @param vote – vote is pushed to tracker if it is not equivocated vote and
     * it was not pushed into the tracker before
     */
    template <typename VoteType>
    void onVote(const VoteType &vote);

    bool completable() const;

    void findPrevote(const Id &primary);

    void gossipPrevote(const SignedPrevote &prevote);

    void findAndGossipPrecommit();

    boost::optional<SignedPrevote> getPrevoteBy(const Id &authority) const;

    SignedPrevote signPrevote(const Prevote &prevote) const;

    SignedPrecommit signPrecommit(const Precommit &precommit) const;

    Fin prepareFin(RoundNumber number);

   private:
    std::shared_ptr<VoterSet> voters_;
    RoundNumber round_number_;
    Duration duration_;  // length of round (T in spec)
    TimePoint start_time_;
    MembershipCounter counter_;
    Id id_;  // id of current peer
    State state_;
    VoteGraph::CumulativeVote threshold_;

    std::unique_ptr<VoteTracker> tracker_;
    std::unique_ptr<VoteGraph> graph_;

    std::shared_ptr<Gossiper> gossiper_;
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<blockchain::BlockTree> block_tree_;

    Timer timer_;

    common::Logger logger_;
  };
}  // namespace kagome::consensus::grandpa

#endif  // KAGOME_CORE_CONSENSUS_GRANDPA_IMPL_VOTING_ROUND_IMPL_HPP