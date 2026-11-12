#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace wallpaper {

enum class DesktopEnvironment {
    KDE_PLASMA,
    GNOME,
    XFCE,
    HYPRLAND,
    SWAY,
    X11_GENERIC,
    WINDOWS,
    UNKNOWN
};

enum class ResolutionFilter {
    ANY,
    FHD_1080P,      // 1920x1080
    QHD_1440P,      // 2560x1440
    UHD_4K,         // 3840x2160
    ULTRAWIDE_21_9, // 3440x1440 or 2560x1080
    CUSTOM
};

struct WallpaperItem {
    std::string id;
    std::string preview_url;
    std::string full_url;
    int width{0};
    int height{0};
    std::string aspect_ratio;
    uint64_t file_size{0};
    std::string file_type;
    std::vector<std::string> tags;
    std::vector<std::string> colors;
    std::string source;
};

// Helper string conversions
inline std::string to_string(DesktopEnvironment de) {
    switch (de) {
        case DesktopEnvironment::KDE_PLASMA:  return "KDE Plasma";
        case DesktopEnvironment::GNOME:       return "GNOME";
        case DesktopEnvironment::XFCE:        return "XFCE";
        case DesktopEnvironment::HYPRLAND:    return "Hyprland";
        case DesktopEnvironment::SWAY:        return "Sway";
        case DesktopEnvironment::X11_GENERIC: return "X11 Generic";
        case DesktopEnvironment::WINDOWS:     return "Windows";
        case DesktopEnvironment::UNKNOWN:
        default:                              return "Unknown";
    }
}

inline DesktopEnvironment desktop_environment_from_string(std::string_view str) {
    if (str == "KDE Plasma" || str == "KDE" || str == "plasma") return DesktopEnvironment::KDE_PLASMA;
    if (str == "GNOME" || str == "gnome")                       return DesktopEnvironment::GNOME;
    if (str == "XFCE" || str == "xfce")                         return DesktopEnvironment::XFCE;
    if (str == "Hyprland" || str == "hyprland")                 return DesktopEnvironment::HYPRLAND;
    if (str == "Sway" || str == "sway")                         return DesktopEnvironment::SWAY;
    if (str == "X11 Generic" || str == "x11")                   return DesktopEnvironment::X11_GENERIC;
    if (str == "Windows" || str == "windows")                   return DesktopEnvironment::WINDOWS;
    return DesktopEnvironment::UNKNOWN;
}

inline std::string to_string(ResolutionFilter res) {
    switch (res) {
        case ResolutionFilter::FHD_1080P:      return "1080p (FHD)";
        case ResolutionFilter::QHD_1440P:      return "1440p (QHD)";
        case ResolutionFilter::UHD_4K:         return "4K (UHD)";
        case ResolutionFilter::ULTRAWIDE_21_9: return "Ultrawide (21:9)";
        case ResolutionFilter::CUSTOM:         return "Custom";
        case ResolutionFilter::ANY:
        default:                               return "Any";
    }
}

inline ResolutionFilter resolution_filter_from_string(std::string_view str) {
    if (str == "1080p (FHD)" || str == "1080p" || str == "FHD")             return ResolutionFilter::FHD_1080P;
    if (str == "1440p (QHD)" || str == "1440p" || str == "QHD")             return ResolutionFilter::QHD_1440P;
    if (str == "4K (UHD)" || str == "4K" || str == "UHD")                   return ResolutionFilter::UHD_4K;
    if (str == "Ultrawide (21:9)" || str == "Ultrawide" || str == "21:9")    return ResolutionFilter::ULTRAWIDE_21_9;
    if (str == "Custom")                                                    return ResolutionFilter::CUSTOM;
    return ResolutionFilter::ANY;
}

// JSON serialization support
inline void to_json(nlohmann::json& j, const WallpaperItem& item) {
    j = nlohmann::json{
        {"id", item.id},
        {"preview_url", item.preview_url},
        {"full_url", item.full_url},
        {"width", item.width},
        {"height", item.height},
        {"aspect_ratio", item.aspect_ratio},
        {"file_size", item.file_size},
        {"file_type", item.file_type},
        {"tags", item.tags},
        {"colors", item.colors},
        {"source", item.source}
    };
}

inline void from_json(const nlohmann::json& j, WallpaperItem& item) {
    j.at("id").get_to(item.id);
    j.at("preview_url").get_to(item.preview_url);
    j.at("full_url").get_to(item.full_url);
    item.width = j.value("width", 0);
    item.height = j.value("height", 0);
    item.aspect_ratio = j.value("aspect_ratio", "");
    item.file_size = j.value("file_size", static_cast<uint64_t>(0));
    item.file_type = j.value("file_type", "");
    item.tags = j.value("tags", std::vector<std::string>{});
    item.colors = j.value("colors", std::vector<std::string>{});
    item.source = j.value("source", "");
}

} // namespace wallpaper
