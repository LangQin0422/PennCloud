#include "KVSClient.hpp"

KVSClient::KVSClient()
{
    transactionID_ = 1;
    clientID_ = nrand();
}

KVSClient::KVSClient(std::vector<std::string> serversIP)
{
    transactionID_ = 1;
    clientID_ = nrand();

    std::vector<std::shared_ptr<KVS::Stub>> servers;
    initCluster(serversIP, servers);
    clusters_.emplace_back(servers);
    assert(clusters_[0].size() == serversIP.size());
}

KVSClient::KVSClient(std::vector<std::vector<std::string>> clusters)
{
    transactionID_ = 1;
    clientID_ = nrand();

    for (std::vector<std::string> cluster : clusters)
    {
        std::vector<std::shared_ptr<KVS::Stub>> servers;
        initCluster(cluster, servers);
        clusters_.emplace_back(servers);
    }
}

void KVSClient::initCluster(std::vector<std::string> cluster, std::vector<std::shared_ptr<KVS::Stub>>& servers)
{
    for (std::string ip : cluster)
    {
        grpc::ChannelArguments channelArgs;
        channelArgs.SetMaxReceiveMessageSize(1024 * 1024 * 1024);

        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(ip, grpc::InsecureChannelCredentials(), channelArgs);
        std::shared_ptr<KVS::Stub> stub = std::move(KVS::NewStub(channel));
        servers.push_back(stub);
        ipToStub_[ip] = stub;
    }

    assert(servers.size() == cluster.size());
}

bool KVSClient::Put(const std::string &row, const std::string &col, const std::string &value, const std::string &key)
{
    validateArgs(row, col);
    return DoPut(row, col, value, "", key, 0);
}

bool KVSClient::CPut(const std::string &row, const std::string &col, const std::string &oldValue, const std::string &newValue, const std::string &key)
{
    validateArgs(row, col);
    return DoPut(row, col, newValue, oldValue, key, 1);
}

bool KVSClient::Get(const std::string &row, const std::string &col, std::string &value, const std::string &key)
{
    validateArgs(row, col);
    return DoGet(row, col, value, key);
}

bool KVSClient::Delete(const std::string &row, const std::string &col, const std::string &key)
{
    validateArgs(row, col);
    return DoPut(row, col, "", "", key, 2);
}

bool KVSClient::SetNX(const std::string &row, std::string &key)
{
    validateArgs(row);
    if (locks_.find(row) != locks_.end())
    {
        return false;
    }
    return DoSetNX(row, key);
}

bool KVSClient::Del(const std::string &row, const std::string &key)
{
    validateArgs(row);
    if (locks_.find(row) == locks_.end())
    {
        return false;
    }
    if (locks_[row] != key)
    {
        return false;
    }
    DoDel(row);
    return true;
}

bool KVSClient::GetAllRows(std::vector<std::string> &rows, const std::string &ip)
{
    // Get all rows from the system if ip is empty
    if (ip.empty())
        return DoGetAllRows(rows);

    // Get all rows from a specific server
    if (ipToStub_.find(ip) == ipToStub_.end())
        return false;

    GetArgs args;
    GetAllReply reply;
    grpc::ClientContext context;
    std::shared_ptr<KVS::Stub> &server = ipToStub_[ip];
    grpc::Status status = server->GetAllRowsByIp(&context, args, &reply);

    if (status.ok())
    {
        for (std::string row : reply.item())
            rows.push_back(row);
    }

    return true;
}

bool KVSClient::GetColsInRow(const std::string &row, std::vector<std::string> &cols, const std::string &key, const std::string &ip)
{
    // Get cols from the system if ip is not specified
    if (ip.empty())
        return DoGetColsInRow(row, cols, key);

    // Get cols from a specific server
    if (ipToStub_.find(ip) == ipToStub_.end())
        return false;

    GetArgs args;
    args.set_row(row);
    args.set_lockid(key);

    GetAllReply reply;
    grpc::ClientContext context;
    std::shared_ptr<KVS::Stub> &server = ipToStub_[ip];
    grpc::Status status = server->GetColsInRowByIp(&context, args, &reply);

    if (status.ok())
    {
        for (std::string col : reply.item())
            cols.push_back(col);
    }

    return true;
}

bool KVSClient::DoGet(const std::string &row, const std::string &col, std::string &value, const std::string &key)
{
    size_t rowIndex = getClusterIndex(row);

    GetArgs args;
    args.set_row(row);
    args.set_col(col);
    args.set_requestid(generateID());
    args.set_lockid(key);

    while (true)
    {
        for (std::shared_ptr<KVS::Stub> &server : clusters_[rowIndex])
        {
            GetReply reply;
            grpc::ClientContext context;
            grpc::Status status = server->GetValue(&context, args, &reply);
            if (status.ok())
            {
                if (reply.success())
                {
                    value = base64::from_base64(reply.value());
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool KVSClient::DoPut(const std::string &row, const std::string &col, const std::string &newValue, const std::string &oldValue, const std::string &key, const int32_t option)
{
    size_t rowIndex = getClusterIndex(row);

    PutArgs args;
    args.set_row(row);
    args.set_col(col);
    args.set_newvalue(base64::to_base64(newValue));
    args.set_currvalue(base64::to_base64(oldValue));
    args.set_option(option);
    args.set_requestid(generateID());
    args.set_lockid(key);

    while (true)
    {
        for (std::shared_ptr<KVS::Stub> &server : clusters_[rowIndex])
        {
            PutReply reply;
            grpc::ClientContext context;
            grpc::Status status = server->PutValue(&context, args, &reply);
            if (status.ok())
                return reply.success();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool KVSClient::DoSetNX(const std::string row, std::string &key)
{
    key = std::to_string(nrand());
    size_t rowIndex = getClusterIndex(row);

    LockArgs args;
    args.set_row(row);
    args.set_lockid(key);
    args.set_requestid(generateID());

    while (true)
    {
        for (std::shared_ptr<KVS::Stub> &server : clusters_[rowIndex])
        {
            LockReply reply;
            grpc::ClientContext context;
            grpc::Status status = server->SetNX(&context, args, &reply);
            if (!status.ok())
                continue;

            if (reply.success())
            {
                locks_.insert({row, key});
                return true;
            }
            else
            {
                return false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool KVSClient::DoDel(const std::string row)
{
    size_t rowIndex = getClusterIndex(row);

    LockArgs args;
    args.set_row(row);
    args.set_requestid(generateID());

    while (true)
    {
        for (std::shared_ptr<KVS::Stub> &server : clusters_[rowIndex])
        {
            LockReply reply;
            grpc::ClientContext context;
            grpc::Status status = server->Del(&context, args, &reply);
            if (status.ok())
            {
                locks_.erase(row);
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool KVSClient::DoGetAllRows(std::vector<std::string> &rows)
{
    for (std::vector<std::shared_ptr<KVS::Stub>> &cluster : clusters_)
    {
        while (true)
        {
            for (std::shared_ptr<KVS::Stub> &server : cluster)
            {
                GetArgs args;
                GetAllReply reply;
                grpc::ClientContext context;
                grpc::Status status = server->GetAllRows(&context, args, &reply);
                if (status.ok())
                {
                    for (std::string row : reply.item())
                        rows.push_back(row);

                    goto CLUSTER_COMPLETE;
                }
            }
CLUSTER_COMPLETE:
            break;
        }
    }
    return true;
}

bool KVSClient::DoGetColsInRow(const std::string &row, std::vector<std::string> &cols, const std::string &key)
{
    size_t rowIndex = getClusterIndex(row);

    GetArgs args;
    args.set_row(row);
    args.set_lockid(key);

    while (true)
    {
        for (std::shared_ptr<KVS::Stub> &server : clusters_[rowIndex])
        {
            GetAllReply reply;
            grpc::ClientContext context;
            grpc::Status status = server->GetColsInRow(&context, args, &reply);
            if (status.ok())
            {
                for (std::string col : reply.item())
                    cols.push_back(col);

                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void KVSClient::validateArgs(const std::string &row, const std::string &col)
{
    if (row.empty() || col.empty())
        throw std::invalid_argument("row and col cannot be empty");

    if (row.find(" ") != std::string::npos || col.find(" ") != std::string::npos)
        throw std::invalid_argument("row and col cannot contain spaces");
}

std::string KVSClient::generateID()
{
    std::stringstream ss;
    ss << clientID_ << '-'
       << std::chrono::system_clock::now().time_since_epoch().count() << '-'
       << transactionID_ << '-'
       << nrand();
    return ss.str();
}

uint64_t KVSClient::nrand(uint64_t min, uint64_t max)
{
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist(min, max);
    return dist(rng);
}

size_t KVSClient::getClusterIndex(const std::string &row)
{
    if (clusters_.size() == 1)
        return 0;

    std::string hash = md5(row);
    unsigned long long part1 = std::stoull(hash.substr(0, 16), nullptr, 16);
    unsigned long long part2 = std::stoull(hash.substr(16, 16), nullptr, 16);
    unsigned long long combined = part1 ^ part2; 
    size_t index = combined % clusters_.size();

    return index;
}

std::string KVSClient::md5(const std::string &input)
{
    unsigned char result[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)input.c_str(), input.size(), result);

    std::ostringstream sout;
    sout << std::hex << std::setfill('0');
    for (long long c: result)
        sout << std::setw(2) << (int)c;

    return sout.str();
}