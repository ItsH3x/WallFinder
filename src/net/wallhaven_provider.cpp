#include "net/wallhaven_provider.hpp"

#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <thread>

#include "core/color_math.hpp"

namespace wallpaper {

WallhavenProvider::WallhavenProvider(std::shared_ptr<HttpClient> http_client)
    : http_client_(std::move(http_client)) {
    if (!http_client_) {
        http_client_ = std::make_shared<HttpClient>();
    }
}

std::string WallhavenProvider::resolution_to_param(ResolutionFilter filter, const std::string& custom_resolution) {
    switch (filter) {
        case ResolutionFilter::FHD_1080P:
            return "1920x1080";
        case ResolutionFilter::QHD_1440P:
            return "2560x1440";
        case ResolutionFilter::UHD_4K:
            return "3840x2160";
        case ResolutionFilter::ULTRAWIDE_21_9:
            return "2560x1080,3440x1440";
        case ResolutionFilter::CUSTOM:
            return custom_resolution;
        case ResolutionFilter::ANY:
        default:
            return "";
    }
}

std::string WallhavenProvider::build_search_url(const SearchQuery& query) const {
    std::string url = std::string(API_BASE_URL) + "/search";
    std::vector<std::string> params;

    std::string q_str = query.keywords;

    // Parámetro colors=HEX de la API de Wallhaven:
    // Solo se envía cuando el usuario selecciona un color específico (query.color).
    // NO se fuerza un color arbitrario (como ff6600 o 0066cc) para temperatura cromática,
    // ya que Wallhaven exigiría esa coincidencia exacta descartando el 95% del catálogo.
    // La clasificación de temperatura se realiza localmente con el motor perceptual CIELAB.
    if (!query.color.empty()) {
        params.push_back("colors=" + HttpClient::url_encode(query.color));
    }

    if (!q_str.empty()) {
        params.push_back("q=" + HttpClient::url_encode(q_str));
    }
    if (!query.categories.empty()) {
        params.push_back("categories=" + HttpClient::url_encode(query.categories));
    }
    if (!query.purity.empty()) {
        params.push_back("purity=" + HttpClient::url_encode(query.purity));
    }
    if (!query.sorting.empty()) {
        params.push_back("sorting=" + HttpClient::url_encode(query.sorting));
    }

    std::string res_str = resolution_to_param(query.resolution, query.custom_resolution);
    if (!res_str.empty()) {
        params.push_back("resolutions=" + HttpClient::url_encode(res_str));
    }

    if (!query.ratio.empty()) {
        params.push_back("ratios=" + HttpClient::url_encode(query.ratio));
    }

    if (query.page > 0) {
        params.push_back("page=" + std::to_string(query.page));
    }

    for (size_t i = 0; i < params.size(); ++i) {
        url += (i == 0 ? "?" : "&") + params[i];
    }

    return url;
}

std::string WallhavenProvider::build_details_url(const std::string& id) const {
    return std::string(API_BASE_URL) + "/w/" + HttpClient::url_encode(id);
}

WallpaperItem WallhavenProvider::parse_single_item(const nlohmann::json& item_json) const {
    WallpaperItem item;
    item.id = item_json.value("id", "");
    item.full_url = item_json.value("path", "");
    item.aspect_ratio = item_json.value("ratio", "");
    item.file_size = item_json.value("file_size", static_cast<uint64_t>(0));
    item.file_type = item_json.value("file_type", "");
    item.source = "wallhaven";

    // Preview URL from thumbs (prefer large, then small, then original)
    if (item_json.contains("thumbs") && item_json["thumbs"].is_object()) {
        const auto& thumbs = item_json["thumbs"];
        if (thumbs.contains("large") && thumbs["large"].is_string() && !thumbs["large"].get<std::string>().empty()) {
            item.preview_url = thumbs["large"].get<std::string>();
        } else if (thumbs.contains("small") && thumbs["small"].is_string() && !thumbs["small"].get<std::string>().empty()) {
            item.preview_url = thumbs["small"].get<std::string>();
        } else if (thumbs.contains("original") && thumbs["original"].is_string()) {
            item.preview_url = thumbs["original"].get<std::string>();
        }
    }

    // Dimension handling
    item.width = item_json.value("dimension_x", 0);
    item.height = item_json.value("dimension_y", 0);
    if (item.width == 0 || item.height == 0) {
        std::string res_str = item_json.value("resolution", "");
        auto x_pos = res_str.find('x');
        if (x_pos != std::string::npos) {
            try {
                item.width = std::stoi(res_str.substr(0, x_pos));
                item.height = std::stoi(res_str.substr(x_pos + 1));
            } catch (...) {
                // Ignore parse failures
            }
        }
    }

    // Tags extraction
    if (item_json.contains("tags") && item_json["tags"].is_array()) {
        for (const auto& tag_val : item_json["tags"]) {
            if (tag_val.is_object() && tag_val.contains("name") && tag_val["name"].is_string()) {
                item.tags.push_back(tag_val["name"].get<std::string>());
            } else if (tag_val.is_string()) {
                item.tags.push_back(tag_val.get<std::string>());
            }
        }
    }

    // Paleta de colores dominantes de la imagen
    if (item_json.contains("colors") && item_json["colors"].is_array()) {
        for (const auto& c_val : item_json["colors"]) {
            if (c_val.is_string()) {
                item.colors.push_back(c_val.get<std::string>());
            }
        }
    }

    return item;
}

std::vector<WallpaperItem> WallhavenProvider::parse_search_json(const std::string& json_str) const {
    std::vector<WallpaperItem> results;
    try {
        auto j = nlohmann::json::parse(json_str);
        if (j.contains("data") && j["data"].is_array()) {
            results.reserve(j["data"].size());
            for (const auto& item_json : j["data"]) {
                if (item_json.is_object()) {
                    results.push_back(parse_single_item(item_json));
                }
            }
        }
    } catch (const nlohmann::json::exception&) {
        // Return whatever was parsed or empty on failure
    }
    return results;
}

std::optional<WallpaperItem> WallhavenProvider::parse_details_json(const std::string& json_str) const {
    try {
        auto j = nlohmann::json::parse(json_str);
        if (j.contains("data") && j["data"].is_object()) {
            return parse_single_item(j["data"]);
        }
    } catch (const nlohmann::json::exception&) {
        // Return nullopt on failure
    }
    return std::nullopt;
}

bool WallhavenProvider::needs_local_color_filter(const SearchQuery& query) {
    return !query.color.empty() || !query.color_temperature.empty();
}

std::vector<WallpaperItem> WallhavenProvider::apply_color_filters(std::vector<WallpaperItem> items, const SearchQuery& query) {
    if (!needs_local_color_filter(query)) {
        return items;
    }

    struct Scored {
        WallpaperItem item;
        float score;
        Scored(WallpaperItem it, float s) : item(std::move(it)), score(s) {}
    };
    std::vector<Scored> kept;
    kept.reserve(items.size());

    const bool dual = query.combine_colors && !query.color.empty() && !query.color_secondary.empty();

    for (auto& it : items) {
        if (dual) {
            auto m = color::evaluate_dual(it.colors, query.color, query.color_secondary);
            if (m.accepted) kept.push_back({std::move(it), m.score});
        } else if (!query.color.empty()) {
            auto m = color::evaluate_single(it.colors, query.color);
            if (m.accepted) kept.push_back({std::move(it), m.score});
        } else if (query.color_temperature == "neutral") {
            if (color::is_strict_monochrome(it.colors)) kept.push_back({std::move(it), 0.0f});
        } else if (query.color_temperature == "warm") {
            if (color::matches_temperature(it.colors, color::Temperature::Warm)) kept.push_back({std::move(it), 0.0f});
        } else if (query.color_temperature == "cool") {
            if (color::matches_temperature(it.colors, color::Temperature::Cool)) kept.push_back({std::move(it), 0.0f});
        } else {
            kept.push_back({std::move(it), 0.0f});
        }
    }

    // RED DE SEGURIDAD CRÍTICA:
    // Si los filtros locales descartaron todos los elementos (ej. combinaciones muy restrictivas),
    // NUNCA devolver una lista vacía si la API devolvió fondos válidos. Conservamos los fondos originales
    // para que el usuario siempre tenga contenido disponible para explorar y aplicar.
    if (kept.empty() && !items.empty()) {
        return items;
    }

    // Orden estable por calidad: las coincidencias más fieles primero (menor score perceptual)
    std::stable_sort(kept.begin(), kept.end(), [](const Scored& a, const Scored& b) {
        return a.score < b.score;
    });

    std::vector<WallpaperItem> out;
    out.reserve(kept.size());
    for (auto& k : kept) out.push_back(std::move(k.item));
    return out;
}

HttpResponse WallhavenProvider::get_throttled(const std::string& url, CancelPredicate is_cancelled) {
    if (is_cancelled && is_cancelled()) {
        HttpResponse cancelled_resp;
        cancelled_resp.error = "Búsqueda cancelada";
        return cancelled_resp;
    }

    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(rate_limit_mutex_);
        if (now < rate_limited_until_) {
            HttpResponse resp;
            resp.status_code = 429;
            resp.error = "Límite de peticiones de Wallhaven activo. Espera unos segundos.";
            resp.success = false;
            return resp;
        }
    }

    // Asegurar espaciado mínimo de 250ms entre peticiones a Wallhaven para evitar ráfagas
    {
        std::unique_lock<std::mutex> lock(rate_limit_mutex_);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_time_).count();
        if (elapsed < 250) {
            long sleep_ms = 250 - elapsed;
            lock.unlock();
            for (long s = 0; s < sleep_ms; s += 25) {
                if (is_cancelled && is_cancelled()) {
                    HttpResponse cancelled_resp;
                    cancelled_resp.error = "Búsqueda cancelada";
                    return cancelled_resp;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(std::min(25L, sleep_ms - s)));
            }
        }
    }

    if (is_cancelled && is_cancelled()) {
        HttpResponse cancelled_resp;
        cancelled_resp.error = "Búsqueda cancelada";
        return cancelled_resp;
    }

    HttpResponse resp = http_client_->get(url);

    {
        std::lock_guard<std::mutex> lock(rate_limit_mutex_);
        last_request_time_ = std::chrono::steady_clock::now();
        if (resp.status_code == 429) {
            // Cooldown de 15 segundos para no empeorar el bloqueo temporal de Wallhaven
            rate_limited_until_ = last_request_time_ + std::chrono::seconds(15);
        }
    }

    return resp;
}

SearchPage WallhavenProvider::search_page(const SearchQuery& query, CancelPredicate is_cancelled) {
    SearchPage result;

    if (is_cancelled && is_cancelled()) {
        result.has_more = false;
        return result;
    }

    // La API solo admite un color por consulta: se pide el Color 1 (que debe ser el
    // dominante) y la validación del Color 2 / colores ajenos se hace localmente.
    SearchQuery api_query = query;
    api_query.combine_colors = false;
    api_query.color_secondary.clear();

    const bool strict = needs_local_color_filter(query);
    // Limitar a máximo 2 páginas API remotas (1 si no hay filtro de color estricto)
    // para no agotar la cuota de 45 peticiones por minuto de Wallhaven.
    const int max_api_pages = strict ? 2 : 1;
    const size_t min_results = strict ? 8 : 1;
    constexpr size_t kApiPageSize = 24;

    int page = std::max(1, query.page);
    int fetched = 0;
    result.has_more = true;

    while (fetched < max_api_pages) {
        if (is_cancelled && is_cancelled()) {
            result.has_more = false;
            break;
        }

        api_query.page = page;
        HttpResponse resp = get_throttled(build_search_url(api_query), is_cancelled);

        if (is_cancelled && is_cancelled()) {
            result.has_more = false;
            break;
        }

        if (resp.status_code == 429) {
            result.http_status = 429;
            result.error_message = "Límite de peticiones de Wallhaven (429). Espera unos segundos.";
            result.has_more = false;
            break;
        }

        if (!resp.success || resp.body.empty()) {
            result.http_status = resp.status_code;
            result.error_message = resp.error;
            result.has_more = !result.items.empty();
            break;
        }

        auto raw = parse_search_json(resp.body);
        int last_page = 0;
        try {
            auto j = nlohmann::json::parse(resp.body);
            if (j.contains("meta") && j["meta"].is_object()) {
                last_page = j["meta"].value("last_page", 0);
            }
        } catch (...) {
        }

        ++page;
        ++fetched;
        size_t raw_count = raw.size();

        auto filtered = apply_color_filters(std::move(raw), query);
        for (auto& it : filtered) result.items.push_back(std::move(it));

        if (raw_count < kApiPageSize || (last_page > 0 && page > last_page)) {
            result.has_more = false;
            break;
        }
        if (result.items.size() >= min_results) {
            break;
        }
    }

    result.next_page = page;
    return result;
}

std::vector<WallpaperItem> WallhavenProvider::search(const SearchQuery& query) {
    return search_page(query).items;
}

std::optional<WallpaperItem> WallhavenProvider::get_details(const std::string& id) {
    if (id.empty()) {
        return std::nullopt;
    }
    std::string url = build_details_url(id);
    HttpResponse resp = get_throttled(url);
    if (!resp.success || resp.body.empty()) {
        return std::nullopt;
    }
    return parse_details_json(resp.body);
}

} // namespace wallpaper
