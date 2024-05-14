#include "KVSClient.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

void testSimple(KVSClient client) {
    std::cout << "Testing simple put and get..." << std::endl;
    std::string value;

    client.Put("row1", "col1", "value1");
    assert(client.Get("row1", "col1", value));
    assert(value == "value1");

    client.Put("row1", "col1", "value2");
    assert(client.Get("row1", "col1", value));
    assert(value == "value2");

    client.Put("abc", "bcd", "5");
    assert(client.Get("abc", "bcd", value));
    assert(value == "5");

    client.CPut("abc", "bcd", "5", "6");
    assert(client.Get("abc", "bcd", value));
    assert(value == "6");

    std::vector<std::string> rows;
    client.GetAllRows(rows);
    assert(rows.size() == 2);

    std::vector<std::string> cols;
    client.Put("row1", "col2", "value2");
    client.Put("row1", "col3", "value2");
    client.GetColsInRow("row1", cols);
    assert(cols.size() == 3);

    client.Delete("abc", "bcd");
    assert(!client.Get("abc", "bcd", value));

    std::cout << "Simple put and get test passed!" << std::endl;
}

void testLock(KVSClient client1, KVSClient client2) {
    std::cout << "Testing lock and unlock..." << std::endl;
    std::string key, value;

    client1.Put("row1", "col1", "value1");
    assert(client1.Get("row1", "col1", value));
    assert(client2.Get("row1", "col1", value));
    assert(value == "value1");

    value = "";
    assert(client1.SetNX("row1", key));
    assert(client1.Get("row1", "col1", value, key));
    assert(value == "value1");
    assert(!client2.Get("row1", "col1", value));

    std::vector<std::string> cols;
    assert(client1.GetColsInRow("row1", cols, key));
    assert(cols.size() == 3);

    std::string tmp;
    assert(!client2.SetNX("row1", tmp));
    assert(!client1.SetNX("row1", tmp));

    assert(client1.Del("row1", key));
    assert(client2.Get("row1", "col1", value));
    assert(value == "value1");

    std::cout << "Lock and unlock test passed!" << std::endl;
}

void testGetAll(KVSClient client) {
    std::cout << "Testing Get All Rows..." << std::endl;

    std::vector<std::string> rows;
    assert(client.GetAllRows(rows));
    assert(rows.size() == 1);

    rows.clear();
    client.Put("newRow", "col1", "value1");
    assert(client.GetAllRows(rows));
    assert(rows.size() == 2);

    std::vector<std::string> cols;
    assert(client.GetColsInRow("newRow", cols));
    assert(cols.size() == 1);
    
    rows.clear();
    assert(client.Delete("newRow", "col1"));
    assert(client.GetAllRows(rows));
    assert(rows.size() == 1);

    std::cout << "Get All Rows test passed!" << std::endl;
}

void testBigFile(KVSClient client) {
    std::cout << "Testing big file..." << std::endl;

    std::string value;
    
    // read file to string
    std::ifstream file("/home/langqin0422/sp24-cis5050-T07/kvs/combinepdf.pdf", std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    client.Put("bigfile", "combinedpdf.pdf", content);
    assert(client.Get("bigfile", "combinedpdf.pdf", value));

    assert(content == value);

    std::cout << "Big file test passed!" << std::endl;
}

void test() {
    std::vector<std::vector<std::string>> clusters = {{"127.0.0.1:50051"}};
    KVSClient client1({"127.0.0.1:50051"}), client2(clusters);
    testSimple(client1);
    testLock(client1, client2);
    testGetAll(client1);
    testBigFile(client1);
}

int main() {
    test();
    return 0;
}