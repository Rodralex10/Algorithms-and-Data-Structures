// HashMap.h
#ifndef HASHMAP_H
#define HASHMAP_H
#include <unordered_map>

template<typename K, typename V>
class HashMap {
    std::unordered_map<K, V> m_;
public:
    using iterator = typename std::unordered_map<K, V>::iterator;
    using const_iterator = typename std::unordered_map<K, V>::const_iterator;

    V& operator[](const K& k) { return m_[k]; }
    iterator       find(const K& k) { return m_.find(k); }
    const_iterator find(const K& k) const { return m_.find(k); }

    iterator       begin() { return m_.begin(); }
    const_iterator begin() const { return m_.begin(); }
    iterator       end() { return m_.end(); }
    const_iterator end()   const { return m_.end(); }

    void clear() { m_.clear(); }
};

#endif

