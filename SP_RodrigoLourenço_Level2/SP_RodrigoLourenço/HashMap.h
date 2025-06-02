// HashMap.h
#ifndef HASHMAP_H
#define HASHMAP_H

#include <functional>
#include "Vector.h"

template<typename K, typename V>
class HashMap {
private:
    struct Entry {
        K key;
        V value;
    };

    Vector<Entry>* buckets;
    size_t bucketCount;

    // Compute bucket index from key
    size_t hashKey(const K& key) const {
        return std::hash<K>{}(key) % bucketCount;
    }

public:
    // Constructor: create “initBuckets” empty buckets
    HashMap(size_t initBuckets = 101) {
        bucketCount = initBuckets;
        buckets = new Vector<Entry>[bucketCount];
    }

    // Destructor: only deletes the bucket‐array; does NOT delete any V inside
    ~HashMap() {
        delete[] buckets;
    }

    // [] operator: if “key” exists, return reference to its value;
    // otherwise insert (key, V()) and return reference to the newly inserted V.
    V& operator[](const K& key) {
        size_t idx = hashKey(key);
        auto& bucket = buckets[idx];
        for (size_t i = 0; i < bucket.size(); ++i) {
            if (bucket[i].key == key) {
                return bucket[i].value;
            }
        }
        // Not found → insert new Entry
        Entry e;
        e.key = key;
        e.value = V();
        bucket.push_back(e);
        return bucket[bucket.size() - 1].value;
    }

    // find(key): return pointer to V if found, or nullptr otherwise
    V* find(const K& key) {
        size_t idx = hashKey(key);
        auto& bucket = buckets[idx];
        for (size_t i = 0; i < bucket.size(); ++i) {
            if (bucket[i].key == key) {
                return &bucket[i].value;
            }
        }
        return nullptr;
    }

    // const‐version of find
    const V* find(const K& key) const {
        size_t idx = hashKey(key);
        auto& bucket = buckets[idx];
        for (size_t i = 0; i < bucket.size(); ++i) {
            if (bucket[i].key == key) {
                return &bucket[i].value;
            }
        }
        return nullptr;
    }
};

#endif // HASHMAP_H