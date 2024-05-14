#ifndef KVS_CONTROLLER_HPP
#define KVS_CONTROLLER_HPP

#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/client_context.h>
#include "proto/controller.pb.h"
#include "proto/controller.grpc.pb.h"

#include "absl/strings/str_format.h"
#include "absl/log/log.h"

#include "KVSServer.hpp"

class KVSController final : public Controller::Service {
public:
    KVSController(const std::string& address) : address_(address) {}

    /**
     * @brief Start the server only if it is not already running.
     * 
     * @return grpc::Status::INVALID_ARGUMENT (3) if failed to parse arguments.
     * @return grpc::Status::ALREADY_EXISTS (6) if server is already running.
     * @return grpc::Status::OK if server is started successfully.
    */
    grpc::Status StartServer(grpc::ServerContext* context, const StartArgs* args, StartReply* reply) override {
        int me = args->index();
        std::vector<std::string> peersIP;
        for (const std::string& ip : args->ips())
            peersIP.push_back(ip);

        if (me < 0 || me >= peersIP.size())
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Index out of bounds.");

        const std::string& ipPort = peersIP[me];
        size_t colonPos = ipPort.find(':');
        if (colonPos == std::string::npos)
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid IP:Port format.");

        std::string ip = ipPort.substr(0, colonPos);
        std::string port = ipPort.substr(colonPos + 1);
        if (ip != address_)
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "IP address does not match.");

        std::lock_guard<std::mutex> lock(mu_);
        if (servers_.find(port) != servers_.end())
            return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "Server already running.");

        // IMPORTANT: One must use a separate thread to start the server
        // otherwise Segmentation Fault will occur.
        std::thread([this, me, port, peersIP]() {
            initializeServer(me, port, peersIP);
        }).detach();

        return grpc::Status::OK;
    }

    /**
     * @brief Stop the server only if it is running.
    */
    grpc::Status StopServer(grpc::ServerContext* context, const StopArgs* args, StopReply* reply) override {
        const std::string& ipPort = args->ip();
        size_t colonPos = ipPort.find(':');
        if (colonPos == std::string::npos)
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid IP:Port format.");

        std::string ip = ipPort.substr(0, colonPos);
        std::string port = ipPort.substr(colonPos + 1);

        if (ip != address_)
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "IP address does not match.");

        std::lock_guard<std::mutex> lock(mu_);
        if (servers_.find(port) == servers_.end())
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Server not found.");

        servers_[port]->Shutdown();
        servers_.erase(port);

        ABSL_LOG(INFO) << absl::StrFormat("Server %s is stopped.", ipPort);

        return grpc::Status::OK;
    }

    /**
     * @brief Get all servers on the machine.
    */
    grpc::Status GetAll(grpc::ServerContext* context, const ServersArgs* args, ServersReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& pair : servers_)
            reply->add_ips(address_ + ":" + pair.first);

        return grpc::Status::OK;
    }

    /**
     * @brief Stop all servers on the machine.
    */
    grpc::Status KillAll(grpc::ServerContext* context, const ServersArgs* args, StopReply* reply) override {
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& pair : servers_) {
            ABSL_LOG(INFO) << absl::StrFormat("Server %s is stopped.", address_ + ":" + pair.first);
            pair.second->Shutdown();
        }
        servers_.clear();
        return grpc::Status::OK;
    }
    
private:
    std::mutex mu_;  // mutex for servers_
    std::string address_;  // address of the controller (127.0.0.1:40050)
    std::unordered_map<std::string, std::unique_ptr<grpc::Server>> servers_;  // servers on the machine

    // Initialize the server and wait for requests
    void initializeServer(int me, std::string port, std::vector<std::string> peersIP) {
        std::string ip = address_.substr(0, address_.find(':'));
        std::string address = absl::StrFormat("0.0.0.0:%s", port);

        grpc::EnableDefaultHealthCheckService(true);
        grpc::ServerBuilder builder;

        // Create directory for logs
        if (!std::filesystem::exists("../../server_logs")) {
            std::filesystem::create_directory("../../server_logs");
        }
        
        // Initialize services
        auto storePtr = std::make_shared<Store>(address + "_sstables", CACHE_SIZE);
        auto loggerPtr = std::make_shared<Logger>("../../server_logs/" + address + "_logs");
        auto paxosServicePtr = std::make_shared<PaxosImpl>(peersIP, me);
        KVSServer kvsService(me, paxosServicePtr, storePtr, loggerPtr);

        // Register services
        builder.AddListeningPort(address, grpc::InsecureServerCredentials());
        builder.RegisterService(paxosServicePtr.get());
        builder.RegisterService(&kvsService);

        // Set message size limit
        builder.SetMaxReceiveMessageSize(1024 * 1024 * 1024);
        builder.SetMaxSendMessageSize(1024 * 1024 * 1024);

        servers_[port] = builder.BuildAndStart();
        ABSL_LOG(INFO) << absl::StrFormat("Server %d is listening on %s:%s", me, ip, port);

        servers_[port]->Wait();
    }
};

#endif