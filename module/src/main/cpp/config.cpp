#include "config.h"
#include "game.h"
#include "log.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <string>

static bool is_valid_package(const std::string &value) {
    if (value.empty() || value.size() >= 256 || value.find('.') == std::string::npos) {
        return false;
    }
    for (unsigned char c : value) {
        if (!(std::isalnum(c) || c == '.' || c == '_')) {
            return false;
        }
    }
    return true;
}

static std::string trim_line(std::string line) {
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' ||
                             line.back() == '\t')) {
        line.pop_back();
    }
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
        ++start;
    }
    return line.substr(start);
}

const char *get_target_package_name() {
    static char package_name[256] = {0};
    static bool initialized = false;
    if (initialized) {
        return package_name;
    }
    initialized = true;

    std::ifstream in(TARGET_PACKAGE_PATH);
    if (!in) {
        LOGW("no target_package; set via KernelSU WebUI");
        return package_name;
    }

    std::string line;
    if (!std::getline(in, line)) {
        LOGW("empty target_package; set via KernelSU WebUI");
        return package_name;
    }

    line = trim_line(line);
    if (!is_valid_package(line)) {
        LOGW("invalid package in %s", TARGET_PACKAGE_PATH);
        return package_name;
    }

    strncpy(package_name, line.c_str(), sizeof(package_name) - 1);
    LOGI("target package: %s", package_name);
    return package_name;
}
