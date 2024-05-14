#ifndef KVS_CTRL_CLIENT_HPP
#define KVS_CTRL_CLIENT_HPP

#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/client_context.h>

#include "proto/controller.pb.h"
#include "proto/controller.grpc.pb.h"

/**
 * @brief A client for the controller.
 * The client can perform the following operations:
 * 1. client.StartServer(0, {"0.0.0.0:40051", ...}):
 *    Start the server only if it is not already running.
 * 2. client.StopServer(ip):
 *    Stop the server only if it is running.
 * 3. client.GetAll():
 *    Get the list of servers' ips.
 * 4. client.KillAll():
 *    Kill all servers.
 */
class KVSCTRLClient
{
public:
    KVSCTRLClient();

    /**
     * @brief Initialize the client for the controller on the given IP address.
     * @param ip the IP address of the controller
     */
    KVSCTRLClient(std::vector<std::string> addrs);

    /**
     * @brief Start the server only if it is not already running.
     *
     * @return grpc::StatusCode::INVALID_ARGUMENT if failed to parse arguments.
     * @return grpc::StatusCode::ALREADY_EXISTS if server is already running.
     * @return grpc::StatusCode::OK if server is started successfully.
     */
    int StartServer(int index, std::vector<std::string> addrs);

    /**
     * @brief Stop the server only if it is running.
     *
     * @param ip the IP address of the server to stop.
     * @return grpc::StatusCode::NOT_FOUND if server is not running.
     * @return grpc::StatusCode::OK if server is stopped successfully.
     */
    int StopServer(std::string addr);

    /**
     * @brief Get the list of servers.
     *
     * @return std::vector<std::string> the list of servers alive.
     */
    std::vector<std::string> GetAll();

    /**
     * @brief Kill all servers.
     *
     * @return grpc::StatusCode::OK if all servers are killed successfully.
     */
    int KillAll();

private:

    // The stubs for the controller
    std::unordered_map<std::string, std::shared_ptr<Controller::Stub>> stubs_;

    // Get the IP address from the address string and check if the controller is already connected
    bool getNValidateIp(const std::string &addr, std::string &ip);
};

#endif