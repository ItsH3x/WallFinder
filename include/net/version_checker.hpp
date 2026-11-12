#pragma once

#include "net/http_client.hpp"
#include <string>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace wallpaper {

struct VersionInfo {
    std::string current_version{"1.0.0"};
    std::string latest_version;
    std::string release_url;
    std::string release_notes;
    bool update_available{false};
    bool check_completed{false};
    bool check_failed{false};
};

class VersionChecker {
public:
    static VersionChecker& instance() {
        static VersionChecker inst;
        return inst;
    }

    // Compara semver "v1.0.1" o "1.0.1" contra current ("1.0.0")
    // Retorna true si remote > current
    static bool is_newer(const std::string& remote, const std::string& current = "1.0.0") {
        auto clean = [](std::string s) {
            if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) {
                s = s.substr(1);
            }
            return s;
        };

        auto parse_parts = [](const std::string& str) -> std::vector<int> {
            std::vector<int> parts;
            size_t start = 0;
            while (start < str.size()) {
                size_t dot = str.find('.', start);
                std::string token = (dot == std::string::npos) ? str.substr(start) : str.substr(start, dot - start);
                try {
                    parts.push_back(std::stoi(token));
                } catch (...) {
                    parts.push_back(0);
                }
                if (dot == std::string::npos) break;
                start = dot + 1;
            }
            while (parts.size() < 3) parts.push_back(0);
            return parts;
        };

        std::vector<int> r_parts = parse_parts(clean(remote));
        std::vector<int> c_parts = parse_parts(clean(current));

        for (size_t i = 0; i < 3; ++i) {
            if (r_parts[i] > c_parts[i]) return true;
            if (r_parts[i] < c_parts[i]) return false;
        }
        return false;
    }

    // Consulta la API de GitHub para la última versión
    // Por defecto consulta el repositorio oficial / configurado
    static VersionInfo check_latest(const std::string& repo = "hex/wallfinder", 
                                    const std::string& current_version = "1.0.0",
                                    long timeout_secs = 5) {
        VersionInfo info;
        info.current_version = current_version;

        HttpClient client;
        client.set_timeout(timeout_secs);
        client.set_user_agent("WallFinder-App/" + current_version);

        std::string url = "https://api.github.com/repos/" + repo + "/releases/latest";
        std::map<std::string, std::string> headers = {
            {"Accept", "application/vnd.github.v3+json"}
        };

        HttpResponse resp = client.get(url, headers);
        if (!resp.success || resp.status_code != 200) {
            info.check_failed = true;
            info.check_completed = true;
            return info;
        }

        try {
            auto j = nlohmann::json::parse(resp.body);
            std::string tag_name = j.value("tag_name", "");
            if (tag_name.empty()) {
                tag_name = j.value("name", "");
            }
            info.latest_version = tag_name;
            info.release_url = j.value("html_url", "https://github.com/" + repo + "/releases");
            info.release_notes = j.value("body", "");
            info.update_available = is_newer(tag_name, current_version);
            info.check_completed = true;
            info.check_failed = false;
        } catch (...) {
            info.check_failed = true;
            info.check_completed = true;
        }

        return info;
    }
};

} // namespace wallpaper
