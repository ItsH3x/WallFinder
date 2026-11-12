#include "core/wallpaper_setter.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace wallpaper {

namespace {

std::string escape_shell_arg(std::string_view arg) {
    std::string result = "'";
    for (char c : arg) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += "'";
    return result;
}

bool is_executable(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec) || !std::filesystem::is_regular_file(p, ec)) {
        return false;
    }
#if !defined(_WIN32) && !defined(_WIN64)
    return ::access(p.c_str(), X_OK) == 0;
#else
    return true;
#endif
}

std::string find_executable(std::string_view name) {
    std::filesystem::path p(name);
    if (p.is_absolute() && is_executable(p)) {
        return p.string();
    }

    const char* path_env = std::getenv("PATH");
    if (!path_env) {
        path_env = "/usr/local/bin:/usr/bin:/bin";
    }

    std::string_view env_view(path_env);
    size_t start = 0;
    while (start < env_view.size()) {
        size_t end = env_view.find(':', start);
        std::string_view dir = (end == std::string_view::npos)
            ? env_view.substr(start)
            : env_view.substr(start, end - start);

        if (!dir.empty()) {
            std::filesystem::path cand = std::filesystem::path(dir) / name;
            if (is_executable(cand)) {
                return cand.string();
            }
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }

    const std::initializer_list<const char*> fallback_dirs = {
        "/usr/bin", "/bin", "/usr/local/bin"
    };
    for (const char* dir : fallback_dirs) {
        std::filesystem::path cand = std::filesystem::path(dir) / name;
        if (is_executable(cand)) {
            return cand.string();
        }
    }

    return {};
}

std::string get_kde_dbus_command(const std::filesystem::path& abs_path) {
    std::string qdbus_tool;
    if (is_executable("/usr/bin/qdbus6")) {
        qdbus_tool = "/usr/bin/qdbus6";
    } else if (is_executable("/usr/bin/qdbus")) {
        qdbus_tool = "/usr/bin/qdbus";
    } else if (is_executable("/usr/bin/qdbus-qt5")) {
        qdbus_tool = "/usr/bin/qdbus-qt5";
    } else if (is_executable("/usr/bin/qdbus-qt6")) {
        qdbus_tool = "/usr/bin/qdbus-qt6";
    } else {
        qdbus_tool = find_executable("qdbus");
        if (qdbus_tool.empty()) qdbus_tool = find_executable("qdbus6");
        if (qdbus_tool.empty()) qdbus_tool = find_executable("qdbus-qt5");
        if (qdbus_tool.empty()) qdbus_tool = find_executable("qdbus-qt6");
    }

    std::string js = "var allDesktops = desktops();for (var i=0;i<allDesktops.length;i++) {"
                     "var d = allDesktops[i];d.wallpaperPlugin = \"org.kde.image\";"
                     "d.currentConfigGroup = Array(\"Wallpaper\", \"org.kde.image\", \"General\");"
                     "d.writeConfig(\"Image\", \"file://" + abs_path.string() + "\");}";

    if (!qdbus_tool.empty()) {
        return qdbus_tool + " org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript " + escape_shell_arg(js);
    }

    std::string dbus_send = find_executable("dbus-send");
    if (!dbus_send.empty()) {
        return dbus_send + " --session --dest=org.kde.plasmashell --type=method_call /PlasmaShell org.kde.PlasmaShell.evaluateScript string:" + escape_shell_arg(js);
    }

    return "qdbus org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript " + escape_shell_arg(js);
}

bool safe_link_or_copy(const std::filesystem::path& source, const std::filesystem::path& target) {
    std::error_code ec;
    if (std::filesystem::exists(target, ec) || std::filesystem::is_symlink(target, ec)) {
        std::filesystem::remove(target, ec);
    }
    ec.clear();
    std::filesystem::create_symlink(source, target, ec);
    if (!ec) {
        return true;
    }
    ec.clear();
    return std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
}

std::string xml_escape(std::string_view str) {
    std::string res;
    res.reserve(str.size());
    for (char c : str) {
        switch (c) {
            case '&':  res += "&amp;"; break;
            case '<':  res += "&lt;"; break;
            case '>':  res += "&gt;"; break;
            case '\"': res += "&quot;"; break;
            case '\'': res += "&apos;"; break;
            default:   res += c; break;
        }
    }
    return res;
}

} // namespace

WallpaperSetter::WallpaperSetter(std::optional<DesktopEnvironment> de) {
    if (de.has_value() && *de != DesktopEnvironment::UNKNOWN) {
        desktop_env_ = *de;
    } else {
        DesktopEnvironment detected = Config::instance().get_desktop_environment();
        if (detected != DesktopEnvironment::UNKNOWN) {
            desktop_env_ = detected;
        } else {
            desktop_env_ = Config::detect_desktop_environment();
        }
    }
}

std::filesystem::path WallpaperSetter::build_filename(
    const WallpaperItem& item,
    const std::filesystem::path& destination_dir) {

    // Clean ID
    std::string clean_id = item.id;
    if (clean_id.rfind("wallhaven-", 0) == 0) {
        clean_id = clean_id.substr(10);
    } else if (clean_id.rfind("wallhaven_", 0) == 0) {
        clean_id = clean_id.substr(10);
    }
    // Sanitize characters
    for (char& c : clean_id) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    if (clean_id.empty()) {
        clean_id = "unknown";
    }

    // Determine extension
    std::string ext;
    if (!item.full_url.empty()) {
        auto pos = item.full_url.rfind('.');
        if (pos != std::string::npos && pos + 1 < item.full_url.length()) {
            std::string candidate = item.full_url.substr(pos);
            auto qpos = candidate.find('?');
            if (qpos != std::string::npos) {
                candidate = candidate.substr(0, qpos);
            }
            if (candidate == ".jpg" || candidate == ".jpeg" || candidate == ".png" ||
                candidate == ".webp" || candidate == ".bmp" || candidate == ".avif" ||
                candidate == ".jxl") {
                ext = candidate;
            }
        }
    }

    if (ext.empty() && !item.file_type.empty()) {
        std::string ft = item.file_type;
        std::transform(ft.begin(), ft.end(), ft.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (ft == "image/jpeg" || ft == "image/jpg" || ft == "jpeg" || ft == "jpg") {
            ext = ".jpg";
        } else if (ft == "image/png" || ft == "png") {
            ext = ".png";
        } else if (ft == "image/webp" || ft == "webp") {
            ext = ".webp";
        } else if (ft == "image/bmp" || ft == "bmp") {
            ext = ".bmp";
        } else if (ft == "image/avif" || ft == "avif") {
            ext = ".avif";
        } else if (ft == "image/jxl" || ft == "jxl") {
            ext = ".jxl";
        }
    }

    if (ext.empty()) {
        ext = ".jpg";
    }

    std::string prefix = "wallhaven";
    if (!item.source.empty() && item.source != "wallhaven") {
        prefix = item.source;
    }

    std::string filename = prefix + "_" + clean_id + "_" +
                           std::to_string(item.width) + "x" +
                           std::to_string(item.height) + ext;

    std::filesystem::path dir = destination_dir.empty()
        ? Config::instance().wallpaper_dir
        : destination_dir;

    return dir / filename;
}

std::filesystem::path WallpaperSetter::get_system_data_dir() const {
    if (!system_data_dir_override_.empty()) {
        return system_data_dir_override_;
    }
    const char* xdg_data = std::getenv("XDG_DATA_HOME");
    if (xdg_data && *xdg_data != '\0') {
        return std::filesystem::path(xdg_data);
    }
    return Config::get_home_directory() / ".local" / "share";
}

bool WallpaperSetter::register_in_system_wallpapers(
    const std::filesystem::path& image_path,
    const WallpaperItem& item) {

    std::error_code ec;
    if (!std::filesystem::exists(image_path, ec) || !std::filesystem::is_regular_file(image_path, ec)) {
        return false;
    }

    std::filesystem::path abs_source = std::filesystem::absolute(image_path, ec);
    if (ec) {
        abs_source = image_path;
    }

    std::filesystem::path base_share = get_system_data_dir();
    if (base_share.empty()) {
        return false;
    }

    // Clean item ID
    std::string clean_id = item.id;
    if (clean_id.rfind("wallhaven-", 0) == 0) {
        clean_id = clean_id.substr(10);
    } else if (clean_id.rfind("wallhaven_", 0) == 0) {
        clean_id = clean_id.substr(10);
    }
    if (clean_id.empty()) {
        clean_id = image_path.stem().string();
    }
    for (char& c : clean_id) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            c = '_';
        }
    }

    std::string ext = image_path.extension().string();
    if (ext.empty()) {
        ext = ".jpg";
    }

    bool any_success = false;

    // 1. KDE Plasma: KDE Plasma ya indexa de forma nativa los archivos de imagen sueltos
    // en el directorio de fondos del usuario (~/Imágenes/Wallpapers/ o similar) y los aplica
    // mediante plasma-apply-wallpaperimage. No debemos crear el paquete KPlugin en
    // ~/.local/share/wallpapers/<pkg_id> porque eso causa que Plasma muestre el fondo dos veces.
    try {
        std::string pkg_id = "wallhaven_" + clean_id;
        std::filesystem::path kde_pkg_dir = base_share / "wallpapers" / pkg_id;
        std::error_code rm_ec;
        if (std::filesystem::exists(kde_pkg_dir, rm_ec)) {
            std::filesystem::remove_all(kde_pkg_dir, rm_ec);
        }

        // Si la imagen está en el directorio de fondos del usuario (o accesible), contamos como éxito para KDE Plasma
        if (std::filesystem::exists(abs_source, rm_ec)) {
            any_success = true;
        }
    } catch (...) {
    }

    // 2. GNOME: ~/.local/share/backgrounds/<filename> + XML en ~/.local/share/gnome-background-properties/
    try {
        std::filesystem::path gnome_bg_dir = base_share / "backgrounds";
        std::filesystem::create_directories(gnome_bg_dir, ec);
        std::filesystem::path gnome_bg_target = gnome_bg_dir / image_path.filename();
        if (safe_link_or_copy(abs_source, gnome_bg_target)) {
            any_success = true;
        }

        std::filesystem::path gnome_props_dir = base_share / "gnome-background-properties";
        std::filesystem::create_directories(gnome_props_dir, ec);
        std::filesystem::path gnome_xml_path = gnome_props_dir / ("wallhaven-" + clean_id + ".xml");

        std::ofstream xml_file(gnome_xml_path);
        if (xml_file.is_open()) {
            xml_file << "<?xml version=\"1.0\"?>\n"
                     << "<!DOCTYPE wallpapers SYSTEM \"gnome-wp-list.dtd\">\n"
                     << "<wallpapers>\n"
                     << "  <wallpaper deleted=\"false\">\n"
                     << "    <name>Wallhaven " << clean_id << "</name>\n"
                     << "    <filename>" << xml_escape(abs_source.string()) << "</filename>\n"
                     << "    <options>zoom</options>\n"
                     << "    <pcolor>#000000</pcolor>\n"
                     << "    <scolor>#000000</scolor>\n"
                     << "  </wallpaper>\n"
                     << "</wallpapers>\n";
            any_success = true;
        }
    } catch (...) {
    }

    // 3. XFCE / Otros: ~/.local/share/xfce4/backdrops/
    try {
        std::filesystem::path xfce_dir = base_share / "xfce4" / "backdrops";
        std::filesystem::create_directories(xfce_dir, ec);
        std::filesystem::path xfce_target = xfce_dir / image_path.filename();
        if (safe_link_or_copy(abs_source, xfce_target)) {
            any_success = true;
        }
    } catch (...) {
    }

    return any_success;
}

std::string WallpaperSetter::get_command_for(const std::filesystem::path& image_path) const {
    return get_command_for(image_path, desktop_env_);
}

std::string WallpaperSetter::get_command_for(
    const std::filesystem::path& image_path,
    DesktopEnvironment de) const {

    std::error_code ec;
    std::filesystem::path abs_path = std::filesystem::absolute(image_path, ec);
    if (ec) {
        abs_path = image_path;
    }

    switch (de) {
        case DesktopEnvironment::KDE_PLASMA: {
            std::string plasma_tool;
            if (is_executable("/usr/bin/plasma-apply-wallpaperimage")) {
                plasma_tool = "/usr/bin/plasma-apply-wallpaperimage";
            } else {
                plasma_tool = find_executable("plasma-apply-wallpaperimage");
            }

            if (!plasma_tool.empty()) {
                return plasma_tool + " " + escape_shell_arg(abs_path.string());
            }

            // Fallback DBus script
            return get_kde_dbus_command(abs_path);
        }

        case DesktopEnvironment::GNOME: {
            std::string gsettings_tool;
            if (is_executable("/usr/bin/gsettings")) {
                gsettings_tool = "/usr/bin/gsettings";
            } else {
                gsettings_tool = find_executable("gsettings");
                if (gsettings_tool.empty()) {
                    gsettings_tool = "gsettings";
                }
            }

            std::string uri = "file://" + abs_path.string();
            return gsettings_tool + " set org.gnome.desktop.background picture-uri " + escape_shell_arg(uri) +
                   " && " + gsettings_tool + " set org.gnome.desktop.background picture-uri-dark " + escape_shell_arg(uri) +
                   " && " + gsettings_tool + " set org.gnome.desktop.background picture-options 'zoom'";
        }

        case DesktopEnvironment::XFCE: {
            std::string xfconf_tool;
            if (is_executable("/usr/bin/xfconf-query")) {
                xfconf_tool = "/usr/bin/xfconf-query";
            } else {
                xfconf_tool = find_executable("xfconf-query");
                if (xfconf_tool.empty()) {
                    xfconf_tool = "xfconf-query";
                }
            }
            return xfconf_tool + " -c xfce4-desktop -p /backdrop/screen0/monitor0/workspace0/last-image -s "
                   + escape_shell_arg(abs_path.string());
        }

        case DesktopEnvironment::HYPRLAND: {
            std::string swww = find_executable("swww");
            if (!swww.empty()) {
                return swww + " img " + escape_shell_arg(abs_path.string());
            }
            std::string hyprctl = find_executable("hyprctl");
            if (!hyprctl.empty()) {
                return hyprctl + " hyprpaper preload " + escape_shell_arg(abs_path.string()) +
                       " && " + hyprctl + " hyprpaper wallpaper " + escape_shell_arg("," + abs_path.string());
            }
            return "swww img " + escape_shell_arg(abs_path.string());
        }

        case DesktopEnvironment::SWAY: {
            std::string swaybg = find_executable("swaybg");
            if (!swaybg.empty()) {
                return swaybg + " -i " + escape_shell_arg(abs_path.string()) + " -m fill";
            }
            std::string swww = find_executable("swww");
            if (!swww.empty()) {
                return swww + " img " + escape_shell_arg(abs_path.string());
            }
            return "swaybg -i " + escape_shell_arg(abs_path.string()) + " -m fill";
        }

        case DesktopEnvironment::X11_GENERIC: {
            std::string feh = find_executable("feh");
            if (!feh.empty()) {
                return feh + " --bg-fill " + escape_shell_arg(abs_path.string());
            }
            std::string nitrogen = find_executable("nitrogen");
            if (!nitrogen.empty()) {
                return nitrogen + " --set-zoom-fill " + escape_shell_arg(abs_path.string());
            }
            return "feh --bg-fill " + escape_shell_arg(abs_path.string());
        }

        case DesktopEnvironment::WINDOWS: {
            return "powershell -Command \"Set-ItemProperty -Path 'HKCU:\\Control Panel\\Desktop' -Name WallPaper -Value '" +
                   abs_path.string() + "'; RUNDLL32.EXE user32.dll,UpdatePerUserSystemParameters\"";
        }

        case DesktopEnvironment::UNKNOWN:
        default: {
            if (is_executable("/usr/bin/plasma-apply-wallpaperimage") || !find_executable("plasma-apply-wallpaperimage").empty()) {
                return get_command_for(abs_path, DesktopEnvironment::KDE_PLASMA);
            }
            if (is_executable("/usr/bin/gsettings") || !find_executable("gsettings").empty()) {
                return get_command_for(abs_path, DesktopEnvironment::GNOME);
            }
            if (!find_executable("feh").empty() || !find_executable("nitrogen").empty()) {
                return get_command_for(abs_path, DesktopEnvironment::X11_GENERIC);
            }
            return "feh --bg-fill " + escape_shell_arg(abs_path.string());
        }
    }
}

int WallpaperSetter::execute_command(const std::string& cmd, std::string& output) const {
    if (command_runner_) {
        return command_runner_(cmd, output);
    }

#if !defined(_WIN32) && !defined(_WIN64)
    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = ::popen(full_cmd.c_str(), "r");
    if (!pipe) {
        output = "popen() failed to run command: " + cmd;
        return -1;
    }

    char buffer[512];
    while (::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    int status = ::pclose(pipe);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return status;
#else
    return ::system(cmd.c_str());
#endif
}

WallpaperSetResult WallpaperSetter::set_wallpaper(const std::filesystem::path& image_path) {
    WallpaperSetResult result;
    result.saved_path = image_path;

    std::error_code ec;
    if (!std::filesystem::exists(image_path, ec)) {
        result.success = false;
        result.message = "File does not exist: " + image_path.string();
        return result;
    }

    if (!std::filesystem::is_regular_file(image_path, ec)) {
        result.success = false;
        result.message = "Path is not a regular file: " + image_path.string();
        return result;
    }

    std::filesystem::path abs_path = std::filesystem::absolute(image_path, ec);
    if (ec) {
        abs_path = image_path;
    }
    result.saved_path = abs_path;

    std::string cmd = get_command_for(abs_path, desktop_env_);
    result.command_used = cmd;

    if (cmd.empty()) {
        result.success = false;
        result.message = "No wallpaper command available for " + to_string(desktop_env_);
        return result;
    }

    if (dry_run_) {
        result.success = true;
        result.message = "Dry-run: wallpaper command generated successfully";
        return result;
    }

    std::string output;
    int exit_code = execute_command(cmd, output);

    // If KDE Plasma failed with plasma-apply-wallpaperimage, try DBus fallback
    if (exit_code != 0 && desktop_env_ == DesktopEnvironment::KDE_PLASMA) {
        if (cmd.find("plasma-apply-wallpaperimage") != std::string::npos) {
            std::string fallback_cmd = get_kde_dbus_command(abs_path);
            if (!fallback_cmd.empty() && fallback_cmd != cmd) {
                std::string fallback_output;
                int fb_code = execute_command(fallback_cmd, fallback_output);
                if (fb_code == 0) {
                    result.success = true;
                    result.command_used = fallback_cmd;
                    result.message = "Wallpaper applied using KDE DBus fallback: " + fallback_cmd;
                    return result;
                }
            }
        }
    }

    result.success = (exit_code == 0);
    if (result.success) {
        result.message = "Wallpaper applied successfully";
    } else {
        while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
            output.pop_back();
        }
        result.message = "Failed to apply wallpaper (exit code " + std::to_string(exit_code) + ")";
        if (!output.empty()) {
            result.message += ": " + output;
        }
    }

    return result;
}

WallpaperSetResult WallpaperSetter::download_and_set(
    const WallpaperItem& item,
    const std::filesystem::path& destination_dir,
    std::function<void(size_t, size_t)> progress_cb) {

    std::filesystem::path target_dir = destination_dir.empty()
        ? Config::instance().wallpaper_dir
        : destination_dir;

    std::error_code ec;
    std::filesystem::create_directories(target_dir, ec);
    if (ec) {
        WallpaperSetResult res;
        res.success = false;
        res.message = "Failed to create destination directory " + target_dir.string() + ": " + ec.message();
        res.saved_path = target_dir;
        return res;
    }

    std::filesystem::path target_file = build_filename(item, target_dir);

    // If file already exists and file_size > 0, avoid re-downloading
    bool already_downloaded = false;
    if (std::filesystem::exists(target_file, ec)) {
        auto sz = std::filesystem::file_size(target_file, ec);
        if (!ec && sz > 0) {
            already_downloaded = true;
        }
    }

    if (!already_downloaded) {
        if (item.full_url.empty()) {
            WallpaperSetResult res;
            res.success = false;
            res.message = "Wallpaper item has no full_url to download";
            res.saved_path = target_file;
            return res;
        }

        bool dl_ok = http_client_.download_file(item.full_url, target_file, progress_cb);
        if (!dl_ok) {
            WallpaperSetResult res;
            res.success = false;
            res.message = "Failed to download wallpaper from " + item.full_url;
            res.saved_path = target_file;
            return res;
        }
    }

    WallpaperSetResult set_res = set_wallpaper(target_file);
    set_res.saved_path = target_file;
    if (already_downloaded && set_res.success) {
        set_res.message = "Wallpaper cached; applied successfully: " + target_file.string();
    }

    // Registrar automáticamente en los selectores de fondos del sistema operativo
    register_in_system_wallpapers(target_file, item);

    return set_res;
}

} // namespace wallpaper
