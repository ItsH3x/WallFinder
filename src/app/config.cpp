#include "app/config.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>

namespace wallpaper {

namespace {

std::string to_lower(std::string_view sv) {
    std::string s(sv);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string get_env_or_empty(const char* name) {
    const char* val = std::getenv(name);
    return val ? std::string(val) : std::string{};
}

} // namespace

Config::Config() {
    reset_defaults();
}

Config& Config::instance() {
    static Config inst;
    return inst;
}

void Config::reset_defaults() {
    desktop_environment = detect_desktop_environment();
    wallpaper_dir = get_default_wallpaper_path();
    preferred_resolution = ResolutionFilter::ANY;
    dark_mode = true;
    results_per_page = 24;
    download_concurrency = 4;
    auto_set_wallpaper = true;
    github_repo = "ItsH3x/WallFinder";
    check_updates_on_startup = true;
}

std::filesystem::path Config::get_home_directory() {
#if defined(_WIN32) || defined(_WIN64)
    std::string user_profile = get_env_or_empty("USERPROFILE");
    if (!user_profile.empty()) {
        return std::filesystem::path(user_profile);
    }
    std::string home_drive = get_env_or_empty("HOMEDRIVE");
    std::string home_path = get_env_or_empty("HOMEPATH");
    if (!home_drive.empty() && !home_path.empty()) {
        return std::filesystem::path(home_drive + home_path);
    }
    return std::filesystem::current_path();
#else
    std::string home = get_env_or_empty("HOME");
    if (!home.empty()) {
        return std::filesystem::path(home);
    }
    return std::filesystem::temp_directory_path();
#endif
}

DesktopEnvironment Config::detect_desktop_environment() {
#if defined(_WIN32) || defined(_WIN64)
    return DesktopEnvironment::WINDOWS;
#endif

    std::string kde_session = get_env_or_empty("KDE_FULL_SESSION");
    std::string gnome_session_id = get_env_or_empty("GNOME_DESKTOP_SESSION_ID");

    std::string current_desktop = to_lower(get_env_or_empty("XDG_CURRENT_DESKTOP"));
    std::string desktop_session = to_lower(get_env_or_empty("DESKTOP_SESSION"));
    std::string session_desktop = to_lower(get_env_or_empty("XDG_SESSION_DESKTOP"));
    std::string session_type = to_lower(get_env_or_empty("XDG_SESSION_TYPE"));

    // KDE Plasma detection
    if (!kde_session.empty() ||
        current_desktop.find("kde") != std::string::npos ||
        current_desktop.find("plasma") != std::string::npos ||
        desktop_session.find("plasma") != std::string::npos ||
        desktop_session.find("kde") != std::string::npos ||
        session_desktop.find("plasma") != std::string::npos) {
        return DesktopEnvironment::KDE_PLASMA;
    }

    // GNOME detection
    if (!gnome_session_id.empty() ||
        current_desktop.find("gnome") != std::string::npos ||
        desktop_session.find("gnome") != std::string::npos ||
        session_desktop.find("gnome") != std::string::npos) {
        return DesktopEnvironment::GNOME;
    }

    // XFCE detection
    if (current_desktop.find("xfce") != std::string::npos ||
        desktop_session.find("xfce") != std::string::npos) {
        return DesktopEnvironment::XFCE;
    }

    // Hyprland detection
    if (current_desktop.find("hyprland") != std::string::npos ||
        desktop_session.find("hyprland") != std::string::npos) {
        return DesktopEnvironment::HYPRLAND;
    }

    // Sway detection
    if (current_desktop.find("sway") != std::string::npos ||
        desktop_session.find("sway") != std::string::npos) {
        return DesktopEnvironment::SWAY;
    }

    // Generic X11 detection
    if (session_type == "x11" || !get_env_or_empty("DISPLAY").empty()) {
        return DesktopEnvironment::X11_GENERIC;
    }

    return DesktopEnvironment::UNKNOWN;
}

std::filesystem::path Config::detect_pictures_directory() {
    std::filesystem::path home = get_home_directory();

    // Check ~/.config/user-dirs.dirs (standard freedesktop XDG user dirs)
    std::filesystem::path user_dirs_file = home / ".config" / "user-dirs.dirs";
    if (std::filesystem::exists(user_dirs_file)) {
        std::ifstream file(user_dirs_file);
        std::string line;
        while (std::getline(file, line)) {
            // Trim whitespace
            auto start = line.find_first_not_of(" \t");
            if (start == std::string::npos || line[start] == '#') {
                continue;
            }
            std::string trimmed = line.substr(start);
            if (trimmed.rfind("XDG_PICTURES_DIR", 0) == 0) {
                auto eq_pos = trimmed.find('=');
                if (eq_pos != std::string::npos) {
                    std::string val = trimmed.substr(eq_pos + 1);
                    // Strip quotes and trailing whitespace/newlines
                    auto val_start = val.find_first_not_of(" \t\"'");
                    auto val_end = val.find_last_not_of(" \t\r\n\"'");
                    if (val_start != std::string::npos && val_end != std::string::npos) {
                        val = val.substr(val_start, val_end - val_start + 1);
                    }

                    // Expand $HOME or ${HOME}
                    if (val.rfind("$HOME", 0) == 0) {
                        val = home.string() + val.substr(5);
                    } else if (val.rfind("${HOME}", 0) == 0) {
                        val = home.string() + val.substr(7);
                    } else if (val.rfind("~", 0) == 0) {
                        val = home.string() + val.substr(1);
                    }

                    std::filesystem::path pic_path(val);
                    if (!val.empty()) {
                        return pic_path;
                    }
                }
            }
        }
    }

    // Fallbacks
    std::filesystem::path pictures_candidate = home / "Pictures";
    if (std::filesystem::exists(pictures_candidate)) {
        return pictures_candidate;
    }

    std::filesystem::path imagenes_candidate = home / "Imágenes";
    if (std::filesystem::exists(imagenes_candidate)) {
        return imagenes_candidate;
    }

    return pictures_candidate;
}

std::filesystem::path Config::get_default_wallpaper_path() {
    std::filesystem::path pic_dir = detect_pictures_directory();
    std::filesystem::path wall_dir = pic_dir / "Wallpapers";

    std::error_code ec;
    std::filesystem::create_directories(wall_dir, ec);

    return wall_dir;
}

std::filesystem::path Config::get_default_config_path() {
    return get_home_directory() / ".config" / "wallpaper-finder" / "config.json";
}

nlohmann::json Config::to_json() const {
    return nlohmann::json{
        {"wallpaper_dir", wallpaper_dir.string()},
        {"preferred_resolution", to_string(preferred_resolution)},
        {"dark_mode", dark_mode},
        {"results_per_page", results_per_page},
        {"download_concurrency", download_concurrency},
        {"auto_set_wallpaper", auto_set_wallpaper},
        {"desktop_environment", to_string(desktop_environment)},
        {"github_repo", github_repo},
        {"check_updates_on_startup", check_updates_on_startup}
    };
}

void Config::from_json(const nlohmann::json& j) {
    if (j.contains("wallpaper_dir") && j["wallpaper_dir"].is_string()) {
        wallpaper_dir = std::filesystem::path(j["wallpaper_dir"].get<std::string>());
    }
    if (j.contains("preferred_resolution") && j["preferred_resolution"].is_string()) {
        preferred_resolution = resolution_filter_from_string(j["preferred_resolution"].get<std::string>());
    }
    if (j.contains("dark_mode") && j["dark_mode"].is_boolean()) {
        dark_mode = j["dark_mode"].get<bool>();
    }
    if (j.contains("results_per_page") && j["results_per_page"].is_number_integer()) {
        results_per_page = j["results_per_page"].get<int>();
    }
    if (j.contains("download_concurrency") && j["download_concurrency"].is_number_integer()) {
        download_concurrency = j["download_concurrency"].get<int>();
    }
    if (j.contains("auto_set_wallpaper") && j["auto_set_wallpaper"].is_boolean()) {
        auto_set_wallpaper = j["auto_set_wallpaper"].get<bool>();
    }
    if (j.contains("desktop_environment") && j["desktop_environment"].is_string()) {
        desktop_environment = desktop_environment_from_string(j["desktop_environment"].get<std::string>());
    }
    if (j.contains("github_repo") && j["github_repo"].is_string()) {
        github_repo = j["github_repo"].get<std::string>();
    }
    if (j.contains("check_updates_on_startup") && j["check_updates_on_startup"].is_boolean()) {
        check_updates_on_startup = j["check_updates_on_startup"].get<bool>();
    }
}

bool Config::load(const std::filesystem::path& custom_path) {
    std::filesystem::path path = custom_path.empty() ? get_default_config_path() : custom_path;
    if (!std::filesystem::exists(path)) {
        return false;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        nlohmann::json j;
        file >> j;
        from_json(j);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config from " << path << ": " << e.what() << '\n';
        return false;
    }
}

bool Config::save(const std::filesystem::path& custom_path) const {
    std::filesystem::path path = custom_path.empty() ? get_default_config_path() : custom_path;
    try {
        std::error_code ec;
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path(), ec);
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j = to_json();
        file << j.dump(4) << '\n';
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving config to " << path << ": " << e.what() << '\n';
        return false;
    }
}

} // namespace wallpaper

