#ifndef KVS_CLIENT_HPP
#define KVS_CLIENT_HPP

#include <random>
#include <limits>
#include <thread>
#include <chrono>

#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/client_context.h>
#include <openssl/md5.h>

#include "base64.hpp"

#include "proto/server.pb.h"
#include "proto/server.grpc.pb.h"

/**
 * @brief A client for the key-value store.
 * The client can perform the following operations:
 * 1. client.Put("row1", "col1", "value1"):
 *    Put a key-value pair into the key-value store.
 * 2. client.CPut("row1", "col1", "value2", "value1"):
 *    Put a key-value pair into the key-value store, but only if the current value is oldValue.
 * 3. client.Get("row1", "col1"):
 *    Get the value of a key-value pair from the key-value store.
 * 4. client.Delete("row1", "col1"):
 *    Delete a key-value pair from the key-value store.
 */

class KVSClient
{
public:
    KVSClient();

    /**
     * @brief Construct a new KVSClient object with a list of server IPs.
     * @note Use this constructor if there is only one cluster of servers.
     * @note Deprecated. Use the constructor with a list of clusters instead.
    */
    KVSClient(std::vector<std::string> serversIP);

    /**
     * @brief Construct a new KVSClient object with a list of clusters.
     * @note Each cluster is a list of server IPs.
    */
    KVSClient(std::vector<std::vector<std::string>> clusters);

    /**
     * @brief Put a key-value pair into the key-value store.
     * @note See validation rules in validateArgs().
     *
     * @param row the row of the key-value pair
     * @param col the column of the key-value pair
     * @param value the value of the key-value pair
     * @return bool whether the operation is successful
     */
    bool Put(const std::string &row, const std::string &col, const std::string &value, const std::string &key = "-");

    /**
     * @brief Put a key-value pair into the key-value store, but only if the curren value is oldValue.
     * @note See validation rules in validateArgs().
     *
     * @param row the row of the key-value pair
     * @param col the column of the key-value pair
     * @param oldValue the old value of the key-value pair to be replaced
     * @param newValue the new value of the key-value pair
     * @return bool whether the operation is successful
     */
    bool CPut(const std::string &row, const std::string &col, const std::string &oldValue, const std::string &newValue, const std::string &key = "-");

    /**
     * @brief Get the value of a key-value pair from the key-value store.
     * @note See validation rules in validateArgs().
     *
     * @param row the row of the key-value pair
     * @param col the column of the key-value pair
     * @param value the value to store the result
     * @return bool whether the operation is successful
     */
    bool Get(const std::string &row, const std::string &col, std::string &value, const std::string &key = "-");

    /**
     * @brief Delete a key-value pair from the key-value store.
     * @note See validation rules in validateArgs().
     * @param row the row of the key-value pair
     * @param col the column of the key-value pair
     */
    bool Delete(const std::string &row, const std::string &col, const std::string &key = "-");

    /**
     * @brief Set a lock on a row if no such lock exists.
     * A unqiue key is generated to identify the lock. One must present
     * a unique key to identify and release the lock to prevent misuse,
     * expecially in concurrent environments.
     *
     * @note See validation rules in validateArgs().
     * One must present a unique key to identify and release the lock.
     *
     * @param row the row of the key-value pair
     * @param key to uniquely identify the lock
     * @return bool whether the lock is acquired for the given row
     */
    bool SetNX(const std::string &row, std::string &key);

    /**
     * @brief Release a lock on a row if it is aquired by this client,
     * if the correct unique key is presented.
     *
     * @note See validation rules in validateArgs().
     *
     * @param row the row of the key-value pair
     * @return bool whether the lock is released for the given row
     */
    bool Del(const std::string &row, const std::string &key);

    /**
     * @brief Get all rows in the key-value store.
     *
     * @param rows the vector to store the result
     * @param ip the IP of the server to get the rows from.
     *           If empty, then the client will get the rows from the storage system via Paxos.
     *           Otherwise, the client will get the rows from the server with the given IP.
     * @return bool whether the operation is successful
     */
    bool GetAllRows(std::vector<std::string> &rows, const std::string &ip = "");

    /**
     * @brief Get all columns in a row.
     * 
     * @note To bypass the locks, the caller could use "LOCK_BYPASS" as the lockId.
     *
     * @param row the row of the key-value pair
     * @param cols the vector to store the result
     * @param ip the IP of the server to get the rows from.
     *           If empty, then the client will get the rows from all servers.
     *           Otherwise, the client will get the rows from the server with the given IP.
     * @return bool whether the operation is successful
     */
    bool GetColsInRow(const std::string &row, std::vector<std::string> &cols, const std::string &key = "-", const std::string &ip = "");

private:

    uint64_t transactionID_;  // monotonically increasing transaction ID
    uint64_t clientID_;       // unique client ID

    std::unordered_map<std::string, std::shared_ptr<KVS::Stub>> ipToStub_;  // map from IP to stub
    std::vector<std::vector<std::shared_ptr<KVS::Stub>>> clusters_;         // list of clusters, each cluster is a list of servers

    std::unordered_map<std::string, std::string> locks_;  // locks on rows

    /**
     * @brief Get the value of a key-value pair from the key-value store.
     * Keep trying until the operation is successful.
     *
     * @param row the row of the key-value pair
     * @param col the column of the key-value pair
     * @param value the value to store the result
     * @return bool whether the operation is successful
     */
    bool DoGet(const std::string &row, const std::string &col, std::string &value, const std::string &key);

    /**
     * @brief Put a key-value pair into the key-value store.
     * Keep trying until the operation is successful.
     * @param row the row of the key-value pair
     * @param col the column of the key-value pair
     * @param value the value of the key-value pair
     * @return bool whether the operation is successful
     */
    bool DoPut(const std::string &row, const std::string &col, const std::string &newValue, const std::string &oldValue, const std::string &key, const int32_t option);

    /**
     * @brief Set a lock on a row if no such lock exists.
     * Keep trying until the operation is successful.
     *
     * @param key the key of the key-value pair
     * @return bool whether the operation is successful
     */
    bool DoSetNX(const std::string row, std::string &key);

    /**
     * @brief Release a lock on a row if it is aquired by this client.
     * Keep trying until the operation is successful.
     *
     * @param row the row which the lock is set
     * @return bool whether the operation is successful
     */
    bool DoDel(const std::string row);

    /**
     * @brief Get all rows in the storage system.
     * Keep trying until the operation is successful.
     * 
     * @param rows the vector to store the result
     * @return bool whether the operation is successful
    */
    bool DoGetAllRows(std::vector<std::string> &rows);

    /**
     * @brief Get all columns in a row from the storage system.
     * Keep trying until the operation is successful.
     * 
     * @param row the row of the key-value pair
     * @param cols the vector to store the result
     * @param key the lockId if necessary
     * @param row the row of the key-value pair
    */
    bool DoGetColsInRow(const std::string &row, std::vector<std::string> &cols, const std::string &key);

    /**
     * @brief Connect to the servers in the given cluster.
     * @param clusters the list of server ips in the cluster
     * @return std::shared_ptr<KVS::Stub> the stub to the server
     */
    void initCluster(std::vector<std::string> cluster, std::vector<std::shared_ptr<KVS::Stub>>& servers);

    /**
     * @brief Check if the row and col are valid.
     * 1. They cannot be empty.
     * 2. They cannot contain spaces.
     * 3. They cannot contain dashes.
     *
     * @param row the row of the key-value pair
     * @param col the column of the key-value pair
     * @throws std::invalid_argument if the row or col is invalid
     */
    void validateArgs(const std::string &row, const std::string &col = "1");

    /**
     * @brief Generate a unique ID for a transaction:
     * ClientID-TimeStamp-ClientMonotonicallyIncreasingTransactionID-
     * HighProbabilityUniqueRandomTransactionID
     * @return std::string
     */
    std::string generateID();

    /**
     * @brief Generate a big random number in the range [min, max].
     * If no arguments are provided, the range is [0, 2^64-1].
     * @param min the lower bound of the range
     * @param max the upper bound of the range
     * @return uint64_t
     */
    uint64_t nrand(uint64_t min = std::numeric_limits<uint64_t>::min(), uint64_t max = std::numeric_limits<uint64_t>::max());

    /**
     * @brief Get the index of the cluster that the row belongs to.
     * 
     * @param row the row of the key-value pair
     * @return size_t the index of the cluster
    */
    size_t getClusterIndex(const std::string &row);

    /**
     * @brief Generate a MD5 hash of a string.
     * @param str the string to be hashed
     * @return std::string the MD5 hash
     */
    std::string md5(const std::string &str);
};

#endif