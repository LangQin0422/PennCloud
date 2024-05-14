#ifndef KVS_SERVER_H
#define KVS_SERVER_H

#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/client_context.h>
#include "proto/server.pb.h"
#include "proto/server.grpc.pb.h"

#include "absl/strings/str_format.h"
#include "absl/log/log.h"

#include "Paxos.hpp"
#include "Scheduler.hpp"
#include "Store.hpp"
#include "Logger.hpp"

#define PUT_ARGS_PUT 0
#define PUT_ARGS_CPUT 1
#define PUT_ARGS_DEL 2

#define CACHE_SIZE 500 * 1024 * 1024

class KVSServer final : public KVS::Service {
public:
    KVSServer(int me, std::shared_ptr<PaxosImpl> paxos, std::shared_ptr<Store> store, std::shared_ptr<Logger> logger) : me_(me), paxos_(paxos), store_(store),  logger_(logger), globalSeq_(-1) {
        if (!logger_->Recoverable()) {
            return;
        }

        logger_->RecoverGlobalSeq(globalSeq_);
        while (logger_->HasNextOp()) {
            Op op;
            logger_->RecoverOp(op);
            applyChange(op);
        }
    }

    /* RPC Functions */

    /**
     * @brief Put a key-value pair into the key-value store.
     * 
     * This RPC call is reponsible for PUT, CPUT, and DELETE operations.
    */
    grpc::Status PutValue(grpc::ServerContext* context, const PutArgs* args, PutReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);

        Op op;
        op.set_row(args->row());
        op.set_col(args->col());
        op.set_currvalue(args->currvalue());
        op.set_newvalue(args->newvalue());
        op.set_requestid(args->requestid());
        op.set_lockid(args->lockid());

        switch (args->option()) {
            case PUT_ARGS_CPUT:
                op.set_type(CPUT);
                break;
            case PUT_ARGS_DEL:
                op.set_type(DELETE);
                break;
            default:
                op.set_type(PUT);
                break;
        }

        ABSL_LOG(INFO) << absl::StrFormat("Server %d recieved Put %s on key: %s", me_, args->requestid(), args->row() + "-" + args->col());

        OpOutput output = makeAgreementAndApplyChange(op);
        reply->set_success(output.success);

        return grpc::Status::OK;
    }

    /**
     * @brief Get the value of a key-value pair from the key-value store.
    */
    grpc::Status GetValue(grpc::ServerContext* context, const GetArgs* args, GetReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);

        Op op;
        op.set_type(GET);
        op.set_row(args->row());
        op.set_col(args->col());
        op.set_requestid(args->requestid());
        op.set_lockid(args->lockid());

        ABSL_LOG(INFO) << absl::StrFormat("Server %d recieved Get %s on key: %s", me_, args->requestid(), args->row() + "-" + args->col());

        OpOutput output = makeAgreementAndApplyChange(op);

        reply->set_success(output.success);
        reply->set_value(output.value);

        return grpc::Status::OK;
    }

    /**
     * @brief Set a lock on a row if no such lock exists.
    */
    grpc::Status SetNX(grpc::ServerContext* context, const LockArgs* args, LockReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);

        Op op;
        op.set_type(SETNX);
        op.set_row(args->row());
        op.set_requestid(args->requestid());
        op.set_lockid(args->lockid());

        ABSL_LOG(INFO) << absl::StrFormat("Server %d recieved SetNX %s on key: %s", me_, args->requestid(), args->row());

        OpOutput output = makeAgreementAndApplyChange(op);

        reply->set_success(output.success);

        return grpc::Status::OK;
    }

    /**
     * @brief Release the lock on a row.
    */
    grpc::Status Del(grpc::ServerContext* context, const LockArgs* args, LockReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);

        Op op;
        op.set_type(DEL);
        op.set_row(args->row());
        op.set_requestid(args->requestid());
        op.set_lockid(args->lockid());

        ABSL_LOG(INFO) << absl::StrFormat("Server %d recieved Del %s on key: %s", me_, args->requestid(), args->row());

        OpOutput output = makeAgreementAndApplyChange(op);

        reply->set_success(output.success);

        return grpc::Status::OK;
    }

    /**
     * @brief Get all rows in the key-value store.
    */
    grpc::Status GetAllRows(grpc::ServerContext* context, const GetArgs* args, GetAllReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);

        Op op;
        op.set_type(GETALLROWS);
        op.set_requestid(args->requestid());

        ABSL_LOG(INFO) << absl::StrFormat("Server %d recieved GetAllRows %s", me_, args->requestid());

        OpOutput output = makeAgreementAndApplyChange(op);

        for (const std::string& row : output.values) {
            reply->add_item(row);
        }

        return grpc::Status::OK;
    }

    /**
     * @brief Get all a row from a specific server.
    */
    grpc::Status GetAllRowsByIp(grpc::ServerContext* context, const GetArgs* args, GetAllReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);

        std::vector<std::string> rows;
        store_->GetAllRows(rows);

        for (const std::string& row : rows) {
            reply->add_item(row);
        }

        return grpc::Status::OK;
    }

    /**
     * @brief Get all columns in a row.
    */
    grpc::Status GetColsInRow(grpc::ServerContext* context, const GetArgs* args, GetAllReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);

        Op op;
        op.set_type(GETCOLSINROW);
        op.set_row(args->row());
        op.set_requestid(args->requestid());
        op.set_lockid(args->lockid());

        ABSL_LOG(INFO) << absl::StrFormat("Server %d recieved GetColsInRow %s on key: %s", me_, args->requestid(), args->row());

        OpOutput output = makeAgreementAndApplyChange(op);

        for (const std::string& col : output.values) {
            reply->add_item(col);
        }

        return grpc::Status::OK;
    }

    /**
     * @brief Get all columns in a row from a specific server.
    */
    grpc::Status GetColsInRowByIp(grpc::ServerContext* context, const GetArgs* args, GetAllReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);

        std::vector<std::string> cols;
        store_->GetColsInRow(args->row(), cols, args->lockid());

        for (const std::string& col : cols) {
            reply->add_item(col);
        }

        return grpc::Status::OK;
    }

private:

    /* Internal Data Structures and Variables */

    struct OpOutput {
        bool success;
        std::string value;
        std::vector<std::string> values;
    };

    int me_;         // this server's index
    std::mutex mu_;  // lock for data_

    int globalSeq_;                                             // the highest sequence number that has been decided
    std::shared_ptr<Store> store_;                              // store instance
    std::unordered_map<std::string, OpOutput> visitedRequests_; // record of visited requests
    std::shared_ptr<PaxosImpl> paxos_;                          // paxos instance
    std::shared_ptr<Logger> logger_;                            // logger instance

    /* Internal Functions */

    // Make agreement and apply change to the key-value store
    // It will catch up all missed operations between globalSeq_ and seq
    // Caller must hold the lock
    OpOutput makeAgreementAndApplyChange(Op& op) {
        // increase seq num
        int seq = globalSeq_ + 1;

        // Propose until operation is successful
        while (true) {
            ABSL_LOG(INFO) << absl::StrFormat("Server %d is proposing seq %d", me_, seq);
            paxos_->Start(seq, op);
            Op agreedOp = waitForAgreement(seq);
            if (op.requestid() == agreedOp.requestid()) {
                break;
            }
            seq++;
        }

        // If [globalSeq_ + 1, seq) is not null, then we need to catch up
        // on missed operations
        for (int i = globalSeq_ + 1; i < seq; i++) {
            Op missedOp = waitForAgreement(i);
            logger_->Log(missedOp, globalSeq_);
            applyChange(missedOp);
        }

        logger_->Log(op, globalSeq_ + 1);
        OpOutput output = applyChange(op);

        globalSeq_ = seq;
        paxos_->Done(seq);

        return output;
    }

    // Wait for seq to be decided by paxos, and return the decided operation (de-serialized)
    Op waitForAgreement(int seq) {
        while (true) {
            Op decision;
            bool success = paxos_->Status(seq, decision);
        
            if (success)
                return decision;

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    // Apply the operation to the key-value store
    // Caller must hold the lock
    OpOutput applyChange(Op& op) {
        // GET operation
        if (op.type() == GET) {
            std::string value;
            if (store_->Get(op.row(), op.col(), value, op.lockid())) {
                return {true, value};
            } else {
                return {false, ""};
            }
        }
    
        // operations that modify the key-value store
        ABSL_LOG(INFO) << absl::StrFormat("Server %d is applying Op: %s", me_, op.requestid());

        OpOutput output;
        switch (op.type()) {
            case PUT:
                output.success = store_->Put(op.row(), op.col(), op.newvalue(), op.lockid());
                break;
            case CPUT:
                output.success = store_->CPut(op.row(), op.col(), op.currvalue(), op.newvalue(), op.lockid());
                break;
            case DELETE:
                output.success = store_->Delete(op.row(), op.col(), op.lockid());
                break;
            case SETNX:
                output.success = store_->SetNX(op.row(), op.lockid());
                break;
            case DEL:
                output.success = store_->Del(op.row());
                break;
            case GETALLROWS:
                output.success = store_->GetAllRows(output.values);
                break;
            case GETCOLSINROW:
                output.success = store_->GetColsInRow(op.row(), output.values, op.lockid());
                break;
            default:
                output.success = false;
                break;
        }

        output.value = "";
        visitedRequests_[op.requestid()] = output;
        return output;
    }
};

#endif