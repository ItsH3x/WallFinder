#pragma once

#include "app/config.hpp"
#include "app/thread_pool.hpp"
#include "core/models.hpp"
#include "core/texture_cache.hpp"
#include "core/wallpaper_setter.hpp"
#include "net/wallpaper_provider.hpp"
#include "net/wallhaven_provider.hpp"
#include "ui/grid_view.hpp"
#include "ui/preview_modal.hpp"

#include <GLFW/glfw3.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace wallpaper {

class UIManager {
public:
    UIManager();
    ~UIManager();

    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    // Window and ImGui initialization
    bool init(int width = 1280, int height = 800, const char* title = "WallFinder");

    // Main application loop
    void run();

    // Cleanup resources
    void shutdown();

    // Trigger async search (reset = true clears previous results; immediate = true skips debounce)
    void trigger_search(bool reset = true, bool immediate = false);

    // Infinite scroll: load next batch of wallpapers
    void load_more();

    // Toast message display
    void show_toast(const std::string& message, bool is_success = true, float duration_seconds = 4.0f);

    [[nodiscard]] GLFWwindow* get_window() const noexcept { return window_; }

private:
    GLFWwindow* window_{nullptr};

    std::shared_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<TextureCache> texture_cache_;
    std::shared_ptr<WallhavenProvider> provider_;
    WallpaperSetter wallpaper_setter_;

    GridView grid_view_;
    PreviewModal preview_modal_;

    // Search and filter state
    char search_text_[256]{};
    int selected_resolution_idx_{0};
    char custom_resolution_[64]{"1920x1080"};
    bool allow_nsfw_{false};

    // Categorías (inactivas por defecto; si ninguna está activa se busca en General)
    bool cat_anime_{false};
    bool cat_people_{false};
    bool cat_nature_{false};
    bool cat_gaming_{false};
    bool cat_cyberpunk_{false};
    bool cat_digital_art_{false};
    bool cat_space_{false};
    bool cat_vehicles_{false};
    bool cat_fantasy_{false};
    bool cat_minimalism_{false};
    bool cat_animals_{false};
    bool cat_architecture_{false};

    int selected_sort_idx_{0};
    std::atomic<int> next_page_{2};          // Siguiente página real de la API (puede saltar varias con filtros estrictos)
    std::atomic<bool> has_more_{true};
    std::atomic<unsigned> search_generation_{0}; // Descarta resultados obsoletos al cambiar filtros rápidamente
    bool focus_search_{false};
    std::unordered_set<std::string> loaded_item_ids_;

    // Zoom dinámico para cuadrículas de fondos
    float card_zoom_width_{220.0f};

    // Filtros de Color y Temperatura de Color
    enum class ColorFilterTab { ByColor, ByTone };
    ColorFilterTab active_color_tab_{ColorFilterTab::ByColor};

    int selected_temp_idx_{0};        // 0: Todos, 1: Cálidos, 2: Fríos
    int selected_color_1_idx_{0};     // Color primario
    int selected_color_2_idx_{0};     // Color secundario (modo bicolor)
    bool combine_colors_{false};      // true: combinar 2 colores en un mismo fondo
    bool show_filters_{false};        // Panel plegable de filtros (minimizado por defecto para barra limpia)

    [[nodiscard]] int get_active_filter_count() const noexcept {
        int count = 0;
        if (allow_nsfw_) ++count;
        if (selected_resolution_idx_ != 0) ++count;
        if (selected_sort_idx_ != 0) ++count;
        if (cat_anime_) ++count;
        if (cat_people_) ++count;
        if (cat_nature_) ++count;
        if (cat_gaming_) ++count;
        if (cat_cyberpunk_) ++count;
        if (cat_digital_art_) ++count;
        if (cat_space_) ++count;
        if (cat_vehicles_) ++count;
        if (cat_fantasy_) ++count;
        if (cat_minimalism_) ++count;
        if (cat_animals_) ++count;
        if (cat_architecture_) ++count;
        if (selected_temp_idx_ != 0) ++count;
        if (selected_color_1_idx_ != 0) ++count;
        if (combine_colors_ && selected_color_2_idx_ != 0) ++count;
        return count;
    }

    mutable std::mutex items_mutex_;
    std::vector<WallpaperItem> items_;
    std::atomic<bool> is_searching_{false};
    std::atomic<bool> is_loading_more_{false};

    // Toast state
    mutable std::mutex toast_mutex_;
    std::string toast_message_{"Listo"};
    std::chrono::steady_clock::time_point toast_expiry_;
    bool toast_is_success_{true};

    bool is_monitor_fullscreen_{false};
    bool was_maximized_{false};
    int saved_win_x_{100}, saved_win_y_{100};
    int saved_win_w_{1280}, saved_win_h_{800};
    void set_monitor_fullscreen(bool enable);

    // Debounce y throttling de búsquedas
    std::atomic<bool> search_pending_{false};
    std::atomic<bool> search_pending_reset_{true};
    std::chrono::steady_clock::time_point search_debounce_deadline_{};
    std::chrono::steady_clock::time_point last_load_more_time_{};
    void execute_search(bool reset = true);
    void process_pending_search();

    // Logo de marca en barra superior
    unsigned int logo_texture_id_{0};
    int logo_width_{0};
    int logo_height_{0};
    void load_brand_logo();

    // Verificación de versiones en GitHub
    std::atomic<bool> update_checked_{false};
    std::atomic<bool> update_available_{false};
    std::string latest_version_tag_;
    std::string release_url_;
    void check_for_updates_async();

    void setup_style();
    void load_fonts();
    void load_window_icon();
    SearchQuery build_query(int page) const;
    void reset_filters();
    void render_ui();
    void render_top_bar();
    void render_filters_panel();
    void render_status_bar();
    std::string get_toast(bool& out_is_success);
};

} // namespace wallpaper
