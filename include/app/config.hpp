#pragma once

#include "core/models.hpp"
#include <filesystem>
#include <string>
#include <memory>

namespace wallpaper {

class Config {
public:
    Config();

    // Configuration properties
    std::filesystem::path wallpaper_dir;
    ResolutionFilter preferred_resolution{ResolutionFilter::ANY};
    bool dark_mode{true};
    int results_per_page{24};
    int download_concurrency{4};
    bool auto_set_wallpaper{true};
    DesktopEnvironment desktop_environment{DesktopEnvironment::UNKNOWN};
    std::string github_repo{"hex/wallfinder"};
    bool check_updates_on_startup{true};

    // Singleton access
    static Config& instance();

    // Reset fields to system defaults
    void reset_defaults();

    // Desktop environment getter / setter
    [[nodiscard]] DesktopEnvironment get_desktop_environment() const noexcept { return desktop_environment; }
    void set_desktop_environment(DesktopEnvironment de) noexcept { desktop_environment = de; }

    // Load from disk (uses default path if custom_path is empty)
    bool load(const std::filesystem::path& custom_path = "");

    // Save to disk (uses default path if custom_path is empty)
    bool save(const std::filesystem::path& custom_path = "") const;

    // Detection helpers
    static DesktopEnvironment detect_desktop_environment();
    static std::filesystem::path detect_pictures_directory();
    static std::filesystem::path get_default_wallpaper_path();
    static std::filesystem::path get_default_config_path();
    static std::filesystem::path get_home_directory();

    // JSON serialization
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);
};

} // namespace wallpaper
