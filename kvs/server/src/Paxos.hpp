#ifndef PAXOS_HPP
#define PAXOS_HPP

#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/client_context.h>
#include "proto/paxos.pb.h"
#include "proto/paxos.grpc.pb.h"

#include <thread>

#define PEER_ID_BITS 8

/**
 * @brief A Paxos implementation. It is used to be included in an application.
 * @author Lang Qin
 * 
 * Manages a sequence of agreed values with a fixed set of peers. This implementation
 * copes with network failures (partitions, message loss, duplication) and peer failures.
 * 
 * APIs:
 * 1. PaxosImpl(std::vector<std::string> peersIP, int me): Constructor
 * 2. void Start(int seq, const std::string& v): 
 *      Start a new proposal with seq number and value
 * 3. bool Status(int seq, std::string& value): 
 *      Check if the seq number is decided and return the value if it is
 * 4. void Done(int seq): 
 *      Called by the application when the application on this machine is done with all instances <= seq
 * 5. int MaxKnownSeq(): 
 *      Get the highest seq number scene, or -1
 * 6. int MinKnownSeq(): 
 *      Get the minimum seq number where instances before this seq have been forgotten
*/

class PaxosImpl final : public Paxos::Service {
public:
    PaxosImpl(std::vector<std::string> peersIP, int me) : me_(me), highestSeqSeen_(-1), doneFreed_(0) {
        for (int i = 0; i < peersIP.size(); i++) {
            peerDone_[i] = -1;

            if (i != me) {
                std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(peersIP[i], grpc::InsecureChannelCredentials());
                std::shared_ptr<Paxos::Stub> stub = std::move(Paxos::NewStub(channel));
                peers_.push_back(stub);
            } else {
                peers_.push_back(nullptr);
            }
        }
    }

    /*       Paxos APIs       */

    /**
     * @brief Start a new proposal with seq number and value.
     * Start() returns immediately and propose in the background.
     * One may call Status() to check if/when the agreement is decided.
    */
    void Start(int seq, const Op& v) {
        // Ignore if the seq number < Min()
        if (seq < MinKnownSeq()) {
            std::cout << "Ignore seq " << seq << " < Min()" << std::endl;
            return;
        }
            

        std::lock_guard<std::mutex> lock(mu_);

        // Update highestSeqSeen_
        if (seq > highestSeqSeen_)
            highestSeqSeen_ = seq;

        // Check if seq is already decided
        auto it = instances_.find(seq);
        if (it != instances_.end() && it->second.Decided)
            return;

        // Non-blocking
        // Return immediate and propose in the background
        std::thread([this, seq, v]() {
            propose(seq, v);
        }).detach();
    }

    /**
     * @brief Check if the seq number is decided and return the value if it is.
     * Propose() requires mu_ lock
     * 
     * @param seq seq number of the proposal
     * @param value value of the proposal returned
     * @return true if the seq number is decided, false otherwise
    */
    bool Status(int seq, Op& value) {
        std::lock_guard<std::mutex> lock(mu_);

        if (instances_[seq].Decided) {
            value = instances_[seq].DecidedV;
            return true;
        } else {
            return false;
        }
    }

    /**
     * @brief Called by the application when the application on this machine is done with
     * all instances <= seq.
     * 
     * @param seq seq number of the proposal
    */
    void Done(int seq) {
        std::lock_guard<std::mutex> lock(mu_);
        
        if (seq > peerDone_[me_])
            peerDone_[me_] = seq;
    }

    /**
     * @brief Get the highest seq number scene.
     * MaxKnownSeq() requires mu_ lock
     * 
     * @return int highestSeqSeen_
    */
    int MaxKnownSeq() {
        std::lock_guard<std::mutex> lock(mu_);
        return highestSeqSeen_;
    }

    /**
     * @brief Get the minimum seq number which is marked as done by all peers.
     * MinKnownSeq() requires mu_ lock
     * 
     * Paxos is required to have forgotten all information about any instances
     * it knows that are < Min(). This is to free up memeory in long-running
     * Paxos-based servers.
     * 
     * Another useage of this is that, when a peer is dead or unreachable, other
     * peers' Min()s will not increase even if all reachable peers call Done.
     * Then, when the unreachable peer comes back to life, it will need to catch
     * up on instances that it missed.
     * 
     * @return int minSeq
    */
    int MinKnownSeq() {
        std::unique_lock<std::mutex> lock(mu_);
        int minSeq = getMinSeqNum();

        // Free memory according to Done
        lock.unlock();
        collectGarbage();

        return minSeq;
    }

    /*       RPC Calls       */

    /**
     * PRCCall Prepare
     * @brief Handles Paxos prepare request
     * Cases:
     * 1. Args.N > HighestSeen for seq number: accept
     * 2. Args.N <= HighestSeen for seq number: reject
    */
    grpc::Status Prepare(grpc::ServerContext* context, const PrepareArgs* args, PrepareReply* reply) override {
        std::unique_lock<std::mutex> lock(mu_);

        Instance acc;
        // Check if this peer has accepted a proposal with the same seq number
        auto it = acceptorIns_.find(args->seq());
        if (it == acceptorIns_.end()) {
            acc.HighestSeen = -1;
            acc.HighestAcN = -1;
        } else {
            acc = it->second;
        }

        if (args->n() > acc.HighestSeen) {
            // Can accept this proposal
            acc.HighestSeen = args->n();
            reply->set_ok(true);
            reply->set_na(acc.HighestAcN);
            reply->set_allocated_va(new Op(acc.HighestAcV));
            acceptorIns_[args->seq()] = acc;

            ABSL_LOG(INFO) << absl::StrFormat("RPCPrepare OK: me %d, N %d, na %d, va %s", me_, args->n(), reply->na(), reply->va().requestid());
        } else {
            // Reject this proposal
            reply->set_ok(false);

            ABSL_LOG(INFO) << absl::StrFormat("RPCPrepare Reject: me %d, N %d, HighestSeen %d", me_, args->n(), reply->na());
        }

        reply->set_done(peerDone_[me_]);

        // Retrieve and update the done value of the sender from args
        if (args->done() > peerDone_[args->sender()])
            peerDone_[args->sender()] = args->done();
                
        lock.unlock();

        // Free memory according to Done
        collectGarbage();

        return grpc::Status::OK;
    }

    /**
     * PRCCall Accept
     * @brief Handles Paxos accept request
     * Cases:
     * 1. Args.N >= HighestSeen for seq number in accept stage: accept
     * 2. Args.N < HighestSeen for seq number in accept stage: reject
    */
    grpc::Status Accept(grpc::ServerContext* context, const AcceptArgs* args, AcceptReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);

        Instance acc;
        // Check if this peer has accepted a proposal with the same seq number
        auto it = acceptorIns_.find(args->seq());
        if (it == acceptorIns_.end()) {
            acc.HighestSeen = -1;
            acc.HighestAcN = -1;
        } else {
            acc = it->second;
        }

        if (args->n() >= acc.HighestSeen) {
            // Accept this proposal
            acc.HighestSeen = args->n();
            acc.HighestAcN = args->n();
            acc.HighestAcV = args->v();
            acceptorIns_[args->seq()] = acc;
            reply->set_ok(true);
            reply->set_n(args->n());

            ABSL_LOG(INFO) << absl::StrFormat("RPCAccept OK: me %d, na %d, va %s", me_, reply->n(), args->v().requestid());
        } else {
            // Reject this proposal
            reply->set_ok(false);

            ABSL_LOG(INFO) << absl::StrFormat("RPCAccept Reject: me %d, N %d, HighestSeen %d", me_, args->n(), acc.HighestSeen);
        }

        return grpc::Status::OK;
    }

    /**
     * PRCCall Decide
     * @brief Handles Paxos decide request by marking the seq num as decided
    */
    grpc::Status Decide(grpc::ServerContext* context, const DecideArgs* args, DecideReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);

        Instance record;
        // Mark the seq num as decided
        auto it = instances_.find(args->seq());
        if (it == instances_.end()) {
            record.Decided = true;
            record.DecidedV = args->v();
            instances_[args->seq()] = record;
        } else {
            it->second.Decided = true;
            it->second.DecidedV = args->v();   
        }

        reply->set_ok(true);

        ABSL_LOG(INFO) << absl::StrFormat("RPCDecide OK: me %d, seq %d, v %s", me_, args->seq(), args->v().requestid());

        return grpc::Status::OK;
    }

private:

    /* Internal Data Structures and Variables */

    struct Instance {
        int HighestAcN;          // Na: highest accepted proposal
        Op HighestAcV;  // Va: value
        int HighestSeen;         // Np: highest seen proposal number
        bool Decided;
        Op DecidedV;

        Instance() : HighestAcN(0), HighestSeen(0), Decided(false) {}
    };

    struct SharedPrepareState {
        int highestNAccepted = -1, prepareOKCount = 0, allResopnse = 0;
        Op nextPhaseV;
        std::mutex mu;
        bool done = false;

        SharedPrepareState(const Op& v) : nextPhaseV(v) {}

        // Mark the prepare phase as done so that no more updates are allowed
        void donePrepare() {
            std::lock_guard<std::mutex> lock(this->mu);
            this->done = true;
        }

        // Update the prepare phase with a reply
        void update(const PrepareReply &reply, bool status = true) {
            std::lock_guard<std::mutex> lock(this->mu);
            if (this->done)
                return;

            this->allResopnse++;
            if (status && reply.ok()) {
                this->prepareOKCount++;
                if (reply.na() > this->highestNAccepted) {
                    this->highestNAccepted = reply.na();
                    this->nextPhaseV = reply.va();
                }
            }
        }
    };

    struct SharedAcceptState {
        int highestNObserved = -1, acceptOKCount = 0, allAcceptResponse = 0;
        std::mutex mu;
        bool done = false;

        // Mark the accept phase as done so that no more updates are allowed
        void doneAccept() {
            std::lock_guard<std::mutex> lock(this->mu);
            this->done = true;
        }

        // Update the accept phase with a reply
        void update(const AcceptReply &reply, bool status = true) {
            std::lock_guard<std::mutex> lock(this->mu);
            if (this->done)
                return;

            this->allAcceptResponse++;
            if (status && reply.ok()) {
                this->acceptOKCount++;
                if (reply.n() > this->highestNObserved) {
                    this->highestNObserved = reply.n();
                }
            }
        }
    };

    std::mutex mu_;
    std::vector<std::shared_ptr<Paxos::Stub>> peers_;
    int me_;  // index into peers_[]

    std::unordered_map<int, Instance> instances_;    // map from seq number to paxos instance
    std::unordered_map<int, Instance> acceptorIns_;  // intermediate phase for acceptor
    int highestSeqSeen_;                             // Highest seq number scene
    std::unordered_map<int, int> peerDone_;          // peers' done
    int doneFreed_;                                  // freed highest done

    /* Internal Functions */

    void propose(int seq, const Op& v) {
        bool isFirst = true;
        int penaltySleep = 10;

        while (true) {
            // Clear extra memory
            collectGarbage();

            // Sleep if not the first time call to let other peers propose
            if (!isFirst) {
                penaltySleep *= 1.5;
                penaltySleep = std::min(penaltySleep, 50);

                // Random backoff to avoid issues during concurrent puts
                int randomSleep = rand() % penaltySleep + penaltySleep;
                ABSL_LOG(INFO) << absl::StrFormat("Forced to sleep %dms (penalty: %d), seq %d, proposor %d", 
                    randomSleep, penaltySleep, seq, me_);
                std::this_thread::sleep_for(std::chrono::milliseconds(penaltySleep));
            }

            isFirst = false;

            // Lock while not decided
            std::unique_lock<std::mutex> lock(mu_);
            if (instances_[seq].Decided)
                return;

            /* Prepare Phase */
            auto it = acceptorIns_.find(seq);
            int highestSeen = it == acceptorIns_.end() ? -1 : it->second.HighestSeen;

            int myDone = peerDone_[me_];
            int peerCount = peers_.size(), majorityPeerCount = peers_.size() / 2 + 1;

            lock.unlock();

            // generate a unique proposal number based on the highest seen proposal number
            int n = generateUniqueN(highestSeen);

            ABSL_LOG(INFO) << absl::StrFormat("Phase 1 Prepare: seq %d, n %d, proposor %d", seq, n, me_);

            auto sharedPrepState = std::make_shared<SharedPrepareState>(v);
            PrepareArgs prepareArgs;
            PrepareReply prepareReply;
            prepareArgs.set_seq(seq);
            prepareArgs.set_n(n);
            prepareArgs.set_sender(me_);
            prepareArgs.set_done(myDone);

            // Call myself
            Prepare(nullptr, &prepareArgs, &prepareReply);

            sharedPrepState->update(prepareReply);

            // Call other peers
            for (int i = 0; i < peerCount; i++) {
                if (i == me_)
                    continue;

                // Non-blocking call
                std::thread([this, i, sharedPrepState, prepareArgs]() {
                    grpc::ClientContext context;
                    PrepareReply reply;
                    grpc::Status status = peers_[i]->Prepare(&context, prepareArgs, &reply);

                    sharedPrepState->update(reply, status.ok());

                    std::lock_guard<std::mutex> lock(mu_);
                    if (reply.done() > peerDone_[i])
                        peerDone_[i] = reply.done();
                }).detach();
            }

            // Wait for all responses
            while (sharedPrepState->prepareOKCount < majorityPeerCount && sharedPrepState->allResopnse < peerCount)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

            sharedPrepState->donePrepare();

            ABSL_LOG(INFO) << absl::StrFormat("Phase 1 Prepare Done with OKCount %d, seq %d, proposor %d, n %d, v %s", 
                sharedPrepState->prepareOKCount, seq, me_, n, sharedPrepState->nextPhaseV.requestid());

            if (sharedPrepState->prepareOKCount < majorityPeerCount)
                continue;
            
            lock.lock();
            // Make a copy of nextPhaseV so that non-returned threads won't affect v
            Op actualV = sharedPrepState->nextPhaseV;
            lock.unlock();

            /* Accept Phase */
            ABSL_LOG(INFO) << absl::StrFormat("Phase 2 Accept: seq %d, n %d, proposor %d, v %s", seq, n, me_, actualV.requestid());

            auto sharedAccState = std::make_shared<SharedAcceptState>();
            AcceptArgs accArgs;
            AcceptReply accReply;
            accArgs.set_seq(seq);
            accArgs.set_n(n);
            accArgs.set_allocated_v(new Op(actualV));

            // Call myself
            Accept(nullptr, &accArgs, &accReply);

            sharedAccState->update(accReply);

            // Call other peers
            for (int i = 0; i < peerCount; i++) {
                if (i == me_)
                    continue;

                // Non-blocking call
                std::thread([this, i, sharedAccState, &accArgs]() {
                    grpc::ClientContext context;
                    AcceptReply reply;
                    grpc::Status status = peers_[i]->Accept(&context, accArgs, &reply);

                    sharedAccState->update(reply, status.ok());
                }).detach();
            }

            // Wait for all responses
            while (sharedAccState->acceptOKCount < majorityPeerCount && sharedAccState->allAcceptResponse < peerCount)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

            sharedAccState->doneAccept();

            if (sharedAccState->acceptOKCount < majorityPeerCount)
                continue;

            ABSL_LOG(INFO) << absl::StrFormat("Phase 3 Decide: seq %d, proposor %d, n %d, v %s", seq, me_, n, actualV.requestid());

            /* Decide Phase */
            DecideArgs decideArgs;
            DecideReply decideReply;
            decideArgs.set_seq(seq);
            decideArgs.set_allocated_v(new Op(actualV));

            // Call myself
            Decide(nullptr, &decideArgs, &decideReply);

            for (int i = 0; i < peerCount; i++) {
                if (i == me_)
                    continue;

                // Non-blocking call
                std::thread([this, i, decideArgs]() {
                    // Retry until success
                    while (true) {
                        grpc::ClientContext context;
                        DecideReply reply;
                        grpc::Status status = peers_[i]->Decide(&context, decideArgs, &reply);

                        if (status.ok() && reply.ok())
                            break;

                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }).detach();
            }

            collectGarbage();
            return;
        }
    }

    // Free memory according to Done
    // We can free memory for all instances with seq number < min Done
    // collectGarbage() requires mu_ lock
    void collectGarbage() {
        std::lock_guard<std::mutex> lock(mu_);

        int currMin = getMinSeqNum();

        // Free memory for all instances with seq number < min Done
        if (currMin > doneFreed_) {
            
            ABSL_LOG(INFO) << absl::StrFormat("Running garbage collection for seq number < %d", currMin);

            // Clean up instances_
            for (auto it = instances_.begin(); it != instances_.end(); ) {
                if (it->first < currMin)
                    it = instances_.erase(it);
                else
                    it++;
            }

            // Clean up acceptorIns_
            for (auto it = acceptorIns_.begin(); it != acceptorIns_.end(); ) {
                if (it->first < currMin)
                    it = acceptorIns_.erase(it);
                else
                    it++;
            }

            // Update doneFreed_
            doneFreed_ = currMin;
        }
    }

    // Get the minimum seq number which is marked as done by all peers
    // Caller should hold mu_ lock
    int getMinSeqNum() {
        int res = INT_MAX;
        for (const auto& pair : peerDone_) {
            res = std::min(res, pair.second);
        }
        return res + 1;
    }

    // Generate a unique proposal number
    int generateUniqueN(int highestSeq) {
        int newID = (highestSeq >> PEER_ID_BITS) + 1;
        newID = (newID << PEER_ID_BITS) | me_;
        return newID;
    }

    // Parse a unique proposal number into sequence number and peer ID
    void parseUniqueN(int n, int& seq, int& peerID) {
        seq = n >> PEER_ID_BITS;
        peerID = n & ((1 << PEER_ID_BITS) - 1);
    }
};

#endif