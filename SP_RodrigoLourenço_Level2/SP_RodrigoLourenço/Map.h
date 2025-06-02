// Map.h
#ifndef MAP_H
#define MAP_H
#include <map>

template<typename K, typename V>
class Map {
    std::map<K, V> m_;
public:
    using iterator = typename std::map<K, V>::iterator;
    using const_iterator = typename std::map<K, V>::const_iterator;

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