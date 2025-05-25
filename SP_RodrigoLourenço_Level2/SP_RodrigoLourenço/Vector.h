// Vector.h
#ifndef VECTOR_H
#define VECTOR_H
#include <cstddef>

template<typename T>
class Vector {
    T* data_ = nullptr;
    size_t sz_ = 0, cap_ = 0;
    void reserve(size_t n) {
        T* newb = new T[n];
        for (size_t i = 0; i < sz_; ++i) newb[i] = data_[i];
        delete[] data_;
        data_ = newb;
        cap_ = n;
    }
public:
    Vector() = default;
    ~Vector() { delete[] data_; }

    void push_back(const T& v) {
        if (sz_ == cap_) reserve(cap_ ? cap_ * 2 : 1);
        data_[sz_++] = v;
    }
    size_t size() const { return sz_; }

    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }

    void clear() { sz_ = 0; }
    void swap(size_t i, size_t j) {
        T tmp = data_[i];
        data_[i] = data_[j];
        data_[j] = tmp;
    }
};

#endif

