#pragma once

#include "core/models.hpp"

#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace wallpaper {

using CancelPredicate = std::function<bool()>;

struct SearchQuery {
    std::string keywords;
    ResolutionFilter resolution = ResolutionFilter::ANY;
    std::string custom_resolution;
    std::string ratio;                  // "16x9", "21x9", etc.
    std::string categories = "111";     // General, Anime, People (binary flags)
    std::string purity = "100";         // SFW, Sketchy, NSFW (binary flags)
    std::string sorting = "toplist";    // "toplist", "views", "date_added", "favorites", "relevance"
    std::string color;                  // Hex sin '#' (ej. "cc0000", "0066cc")
    std::string color_secondary;        // Segundo color para combinación bicolor
    bool combine_colors = false;        // true: busca ambos colores en el mismo fondo
    std::string color_temperature;      // "warm", "cool", "neutral", o vacío
    int page = 1;
};

// Resultado paginado: el proveedor puede consumir varias páginas de la API remota
// cuando aplica filtros locales estrictos, por eso informa la siguiente página real.
struct SearchPage {
    std::vector<WallpaperItem> items;
    int next_page = 2;
    bool has_more = true;
    int http_status = 200;
    std::string error_message;
};

class IWallpaperProvider {
public:
    virtual ~IWallpaperProvider() = default;

    [[nodiscard]] virtual std::string get_name() const = 0;
    [[nodiscard]] virtual std::vector<WallpaperItem> search(const SearchQuery& query) = 0;

    [[nodiscard]] virtual SearchPage search_page(const SearchQuery& query, CancelPredicate is_cancelled = nullptr) {
        SearchPage page;
        if (is_cancelled && is_cancelled()) {
            page.has_more = false;
            return page;
        }
        page.items = search(query);
        page.next_page = query.page + 1;
        page.has_more = !page.items.empty();
        return page;
    }
    [[nodiscard]] virtual std::optional<WallpaperItem> get_details(const std::string& id) = 0;
};

} // namespace wallpaper
