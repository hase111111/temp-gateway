#include "stdin_writer.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>

#include "thread_safe_store.h"

namespace {

std::string Trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool TryParseBool(const std::string& s, bool& out) {
    const std::string lower = ToLower(s);
    if (lower == "true" || lower == "1") {
        out = true;
        return true;
    }
    if (lower == "false" || lower == "0") {
        out = false;
        return true;
    }
    return false;
}

bool TryParseInt(const std::string& s, int& out) {
    errno = 0;
    char* end = nullptr;
    long value = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0' || errno != 0) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

bool TryParseDouble(const std::string& s, double& out) {
    errno = 0;
    char* end = nullptr;
    double value = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || errno != 0) {
        return false;
    }
    out = value;
    return true;
}

}

void StdinWriter::Run() {
    std::string line;
    while (!g_thread_safe_store.Get<bool>("fin")) {
        if (!std::getline(std::cin, line)) {
            break;
        }
        std::cout << g_thread_safe_store.Get<bool>("fin") << std::endl;
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, eq));
        const std::string val = Trim(line.substr(eq + 1));
        if (key.empty()) {
            continue;
        }

        const auto type = g_thread_safe_store.GetType(key);
        if (type == ThreadSafeStore::ValueType::kBool) {
            bool parsed = false;
            if (TryParseBool(val, parsed)) {
                std::cout << "[StdinWriter] Set " << key
                          << " = " << (parsed ? "true" : "false") << std::endl;
                g_thread_safe_store.Set<bool>(key, parsed);
            }
            continue;
        } else if (type == ThreadSafeStore::ValueType::kInt) {
            int parsed = 0;
            if (TryParseInt(val, parsed)) {
                g_thread_safe_store.Set<int>(key, parsed);
            }
            continue;
        } else if (type == ThreadSafeStore::ValueType::kDouble) {
            double parsed = 0.0;
            if (TryParseDouble(val, parsed)) {
                g_thread_safe_store.Set<double>(key, parsed);
            }
            continue;
        } else if (type == ThreadSafeStore::ValueType::kString) {
            g_thread_safe_store.Set<std::string>(key, val);
            continue;
        }
    }
}
