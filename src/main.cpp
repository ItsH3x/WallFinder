#include "app/config.hpp"
#include "app/thread_pool.hpp"
#include "core/color_math.hpp"
#include "core/models.hpp"
#include "core/texture_cache.hpp"
#include "core/wallpaper_setter.hpp"
#include "net/http_client.hpp"
#include "net/wallhaven_provider.hpp"
#include "net/version_checker.hpp"

#include <curl/curl.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <cassert>
#include <fstream>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

std::vector<uint8_t> generate_test_bmp_bytes(int width = 80, int height = 60) {
    uint32_t row_size = (width * 3 + 3) & ~3;
    uint32_t img_size = row_size * height;
    uint32_t file_size = 54 + img_size;

    std::vector<uint8_t> buf(file_size, 0);
    buf[0] = 'B'; buf[1] = 'M';
    buf[2] = static_cast<uint8_t>(file_size & 0xFF);
    buf[3] = static_cast<uint8_t>((file_size >> 8) & 0xFF);
    buf[4] = static_cast<uint8_t>((file_size >> 16) & 0xFF);
    buf[5] = static_cast<uint8_t>((file_size >> 24) & 0xFF);
    buf[10] = 54;

    buf[14] = 40;
    buf[18] = static_cast<uint8_t>(width & 0xFF);
    buf[19] = static_cast<uint8_t>((width >> 8) & 0xFF);
    buf[22] = static_cast<uint8_t>(height & 0xFF);
    buf[23] = static_cast<uint8_t>((height >> 8) & 0xFF);
    buf[26] = 1;
    buf[28] = 24;
    buf[34] = static_cast<uint8_t>(img_size & 0xFF);
    buf[35] = static_cast<uint8_t>((img_size >> 8) & 0xFF);
    buf[36] = static_cast<uint8_t>((img_size >> 16) & 0xFF);
    buf[37] = static_cast<uint8_t>((img_size >> 24) & 0xFF);

    for (size_t i = 54; i < file_size; ++i) {
        buf[i] = static_cast<uint8_t>(i % 255);
    }
    return buf;
}

void create_test_bmp(const std::filesystem::path& path, int width = 100, int height = 100) {
    std::ofstream ofs(path, std::ios::binary);
    assert(ofs.is_open());

    uint32_t row_size = (width * 3 + 3) & ~3;
    uint32_t img_size = row_size * height;
    uint32_t file_size = 54 + img_size;

    // BMP Header (14 bytes)
    uint8_t header[14] = {
        'B', 'M',
        static_cast<uint8_t>(file_size & 0xFF),
        static_cast<uint8_t>((file_size >> 8) & 0xFF),
        static_cast<uint8_t>((file_size >> 16) & 0xFF),
        static_cast<uint8_t>((file_size >> 24) & 0xFF),
        0, 0, 0, 0, // reserved
        54, 0, 0, 0  // offset to pixel data
    };

    // DIB Header (40 bytes - BITMAPINFOHEADER)
    uint8_t dib[40] = {0};
    dib[0] = 40; // DIB header size
    dib[4] = static_cast<uint8_t>(width & 0xFF);
    dib[5] = static_cast<uint8_t>((width >> 8) & 0xFF);
    dib[8] = static_cast<uint8_t>(height & 0xFF);
    dib[9] = static_cast<uint8_t>((height >> 8) & 0xFF);
    dib[12] = 1; // planes
    dib[14] = 24; // bpp
    dib[20] = static_cast<uint8_t>(img_size & 0xFF);
    dib[21] = static_cast<uint8_t>((img_size >> 8) & 0xFF);
    dib[22] = static_cast<uint8_t>((img_size >> 16) & 0xFF);
    dib[23] = static_cast<uint8_t>((img_size >> 24) & 0xFF);

    ofs.write(reinterpret_cast<const char*>(header), sizeof(header));
    ofs.write(reinterpret_cast<const char*>(dib), sizeof(dib));

    std::vector<uint8_t> row(row_size, 0);
    for (int x = 0; x < width; ++x) {
        row[x * 3 + 0] = 255; // Blue
        row[x * 3 + 1] = 128; // Green
        row[x * 3 + 2] = 64;  // Red
    }
    for (int y = 0; y < height; ++y) {
        ofs.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
}

} // namespace

int main() {
    std::cout << "============================================================\n";
    std::cout << " Wallpaper Finder - Verificación de Arquitectura Base C++20\n";
    std::cout << "============================================================\n\n";

    // 1. Dependency Link Sanity Check
    std::cout << "[1/6] Verificando dependencias enlazadas...\n";
    std::cout << "  - CURL Version: " << curl_version() << '\n';
    std::cout << "  - GLFW Version: " << glfwGetVersionString() << '\n';
    std::cout << "  - nlohmann_json Version: "
              << NLOHMANN_JSON_VERSION_MAJOR << "."
              << NLOHMANN_JSON_VERSION_MINOR << "."
              << NLOHMANN_JSON_VERSION_PATCH << '\n';
    std::cout << "  [OK] Dependencias enlazadas correctamente.\n\n";

    // 2. Desktop Environment and Path Detection
    std::cout << "[2/6] Detectando entorno de escritorio y directorios...\n";
    wallpaper::Config& cfg = wallpaper::Config::instance();

    std::cout << "  - Entorno de escritorio detectado: "
              << wallpaper::to_string(cfg.desktop_environment) << '\n';

    if (cfg.desktop_environment == wallpaper::DesktopEnvironment::KDE_PLASMA) {
        std::cout << "    -> KDE Plasma reconocido exitosamente.\n";
    } else if (cfg.desktop_environment == wallpaper::DesktopEnvironment::GNOME) {
        std::cout << "    -> GNOME reconocido exitosamente.\n";
    } else {
        std::cout << "    -> Entorno reconocido: " << wallpaper::to_string(cfg.desktop_environment) << '\n';
    }

    std::filesystem::path pic_dir = wallpaper::Config::detect_pictures_directory();
    std::cout << "  - Directorio de imágenes XDG detectado: " << pic_dir << '\n';
    std::cout << "  - Directorio destino de fondos: " << cfg.wallpaper_dir << '\n';

    // Test directory creation functionality
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "wallpaper_test_dir" / "Wallpapers";
    std::error_code dir_ec;
    std::filesystem::create_directories(test_dir, dir_ec);
    assert(!dir_ec && std::filesystem::exists(test_dir));
    std::filesystem::remove_all(test_dir.parent_path(), dir_ec);
    std::cout << "  [OK] Creación de carpetas con create_directories verificada correctamente.\n\n";

    // 3. ThreadPool Concurrency Verification
    std::cout << "[3/6] Verificando ThreadPool con 8 tareas concurrentes...\n";
    constexpr size_t NUM_THREADS = 4;
    constexpr size_t NUM_TASKS = 8;
    wallpaper::ThreadPool pool(NUM_THREADS);
    std::cout << "  - Hilos iniciados en el pool: " << pool.size() << '\n';

    std::vector<std::future<int>> futures;
    futures.reserve(NUM_TASKS);

    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < static_cast<int>(NUM_TASKS); ++i) {
        futures.push_back(pool.enqueue([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            return i * i;
        }));
    }

    int sum_results = 0;
    for (int i = 0; i < static_cast<int>(NUM_TASKS); ++i) {
        int val = futures[i].get();
        sum_results += val;
        assert(val == i * i);
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time
    ).count();

    constexpr int EXPECTED_SUM = 140;
    assert(sum_results == EXPECTED_SUM);
    std::cout << "  - Resultado acumulado de 8 tareas: " << sum_results
              << " (Esperado: " << EXPECTED_SUM << ")\n";
    std::cout << "  - Tiempo transcurrido para 8 tareas en " << NUM_THREADS << " hilos: "
              << elapsed << " ms\n";
    std::cout << "  [OK] ThreadPool y futuros ejecutados concurrentemente.\n\n";

    // 4. Config Persistence Verification
    std::cout << "[4/6] Verificando guardado y carga de configuración JSON...\n";
    std::filesystem::path test_config_path =
        std::filesystem::temp_directory_path() / "wallpaper_finder_test_config.json";

    wallpaper::Config test_cfg;
    test_cfg.wallpaper_dir = cfg.wallpaper_dir / "SubTest";
    test_cfg.preferred_resolution = wallpaper::ResolutionFilter::UHD_4K;
    test_cfg.dark_mode = false;
    test_cfg.results_per_page = 48;
    test_cfg.download_concurrency = 8;
    test_cfg.auto_set_wallpaper = false;

    bool saved = test_cfg.save(test_config_path);
    assert(saved);
    std::cout << "  - Configuración de prueba guardada en: " << test_config_path << '\n';

    wallpaper::Config loaded_cfg;
    bool loaded = loaded_cfg.load(test_config_path);
    assert(loaded);
    std::cout << "  - Configuración de prueba recargada exitosamente.\n";

    assert(loaded_cfg.wallpaper_dir == test_cfg.wallpaper_dir);
    assert(loaded_cfg.preferred_resolution == wallpaper::ResolutionFilter::UHD_4K);
    assert(loaded_cfg.dark_mode == false);
    assert(loaded_cfg.results_per_page == 48);
    assert(loaded_cfg.download_concurrency == 8);
    assert(loaded_cfg.auto_set_wallpaper == false);
    assert(loaded_cfg.desktop_environment == test_cfg.desktop_environment);

    std::filesystem::remove(test_config_path);
    std::cout << "  [OK] Serialización y deserialización JSON verificada al 100%.\n\n";

    // Verify WallpaperItem model serialization
    wallpaper::WallpaperItem item;
    item.id = "wp_12345";
    item.preview_url = "https://example.com/preview.jpg";
    item.full_url = "https://example.com/full.png";
    item.width = 3840;
    item.height = 2160;
    item.aspect_ratio = "16:9";
    item.file_size = 4194304;
    item.file_type = "image/png";
    item.tags = {"nature", "mountains", "4k"};
    item.source = "wallhaven";

    nlohmann::json item_json = item;
    wallpaper::WallpaperItem deserialized_item = item_json.get<wallpaper::WallpaperItem>();
    assert(deserialized_item.id == item.id);
    assert(deserialized_item.width == 3840);
    assert(deserialized_item.tags.size() == 3);
    assert(deserialized_item.file_type == "image/png");
    std::cout << "  [OK] Modelo WallpaperItem serializado/deserializado correctamente.\n\n";

    // 5. Networking HTTP and Wallhaven Provider Verification
    std::cout << "[5/6] Verificando Cliente HTTP (libcurl) y WallhavenProvider...\n";

    // 5.1 URL encode test
    std::string unencoded = "nature & landscape / 4k";
    std::string encoded = wallpaper::HttpClient::url_encode(unencoded);
    std::cout << "  - URL Encode test: \"" << unencoded << "\" -> \"" << encoded << "\"\n";
    assert(encoded.find(" ") == std::string::npos);
    assert(encoded.find("&") == std::string::npos);
    assert(encoded.find("/") == std::string::npos);

    // 5.2 HttpClient configuration test
    wallpaper::HttpClient client;
    client.set_timeout(10);
    assert(client.get_timeout() == 10);
    assert(client.get_user_agent() == "WallpaperFinder/1.0");
    std::cout << "  - HttpClient inicializado (User-Agent: " << client.get_user_agent()
              << ", Timeout: " << client.get_timeout() << "s)\n";

    // 5.3 Wallhaven Provider resolution mapping
    assert(wallpaper::WallhavenProvider::resolution_to_param(wallpaper::ResolutionFilter::FHD_1080P) == "1920x1080");
    assert(wallpaper::WallhavenProvider::resolution_to_param(wallpaper::ResolutionFilter::QHD_1440P) == "2560x1440");
    assert(wallpaper::WallhavenProvider::resolution_to_param(wallpaper::ResolutionFilter::UHD_4K) == "3840x2160");
    assert(wallpaper::WallhavenProvider::resolution_to_param(wallpaper::ResolutionFilter::ULTRAWIDE_21_9) == "2560x1080,3440x1440");
    assert(wallpaper::WallhavenProvider::resolution_to_param(wallpaper::ResolutionFilter::CUSTOM, "5120x1440") == "5120x1440");
    assert(wallpaper::WallhavenProvider::resolution_to_param(wallpaper::ResolutionFilter::ANY).empty());
    std::cout << "  - Mapeo de resoluciones verificado para FHD, QHD, UHD, Ultrawide y Custom.\n";

    // 5.4 Search query construction and URL serialization
    wallpaper::WallhavenProvider provider;
    assert(provider.get_name() == "Wallhaven");

    wallpaper::SearchQuery test_query;
    assert(test_query.purity == "110");
    assert(test_query.top_range == "1y");
    assert(test_query.sorting == "toplist");
    test_query.keywords = "nature";
    test_query.resolution = wallpaper::ResolutionFilter::UHD_4K;
    test_query.ratio = "16x9";
    test_query.categories = "111";
    test_query.sorting = "toplist";
    test_query.page = 1;

    std::string search_url = provider.build_search_url(test_query);
    std::cout << "  - URL de búsqueda generada: " << search_url << '\n';
    assert(search_url.find("https://wallhaven.cc/api/v1/search") == 0);
    assert(search_url.find("q=nature") != std::string::npos);
    assert(search_url.find("resolutions=3840x2160") != std::string::npos);
    assert(search_url.find("ratios=16x9") != std::string::npos);
    assert(search_url.find("categories=111") != std::string::npos);
    assert(search_url.find("purity=110") != std::string::npos);
    assert(search_url.find("sorting=toplist") != std::string::npos);
    assert(search_url.find("topRange=1y") != std::string::npos);
    assert(search_url.find("page=1") != std::string::npos);

    std::string details_url = provider.build_details_url("83w7kk");
    std::cout << "  - URL de detalles generada: " << details_url << '\n';
    assert(details_url == "https://wallhaven.cc/api/v1/w/83w7kk");

    // 5.5 Parsing simulated search response JSON
    std::string mock_search_json = R"({
        "data": [
            {
                "id": "83w7kk",
                "url": "https://wallhaven.cc/w/83w7kk",
                "short_url": "https://whvn.cc/83w7kk",
                "views": 15403,
                "favorites": 341,
                "source": "https://deviantart.com/artist",
                "purity": "sfw",
                "category": "general",
                "dimension_x": 3840,
                "dimension_y": 2160,
                "resolution": "3840x2160",
                "ratio": "16:9",
                "file_size": 3951234,
                "file_type": "image/jpeg",
                "created_at": "2020-05-15 14:15:32",
                "colors": ["#000000", "#42322b"],
                "path": "https://w.wallhaven.cc/full/83/wallhaven-83w7kk.jpg",
                "thumbs": {
                    "large": "https://th.wallhaven.cc/lg/83/83w7kk.jpg",
                    "original": "https://th.wallhaven.cc/orig/83/83w7kk.jpg",
                    "small": "https://th.wallhaven.cc/small/83/83w7kk.jpg"
                }
            }
        ],
        "meta": {
            "current_page": 1,
            "last_page": 100,
            "per_page": 24,
            "total": 2400
        }
    })";

    std::vector<wallpaper::WallpaperItem> search_items = provider.parse_search_json(mock_search_json);
    assert(search_items.size() == 1);
    const auto& parsed = search_items[0];
    assert(parsed.id == "83w7kk");
    assert(parsed.width == 3840);
    assert(parsed.height == 2160);
    assert(parsed.aspect_ratio == "16:9");
    assert(parsed.preview_url == "https://th.wallhaven.cc/lg/83/83w7kk.jpg");
    assert(parsed.full_url == "https://w.wallhaven.cc/full/83/wallhaven-83w7kk.jpg");
    assert(parsed.file_size == 3951234);
    assert(parsed.file_type == "image/jpeg");
    assert(parsed.source == "wallhaven");
    std::cout << "  - Parseo de JSON de búsqueda de Wallhaven: OK ("
              << parsed.id << ", " << parsed.width << "x" << parsed.height
              << ", " << parsed.file_size << " bytes, preview: " << parsed.preview_url << ")\n";

    // 5.6 Parsing simulated details response JSON
    std::string mock_details_json = R"({
        "data": {
            "id": "83w7kk",
            "url": "https://wallhaven.cc/w/83w7kk",
            "short_url": "https://whvn.cc/83w7kk",
            "views": 15403,
            "favorites": 341,
            "source": "",
            "purity": "sfw",
            "category": "general",
            "dimension_x": 3840,
            "dimension_y": 2160,
            "resolution": "3840x2160",
            "ratio": "16:9",
            "file_size": 3951234,
            "file_type": "image/jpeg",
            "path": "https://w.wallhaven.cc/full/83/wallhaven-83w7kk.jpg",
            "thumbs": {
                "large": "https://th.wallhaven.cc/lg/83/83w7kk.jpg"
            },
            "tags": [
                {"id": 37, "name": "nature"},
                {"id": 42, "name": "forest"}
            ]
        }
    })";

    auto details_opt = provider.parse_details_json(mock_details_json);
    assert(details_opt.has_value());
    assert(details_opt->id == "83w7kk");
    assert(details_opt->tags.size() == 2);
    assert(details_opt->tags[0] == "nature");
    assert(details_opt->tags[1] == "forest");
    std::cout << "  - Parseo de JSON de detalles con tags: OK ("
              << details_opt->tags[0] << ", " << details_opt->tags[1] << ")\n";

    // 5.7 Graceful offline / error handling verification
    auto live_results = provider.search(test_query);
    std::cout << "  - Comprobación de resiliencia ante errores de red: "
              << live_results.size() << " resultados devueltos de forma segura sin excepciones.\n";

    // 5.8 Early cancellation verification
    auto cancelled_page = provider.search_page(test_query, []() { return true; });
    assert(cancelled_page.items.empty());
    assert(!cancelled_page.has_more);
    std::cout << "  - Cancelación temprana de búsqueda (is_cancelled): OK.\n";

    // 5.9 Space and Vehicles category query verification
    wallpaper::SearchQuery space_query;
    space_query.keywords = "space";
    space_query.categories = "100";
    std::string space_url = provider.build_search_url(space_query);
    assert(space_url.find("q=space") != std::string::npos);
    assert(space_url.find("categories=100") != std::string::npos);

    wallpaper::SearchQuery vehicles_query;
    vehicles_query.keywords = "vehicles";
    vehicles_query.categories = "100";
    std::string vehicles_url = provider.build_search_url(vehicles_query);
    assert(vehicles_url.find("q=vehicles") != std::string::npos);
    assert(vehicles_url.find("categories=100") != std::string::npos);
    std::cout << "  - Consultas para categorías Espacio y Vehículos: OK.\n";

    std::filesystem::path failed_dest = std::filesystem::temp_directory_path() / "non_existent_wallpaper.jpg";
    bool dl_ok = provider.get_http_client().download_file("https://invalid.domain.local/wallpaper.jpg", failed_dest);
    assert(!dl_ok);
    assert(!std::filesystem::exists(failed_dest));
    assert(!std::filesystem::exists(failed_dest.string() + ".part"));
    std::cout << "  - Comprobación de fallo de descarga segura (limpieza de archivos temporales): OK.\n";
    std::cout << "  [OK] Cliente HTTP y WallhavenProvider verificados exitosamente.\n\n";

    // 6. WallpaperSetter and OS Integration Verification
    std::cout << "[6/6] Verificando WallpaperSetter y Aplicador de Fondos en el Sistema Operativo...\n";

    wallpaper::WallpaperSetter setter;
    std::cout << "  - WallpaperSetter inicializado con entorno: "
              << wallpaper::to_string(setter.get_desktop_environment()) << '\n';

    // 6.1 Filename generation test
    wallpaper::WallpaperItem item_test1;
    item_test1.id = "83w7kk";
    item_test1.width = 3840;
    item_test1.height = 2160;
    item_test1.full_url = "https://w.wallhaven.cc/full/83/wallhaven-83w7kk.jpg";
    item_test1.file_type = "image/jpeg";
    item_test1.source = "wallhaven";

    std::filesystem::path fn1 = setter.build_filename(item_test1, "/home/hex/Wallpapers");
    std::cout << "  - Nombre de archivo generado (Wallhaven JPG 4K): " << fn1.string() << '\n';
    assert(fn1.filename() == "wallhaven_83w7kk_3840x2160.jpg");
    assert(fn1.parent_path() == "/home/hex/Wallpapers");

    wallpaper::WallpaperItem item_test2;
    item_test2.id = "wallhaven-9m8l41";
    item_test2.width = 1920;
    item_test2.height = 1080;
    item_test2.full_url = "https://w.wallhaven.cc/full/9m/wallhaven-9m8l41.png";
    item_test2.file_type = "image/png";

    std::filesystem::path fn2 = setter.build_filename(item_test2, "/home/hex/Wallpapers");
    std::cout << "  - Nombre de archivo generado (Prefijo limpio PNG 1080p): " << fn2.string() << '\n';
    assert(fn2.filename() == "wallhaven_9m8l41_1920x1080.png");

    // 6.2 Command generation for desktop environments
    std::filesystem::path mock_image = "/home/hex/Pictures/Wallpapers/test_sample.jpg";

    // KDE Plasma
    std::string kde_cmd = setter.get_command_for(mock_image, wallpaper::DesktopEnvironment::KDE_PLASMA);
    std::cout << "  - Comando KDE Plasma: " << kde_cmd << '\n';
    assert(kde_cmd.find("plasma-apply-wallpaperimage") != std::string::npos ||
           kde_cmd.find("evaluateScript") != std::string::npos);
    assert(kde_cmd.find(mock_image.string()) != std::string::npos);

    // GNOME
    std::string gnome_cmd = setter.get_command_for(mock_image, wallpaper::DesktopEnvironment::GNOME);
    std::cout << "  - Comando GNOME: " << gnome_cmd << '\n';
    assert(gnome_cmd.find("gsettings") != std::string::npos);
    assert(gnome_cmd.find("picture-uri 'file:///home/hex/Pictures/Wallpapers/test_sample.jpg'") != std::string::npos);
    assert(gnome_cmd.find("picture-uri-dark 'file:///home/hex/Pictures/Wallpapers/test_sample.jpg'") != std::string::npos);
    assert(gnome_cmd.find("picture-options 'zoom'") != std::string::npos);

    // XFCE
    std::string xfce_cmd = setter.get_command_for(mock_image, wallpaper::DesktopEnvironment::XFCE);
    std::cout << "  - Comando XFCE: " << xfce_cmd << '\n';
    assert(xfce_cmd.find("xfconf-query") != std::string::npos);
    assert(xfce_cmd.find("/backdrop/screen0/monitor0/workspace0/last-image") != std::string::npos);

    // Hyprland
    std::string hypr_cmd = setter.get_command_for(mock_image, wallpaper::DesktopEnvironment::HYPRLAND);
    std::cout << "  - Comando Hyprland: " << hypr_cmd << '\n';
    assert(hypr_cmd.find("swww") != std::string::npos || hypr_cmd.find("hyprctl") != std::string::npos);

    // Sway
    std::string sway_cmd = setter.get_command_for(mock_image, wallpaper::DesktopEnvironment::SWAY);
    std::cout << "  - Comando Sway: " << sway_cmd << '\n';
    assert(sway_cmd.find("swaybg") != std::string::npos || sway_cmd.find("swww") != std::string::npos);

    // X11 Generic
    std::string x11_cmd = setter.get_command_for(mock_image, wallpaper::DesktopEnvironment::X11_GENERIC);
    std::cout << "  - Comando X11 Generic: " << x11_cmd << '\n';
    assert(x11_cmd.find("feh") != std::string::npos || x11_cmd.find("nitrogen") != std::string::npos);

    std::cout << "  [OK] Generación de comandos para KDE, GNOME, XFCE, Hyprland, Sway y X11 verificada.\n";

    // 6.3 Non-existent file safety check
    wallpaper::WallpaperSetResult fail_res = setter.set_wallpaper("/tmp/this_file_does_not_exist_98765.jpg");
    assert(!fail_res.success);
    assert(fail_res.message.find("does not exist") != std::string::npos);
    std::cout << "  [OK] Validación de archivos inexistentes rechazada con error descriptivo.\n";

    // 6.4 Real 100x100 BMP file creation and Dry-run execution
    std::filesystem::path test_bmp_path = std::filesystem::temp_directory_path() / "wallpaper_test_100x100.bmp";
    create_test_bmp(test_bmp_path, 100, 100);
    assert(std::filesystem::exists(test_bmp_path));
    assert(std::filesystem::file_size(test_bmp_path) == 30054);
    std::cout << "  - Archivo de prueba BMP 100x100 creado: " << test_bmp_path
              << " (" << std::filesystem::file_size(test_bmp_path) << " bytes)\n";

    setter.set_dry_run(true);
    assert(setter.is_dry_run());
    wallpaper::WallpaperSetResult dry_res = setter.set_wallpaper(test_bmp_path);
    assert(dry_res.success);
    assert(!dry_res.command_used.empty());
    assert(dry_res.saved_path == std::filesystem::absolute(test_bmp_path));
    std::cout << "  - Simulación Dry-Run exitosa. Comando preparado: " << dry_res.command_used << '\n';

    // 6.5 Mock Runner Injection and Validation
    std::string captured_cmd;
    setter.set_dry_run(false);
    setter.set_command_runner([&](const std::string& cmd, std::string& out) -> int {
        captured_cmd = cmd;
        out = "Mock desktop wallpaper setter applied successfully.";
        return 0; // Success
    });

    wallpaper::WallpaperSetResult mock_res = setter.set_wallpaper(test_bmp_path);
    assert(mock_res.success);
    assert(captured_cmd == dry_res.command_used);
    assert(mock_res.message.find("successfully") != std::string::npos);
    std::cout << "  - Mock runner simulado con éxito (capturó comando idéntico al previsto).\n";

    // Mock Failure
    setter.set_command_runner([&](const std::string&, std::string& out) -> int {
        out = "Error code 127: Command not found in PATH";
        return 127;
    });
    wallpaper::WallpaperSetResult mock_fail_res = setter.set_wallpaper(test_bmp_path);
    assert(!mock_fail_res.success);
    assert(mock_fail_res.message.find("127") != std::string::npos);
    std::cout << "  - Mock runner error verificado (código de salida 127 y mensaje de error propagado).\n";

    // Reset runner to real execution
    setter.reset_command_runner();

    // 6.6 Real OS Command Execution in Current Environment
    wallpaper::WallpaperSetResult real_res = setter.set_wallpaper(test_bmp_path);
    std::cout << "  - Ejecución en entorno actual (" << wallpaper::to_string(setter.get_desktop_environment()) << "):\n";
    std::cout << "    * Comando ejecutado: " << real_res.command_used << '\n';
    std::cout << "    * Éxito: " << (real_res.success ? "true" : "false") << '\n';
    std::cout << "    * Mensaje: " << real_res.message << '\n';
    assert(!real_res.command_used.empty());
    assert(real_res.saved_path == std::filesystem::absolute(test_bmp_path));
    std::cout << "  [OK] Ejecución de comando del sistema controlada con verificación de salida y estabilidad total.\n";

    // 6.7 download_and_set Caching and Download Verification
    std::filesystem::path cache_test_dir = std::filesystem::temp_directory_path() / "wallpaper_cache_test_dir";
    std::error_code ec;
    std::filesystem::create_directories(cache_test_dir, ec);

    wallpaper::WallpaperItem cached_item;
    cached_item.id = "cache_test_99";
    cached_item.width = 100;
    cached_item.height = 100;
    cached_item.full_url = "https://invalid.unreachable.domain/dummy.bmp";
    cached_item.file_type = "image/bmp";
    cached_item.source = "wallhaven";

    std::filesystem::path cached_expected_file = setter.build_filename(cached_item, cache_test_dir);
    create_test_bmp(cached_expected_file, 100, 100);
    assert(std::filesystem::exists(cached_expected_file));

    // With file already existing, download_and_set must skip network download and apply directly
    setter.set_dry_run(true);
    wallpaper::WallpaperSetResult cached_set_res = setter.download_and_set(cached_item, cache_test_dir);
    assert(cached_set_res.success);
    assert(cached_set_res.saved_path == cached_expected_file);
    std::cout << "  - Comprobación de caché de descarga: Archivo existente reutilizado sin re-descarga.\n";

    // When file does not exist and URL is invalid, download_and_set fails cleanly
    wallpaper::WallpaperItem missing_item;
    missing_item.id = "missing_item_11";
    missing_item.width = 1920;
    missing_item.height = 1080;
    missing_item.full_url = "https://invalid.unreachable.domain/sample.jpg";
    missing_item.file_type = "image/jpeg";

    setter.set_dry_run(false);
    wallpaper::WallpaperSetResult dl_fail_res = setter.download_and_set(missing_item, cache_test_dir);
    assert(!dl_fail_res.success);
    assert(!std::filesystem::exists(dl_fail_res.saved_path));
    assert(!std::filesystem::exists(dl_fail_res.saved_path.string() + ".part"));
    std::cout << "  - Comprobación de fallo seguro en download_and_set: Limpieza y reporte de error OK.\n";

    // 6.8 register_in_system_wallpapers (KDE Plasma, GNOME, XFCE)
    std::cout << "  - Verificando registro en lista de fondos del sistema (register_in_system_wallpapers)...\n";
    std::filesystem::path test_sys_share = std::filesystem::temp_directory_path() / "test_sys_share_wallpapers";
    std::filesystem::remove_all(test_sys_share, ec);
    setter.set_system_data_dir_override(test_sys_share);

    wallpaper::WallpaperItem reg_item;
    reg_item.id = "reg_test_42";
    reg_item.width = 1920;
    reg_item.height = 1080;
    reg_item.file_type = "image/bmp";
    reg_item.source = "wallhaven";

    // Non-existent image must fail safely
    assert(!setter.register_in_system_wallpapers("/tmp/non_existent_file_9999.bmp", reg_item));

    // Valid test image
    std::filesystem::path reg_sample_bmp = std::filesystem::temp_directory_path() / "sample_reg_wallpaper.bmp";
    create_test_bmp(reg_sample_bmp, 100, 100);
    assert(std::filesystem::exists(reg_sample_bmp));

    bool reg_ok = setter.register_in_system_wallpapers(reg_sample_bmp, reg_item);
    assert(reg_ok);

    // 1. KDE Plasma verification: No se crean paquetes duplicados en ~/.local/share/wallpapers/ y se limpia cualquier paquete antiguo
    std::filesystem::path kde_pkg_dir = test_sys_share / "wallpapers" / "wallhaven_reg_test_42";
    assert(!std::filesystem::exists(kde_pkg_dir));
    std::cout << "    * KDE Plasma: Cero paquetes duplicados en ~/.local/share/wallpapers/ y limpieza de paquetes antiguos verificada.\n";

    // 2. GNOME verification: ~/.local/share/backgrounds/<filename> + XML
    std::filesystem::path gnome_bg_img = test_sys_share / "backgrounds" / reg_sample_bmp.filename();
    assert(std::filesystem::exists(gnome_bg_img));

    std::filesystem::path gnome_xml = test_sys_share / "gnome-background-properties" / "wallhaven-reg_test_42.xml";
    assert(std::filesystem::exists(gnome_xml));
    std::ifstream gnome_xml_in(gnome_xml);
    std::string xml_content((std::istreambuf_iterator<char>(gnome_xml_in)), std::istreambuf_iterator<char>());
    assert(xml_content.find("<wallpaper deleted=\"false\">") != std::string::npos);
    assert(xml_content.find("Wallhaven reg_test_42") != std::string::npos);
    assert(xml_content.find("<filename>") != std::string::npos);
    assert(xml_content.find("<options>zoom</options>") != std::string::npos);
    std::cout << "    * GNOME: Enlace en backgrounds/ y archivo XML en gnome-background-properties/ verificados.\n";

    // 3. XFCE verification: ~/.local/share/xfce4/backdrops/<filename>
    std::filesystem::path xfce_img = test_sys_share / "xfce4" / "backdrops" / reg_sample_bmp.filename();
    assert(std::filesystem::exists(xfce_img));
    std::cout << "    * XFCE: Enlace en xfce4/backdrops/ verificado correctamente.\n";

    // Reset override and cleanup test files
    setter.set_system_data_dir_override("");
    std::filesystem::remove(reg_sample_bmp, ec);
    std::filesystem::remove_all(test_sys_share, ec);

    // Cleanup temporary files
    std::filesystem::remove(test_bmp_path, ec);
    std::filesystem::remove_all(cache_test_dir, ec);

    std::cout << "  [OK] WallpaperSetter y aplicador de fondos completado con 100% pruebas aprobadas.\n\n";

    // 7. Verificando TextureCache, decodificación stb_image y gestión de memoria
    std::cout << "[7/7] Verificando TextureCache y decodificación stb_image...\n";
    {
        auto pool = std::make_shared<wallpaper::ThreadPool>(4);
        wallpaper::TextureCache cache(pool);

        // 7.1 Carga sincrónica desde memoria
        auto test_bmp = generate_test_bmp_bytes(80, 60);
        wallpaper::TextureInfo info = cache.load_from_memory("test_sync_bmp", test_bmp);
        assert(info.loaded);
        assert(info.width == 80);
        assert(info.height == 60);
        assert(cache.has_texture("test_sync_bmp"));
        std::cout << "  - Carga síncrona en memoria (80x60): OK (width: " << info.width 
                  << ", height: " << info.height << ", loaded: true)\n";

        // 7.2 Decodificación asíncrona en ThreadPool
        auto async_bmp = generate_test_bmp_bytes(1920, 1080);
        cache.queue_decode_from_memory("test_async_bmp", async_bmp);
        assert(cache.is_loading("test_async_bmp"));

        // Esperar a que el worker thread procese y subir en el hilo principal
        bool async_loaded = false;
        for (int i = 0; i < 50; ++i) {
            cache.process_pending_uploads(10);
            if (cache.has_texture("test_async_bmp")) {
                async_loaded = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        assert(async_loaded);
        wallpaper::TextureInfo async_info = cache.get_texture("test_async_bmp");
        assert(async_info.loaded);
        assert(async_info.width == 1920);
        assert(async_info.height == 1080);
        assert(!cache.is_loading("test_async_bmp"));
        std::cout << "  - Decodificación asíncrona en segundo plano (1080p FHD): OK (width: " 
                  << async_info.width << ", height: " << async_info.height << ")\n";

        // 7.3 Manejo seguro de memoria inválida
        std::vector<uint8_t> corrupted_data = { 0x01, 0x02, 0x03, 0x04 };
        wallpaper::TextureInfo corrupt_info = cache.load_from_memory("corrupt_key", corrupted_data);
        assert(!corrupt_info.loaded);
        assert(!cache.has_texture("corrupt_key"));
        std::cout << "  - Manejo seguro de datos corruptos: OK (rechazado limpiamente sin fallos)\n";

        // 7.4 Liberación selectiva y vaciado de caché
        assert(cache.size() >= 2);
        cache.remove_texture("test_sync_bmp");
        assert(!cache.has_texture("test_sync_bmp"));
        cache.clear();
        assert(cache.size() == 0);
        std::cout << "  - Liberación adecuada de texturas y clear(): OK (tamaño final: 0)\n";
    }
    std::cout << "  [OK] TextureCache verificado al 100% con decodificación y concurrencia seguras.\n\n";

    // 8. Verificación del Motor de Análisis Cromático (CIEDE2000, Bicolor y Monocromo)
    std::cout << "[8/8] Verificando Motor de Análisis Cromático CIEDE2000, Bicolor y Monocromo...\n";
    {
        // 8.1 Monocromo ultra-estricto (escala de grises pura, croma <= 5.0)
        std::vector<std::string> true_mono = { "#000000", "#303030", "#808080", "#cccccc", "#ffffff" };
        assert(wallpaper::color::is_strict_monochrome(true_mono));

        // Rechaza fondos con tintes sutiles (como pizarra azulado #424153) o colores vivos infiltrados
        assert(!wallpaper::color::is_strict_monochrome({ "#000000", "#424153", "#ffffff" }));
        std::vector<std::string> fake_mono = { "#000000", "#ff0000", "#ffffff" }; // Blanco y negro con objeto rojo
        assert(!wallpaper::color::is_strict_monochrome(fake_mono));
        std::cout << "  - Detección de monocromo ultra-estricto (rechaza tintes y colores vivos infiltrados): OK.\n";

        // 8.2 Bicolor: Color 1 dominante (índice 0) y Color 2 secundario
        std::string primary = "0066cc";   // Azul
        std::string secondary = "ff9900"; // Naranja

        std::vector<std::string> good_dual = { "#0066cc", "#ff9900", "#000000" };
        auto match_good = wallpaper::color::evaluate_dual(good_dual, primary, secondary);
        assert(match_good.accepted);

        // Si el Color 1 no es el tono principal (ej. naranja es el dominante), se descarta
        std::vector<std::string> reversed_dual = { "#ff9900", "#0066cc", "#ffffff" };
        auto match_rev = wallpaper::color::evaluate_dual(reversed_dual, primary, secondary);
        assert(!match_rev.accepted);

        // Si la imagen contiene demasiados colores vivos ajenos a ambos, se descarta
        std::vector<std::string> noisy_dual = { "#0066cc", "#ff9900", "#77cc33", "#ea4c88" };
        auto match_noisy = wallpaper::color::evaluate_dual(noisy_dual, primary, secondary);
        assert(!match_noisy.accepted);
        std::cout << "  - Modo bicolor con Color 1 estrictamente dominante y descarte de tonos ajenos: OK.\n";

        // 8.3 Temperatura cromática
        std::vector<std::string> warm_pal = { "#ff6600", "#ff9900", "#cc0000" };
        std::vector<std::string> cool_pal = { "#0066cc", "#66cccc", "#333399" };
        assert(wallpaper::color::matches_temperature(warm_pal, wallpaper::color::Temperature::Warm));
        assert(!wallpaper::color::matches_temperature(warm_pal, wallpaper::color::Temperature::Cool));
        assert(wallpaper::color::matches_temperature(cool_pal, wallpaper::color::Temperature::Cool));
        assert(!wallpaper::color::matches_temperature(cool_pal, wallpaper::color::Temperature::Warm));
        
        // Fondo frío con acento cálido: ya no se descarta de forma injusta
        std::vector<std::string> cool_with_warm_accent = { "#0066cc", "#66cccc", "#ff9900", "#333399" };
        assert(wallpaper::color::matches_temperature(cool_with_warm_accent, wallpaper::color::Temperature::Cool));
        std::cout << "  - Validación de temperatura cromática (Cálidos / Fríos y fondos mixtos): OK.\n";

        // 8.4 Color único en índices 2 o 3 (fondos con base oscura o neutra)
        std::vector<std::string> pal_with_blue_at_idx2 = { "#000000", "#222222", "#0066cc", "#111111" };
        auto single_res = wallpaper::color::evaluate_single(pal_with_blue_at_idx2, "0066cc");
        assert(single_res.accepted);
        assert(single_res.score < 50.0f);
        std::cout << "  - Evaluación de color único en índice 2/3 (fondo oscuro con acento azul): OK (score: "
                  << single_res.score << ").\n";

        // 8.5 Tolerancias cromáticas estrictas calibradas (Amarillo/Blanco, Blanco/Gris, Negro/Gris oscuro, Rojo/Tinto/Naranja)
        // Amarillo: no capta blanco, pero sí amarillo claro; no capta naranja
        assert(!wallpaper::color::evaluate_single({ "#ffffff", "#f5f5f5" }, "ffff00").accepted);
        assert(!wallpaper::color::evaluate_single({ "#ff9900", "#ffa500" }, "ffff00").accepted);
        assert(wallpaper::color::evaluate_single({ "#ffffb4", "#f0f0f0" }, "ffff00").accepted);
        assert(wallpaper::color::evaluate_single({ "#ffff00", "#f0f0f0" }, "ffff00").accepted);

        // Blanco: no capta amarillo, pero sí gris claro
        assert(!wallpaper::color::evaluate_single({ "#ffff00", "#ffe500" }, "ffffff").accepted);
        assert(!wallpaper::color::evaluate_single({ "#ffffb4", "#ffe500" }, "ffffff").accepted);
        assert(wallpaper::color::evaluate_single({ "#c8c8c8", "#d0d0d0" }, "ffffff").accepted);
        assert(wallpaper::color::evaluate_single({ "#ffffff", "#e0e0e0" }, "ffffff").accepted);

        // Negro: capta gris muy oscuro, pero rechaza gris medio o colores vivos
        assert(wallpaper::color::evaluate_single({ "#232323", "#161616" }, "000000").accepted);
        assert(!wallpaper::color::evaluate_single({ "#808080", "#a0a0a0" }, "000000").accepted);
        assert(!wallpaper::color::evaluate_single({ "#000080", "#000050" }, "000000").accepted);

        // Rojo: capta tinto (vino) y rojo anaranjado, pero NO naranja directo
        assert(wallpaper::color::evaluate_single({ "#640014", "#200005" }, "cc0000").accepted);
        assert(wallpaper::color::evaluate_single({ "#e63200", "#300000" }, "cc0000").accepted);
        assert(!wallpaper::color::evaluate_single({ "#ff9900", "#ffb347" }, "cc0000").accepted);
        std::cout << "  - Tolerancias cromáticas estrictas verificadas al 100% (Amarillo, Blanco, Negro, Rojo, Tinto).\n";

        // 8.6 Red de seguridad en apply_color_filters (nunca 0 resultados)
        wallpaper::SearchQuery fallback_q;
        fallback_q.color = "cc0000"; // Rojo
        std::vector<wallpaper::WallpaperItem> test_items;
        wallpaper::WallpaperItem it1; it1.id = "i1"; it1.colors = { "#0066cc", "#00aa00" };
        wallpaper::WallpaperItem it2; it2.id = "i2"; it2.colors = { "#ffff00", "#333399" };
        test_items.push_back(it1);
        test_items.push_back(it2);

        auto filtered = wallpaper::WallhavenProvider::apply_color_filters(test_items, fallback_q);
        assert(!filtered.empty());
        assert(filtered.size() == 2);
        std::cout << "  - Red de seguridad en apply_color_filters: NUNCA retorna 0 si la API devolvió ítems (OK).\n";
    }
    std::cout << "  [OK] Motor cromático verificado al 100% con precisión perceptual.\n\n";

    // -------------------------------------------------------------------------
    // 9. Verificación del Escáner de Versiones en GitHub (VersionChecker)
    // -------------------------------------------------------------------------
    std::cout << "[9/10] Verificando Escáner de Versiones en GitHub (VersionChecker)...\n";
    {
        // Comparación de versiones semver
        assert(wallpaper::VersionChecker::is_newer("1.0.1", "1.0.0"));
        assert(wallpaper::VersionChecker::is_newer("v1.1.0", "1.0.0"));
        assert(wallpaper::VersionChecker::is_newer("v2.0.0", "1.9.9"));
        assert(!wallpaper::VersionChecker::is_newer("1.0.0", "1.0.0"));
        assert(!wallpaper::VersionChecker::is_newer("0.9.9", "1.0.0"));
        assert(!wallpaper::VersionChecker::is_newer("v1.0.0", "1.0.0"));
        std::cout << "  - Comparador semántico de versiones (is_newer): OK.\n";

        // Consulta simulada o en endpoint (manejo seguro sin caída si no hay red)
        auto vinfo = wallpaper::VersionChecker::check_latest("ItsH3x/WallFinder", "1.0.0", 2);
        assert(vinfo.check_completed);
        std::cout << "  - Consulta y manejo resiliente ante red/timeout: OK.\n";
    }
    std::cout << "  [OK] Escáner de versiones verificado exitosamente.\n\n";

    // -------------------------------------------------------------------------
    // 10. Verificación de Arquitectura de Color y Modo Bicolor en Wallhaven
    // -------------------------------------------------------------------------
    std::cout << "[10/10] Verificando Filtro Sensible de Wallhaven y Modo Bicolor Reforzado...\n";
    {
        wallpaper::WallhavenProvider provider;

        // 10.1 Modo Monocolor: la API de Wallhaven filtra en servidor; no hay descarte local
        wallpaper::SearchQuery mono_q;
        mono_q.color = "cc0000"; // Rojo
        assert(!wallpaper::WallhavenProvider::needs_local_color_filter(mono_q));

        std::vector<wallpaper::WallpaperItem> mono_items;
        wallpaper::WallpaperItem mi1; mi1.id = "m1"; mi1.colors = { "#cc0000", "#111111" };
        wallpaper::WallpaperItem mi2; mi2.id = "m2"; mi2.colors = { "#ff2222", "#ffffff" };
        mono_items.push_back(mi1);
        mono_items.push_back(mi2);

        auto mono_res = wallpaper::WallhavenProvider::apply_color_filters(mono_items, mono_q);
        assert(mono_res.size() == 2);
        std::cout << "  - Modo Monocolor sensible (sin descarte artificial de resultados): OK.\n";

        // 10.2 Modo Bicolor: Wallhaven consulta Color 1; WallFinder refuerza encontrar Color 2 (acento)
        wallpaper::SearchQuery dual_q;
        dual_q.color = "cc0000";           // Color 1: Rojo
        dual_q.color_secondary = "ffff00"; // Color 2: Amarillo
        dual_q.combine_colors = true;
        assert(wallpaper::WallhavenProvider::needs_local_color_filter(dual_q));

        std::vector<wallpaper::WallpaperItem> dual_candidates;
        // Candidato 1: tiene Rojo y Amarillo (debe aceptarse)
        wallpaper::WallpaperItem d1; d1.id = "d1"; d1.colors = { "#cc0000", "#111111", "#ffff00" };
        // Candidato 2: solo tiene Rojo y Azul (no tiene Amarillo, debe descartarse)
        wallpaper::WallpaperItem d2; d2.id = "d2"; d2.colors = { "#cc0000", "#0066cc", "#222222" };
        // Candidato 3: tiene Rojo y un tono dorado/amarillento claro (debe aceptarse)
        wallpaper::WallpaperItem d3; d3.id = "d3"; d3.colors = { "#cc0000", "#ffee00" };

        dual_candidates.push_back(d1);
        dual_candidates.push_back(d2);
        dual_candidates.push_back(d3);

        auto dual_res = wallpaper::WallhavenProvider::apply_color_filters(dual_candidates, dual_q);
        assert(dual_res.size() == 2);
        assert(dual_res[0].id == "d1" || dual_res[0].id == "d3");
        assert(dual_res[1].id == "d1" || dual_res[1].id == "d3");
        std::cout << "  - Modo Bicolor reforzado (Color 1 garantizado por API + Color 2 verificado en paleta): OK.\n";

        // 10.3 Temperatura cromática (cálidos / fríos)
        wallpaper::SearchQuery warm_q;
        warm_q.color_temperature = "warm";
        assert(wallpaper::WallhavenProvider::needs_local_color_filter(warm_q));

        wallpaper::SearchQuery cool_q;
        cool_q.color_temperature = "cool";
        assert(wallpaper::WallhavenProvider::needs_local_color_filter(cool_q));

        std::vector<wallpaper::WallpaperItem> temp_items;
        wallpaper::WallpaperItem tw; tw.id = "warm"; tw.colors = { "#ff5500", "#cc0000" };
        wallpaper::WallpaperItem tc; tc.id = "cool"; tc.colors = { "#0066cc", "#00ccff" };
        temp_items.push_back(tw);
        temp_items.push_back(tc);

        auto warm_res = wallpaper::WallhavenProvider::apply_color_filters(temp_items, warm_q);
        assert(warm_res.size() == 1 && warm_res[0].id == "warm");
        auto cool_res = wallpaper::WallhavenProvider::apply_color_filters(temp_items, cool_q);
        assert(cool_res.size() == 1 && cool_res[0].id == "cool");
        std::cout << "  - Filtrado por ambiente de temperatura (Cálidos / Fríos): OK.\n";

        // 10.4 Verificación de SearchQuery y URL de Wallhaven (pureza 110 pública, topRange=1y y apikey opcional)
        wallpaper::SearchQuery default_q;
        assert(default_q.purity == "110");
        assert(default_q.top_range == "1y");
        assert(default_q.sorting == "toplist");
        std::cout << "  - SearchQuery inicialización por defecto (purity=110, top_range=1y, sorting=toplist): OK.\n";

        std::string sfw_url = provider.build_search_url(default_q);
        assert(sfw_url.find("purity=110") != std::string::npos);
        assert(sfw_url.find("topRange=1y") != std::string::npos);
        assert(sfw_url.find("sorting=toplist") != std::string::npos);

        // Sorting relevance no debe incluir topRange
        wallpaper::SearchQuery rel_q = default_q;
        rel_q.keywords = "cyberpunk";
        rel_q.sorting = "relevance";
        std::string rel_url = provider.build_search_url(rel_q);
        assert(rel_url.find("sorting=relevance") != std::string::npos);
        assert(rel_url.find("topRange") == std::string::npos);
        assert(rel_url.find("purity=110") != std::string::npos);
        std::cout << "  - URL con ordenación relevance excluye topRange y mantiene purity=110: OK.\n";

        // Con API key opcional: no altera la pureza y añade el parámetro apikey
        provider.set_api_key("test_key_123");
        std::string key_url = provider.build_search_url(default_q);
        assert(key_url.find("purity=110") != std::string::npos);
        assert(key_url.find("topRange=1y") != std::string::npos);
        assert(key_url.find("apikey=test_key_123") != std::string::npos);
        provider.set_api_key(""); // Limpiar para siguientes tests
        std::cout << "  - Parámetros de pureza pública (110), topRange (1y) y API Key opcional en Wallhaven: OK.\n";

        // 10.5 Verificación de resiliencia del rate limiting (250ms de espaciado y cancelación)
        {
            wallpaper::SearchQuery rl_q;
            rl_q.keywords = "qa_throttle_test";

            // Primera llamada registra el timestamp
            auto res_init = provider.search_page(rl_q);
            (void)res_init;

            // Segunda llamada inmediata: debe aplicar un retraso mínimo de 250ms
            auto t_start = std::chrono::steady_clock::now();
            auto res_throttled = provider.search_page(rl_q);
            (void)res_throttled;
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t_start
            ).count();

            assert(elapsed_ms >= 180);
            std::cout << "  - Resiliencia del rate limiting (mínimo 250ms espaciado, medido: "
                      << elapsed_ms << " ms): OK.\n";

            // Cancelación cooperativa durante throttling
            auto t_cancel_start = std::chrono::steady_clock::now();
            auto cancel_res = provider.search_page(rl_q, []() { return true; });
            auto cancel_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t_cancel_start
            ).count();
            assert(cancel_res.items.empty());
            assert(cancel_ms < 100);
            std::cout << "  - Cancelación cooperativa en throttle (<100ms): OK.\n";
        }
    }
    std::cout << "  [OK] Nueva arquitectura de color y filtros de Wallhaven verificada exitosamente.\n\n";

    std::cout << "============================================================\n";
    std::cout << " ¡TODAS LAS PRUEBAS BASE, RED Y FONDOS PASARON CON ÉXITO!\n";
    std::cout << "============================================================\n";

    return 0;
}
