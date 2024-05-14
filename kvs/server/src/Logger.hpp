#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>

#define GLOBAL_SEQ_LOG "global_seq.state"

namespace fs = std::filesystem;

class Logger {
public:
    Logger(const std::string& directory) {
        logDir_ = fs::path(directory);
    }

    /**
     * @brief Recover the state of the key-value store from the log files.
     * 
     * @return if there is any recoverable state, return the global sequence number.
    */
    bool Recoverable() {
        if (!fs::exists(logDir_))
            return false;

        counter_ = getMaxLogIndex() + 1;
        return counter_ > 0;
    }

    /**
     * @brief Recover the state of the key-value store from the log files.
     * 
     * @param int the global sequence number to be recovered
     * @return true if the state is recovered successfully, false otherwise.
    */
    bool RecoverGlobalSeq(int& globalSeq) {
        std::ifstream globalSeqFile(logDir_ / GLOBAL_SEQ_LOG);
        if (!globalSeqFile.is_open()) {
            return false;
        }

        globalSeqFile >> globalSeq;
        globalSeqFile.close();

        ABSL_LOG(INFO) << absl::StrFormat("Recovered globalseq %d from log directory %s", globalSeq, logDir_.string());

        return true;
    }

    /**
     * @brief Check if there is any operation left to recover.
     * 
     * @return true if there is any operation left to recover, false otherwise.
    */
    bool HasNextOp() {
        return currLogIndex_ < counter_;
    }

    /**
     * @brief Recover the next operation from the log file.
     * 
     * @param op the operation to be recovered
    */
    void RecoverOp(Op& op) {
        std::ifstream ifs(logDir_ / (std::to_string(currLogIndex_) + ".log"));
        if (!ifs.is_open()) {
            throw std::runtime_error("Can't open log file.");
        }

        op.ParseFromIstream(&ifs);
        ifs.close();
        currLogIndex_++;
    }

    /**
     * @brief Log an operation to the log file.
     * 
     * @param op the operation to be logged
     * @param globalSeq the global sequence number
     * @return true if the operation is logged successfully, false otherwise.
    */
    bool Log(Op& op, int globalSeq) {
        if (!fs::exists(logDir_)) {
            fs::create_directory(logDir_);
        }

        // Log the operation
        std::ofstream ofs(logDir_ / (std::to_string(counter_) + ".log"));
        if (!ofs.is_open()) {
            return false;
        }

        op.SerializeToOstream(&ofs);
        ofs.close();

        // Log the global sequence number
        std::ofstream globalSeqFile(logDir_ / GLOBAL_SEQ_LOG);
        if (!globalSeqFile.is_open()) {
            return false;
        }
        globalSeqFile << globalSeq;

        counter_++;
        return true;
    }

private:

    fs::path logDir_;  // The directory where the log files are stored.
    int counter_ = 0;  // The counter for the log files.
    int currLogIndex_ = 0;  // The current log index for recovery.

    // Search for the highest log index in the log directory,
    // where log files are named as "index.log" and index increases
    // by 1 for each log file.
    int getMaxLogIndex() {
        int maxIndex = -1;

        for (const auto& entry : fs::directory_iterator(logDir_)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".log")
                    maxIndex = std::max(maxIndex, std::stoi(filename.substr(0, filename.size() - 4)));
            }
        }

        return maxIndex;
    }

};

#endif