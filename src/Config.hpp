#pragma once
#include <Geode/Geode.hpp>
#include <fstream>
#include <string>

using namespace geode::prelude;

inline bool g_aiControlling = false;

inline std::string trimStr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

inline void ensureConfigTemplate() {
    auto cfgPath = Mod::get()->getConfigDir() / "ai_config.txt";
    if (!std::filesystem::exists(cfgPath)) {
        std::ofstream out(cfgPath);
        if (out) {
            out << "CF_ID=\n";
            out << "CF_KEY=\n";
        }
    }
}

inline std::string readConfigValue(const std::string& keyName) {
    ensureConfigTemplate();
    auto cfgPath = Mod::get()->getConfigDir() / "ai_config.txt";
    std::ifstream in(cfgPath);
    std::string line;
    while (std::getline(in, line)) {
        line = trimStr(line);
        if (line.rfind(keyName + "=", 0) == 0) {
            return trimStr(line.substr(keyName.size() + 1));
        }
    }
    return "";
}

inline std::string extractHex32(const std::string& raw) {
    std::string hexOnly;
    for (char c : raw) {
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            hexOnly.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    if (hexOnly.size() >= 32) return hexOnly.substr(hexOnly.size() - 32);
    return hexOnly;
}
