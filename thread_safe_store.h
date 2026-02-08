#pragma once

#include <any>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <unordered_map>

class ThreadSafeStore final {
public:
    enum class ValueType {
        kMissing,
        kBool,
        kInt,
        kDouble,
        kString,
        kOther,
    };

    ThreadSafeStore() = default;
    ~ThreadSafeStore() = default;

    ThreadSafeStore(const ThreadSafeStore&) = delete;
    ThreadSafeStore& operator=(const ThreadSafeStore&) = delete;

    // =========================
    // Set (write exclusive)
    // =========================
    template <typename T>
    void Set(const std::string& key, const T& val) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        data_[key] = val;
    }

    // =========================
    // Get (read shared)
    // =========================
    template <typename T>
    T Get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = data_.find(key);
        if (it == data_.end()) {
            throw std::runtime_error("Key not found: " + key);
        }

        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            throw std::runtime_error("Type mismatch for key: " + key);
        }
    }

    // =========================
    // TryGet (no exception)
    // =========================
    template <typename T>
    std::optional<T> TryGet(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = data_.find(key);
        if (it == data_.end()) {
            return std::nullopt;
        }

        try {
            return std::any_cast<T>(it->second);
        } catch (...) {
            return std::nullopt;
        }
    }

    // =========================
    // Has
    // =========================
    bool Has(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return data_.count(key) > 0;
    }

    // =========================
    // GetType
    // =========================
    ValueType GetType(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        const auto it = data_.find(key);
        if (it == data_.end()) {
            return ValueType::kMissing;
        }

        const std::type_info& type = it->second.type();
        if (type == typeid(bool)) {
            return ValueType::kBool;
        }
        if (type == typeid(int)) {
            return ValueType::kInt;
        }
        if (type == typeid(double)) {
            return ValueType::kDouble;
        }
        if (type == typeid(std::string)) {
            return ValueType::kString;
        }
        return ValueType::kOther;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::any> data_;
};
