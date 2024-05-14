#include "KVSController.hpp"

#define DEFAULT_PORT "40050"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <public ip>" << std::endl;
        return 1;
    }
    grpc::EnableDefaultHealthCheckService(true);
    grpc::ServerBuilder builder;

    KVSController kvsController(argv[1]);
    builder.AddListeningPort(absl::StrFormat("0.0.0.0:%s", DEFAULT_PORT), grpc::InsecureServerCredentials());
    builder.RegisterService(&kvsController);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    ABSL_LOG(INFO) << absl::StrFormat("Controller is listening on %s:%s", argv[1], DEFAULT_PORT);

    server->Wait();
    return 0;
}