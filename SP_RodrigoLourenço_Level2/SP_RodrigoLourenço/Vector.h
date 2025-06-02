// Vector.h
#ifndef VECTOR_H
#define VECTOR_H

#include <cstddef>
#include <new>
#include <cassert>

template<typename T>
class Vector {
private:
    T* data = nullptr;
    size_t _size = 0;
    size_t _capacity = 0;

public:
    Vector() = default;

    // Copy constructor
    Vector(const Vector& other)
        : data(nullptr), _size(other._size), _capacity(other._capacity) {
        if (_capacity > 0) {
            data = static_cast<T*>(operator new[](_capacity * sizeof(T)));
            for (size_t i = 0; i < _size; ++i) {
                new (&data[i]) T(other.data[i]);
            }
        }
    }

    // Copy assignment
    Vector& operator=(const Vector& other) {
        if (this == &other) return *this;
        // Destroy existing
        for (size_t i = 0; i < _size; ++i) {
            data[i].~T();
        }
        operator delete[](data);

        _size = other._size;
        _capacity = other._capacity;
        data = nullptr;
        if (_capacity > 0) {
            data = static_cast<T*>(operator new[](_capacity * sizeof(T)));
            for (size_t i = 0; i < _size; ++i) {
                new (&data[i]) T(other.data[i]);
            }
        }
        return *this;
    }

    // Move constructor
    Vector(Vector&& other) noexcept
        : data(other.data), _size(other._size), _capacity(other._capacity) {
        other.data = nullptr;
        other._size = 0;
        other._capacity = 0;
    }

    // Move assignment
    Vector& operator=(Vector&& other) noexcept {
        if (this == &other) return *this;
        // Destroy existing
        for (size_t i = 0; i < _size; ++i) {
            data[i].~T();
        }
        operator delete[](data);

        // Steal other's data
        data = other.data;
        _size = other._size;
        _capacity = other._capacity;

        other.data = nullptr;
        other._size = 0;
        other._capacity = 0;
        return *this;
    }

    // Destructor
    ~Vector() {
        for (size_t i = 0; i < _size; ++i) {
            data[i].~T();
        }
        operator delete[](data);
    }

    // Bounds‐checked operator[]
    T& operator[](size_t i) {
        assert(i < _size);   // CRASH immediately if i >= _size
        return data[i];
    }
    const T& operator[](size_t i) const {
        assert(i < _size);
        return data[i];
    }

    // Return current number of elements
    size_t size() const { return _size; }

    // True if empty
    bool empty() const { return _size == 0; }

    // Add a copy of value at the end
    void push_back(const T& value) {
        if (_size >= _capacity) {
            size_t newCap = (_capacity == 0 ? 1 : _capacity * 2);
            T* newData = static_cast<T*>(operator new[](newCap * sizeof(T)));
            for (size_t j = 0; j < _size; ++j) {
                new (&newData[j]) T(std::move(data[j]));
                data[j].~T();
            }
            operator delete[](data);
            data = newData;
            _capacity = newCap;
        }
        new (&data[_size]) T(value);
        _size++;
    }

    // Add a moved value at the end
    void push_back(T&& value) {
        if (_size >= _capacity) {
            size_t newCap = (_capacity == 0 ? 1 : _capacity * 2);
            T* newData = static_cast<T*>(operator new[](newCap * sizeof(T)));
            for (size_t j = 0; j < _size; ++j) {
                new (&newData[j]) T(std::move(data[j]));
                data[j].~T();
            }
            operator delete[](data);
            data = newData;
            _capacity = newCap;
        }
        new (&data[_size]) T(std::move(value));
        _size++;
    }

    // Remove last element
    void pop_back() {
        assert(_size > 0);
        data[_size - 1].~T();
        _size--;
    }

    // Remove all elements
    void clear() {
        for (size_t i = 0; i < _size; ++i) {
            data[i].~T();
        }
        _size = 0;
    }
};

#endif // VECTOR_H