#include "net/unsplash_provider.hpp"
#include "core/color_math.hpp"

#include <iostream>
#include <algorithm>

namespace wallpaper {

UnsplashProvider::UnsplashProvider(std::shared_ptr<HttpClient> http_client)
    : http_client_(std::move(http_client)) {
    if (!http_client_) {
        http_client_ = std::make_shared<HttpClient>();
    }
}

std::string UnsplashProvider::map_color_to_param(const std::string& hex, const std::string& temp) {
    if (temp == "neutral") return "black_and_white";
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
    if (h >= 170.0f && h < 225.0f) return "teal";
    if (h >= 225.0f && h < 298.0f) return "blue";
    if (h >= 298.0f && h < 335.0f) return "purple";
    return "magenta";
}

std::string UnsplashProvider::build_search_url(const SearchQuery& query) const {
    std::string url;
    std::vector<std::string> params;

    std::string q = query.keywords;
    if (q.empty()) {
        q = "wallpaper";
    }

    url = std::string(API_BASE_URL) + "/search/photos";
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

    if (query.sorting == "date_added") {
        params.push_back("order_by=latest");
    } else {
        params.push_back("order_by=relevant");
    }

    for (size_t i = 0; i < params.size(); ++i) {
        url += (i == 0 ? "?" : "&") + params[i];
    }
    return url;
}

WallpaperItem UnsplashProvider::parse_single_photo(const nlohmann::json& j) const {
    WallpaperItem item;
    std::string id = j.value("id", "");
    item.id = "unsplash_" + id;
    item.source = "unsplash";

    item.width = j.value("width", 0);
    item.height = j.value("height", 0);
    if (item.width > 0 && item.height > 0) {
        item.aspect_ratio = std::to_string(item.width) + "x" + std::to_string(item.height);
    }

    if (j.contains("urls") && j["urls"].is_object()) {
        const auto& urls = j["urls"];
        std::string raw = urls.value("raw", "");
        if (!raw.empty()) {
            item.full_url = raw + "&w=3840&q=85&auto=format";
        } else {
            item.full_url = urls.value("full", "");
        }
        item.preview_url = urls.value("small", urls.value("regular", item.full_url));
    }

    std::string col = j.value("color", "");
    if (!col.empty()) {
        if (col.front() == '#') col.erase(0, 1);
        item.colors.push_back(col);
    }

    std::string alt = j.value("alt_description", "");
    if (!alt.empty()) item.tags.push_back(alt);

    if (j.contains("tags") && j["tags"].is_array()) {
        for (const auto& tag : j["tags"]) {
            if (tag.is_object() && tag.contains("title")) {
                item.tags.push_back(tag.value("title", ""));
            }
        }
    }

    return item;
}

std::vector<WallpaperItem> UnsplashProvider::parse_search_json(const std::string& json_str) const {
    std::vector<WallpaperItem> items;
    try {
        auto j = nlohmann::json::parse(json_str);
        if (j.contains("results") && j["results"].is_array()) {
            for (const auto& photo : j["results"]) {
                if (photo.is_object()) {
                    items.push_back(parse_single_photo(photo));
                }
            }
        }
    } catch (...) {}
    return items;
}

std::vector<WallpaperItem> UnsplashProvider::search(const SearchQuery& query) {
    SearchPage p = search_page(query, nullptr);
    return p.items;
}

SearchPage UnsplashProvider::search_page(const SearchQuery& query, CancelPredicate is_cancelled) {
    SearchPage page;
    page.next_page = query.page + 1;

    // Descarte inteligente: si la búsqueda es exclusivamente Anime, Unsplash no aplica
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
    if (!access_key_.empty()) {
        headers["Authorization"] = "Client-ID " + access_key_;
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

std::optional<WallpaperItem> UnsplashProvider::get_details(const std::string& id) {
    WallpaperItem item;
    item.id = id;
    item.source = "unsplash";
    return item;
}

} // namespace wallpaper
