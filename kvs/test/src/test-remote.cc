#include "KVSClient.hpp"
#include "KVSCTRLClient.hpp"

void perror(const std::string& msg) {
    std::cerr << "ERROR: " << msg << std::endl;
    exit(1);
}

void test() {
    std::vector<std::string> controllers = {"34.171.122.180:40050", "34.70.254.14:40050"};
    std::vector<std::vector<std::string>> clusters = {
        {"34.171.122.180:50051", "34.171.122.180:50052", "34.171.122.180:50053"},
        {"34.70.254.14:50051", "34.70.254.14:50052", "34.70.254.14:50053"}};

    KVSCTRLClient controller(controllers);
    KVSClient client(clusters);

    std::cout << "Controller starting servers..." << std::endl;

    for (std::vector<std::string> cluster : clusters) {
        assert(controller.StartServer(0, cluster) == grpc::StatusCode::OK);
        assert(controller.StartServer(1, cluster) == grpc::StatusCode::OK);
        assert(controller.StartServer(2, cluster) == grpc::StatusCode::OK);
    }

    std::cout << "Controller started all servers!" << std::endl;

    std::cout << "Client filling the kv store..." << std::endl;

    for (int r = 0; r < 100; r++) {
        std::string row = std::to_string(r);
        for (int c = 0; c < 100; c++) {
            std::string col = std::to_string(c);
            if (!client.Put(row, col, row + "-" + col))
                perror("Failed to put: <" + row + "-" + col + ">");
        }
    }

    std::cout << "Client filled the kv store!" << std::endl;

    std::cout << "Checking the rows and columns..." << std::endl;

    std::vector<std::string> rows;
    if (!client.GetAllRows(rows))
        perror("Failed to get all rows.");

    if (rows.size() != 100)
        perror("Expected 100 rows, got " + std::to_string(rows.size()));

    for (std::string row : rows) {
        std::vector<std::string> cols;
        if (!client.GetColsInRow(row, cols))
            perror("Failed to get row: " + row);

        if (cols.size() != 100)
            perror("Expected 100 columns, got " + std::to_string(cols.size()));

        for (std::string col : cols) {
            std::string value;
            if (!client.Get(row, col, value))
                perror("Failed to get: <" + row + "-" + col + ">");

            if (value != row + "-" + col)
                perror("Expected <" + row + "-" + col + ">, got <" + value + ">");
        }
    }

    std::cout << "All rows and columns are correct!" << std::endl;

    std::cout << "Deleting some rows and columns..." << std::endl;
    
    for (int i = 0; i < 10; i++) {
        std::string row = std::to_string(i);
        std::vector<std::string> tmp;
        if (!client.GetColsInRow(row, tmp))
            perror("Failed to get row: " + std::to_string(i));

        for (std::string col : tmp) {
            if (!client.Delete(row, col))
                perror("Failed to delete: <" + row + "-" + col + ">");
        }

        tmp.clear();
        if (!client.GetAllRows(tmp))
            perror("Failed to get row: " + std::to_string(i));

        if (tmp.size() != 100 - i - 1)
            perror("Expected " + std::to_string(100 - i - 1) + " rows, got " + std::to_string(tmp.size()));
    }

    std::cout << "Deleted some rows and columns!" << std::endl;

    std::cout << "Controller stopping servers..." << std::endl;

    if (!controller.KillAll())
        perror("Failed to kill all servers.");

    std::cout << "Controller stopped all servers!" << std::endl;
}

int main() {
    test();
    return 0;
}