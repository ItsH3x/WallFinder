#pragma once

#include "net/wallpaper_provider.hpp"
#include "net/http_client.hpp"

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace wallpaper {

class UnsplashProvider : public IWallpaperProvider {
public:
    static constexpr const char* API_BASE_URL = "https://api.unsplash.com";

    explicit UnsplashProvider(std::shared_ptr<HttpClient> http_client = nullptr);
    ~UnsplashProvider() override = default;

    [[nodiscard]] std::string get_name() const override { return "Unsplash"; }

    [[nodiscard]] std::vector<WallpaperItem> search(const SearchQuery& query) override;
    [[nodiscard]] SearchPage search_page(const SearchQuery& query, CancelPredicate is_cancelled = nullptr) override;

    [[nodiscard]] std::optional<WallpaperItem> get_details(const std::string& id) override;

    void set_access_key(const std::string& key) { access_key_ = key; }
    [[nodiscard]] const std::string& get_access_key() const { return access_key_; }

    // Helpers expuestos para pruebas
    [[nodiscard]] std::string build_search_url(const SearchQuery& query) const;
    [[nodiscard]] static std::string map_color_to_param(const std::string& hex, const std::string& temp);
    [[nodiscard]] std::vector<WallpaperItem> parse_search_json(const std::string& json_str) const;

private:
    std::shared_ptr<HttpClient> http_client_;
    std::string access_key_{"sV_2v7XG5JvK1Wk9iU92m7dYV4-HkX7v4XvU6o6eK6U"}; // Clave pública demo

    [[nodiscard]] WallpaperItem parse_single_photo(const nlohmann::json& photo_json) const;
};

} // namespace wallpaper
