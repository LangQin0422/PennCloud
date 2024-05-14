#include "KVSCTRLClient.hpp"

void testSimple()
{
    KVSCTRLClient client({"34.132.131.119:40050"});

    std::cout << "Testing simple start and stop..." << std::endl;
    std::vector<std::string> ips = {"34.132.131.119:50051", "34.132.131.119:50052"};
    assert(client.StartServer(0, ips) == grpc::StatusCode::OK);

    std::vector<std::string> servers = client.GetAll();
    assert(servers.size() == 1);
    assert(servers[0] == "34.132.131.119:50051");

    assert(client.StartServer(1, ips) == grpc::StatusCode::OK);
    servers = client.GetAll();
    assert(servers.size() == 2);

    assert(client.StopServer("34.132.131.119:50051") == grpc::StatusCode::OK);

    servers = client.GetAll();
    assert(servers.size() == 1);
    assert(servers[0] == "34.132.131.119:50052");

    assert(client.KillAll() == grpc::StatusCode::OK);
    servers = client.GetAll();
    assert(servers.size() == 0);

    std::cout << "Simple start and stop test passed!" << std::endl;
}

void test()
{
    testSimple();
}

int main()
{
    test();
    return 0;
}