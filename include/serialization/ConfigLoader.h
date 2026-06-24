#pragma once
#include "SatelliteConfig.h"
#include <string>

namespace obcsw {

/**
 * @brief Loads SatelliteConfig from a YAML file (yaml-cpp).
 *
 * All fields have safe defaults so a minimal YAML can omit anything
 * that isn't mission-critical.
 */
class ConfigLoader {
public:
    /// Load and validate config from the given YAML path.
    /// Throws std::runtime_error on file I/O or parse failure.
    static SatelliteConfig load(const std::string& yaml_path);
};

} // namespace obcsw
