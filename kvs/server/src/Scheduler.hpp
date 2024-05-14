#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <list>
#include <unordered_map>
#include <map>
#include <functional>

/**
 * @brief A cache scheduler based on LRU (Least Recently Used) policy with a fixed capacity in bytes.
 * @author Lang Qin
 * 
 * Manages a cache with a fixed capacity in bytes. When the cache is full, the least recently used item will be evicted.
 * 
 * @tparam Key the type of the key
 * @tparam Value the type of the value. The value type must have a capacity() method (like std::string).
 * 
 * APIs:
 * 1. bool Put(const Key& key, const Value& value):
 *     Put a key-value pair into the cache.
 * 2. bool get(const Key& key, Value& value):
 *     Get the value of a key-value pair from the cache.
 * 3. bool Delete(const Key& key):
 *     Delete a key-value pair from the cache.
 * 4. bool GetAllRows(std::vector<Key>& rows):
 *     Get all rows in the cache.
 * 5. bool GetColsInRow(const Key& key, std::vector<Key>& cols):
 *     Get all cols in a row.
*/

template<typename Row, typename Col, typename Value>
class Scheduler {
public:

    /**
     * @brief Construct a new Scheduler object with a fixed capacity.
     * 
     * @param capacity the capacity of the cache in bytes
     * @param evictCallback the callback function when an item is evicted
    */
    Scheduler(size_t capacity, std::function<void(Row, Col, Value)> evictCallback = nullptr) : capacity_(capacity), onEvict_(evictCallback) {}

    /**
     * @brief Put a key-value pair into the cache.
     * If the key already exists, update the value and move it to the end of the list.
     * If the key does not exist, insert the key-value pair to the end of the list.
     * If the cache is full, evict the least recently used item.
     * 
     * @param row the row
     * @param col the col
     * @param value the value
     * @throw std::runtime_error if the value size exceeds the cache capacity
    */
    void Put(const Row& row, const Col& col, const Value& value) {
        size_t valueSize = sizeof(Value) + value.capacity();

        // Check if the single item exceeds cache capacity
        if (valueSize > capacity_) {
            throw std::runtime_error("The value size exceeds the cache capacity");
            return;
        }

        auto itRow = cacheMap_.find(row);
        if (itRow != cacheMap_.end() && itRow->second.find(col) != itRow->second.end()) {
            currSize_ -= sizeof(Value) + itRow->second[col]->second.capacity();
            cacheList_.erase(itRow->second[col]);
            itRow->second.erase(col);

            // Remove the row if it is empty
            if (itRow->second.empty()) {
                cacheMap_.erase(itRow);
            }
        }

        // Check if adding this item exceeds cache capacity
        while (currSize_ + valueSize > capacity_ && !cacheList_.empty()) {
            // Evict the least recently used item
            auto lru = cacheList_.front();
            if (onEvict_) {
                onEvict_(lru.first.first, lru.first.second, lru.second);
            }

            // Update the current size
            currSize_ -= sizeof(Value) + lru.second.capacity();

            // Update the cache
            cacheMap_[lru.first.first].erase(lru.first.second);
            if (cacheMap_[lru.first.first].empty()) {
                cacheMap_.erase(lru.first.first);
            }

            // Remove the least recently used item
            cacheList_.pop_front();
        }

        Key key = std::make_pair(row, col);
        // Insert the new or updated key-value pair
        cacheList_.push_back(KeyValuePair(key, value));
        cacheMap_[row][col] = --cacheList_.end();
        currSize_ += valueSize;
    }

    /**
     * @brief Get the value of a key-value pair from the cache.
     * If the key exists, move the key-value pair to the end of the list.
     * 
     * @param key the key
     * @param value the value to store the result
     * @return bool whether the operation is successful
    */
    bool Get(const Row& row, const Col& col, Value& value) {
        auto itRow = cacheMap_.find(row);
        if ((itRow = cacheMap_.find(row)) == cacheMap_.end()) {
            return false;
        }

        std::map<Col, ListIterator>& rowMap = cacheMap_[row];
        auto it = rowMap.find(col);
        if (it != rowMap.end()) {
            // If found, move to the end to mark as recently used
            cacheList_.splice(cacheList_.end(), cacheList_, it->second);
            value = it->second->second;
            return true;
        }
        return false;
    }

    /**
     * @brief Delete a key-value pair from the cache.
     * 
     * @param key the key
     * @return bool whether the operation is successful
    */
    bool Delete(const Row& row, const Col& col) {
        if (cacheMap_.find(row) == cacheMap_.end()) {
            return false;
        }

        std::map<Col, ListIterator>& rowMap = cacheMap_[row];
        auto it = rowMap.find(col);
        if (it != rowMap.end()) {
            currSize_ -= sizeof(Value) + it->second->second.capacity();
            cacheList_.erase(it->second);
            rowMap.erase(it);

            if (rowMap.empty()) {
                cacheMap_.erase(row);
            }
            
            return true;
        }
        return false;
    }

    /**
     * @brief Get all rows in the cache.
     * 
     * @param rows the vector to store the rows
     * @return bool whether the operation is successful
    */
    bool GetAllRows(std::vector<Row>& rows) {
        for (auto it = cacheMap_.begin(); it != cacheMap_.end(); it++) {
            rows.push_back(it->first);
        }
        return true;
    }

    /**
     * @brief Get all cols in a row.
     * 
     * @param row the row
     * @param cols the vector to store the cols
     * @return if there is such a row, return true, false otherwise
    */
    bool GetColsInRow(const Row& row, std::vector<Col>& cols) {
        if (cacheMap_.find(row) == cacheMap_.end()) {
            return false;
        }

        for (auto it = cacheMap_[row].begin(); it != cacheMap_[row].end(); it++) {
            cols.push_back(it->first);
        }
        return true;
    }

private:
    using Key = std::pair<Row, Col>;
    using KeyValuePair = std::pair<Key, Value>;
    using ListIterator = typename std::list<KeyValuePair>::iterator;

    std::list<KeyValuePair> cacheList_;
    std::unordered_map<Row, std::map<Col, ListIterator>> cacheMap_;
    size_t capacity_, currSize_ = 0;
    std::function<void(Row, Col, Value)> onEvict_;
};

#endif
