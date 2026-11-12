#include "net/aggregate_provider.hpp"
#include <future>
#include <algorithm>
#include <iostream>

namespace wallpaper {

AggregateProvider::AggregateProvider(std::shared_ptr<ThreadPool> thread_pool,
                                     std::shared_ptr<HttpClient> http_client)
    : thread_pool_(std::move(thread_pool)) {
    if (!thread_pool_) {
        thread_pool_ = std::make_shared<ThreadPool>(4);
    }
    if (!http_client) {
        http_client = std::make_shared<HttpClient>();
    }

    wallhaven_ = std::make_shared<WallhavenProvider>(http_client);
    reddit_ = std::make_shared<RedditProvider>(http_client);
    unsplash_ = std::make_shared<UnsplashProvider>(http_client);
    pexels_ = std::make_shared<PexelsProvider>(http_client);
}

std::string AggregateProvider::get_name() const {
    if (current_source_ == WallpaperSource::ALL) {
        return "MultiFuente";
    }
    return to_display_name(current_source_);
}

bool AggregateProvider::satisfies_resolution(const WallpaperItem& item, ResolutionFilter res, const std::string& custom_res) {
    if (res == ResolutionFilter::ANY) return true;
    if (item.width <= 0 || item.height <= 0) return true;

    int min_w = 0, min_h = 0;
    switch (res) {
        case ResolutionFilter::FHD_1080P: min_w = 1920; min_h = 1080; break;
        case ResolutionFilter::QHD_1440P: min_w = 2560; min_h = 1440; break;
        case ResolutionFilter::UHD_4K:    min_w = 3840; min_h = 2160; break;
        case ResolutionFilter::ULTRAWIDE_21_9:
            min_w = 2560; min_h = 1080;
            if (item.height > 0) {
                float aspect = static_cast<float>(item.width) / static_cast<float>(item.height);
                if (aspect < 2.05f) return false;
            }
            break;
        case ResolutionFilter::CUSTOM: {
            auto pos = custom_res.find('x');
            if (pos != std::string::npos) {
                try {
                    min_w = std::stoi(custom_res.substr(0, pos));
                    min_h = std::stoi(custom_res.substr(pos + 1));
                } catch (...) {}
            }
            break;
        }
        default: break;
    }
    return (item.width >= min_w && item.height >= min_h);
}

std::vector<WallpaperItem> AggregateProvider::search(const SearchQuery& query) {
    SearchPage p = search_page(query, nullptr);
    return p.items;
}

SearchPage AggregateProvider::search_page(const SearchQuery& query, CancelPredicate is_cancelled) {
    if (is_cancelled && is_cancelled()) {
        SearchPage empty;
        empty.has_more = false;
        return empty;
    }

    // Si hay una fuente específica seleccionada, delegar directamente
    switch (current_source_) {
        case WallpaperSource::WALLHAVEN:
            return wallhaven_->search_page(query, is_cancelled);
        case WallpaperSource::REDDIT:
            return reddit_->search_page(query, is_cancelled);
        case WallpaperSource::UNSPLASH:
            return unsplash_->search_page(query, is_cancelled);
        case WallpaperSource::PEXELS:
            return pexels_->search_page(query, is_cancelled);
        case WallpaperSource::ALL:
        default:
            break;
    }

    // Modo "Todas las fuentes": búsqueda combinada en paralelo
    SearchPage combined;
    combined.next_page = query.page + 1;

    bool is_anime_exclusive = (query.categories.size() >= 3 && query.categories[1] == '1' && query.categories[0] == '0');

    // Lanzar tareas en el ThreadPool
    auto fut_wh = thread_pool_->enqueue([this, query, is_cancelled]() {
        return wallhaven_->search_page(query, is_cancelled);
    });

    auto fut_rd = thread_pool_->enqueue([this, query, is_cancelled]() {
        return reddit_->search_page(query, is_cancelled);
    });

    std::future<SearchPage> fut_un;
    std::future<SearchPage> fut_px;

    if (!is_anime_exclusive) {
        fut_un = thread_pool_->enqueue([this, query, is_cancelled]() {
            return unsplash_->search_page(query, is_cancelled);
        });
        fut_px = thread_pool_->enqueue([this, query, is_cancelled]() {
            return pexels_->search_page(query, is_cancelled);
        });
    }

    // Recoger resultados
    SearchPage p_wh = fut_wh.get();
    SearchPage p_rd = fut_rd.get();
    SearchPage p_un;
    SearchPage p_px;

    if (!is_anime_exclusive) {
        p_un = fut_un.get();
        p_px = fut_px.get();
    }

    combined.has_more = (p_wh.has_more || p_rd.has_more || p_un.has_more || p_px.has_more);

    // Intercalado equitativo (Round-Robin) para mezcla armónica de fuentes
    std::vector<WallpaperItem>& w_items = p_wh.items;
    std::vector<WallpaperItem>& r_items = p_rd.items;
    std::vector<WallpaperItem>& u_items = p_un.items;
    std::vector<WallpaperItem>& p_items = p_px.items;

    size_t max_items = std::max({w_items.size(), r_items.size(), u_items.size(), p_items.size()});
    std::vector<WallpaperItem> raw_merged;
    raw_merged.reserve(w_items.size() + r_items.size() + u_items.size() + p_items.size());

    for (size_t i = 0; i < max_items; ++i) {
        if (i < w_items.size()) raw_merged.push_back(std::move(w_items[i]));
        if (i < r_items.size()) raw_merged.push_back(std::move(r_items[i]));
        if (i < u_items.size()) raw_merged.push_back(std::move(u_items[i]));
        if (i < p_items.size()) raw_merged.push_back(std::move(p_items[i]));
    }

    // Segunda capa de validación unificada (resolución y filtros de seguridad)
    for (auto& item : raw_merged) {
        // Validación estricta de resolución
        if (!satisfies_resolution(item, query.resolution, query.custom_resolution)) {
            continue;
        }

        // Validación de color en caso de que el ítem tenga metadatos de color
        if (WallhavenProvider::needs_local_color_filter(query) && !item.colors.empty()) {
            auto filtered = WallhavenProvider::apply_color_filters({item}, query);
            if (filtered.empty()) {
                continue; // No cumplió la validación cromática CIELAB
            }
        }

        combined.items.push_back(std::move(item));
    }

    return combined;
}

std::optional<WallpaperItem> AggregateProvider::get_details(const std::string& id) {
    if (id.rfind("reddit_", 0) == 0) {
        return reddit_->get_details(id);
    }
    if (id.rfind("unsplash_", 0) == 0) {
        return unsplash_->get_details(id);
    }
    if (id.rfind("pexels_", 0) == 0) {
        return pexels_->get_details(id);
    }
    return wallhaven_->get_details(id);
}

} // namespace wallpaper
