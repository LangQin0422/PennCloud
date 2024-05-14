#ifndef STORE_HPP
#define STORE_HPP

#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <chrono>

#define BY_PASS_LOCK_ID "LOCK_BYPASS"
#define LOCK_MAX_DURATION 10

/**
 * @brief A key-value store that supports PUT, GET, DELETE, and CPUT operations.
 * @author Lang Qin
 * 
 * The key-value store is backed by a LRU cache and SSTable files on disk. Operations
 * are first performed on the cache. If the key-value pair is not found in the cache,
 * the key-value pair is read from the disk. If the key-value pair is found in the disk,
 * it is stored in the cache.
 * 
 * The key-value pair is stored in the disk under the folder [sstableDirectory_]. The key
 * is of the form "row-col". Each key-value pair is stored in a new file of the form
 * "[sstableDirectory_]/row/col.dat". The value is stored in the file.
 * 
 * APIs:
 * 1. bool Put(std::string& key, std::string& value):
 *     Put a key-value pair into the key-value store.
 * 2. bool Get(std::string& key, std::string& value):
 *     Get the value of a key-value pair from the key-value store.
 * 3. bool Delete(std::string& key):
 *     Delete a key-value pair from the key-value store.
 * 4. bool CPut(std::string& key, std::string& currValue, std::string& newValue):
 *     Conditional put a key-value pair into the key-value store.
 * 5. void Clear():
 *     Clear the key-value store by removing all the SSTable files under the folder [sstableDirectory_].
 * 6. bool GetAllRows(std::vector<Key>& rows):
 *     Get all rows in the kvs.
 * 7. bool GetColsInRow(const Key& key, std::vector<Key>& cols):
 *     Get all cols from a row.
*/

class Store {
public:
    Store(const std::string& dir, size_t cacheSize) : 
        sstableDirectory_(dir),
        scheduler_(Scheduler<std::string, std::string, std::string>(cacheSize, [this](std::string row, std::string col, const std::string& value) {
            this->flushToDisk(row, col, value);
        })) {}

    /**
     * @brief Put a key-value pair into the key-value store.
     * By default, the key-value pair is stored in the LRU cache. Any key-value pair
     * that is evicted from the cache is stored in the disk under the folder [sstableDirectory_].
     * 
     * @param row the row
     * @param col the col
     * @param value the value
     * @param opId the operation id
     */
    bool Put(const std::string& row, const std::string& col, const std::string& value, const std::string& lockId) {
        if (isResourceLocked(row, lockId))
            return false;

        try {
            scheduler_.Put(row, col, value);
        } catch (std::runtime_error& e) {
            // If the value size exceeds the cache capacity, store it in the disk
            flushToDisk(row, col, value);
        }
        return true;
    }

    /**
     * @brief Get the value of a key-value pair from the key-value store.
     * If the key-value pair is in the LRU cache, return the value directly.
     * Otherwise, read the key-value pair from the disk under the folder [sstableDirectory_].
     * If the key-value pair is found, store it in the LRU cache.
     * 
     * @param row the row
     * @param col the col
     * @param value the value
     * @param opId the operation id
     * @return true if the key-value pair is found, false otherwise
     */
    bool Get(const std::string& row, const std::string& col, std::string& value, const std::string& lockId) {
        if (isResourceLocked(row, lockId))
            return false;

        if (scheduler_.Get(row, col, value)) {
            return true;
        }

        // Read from disk
        if (readFromDisk(row, col, value)) {
            scheduler_.Put(row, col, value);
            return true;
        }
        return false;
    }

    /**
     * @brief Delete a key-value pair from the key-value store.
     * If the key-value pair is in the LRU cache, delete it directly.
     * Otherwise, delete the key-value pair from the disk under the folder [sstableDirectory_].
     * 
     * @param key the key
     * @param opId the operation id
     * @return true if the key-value pair is deleted, false otherwise
     */
    bool Delete(const std::string& row, const std::string& col, const std::string& lockId) {
        if (isResourceLocked(row, lockId))
            return false;

        if (scheduler_.Delete(row, col))
            return true;

        // Delete from disk
        std::string path = sstableDirectory_ + "/" + row + "/" + col + ".dat";
        std::filesystem::remove(path);

        // Delete the row directory if it is empty
        std::string dir = sstableDirectory_ + "/" + row;
        if (std::filesystem::exists(dir) && std::filesystem::is_empty(dir))
            std::filesystem::remove(dir);

        return true;
    }

    /**
     * @brief Conditional put a key-value pair into the key-value store.
     * If the key-value pair is in the LRU cache and the current value is the same as the expected value,
     * update the value with the new value. Otherwise, do nothing.
     * 
     * @param row the row
     * @param col the col
     * @param currValue the current value
     * @param newValue the new value
     * @return true if the key-value pair is updated, false otherwise
     */
    bool CPut(const std::string& row, const std::string& col, const std::string& currValue, const std::string& newValue, const std::string& lockId) {
        if (isResourceLocked(row, lockId))
            return false;

        std::string value;
        if (Get(row, col, value, lockId) && value == currValue) {
            Put(row, col, newValue, lockId);
            return true;
        }
        return false;
    }

    /**
     * @brief Set a lock on a row if no such lock exists.
     * 
     * @param row the row
     * @param opId the operation id
     * @return true if the lock is acquired, false otherwise
     */
    bool SetNX(const std::string& row, const std::string& lockId) {
        if (isResourceLocked(row, lockId))
            return false;

        locks_[row] = LockInfo(lockId);
        return true;
    }

    /**
     * @brief Release the lock on a row.
     * 
     * @param row the row
     */
    bool Del(const std::string& row) {
        locks_.erase(row);
        return true;
    }

    /**
     * @brief Get all rows in the key-value store.
     * 
     * @param rows the vector to store the rows
    */
    bool GetAllRows(std::vector<std::string>& rows) {
        scheduler_.GetAllRows(rows);
        readAllRows(rows);
        return true;
    }

    /**
     * @brief Get all cols in a row.
     * 
     * @param row the row
     * @param cols the vector to store the cols
     * @param lockId the lock id
     * @return true if the row exists, false otherwise
     */
    bool GetColsInRow(const std::string& row, std::vector<std::string>& cols, const std::string& lockId) {
        if (isResourceLocked(row, lockId))
            return false;
    
        std::string dir = sstableDirectory_ + "/" + row;
        bool exists = std::filesystem::exists(dir);
        if (!scheduler_.GetColsInRow(row, cols) && !exists) {
            return false;
        }

        if (!exists)
            return true;

        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            cols.push_back(entry.path().filename());
        }
        return true;
    }

    /**
     * @brief Clear the key-value store.
     * Remove all the SSTable files under the folder [sstableDirectory_].
     */
    void Clear() {
        std::filesystem::remove_all(sstableDirectory_);
    }

private:
    struct LockInfo {
        std::string lockId;                           // id of the lock
        std::chrono::steady_clock::time_point start;  // timestamp of the lock when it is acquired

        LockInfo() : lockId(""), start() {}
        LockInfo(const std::string& id) : lockId(id), start(std::chrono::steady_clock::now()) {}

        bool expired() {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() > LOCK_MAX_DURATION;
        }
    };

    Scheduler<std::string, std::string, std::string> scheduler_;  // LRU cache
    std::string sstableDirectory_;                   // Folder to store SSTable files

    std::unordered_map<std::string, LockInfo> locks_;  // Lock and the client that owns it

    // Read the key-value pair from disk under the folder [sstableDirectory_].
    // Key is of the form "row-col". The value is expected to be stored in a
    // file of the form "[sstableDirectory_]/row/col.dat".
    bool readFromDisk(const std::string& row, const std::string& col, std::string& value) {
        std::string file = sstableDirectory_ + "/" + row + "/" + col + ".dat";
        std::ifstream ifs(file);
        std::string line;

        // Read the key
        std::getline(ifs, line);
        if (line != row + "-" + col)
            return false;

        // Read the value
        std::stringstream ss;
        ss << ifs.rdbuf();
        value = ss.str();
        return true;
    }
    
    // Flush the key-value pair to disk under the folder [sstableDirectory_].
    // Key is of the form "row-col". Each key-value pair is store in a new
    // file of the form "[sstableDirectory_]/row/col.dat". The value is stored
    // in the file.
    void flushToDisk(std::string row, std::string col, const std::string& value) {
        std::string dir = sstableDirectory_ + "/" + row;
        std::string file = dir + "/" + col + ".dat";

        // Create the directory if it does not exist
        if (!std::filesystem::exists(dir))
            std::filesystem::create_directories(dir);

        std::ofstream ofs(file);
        if (!ofs.is_open())
            throw std::runtime_error("Failed to open file: " + file);

        ofs << row << "-" << col << std::endl;
        ofs << value;
    }

    // Read all the rows in the SSTable files under the folder [sstableDirectory_].
    void readAllRows(std::vector<std::string>& rows) {
        if (!std::filesystem::exists(sstableDirectory_))
            return;

        for (const auto& entry : std::filesystem::directory_iterator(sstableDirectory_)) {
            rows.push_back(entry.path().filename());
        }
    }

    // Check if is the resource can be accessed by the lockId
    bool isResourceLocked(const std::string& row, const std::string& lockId) {
        if (lockId == BY_PASS_LOCK_ID)
            return false;

        if (locks_.find(row) != locks_.end() && !locks_[row].expired() && locks_[row].lockId != lockId)
            return true;

        return false;
    }
};

#endif
