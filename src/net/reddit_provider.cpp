#include "net/reddit_provider.hpp"
#include <iostream>
#include <regex>
#include <algorithm>

namespace wallpaper {

namespace {

std::string clean_amp(std::string s) {
    const std::string from = "&amp;";
    const std::string to = "&";
    size_t start_pos = 0;
    while ((start_pos = s.find(from, start_pos)) != std::string::npos) {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return s;
}

} // namespace

RedditProvider::RedditProvider(std::shared_ptr<HttpClient> http_client)
    : http_client_(std::move(http_client)) {
    if (!http_client_) {
        http_client_ = std::make_shared<HttpClient>();
    }
    http_client_->set_user_agent("WallFinder/1.0 (Linux; Desktop Wallpaper Manager)");
}

std::string RedditProvider::unescape_url(std::string url) {
    return clean_amp(std::move(url));
}

std::string RedditProvider::determine_subreddits(const SearchQuery& query) const {
    // Si la categoría específica es Anime
    if (query.categories.size() >= 3 && query.categories[1] == '1' && query.categories[0] == '0') {
        return query.allow_nsfw ? "Animewallpaper+ecchi" : "Animewallpaper";
    }

    std::string kw = query.keywords;
    std::transform(kw.begin(), kw.end(), kw.begin(), [](unsigned char c) { return std::tolower(c); });

    if (kw.find("anime") != std::string::npos) {
        return query.allow_nsfw ? "Animewallpaper+ecchi" : "Animewallpaper";
    }
    if (kw.find("space") != std::string::npos || kw.find("espacio") != std::string::npos) {
        return "spaceporn";
    }
    if (kw.find("landscape") != std::string::npos || kw.find("nature") != std::string::npos || kw.find("paisaje") != std::string::npos) {
        return "EarthPorn+IslandPorn+wallpaper";
    }
    if (kw.find("cyberpunk") != std::string::npos) {
        return "Cyberpunk+outrun";
    }
    if (kw.find("gaming") != std::string::npos || kw.find("juego") != std::string::npos) {
        return "gamingwallpapers";
    }
    if (kw.find("fantasy") != std::string::npos || kw.find("fantasia") != std::string::npos) {
        return "ImaginaryLandscapes";
    }
    if (kw.find("minimal") != std::string::npos) {
        return "MinimalWallpaper";
    }
    if (kw.find("digital art") != std::string::npos) {
        return "DigitalArt+ImaginaryTechnology";
    }
    if (kw.find("car") != std::string::npos || kw.find("vehicle") != std::string::npos) {
        return "carporn";
    }
    if (kw.find("animal") != std::string::npos) {
        return "AnimalPorn";
    }
    if (kw.find("architecture") != std::string::npos || kw.find("arquitectura") != std::string::npos) {
        return "CityPorn+ArchitecturePorn";
    }
    if (kw.find("people") != std::string::npos || kw.find("persona") != std::string::npos) {
        return "HumanPorn";
    }

    if (query.allow_nsfw) {
        return "wallpapers+wallpaper+WQHD_Wallpaper+NSFWWallpapers";
    }
    return "wallpapers+wallpaper+WQHD_Wallpaper";
}

std::string RedditProvider::build_search_url(const SearchQuery& query, const std::string& after) const {
    std::string subs = determine_subreddits(query);
    std::string url;
    std::string nsfw_flag = query.allow_nsfw ? "on" : "off";

    if (!query.keywords.empty()) {
        url = std::string(API_BASE_URL) + "/r/" + subs + "/search.json?q=" + HttpClient::url_encode(query.keywords) +
              "&restrict_sr=on&limit=30&raw_json=1&include_over_18=" + nsfw_flag;
        if (query.sorting == "date_added") {
            url += "&sort=new";
        } else if (query.sorting == "views" || query.sorting == "favorites") {
            url += "&sort=top&t=all";
        } else if (query.sorting == "toplist") {
            url += "&sort=top&t=month";
        } else {
            url += "&sort=relevance";
        }
    } else {
        std::string section = "hot.json";
        std::string extra;
        if (query.sorting == "date_added") {
            section = "new.json";
        } else if (query.sorting == "views" || query.sorting == "favorites") {
            section = "top.json";
            extra = "&t=all";
        } else if (query.sorting == "toplist") {
            section = "top.json";
            extra = "&t=month";
        }
        url = std::string(API_BASE_URL) + "/r/" + subs + "/" + section + "?limit=30&raw_json=1&include_over_18=" + nsfw_flag + extra;
    }

    if (!after.empty()) {
        url += "&after=" + HttpClient::url_encode(after);
    }

    return url;
}

WallpaperItem RedditProvider::parse_single_post(const nlohmann::json& pdata) const {
    WallpaperItem item;
    std::string id = pdata.value("id", "");
    item.id = "reddit_" + id;
    item.source = "reddit";

    std::string full_url = clean_amp(pdata.value("url", ""));
    item.full_url = full_url;
    item.preview_url = full_url;

    // Extracción de dimensiones y previsualización de preview.images
    if (pdata.contains("preview") && pdata["preview"].is_object() &&
        pdata["preview"].contains("images") && pdata["preview"]["images"].is_array() &&
        !pdata["preview"]["images"].empty()) {
        
        const auto& img0 = pdata["preview"]["images"][0];
        if (img0.contains("source") && img0["source"].is_object()) {
            item.width = img0["source"].value("width", 0);
            item.height = img0["source"].value("height", 0);
            std::string src_url = clean_amp(img0["source"].value("url", ""));
            if (!src_url.empty() && item.full_url.empty()) {
                item.full_url = src_url;
            }
        }

        // Selección de miniatura adecuada para preview (ancho ~ 320-640)
        if (img0.contains("resolutions") && img0["resolutions"].is_array() && !img0["resolutions"].empty()) {
            const auto& res_list = img0["resolutions"];
            size_t chosen_idx = 0;
            for (size_t i = 0; i < res_list.size(); ++i) {
                int w = res_list[i].value("width", 0);
                if (w >= 300) {
                    chosen_idx = i;
                    if (w <= 800) break;
                }
            }
            std::string thumb = clean_amp(res_list[chosen_idx].value("url", ""));
            if (!thumb.empty()) {
                item.preview_url = thumb;
            }
        }
    }

    // Fallback de dimensiones parseando el título (ej. [3840x2160])
    std::string title = pdata.value("title", "");
    if (item.width == 0 || item.height == 0) {
        static const std::regex res_re(R"(\b([1-9][0-9]{2,4})\s*[xX×]\s*([1-9][0-9]{2,4})\b)");
        std::smatch m;
        if (std::regex_search(title, m, res_re) && m.size() >= 3) {
            try {
                item.width = std::stoi(m[1].str());
                item.height = std::stoi(m[2].str());
            } catch (...) {}
        }
    }

    if (item.width > 0 && item.height > 0) {
        item.aspect_ratio = std::to_string(item.width) + "x" + std::to_string(item.height);
    }

    // Tags
    std::string sub = pdata.value("subreddit", "");
    if (!sub.empty()) item.tags.push_back("r/" + sub);
    std::string author = pdata.value("author", "");
    if (!author.empty()) item.tags.push_back("u/" + author);
    if (!title.empty()) item.tags.push_back(title);

    return item;
}

std::vector<WallpaperItem> RedditProvider::parse_listing_json(const std::string& json_str, bool allow_nsfw, std::string* out_after) const {
    std::vector<WallpaperItem> items;
    try {
        auto j = nlohmann::json::parse(json_str);
        if (!j.contains("data") || !j["data"].is_object()) return items;

        const auto& data = j["data"];
        if (out_after) {
            *out_after = data.value("after", "");
        }

        if (data.contains("children") && data["children"].is_array()) {
            for (const auto& child : data["children"]) {
                if (!child.contains("data") || !child["data"].is_object()) continue;
                const auto& pdata = child["data"];

                bool over_18 = pdata.value("over_18", false);
                if (over_18 && !allow_nsfw) {
                    continue; // Descarte de seguridad si NSFW no está permitido
                }

                // Filtrar solo publicaciones con imágenes
                std::string url = pdata.value("url", "");
                std::string post_hint = pdata.value("post_hint", "");
                bool has_preview = pdata.contains("preview") && pdata["preview"].is_object();

                auto ends_with_img = [](const std::string& u) {
                    std::string lower = u;
                    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
                    return lower.ends_with(".jpg") || lower.ends_with(".jpeg") ||
                           lower.ends_with(".png") || lower.ends_with(".webp");
                };

                bool is_image = (post_hint == "image") || has_preview || ends_with_img(url);
                if (!is_image) continue;

                items.push_back(parse_single_post(pdata));
            }
        }
    } catch (...) {}
    return items;
}

std::vector<WallpaperItem> RedditProvider::search(const SearchQuery& query) {
    SearchPage p = search_page(query, nullptr);
    return p.items;
}

SearchPage RedditProvider::search_page(const SearchQuery& query, CancelPredicate is_cancelled) {
    SearchPage result;
    result.next_page = query.page + 1;

    if (is_cancelled && is_cancelled()) {
        result.has_more = false;
        return result;
    }

    std::string cache_key = query.keywords + "_" + query.categories + "_" + query.sorting;
    std::string after_token;
    {
        std::lock_guard<std::mutex> lock(page_tokens_mutex_);
        if (query.page > 1) {
            std::string prev_key = cache_key + "_p" + std::to_string(query.page - 1);
            auto it = page_tokens_.find(prev_key);
            if (it != page_tokens_.end()) {
                after_token = it->second;
            }
        }
    }

    std::string url = build_search_url(query, after_token);
    HttpResponse res = http_client_->get(url);
    result.http_status = res.status_code;

    if (!res.success) {
        result.error_message = res.error;
        result.has_more = false;
        return result;
    }

    std::string next_after;
    result.items = parse_listing_json(res.body, query.allow_nsfw, &next_after);
    result.has_more = !next_after.empty() && !result.items.empty();

    if (!next_after.empty()) {
        std::lock_guard<std::mutex> lock(page_tokens_mutex_);
        page_tokens_[cache_key + "_p" + std::to_string(query.page)] = next_after;
    }

    return result;
}

std::optional<WallpaperItem> RedditProvider::get_details(const std::string& id) {
    // Para Reddit los metadatos completos ya están presentes en el ítem
    WallpaperItem item;
    item.id = id;
    item.source = "reddit";
    return item;
}

} // namespace wallpaper
