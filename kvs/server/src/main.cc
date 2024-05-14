#include "KVSServer.hpp"

void RunServer(const int me, const std::vector<std::string> peersIP) {
    if (me < 0 || me >= peersIP.size()) {
        std::cerr << "Error: Index out of bounds." << std::endl;
        return;
    }

    const std::string& ipPort = peersIP[me];
    size_t colonPos = ipPort.find(':');
    if (colonPos == std::string::npos) {
        std::cerr << "Error: Invalid IP:Port format." << std::endl;
        return;
    }
    std::string address = absl::StrFormat("0.0.0.0:%s", ipPort.substr(colonPos + 1));

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

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    ABSL_LOG(INFO) << absl::StrFormat("Server %d is listening on %s", me, address);

    server->Wait();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " index ip1 [ip2 ...]" << std::endl;
        return 1;
    }

    // Parse index
    int index = std::stoi(argv[1]);

    // Parse IPs
    std::vector<std::string> ips;
    for (int i = 2; i < argc; ++i) {
        ips.push_back(std::string(argv[i]));
    }

    RunServer(index, ips);
    return 0;
}