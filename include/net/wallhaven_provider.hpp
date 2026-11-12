#pragma once

#include "net/wallpaper_provider.hpp"
#include "net/http_client.hpp"

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

#include <chrono>
#include <mutex>

namespace wallpaper {

class WallhavenProvider : public IWallpaperProvider {
public:
    static constexpr const char* API_BASE_URL = "https://wallhaven.cc/api/v1";

    explicit WallhavenProvider(std::shared_ptr<HttpClient> http_client = nullptr);
    ~WallhavenProvider() override = default;

    [[nodiscard]] std::string get_name() const override { return "Wallhaven"; }

    [[nodiscard]] std::vector<WallpaperItem> search(const SearchQuery& query) override;
    [[nodiscard]] SearchPage search_page(const SearchQuery& query, CancelPredicate is_cancelled = nullptr) override;

    // Filtro cromático local (público para pruebas): devuelve los fondos que cumplen
    // los criterios de color/temperatura, ordenados por calidad de coincidencia.
    [[nodiscard]] static std::vector<WallpaperItem> apply_color_filters(std::vector<WallpaperItem> items, const SearchQuery& query);
    [[nodiscard]] static bool needs_local_color_filter(const SearchQuery& query);
    [[nodiscard]] std::optional<WallpaperItem> get_details(const std::string& id) override;

    // Helpers exposed for direct testing and modular URL/JSON processing
    [[nodiscard]] std::string build_search_url(const SearchQuery& query) const;
    [[nodiscard]] std::string build_details_url(const std::string& id) const;
    [[nodiscard]] static std::string resolution_to_param(ResolutionFilter filter, const std::string& custom_resolution = "");

    [[nodiscard]] std::vector<WallpaperItem> parse_search_json(const std::string& json_str) const;
    [[nodiscard]] std::optional<WallpaperItem> parse_details_json(const std::string& json_str) const;

    void set_api_key(const std::string& key) { api_key_ = key; }
    [[nodiscard]] const std::string& get_api_key() const { return api_key_; }

    [[nodiscard]] HttpClient& get_http_client() { return *http_client_; }
    [[nodiscard]] const HttpClient& get_http_client() const { return *http_client_; }

private:
    std::shared_ptr<HttpClient> http_client_;
    std::string api_key_;
    mutable std::mutex rate_limit_mutex_;
    std::chrono::steady_clock::time_point rate_limited_until_{};
    std::chrono::steady_clock::time_point last_request_time_{};

    [[nodiscard]] HttpResponse get_throttled(const std::string& url, CancelPredicate is_cancelled = nullptr);
    [[nodiscard]] WallpaperItem parse_single_item(const nlohmann::json& item_json) const;
};

} // namespace wallpaper
