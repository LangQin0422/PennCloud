#include <string>
#include <cassert> // For basic assertions
#include <iostream> // For std::cout

#include "Scheduler.hpp"

void testBasicInsertion() {
    std::cout << "Test Basic Insertion: Starting..." << std::endl;

    Scheduler<int, int, std::string> scheduler(1024, [](int, int, std::string){}); // 1024 bytes capacity
    scheduler.Put(1, 1, "value");
    std::string result;
    bool found = scheduler.Get(1, 1, result);

    assert(found && result == "value");
    std::cout << "Test Basic Insertion: Passed" << std::endl;
}

void testCapacityEnforcement() {
    std::cout << "Test Capacity Enforcement: Starting..." << std::endl;
    Scheduler<int, int, std::string> scheduler(100, [](int, int, std::string){});

    // sizeof(std::string) = 32 bytes
    // std::string(18, 'a') = 18 bytes (std::string is by default allocated 15 bytes of memory)

    // Fill the cache
    scheduler.Put(1, 1, std::string(18, 'a'));
    scheduler.Put(2, 1, std::string(18, 'b'));

    // Trigger eviction
    scheduler.Put(3, 3, std::string(18, 'c'));

    std::string result;
    bool found1 = scheduler.Get(1, 1, result);
    assert(!found1);
    bool found2 = scheduler.Get(2, 1, result);
    assert(found2);
    bool found3 = scheduler.Get(3, 3, result);
    assert(found3);

    std::cout << "Test Capacity Enforcement: Passed" << std::endl;
}

void testEvictionOrder() {
    std::cout << "Test Eviction Order: Starting..." << std::endl;

    Scheduler<int, int, std::string> scheduler(100, [](int, int, std::string){});
    scheduler.Put(1, 1, std::string(18, 'a'));
    scheduler.Put(2, 1, std::string(18, 'b'));

    std::string result;
    scheduler.Get(1, 1, result); // Access key 1 to make it recently used
    scheduler.Put(3, 1, std::string(18, 'c')); // Should trigger eviction of 2

    bool found1 = scheduler.Get(1, 1, result);
    bool found2 = scheduler.Get(2, 1, result);

    assert(found1 && !found2);
    std::cout << "Test Eviction Order: Passed" << std::endl;
}

void testOversizedItem() {
    std::cout << "Test Oversized Item: Starting..." << std::endl;

    Scheduler<int, int, std::string> scheduler(30, [](int, int, std::string){});

    try {
        scheduler.Put(1, 1, std::string(40, 'a'));
    } catch (std::runtime_error& e) {
        std::cout << "Test Oversized Item: Passed" << std::endl;
        return;
    }
    
    std::cout << "Test Oversized Item: Failed" << std::endl;
}

int main() {
    testBasicInsertion();
    testCapacityEnforcement();
    testEvictionOrder();
    testOversizedItem();

    return 0;
}
