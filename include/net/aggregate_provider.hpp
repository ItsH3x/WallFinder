#pragma once

#include "net/wallpaper_provider.hpp"
#include "net/wallhaven_provider.hpp"
#include "net/reddit_provider.hpp"
#include "net/unsplash_provider.hpp"
#include "net/pexels_provider.hpp"
#include "app/thread_pool.hpp"

#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace wallpaper {

enum class WallpaperSource {
    ALL = 0,
    WALLHAVEN,
    REDDIT,
    UNSPLASH,
    PEXELS
};

inline const char* to_display_name(WallpaperSource src) {
    switch (src) {
        case WallpaperSource::WALLHAVEN: return "Wallhaven";
        case WallpaperSource::REDDIT:    return "Reddit";
        case WallpaperSource::UNSPLASH:  return "Unsplash";
        case WallpaperSource::PEXELS:    return "Pexels";
        case WallpaperSource::ALL:
        default:                         return "Todas las fuentes";
    }
}

class AggregateProvider : public IWallpaperProvider {
public:
    explicit AggregateProvider(std::shared_ptr<ThreadPool> thread_pool = nullptr,
                               std::shared_ptr<HttpClient> http_client = nullptr);
    ~AggregateProvider() override = default;

    [[nodiscard]] std::string get_name() const override;

    [[nodiscard]] std::vector<WallpaperItem> search(const SearchQuery& query) override;
    [[nodiscard]] SearchPage search_page(const SearchQuery& query, CancelPredicate is_cancelled = nullptr) override;

    [[nodiscard]] std::optional<WallpaperItem> get_details(const std::string& id) override;

    void set_source(WallpaperSource source) noexcept { current_source_ = source; }
    [[nodiscard]] WallpaperSource get_source() const noexcept { return current_source_; }

    [[nodiscard]] std::shared_ptr<WallhavenProvider> get_wallhaven() const { return wallhaven_; }
    [[nodiscard]] std::shared_ptr<RedditProvider> get_reddit() const { return reddit_; }
    [[nodiscard]] std::shared_ptr<UnsplashProvider> get_unsplash() const { return unsplash_; }
    [[nodiscard]] std::shared_ptr<PexelsProvider> get_pexels() const { return pexels_; }

    [[nodiscard]] static bool satisfies_resolution(const WallpaperItem& item, ResolutionFilter res, const std::string& custom_res);

private:
    std::shared_ptr<ThreadPool> thread_pool_;
    std::shared_ptr<WallhavenProvider> wallhaven_;
    std::shared_ptr<RedditProvider> reddit_;
    std::shared_ptr<UnsplashProvider> unsplash_;
    std::shared_ptr<PexelsProvider> pexels_;

    WallpaperSource current_source_{WallpaperSource::ALL};
};

} // namespace wallpaper
