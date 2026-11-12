#pragma once

#include "net/wallpaper_provider.hpp"
#include "net/http_client.hpp"

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace wallpaper {

class PexelsProvider : public IWallpaperProvider {
public:
    static constexpr const char* API_BASE_URL = "https://api.pexels.com/v1";

    explicit PexelsProvider(std::shared_ptr<HttpClient> http_client = nullptr);
    ~PexelsProvider() override = default;

    [[nodiscard]] std::string get_name() const override { return "Pexels"; }

    [[nodiscard]] std::vector<WallpaperItem> search(const SearchQuery& query) override;
    [[nodiscard]] SearchPage search_page(const SearchQuery& query, CancelPredicate is_cancelled = nullptr) override;

    [[nodiscard]] std::optional<WallpaperItem> get_details(const std::string& id) override;

    void set_api_key(const std::string& key) { api_key_ = key; }
    [[nodiscard]] const std::string& get_api_key() const { return api_key_; }

    // Helpers expuestos para pruebas
    [[nodiscard]] std::string build_search_url(const SearchQuery& query) const;
    [[nodiscard]] static std::string map_color_to_param(const std::string& hex, const std::string& temp);
    [[nodiscard]] std::vector<WallpaperItem> parse_search_json(const std::string& json_str) const;

private:
    std::shared_ptr<HttpClient> http_client_;
    std::string api_key_{"563492ad6f91700001000001e3b5e4c0a5d4447385a49fb59cf3c959"}; // Clave demo pública

    [[nodiscard]] WallpaperItem parse_single_photo(const nlohmann::json& photo_json) const;
};

} // namespace wallpaper
