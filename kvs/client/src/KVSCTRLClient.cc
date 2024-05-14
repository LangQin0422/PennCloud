#include "KVSCTRLClient.hpp"

KVSCTRLClient::KVSCTRLClient() {}

KVSCTRLClient::KVSCTRLClient(std::vector<std::string> addrs)
{
    for (const std::string &addr : addrs)
    {
        std::string ip = addr.substr(0, addr.find(":"));

        std::cout << "Connecting to controller at " << ip << std::endl;

        std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
        std::shared_ptr<Controller::Stub> stub = std::move(Controller::NewStub(channel));
        stubs_[ip] = stub;
    }
}

int KVSCTRLClient::StartServer(int index, std::vector<std::string> addrs)
{
    std::string ip;
    if (!getNValidateIp(addrs[index], ip))
        return grpc::StatusCode::INVALID_ARGUMENT;

    grpc::ClientContext context;
    StartArgs args;
    args.set_index(index);
    for (const std::string &addr : addrs)
        args.add_ips(addr);

    StartReply reply;
    grpc::Status status = stubs_[ip]->StartServer(&context, args, &reply);

    if (!status.ok())
    {
        std::cout << "Error: " << status.error_message() << std::endl;
        return status.error_code();
    }
    return status.error_code();
}

int KVSCTRLClient::StopServer(std::string addr)
{
    std::string ip;
    if (!getNValidateIp(addr, ip))
        return grpc::StatusCode::INVALID_ARGUMENT;

    grpc::ClientContext context;
    StopArgs args;
    args.set_ip(addr);
    StopReply reply;
    grpc::Status status = stubs_[ip]->StopServer(&context, args, &reply);

    if (!status.ok())
    {
        std::cout << "Error: " << status.error_details() << std::endl;
        return status.error_code();
    }
    return status.error_code();
}

std::vector<std::string> KVSCTRLClient::GetAll()
{
    std::vector<std::string> addrs;
    for (auto &pair : stubs_)
    {
        std::shared_ptr<Controller::Stub> stub = pair.second;

        grpc::ClientContext context;
        ServersArgs args;
        ServersReply reply;
        grpc::Status status = stub->GetAll(&context, args, &reply);

        if (!status.ok())
            continue;

        for (const std::string &ip : reply.ips())
            addrs.push_back(ip);
    }

    return addrs;
}

int KVSCTRLClient::KillAll()
{
    for (auto &pair : stubs_)
    {
        std::shared_ptr<Controller::Stub> stub = pair.second;

        grpc::ClientContext context;
        ServersArgs args;
        StopReply reply;
        grpc::Status status = stub->KillAll(&context, args, &reply);
        if (!status.ok())
        {
            std::cout << "Error: " << status.error_message() << std::endl;
            return status.error_code();
        }
    }
    return grpc::StatusCode::OK;
}

bool KVSCTRLClient::getNValidateIp(const std::string &addr, std::string &ip) {
    ip = addr.substr(0, addr.find(":"));
    
    if (ip.empty() || stubs_.find(ip) == stubs_.end())
        return false;

    return true;
}