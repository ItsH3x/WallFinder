#include "net/pexels_provider.hpp"
#include "core/color_math.hpp"

#include <iostream>
#include <algorithm>

namespace wallpaper {

PexelsProvider::PexelsProvider(std::shared_ptr<HttpClient> http_client)
    : http_client_(std::move(http_client)) {
    if (!http_client_) {
        http_client_ = std::make_shared<HttpClient>();
    }
}

std::string PexelsProvider::map_color_to_param(const std::string& hex, const std::string& temp) {
    if (temp == "neutral") return "gray";
    if (hex.empty()) return "";

    auto rgb = color::parse_hex(hex);
    if (!rgb) return "";
    auto lab = color::to_lab(*rgb);
    float c = color::chroma(lab);
    if (c <= 10.0f) {
        return lab.L >= 70.0f ? "white" : "black";
    }

    float h = color::hue_deg(lab);
    if (h >= 25.0f && h < 50.0f) return "red";
    if (h >= 50.0f && h < 78.0f) return "orange";
    if (h >= 78.0f && h < 115.0f) return "yellow";
    if (h >= 115.0f && h < 170.0f) return "green";
    if (h >= 170.0f && h < 225.0f) return "turquoise";
    if (h >= 225.0f && h < 298.0f) return "blue";
    if (h >= 298.0f && h < 335.0f) return "violet";
    return "pink";
}

std::string PexelsProvider::build_search_url(const SearchQuery& query) const {
    std::string url;
    std::vector<std::string> params;

    std::string q = query.keywords;
    if (q.empty()) {
        q = "wallpaper";
    }

    url = std::string(API_BASE_URL) + "/search";
    params.push_back("query=" + HttpClient::url_encode(q));
    params.push_back("page=" + std::to_string(std::max(1, query.page)));
    params.push_back("per_page=25");

    if (query.ratio == "9x16") {
        params.push_back("orientation=portrait");
    } else {
        params.push_back("orientation=landscape");
    }

    std::string col = map_color_to_param(query.color, query.color_temperature);
    if (!col.empty()) {
        params.push_back("color=" + HttpClient::url_encode(col));
    }

    for (size_t i = 0; i < params.size(); ++i) {
        url += (i == 0 ? "?" : "&") + params[i];
    }
    return url;
}

WallpaperItem PexelsProvider::parse_single_photo(const nlohmann::json& j) const {
    WallpaperItem item;
    uint64_t id = j.value("id", static_cast<uint64_t>(0));
    item.id = "pexels_" + std::to_string(id);
    item.source = "pexels";

    item.width = j.value("width", 0);
    item.height = j.value("height", 0);
    if (item.width > 0 && item.height > 0) {
        item.aspect_ratio = std::to_string(item.width) + "x" + std::to_string(item.height);
    }

    if (j.contains("src") && j["src"].is_object()) {
        const auto& src = j["src"];
        item.full_url = src.value("original", src.value("large2x", ""));
        item.preview_url = src.value("medium", src.value("large", item.full_url));
    }

    std::string col = j.value("avg_color", "");
    if (!col.empty()) {
        if (col.front() == '#') col.erase(0, 1);
        item.colors.push_back(col);
    }

    std::string alt = j.value("alt", "");
    if (!alt.empty()) item.tags.push_back(alt);
    std::string photographer = j.value("photographer", "");
    if (!photographer.empty()) item.tags.push_back("Foto por " + photographer);

    return item;
}

std::vector<WallpaperItem> PexelsProvider::parse_search_json(const std::string& json_str) const {
    std::vector<WallpaperItem> items;
    try {
        auto j = nlohmann::json::parse(json_str);
        if (j.contains("photos") && j["photos"].is_array()) {
            for (const auto& photo : j["photos"]) {
                if (photo.is_object()) {
                    items.push_back(parse_single_photo(photo));
                }
            }
        }
    } catch (...) {}
    return items;
}

std::vector<WallpaperItem> PexelsProvider::search(const SearchQuery& query) {
    SearchPage p = search_page(query, nullptr);
    return p.items;
}

SearchPage PexelsProvider::search_page(const SearchQuery& query, CancelPredicate is_cancelled) {
    SearchPage page;
    page.next_page = query.page + 1;

    // Descarte inteligente: si la categoría es exclusivamente Anime, Pexels no aplica
    if (query.categories.size() >= 3 && query.categories[1] == '1' && query.categories[0] == '0') {
        page.has_more = false;
        return page;
    }

    if (is_cancelled && is_cancelled()) {
        page.has_more = false;
        return page;
    }

    std::string url = build_search_url(query);
    std::map<std::string, std::string> headers;
    if (!api_key_.empty()) {
        headers["Authorization"] = api_key_;
    }

    HttpResponse res = http_client_->get(url, headers);
    page.http_status = res.status_code;

    if (!res.success) {
        page.error_message = res.error;
        page.has_more = false;
        return page;
    }

    page.items = parse_search_json(res.body);
    page.has_more = !page.items.empty();
    return page;
}

std::optional<WallpaperItem> PexelsProvider::get_details(const std::string& id) {
    WallpaperItem item;
    item.id = id;
    item.source = "pexels";
    return item;
}

} // namespace wallpaper
