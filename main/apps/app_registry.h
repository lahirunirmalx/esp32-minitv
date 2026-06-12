/**
 * @file app_registry.h
 * @brief Lookup table of installed apps (name -> Mooncake app ID), in cycle
 *        order. Populated by apps::install_all(); the UI task cycles through
 *        it on each tap of the top pad.
 */
#pragma once

#include <string>
#include <vector>

namespace app_registry {

struct Entry {
    std::string name;
    int id;
};

inline std::vector<Entry>& entries()
{
    static std::vector<Entry> e;
    return e;
}

// Resolve an app name to its installed Mooncake ID, or -1 if not found.
inline int id_of(const std::string& name)
{
    for (const auto& e : entries())
        if (e.name == name) return e.id;
    return -1;
}

} // namespace app_registry
