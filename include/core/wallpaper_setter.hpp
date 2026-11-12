#pragma once

#include "app/config.hpp"
#include "core/models.hpp"
#include "net/http_client.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace wallpaper {

struct WallpaperSetResult {
    bool success{false};
    std::string message;
    std::filesystem::path saved_path;
    std::string command_used;
};

class WallpaperSetter {
public:
    // Constructor with optional DesktopEnvironment (defaults to Config::get_desktop_environment() or detect)
    explicit WallpaperSetter(std::optional<DesktopEnvironment> de = std::nullopt);

    // Apply wallpaper from an existing local image path
    WallpaperSetResult set_wallpaper(const std::filesystem::path& image_path);

    // Download item if not already cached and apply as wallpaper
    WallpaperSetResult download_and_set(
        const WallpaperItem& item,
        const std::filesystem::path& destination_dir,
        std::function<void(size_t, size_t)> progress_cb = nullptr
    );

    // Build standard filename for wallpaper item: wallhaven_<id>_<width>x<height>.<ext>
    std::filesystem::path build_filename(
        const WallpaperItem& item,
        const std::filesystem::path& destination_dir
    );

    // Register wallpaper in desktop environment wallpaper lists (KDE Plasma, GNOME, XFCE)
    bool register_in_system_wallpapers(
        const std::filesystem::path& image_path,
        const WallpaperItem& item
    );

    // System data directory override (~/.local/share) for testing and customization
    void set_system_data_dir_override(const std::filesystem::path& dir) { system_data_dir_override_ = dir; }
    [[nodiscard]] std::filesystem::path get_system_data_dir() const;

    // Command preview / logging
    [[nodiscard]] std::string get_command_for(const std::filesystem::path& image_path) const;
    [[nodiscard]] std::string get_command_for(const std::filesystem::path& image_path, DesktopEnvironment de) const;

    // Desktop environment management
    [[nodiscard]] DesktopEnvironment get_desktop_environment() const noexcept { return desktop_env_; }
    void set_desktop_environment(DesktopEnvironment de) noexcept { desktop_env_ = de; }

    // Dry-run mode for simulation / headless environments
    [[nodiscard]] bool is_dry_run() const noexcept { return dry_run_; }
    void set_dry_run(bool dry_run) noexcept { dry_run_ = dry_run; }

    // Custom command runner for testing and mock injection
    using CommandRunner = std::function<int(const std::string& command, std::string& output)>;
    void set_command_runner(CommandRunner runner) { command_runner_ = std::move(runner); }
    void reset_command_runner() { command_runner_ = nullptr; }

    // HTTP client access
    HttpClient& get_http_client() noexcept { return http_client_; }
    const HttpClient& get_http_client() const noexcept { return http_client_; }

private:
    DesktopEnvironment desktop_env_{DesktopEnvironment::UNKNOWN};
    HttpClient http_client_;
    bool dry_run_{false};
    CommandRunner command_runner_{nullptr};
    std::filesystem::path system_data_dir_override_;

    // Internal command execution helper
    int execute_command(const std::string& cmd, std::string& output) const;
};

} // namespace wallpaper
