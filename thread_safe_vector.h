#pragma once

#include <cstddef>
#include <mutex>
#include <vector>

template <typename T>
class ThreadSafeVector final {
public:
    explicit ThreadSafeVector(size_t size = 0)
        : data_(size) {}

    void Resize(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.resize(size);
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.size();
    }

    void Set(size_t index, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[index] = value;
    }

    T Get(size_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_[index];
    }

    T Back() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.back();
    }

private:
    mutable std::mutex mutex_;
    std::vector<T> data_;
};
