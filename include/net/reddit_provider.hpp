#pragma once

#include "net/wallpaper_provider.hpp"
#include "net/http_client.hpp"

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace wallpaper {

class RedditProvider : public IWallpaperProvider {
public:
    static constexpr const char* API_BASE_URL = "https://www.reddit.com";

    explicit RedditProvider(std::shared_ptr<HttpClient> http_client = nullptr);
    ~RedditProvider() override = default;

    [[nodiscard]] std::string get_name() const override { return "Reddit"; }

    [[nodiscard]] std::vector<WallpaperItem> search(const SearchQuery& query) override;
    [[nodiscard]] SearchPage search_page(const SearchQuery& query, CancelPredicate is_cancelled = nullptr) override;

    [[nodiscard]] std::optional<WallpaperItem> get_details(const std::string& id) override;

    // Helpers expuestos para pruebas unitarias y construcción de URLs
    [[nodiscard]] std::string determine_subreddits(const SearchQuery& query) const;
    [[nodiscard]] std::string build_search_url(const SearchQuery& query, const std::string& after = "") const;
    [[nodiscard]] std::vector<WallpaperItem> parse_listing_json(const std::string& json_str, bool allow_nsfw, std::string* out_after = nullptr) const;
    [[nodiscard]] static std::string unescape_url(std::string url);

private:
    std::shared_ptr<HttpClient> http_client_;
    mutable std::mutex page_tokens_mutex_;
    // Mapeo de query_hash + page -> token 'after' de Reddit
    mutable std::unordered_map<std::string, std::string> page_tokens_;

    [[nodiscard]] WallpaperItem parse_single_post(const nlohmann::json& post_data) const;
};

} // namespace wallpaper
