#include "ui/ui_manager.hpp"
#include "ui/theme.hpp"
#include "ui/ui_icons.hpp"
#include "app/config.hpp"
#include "net/wallhaven_provider.hpp"
#include "net/version_checker.hpp"
#include "stb_image.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>

namespace wallpaper {

namespace {

namespace fs = std::filesystem;
using theme::FontScope;
using theme::fonts;

void glfw_error_callback(int error, const char* description) {
    std::cerr << "[GLFW Error " << error << "]: " << description << std::endl;
}

struct ColorPreset {
    const char* name;
    const char* hex;
    ImU32 col_u32;
};

// Todos los tonos pertenecen a la paleta oficial de Wallhaven (coincidencia exacta en la API)
const ColorPreset kColorPresets[] = {
    { "Ninguno",  "",       IM_COL32(70, 75, 95, 255) },
    { "Rojo",     "cc0000", IM_COL32(204, 0, 0, 255) },
    { "Naranja",  "ff9900", IM_COL32(255, 153, 0, 255) },
    { "Amarillo", "ffff00", IM_COL32(255, 230, 0, 255) },
    { "Verde",    "669900", IM_COL32(102, 153, 0, 255) },
    { "Cian",     "66cccc", IM_COL32(102, 204, 204, 255) },
    { "Azul",     "0066cc", IM_COL32(0, 102, 204, 255) },
    { "Morado",   "663399", IM_COL32(102, 51, 153, 255) },
    { "Rosa",     "ea4c88", IM_COL32(234, 76, 136, 255) },
    { "Blanco",   "ffffff", IM_COL32(245, 245, 245, 255) },
    { "Negro",    "000000", IM_COL32(16, 16, 20, 255) }
};
constexpr int kNumColorPresets = static_cast<int>(sizeof(kColorPresets) / sizeof(kColorPresets[0]));

// Orden: etiquetas en lenguaje natural mapeadas a los criterios de la API
struct SortOption {
    const char* label;
    const char* key;
    const char* hint;
};
const SortOption kSortOptions[] = {
    { "Más populares",  "toplist",    "Los mejor valorados por la comunidad este mes" },
    { "En tendencia",   "hot",        "Lo que está ganando popularidad ahora mismo" },
    { "Más recientes",  "date_added", "Los últimos fondos publicados" },
    { "Más vistos",     "views",      "Los fondos con más visitas de todos los tiempos" },
    { "Más guardados",  "favorites",  "Los que más usuarios han marcado como favoritos" },
    { "Más relevantes", "relevance",  "Los que mejor coinciden con tu búsqueda" },
    { "Aleatorio",      "random",     "Una selección sorpresa en cada búsqueda" }
};
constexpr int kNumSortOptions = static_cast<int>(sizeof(kSortOptions) / sizeof(kSortOptions[0]));

const char* kResolutionOptions[] = {
    "Cualquiera",
    "Full HD · 1080p",
    "2K · 1440p",
    "4K · 2160p",
    "Ultrawide · 21:9",
    "Personalizada"
};

constexpr float kZoomMin = 160.0f;
constexpr float kZoomMax = 480.0f;
constexpr float kZoomDefault = 220.0f;
constexpr float kZoomStep = 25.0f;

// Muestra de color circular con anillo de selección animado
bool RenderColorSwatch(const char* str_id, ImU32 color_u32, bool is_selected, bool is_none,
                       const char* tooltip, float diameter = 24.0f) {
    bool clicked = ImGui::InvisibleButton(str_id, ImVec2(diameter, diameter));
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    float t_sel = theme::Animate(ImGui::GetItemID(), is_selected ? 1.0f : 0.0f, 16.0f);

    ImVec2 bb_min = ImGui::GetItemRectMin();
    ImVec2 bb_max = ImGui::GetItemRectMax();
    ImVec2 center(std::round((bb_min.x + bb_max.x) * 0.5f), std::round((bb_min.y + bb_max.y) * 0.5f));
    float radius = diameter * 0.5f - 3.0f * t_sel;
    if (active) radius -= 1.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (is_none) {
        dl->AddCircleFilled(center, radius, hovered ? theme::kSurface3 : theme::kSurface2, 32);
        dl->AddCircle(center, radius, theme::kBorderHi, 32, 1.0f);
        float d = radius * 0.62f;
        dl->AddLine(ImVec2(center.x - d, center.y + d), ImVec2(center.x + d, center.y - d),
                    hovered ? theme::kText : theme::kTextMuted, 1.6f);
    } else {
        dl->AddCircleFilled(center, radius, color_u32, 32);
        dl->AddCircle(center, radius, IM_COL32(255, 255, 255, 36), 32, 1.0f);
    }

    float ring_r = diameter * 0.5f;
    if (t_sel > 0.01f) {
        dl->AddCircle(center, ring_r, theme::Lerp(IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 240), t_sel), 32, 2.0f);
    } else if (hovered) {
        dl->AddCircle(center, ring_r, IM_COL32(255, 255, 255, 90), 32, 1.5f);
    }

    if (hovered && tooltip && tooltip[0] != '\0') {
        ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

// Fila de muestras de color con ajuste de línea automático al ancho disponible
bool RenderSwatchRow(const char* id_prefix, int* selected_idx, float diameter, float gap) {
    bool changed = false;
    float avail = ImGui::GetContentRegionAvail().x;
    float x0 = ImGui::GetCursorPosX();
    float x = 0.0f;
    ImGui::PushID(id_prefix);
    for (int i = 0; i < kNumColorPresets; ++i) {
        if (i > 0) {
            if (x + gap + diameter <= avail) {
                ImGui::SameLine(0.0f, gap);
                x += gap;
            } else {
                ImGui::SetCursorPosX(x0);
                x = 0.0f;
            }
        }
        ImGui::PushID(i);
        bool sel = (*selected_idx == i);
        if (RenderColorSwatch("##sw", kColorPresets[i].col_u32, sel, i == 0,
                              i == 0 ? "Sin color" : kColorPresets[i].name, diameter)) {
            *selected_idx = (sel && i != 0) ? 0 : i;
            changed = true;
        }
        ImGui::PopID();
        x += diameter;
    }
    ImGui::PopID();
    return changed;
}

fs::path find_assets_dir() {
    std::vector<fs::path> candidates;
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        candidates.push_back(exe.parent_path() / "assets");
        candidates.push_back(exe.parent_path().parent_path() / "assets");
        candidates.push_back(exe.parent_path().parent_path() / "share" / "wallpaper-finder" / "assets");
    }
#ifdef WF_ASSETS_DIR
    candidates.emplace_back(WF_ASSETS_DIR);
#endif
    candidates.push_back(fs::current_path(ec) / "assets");
    for (const auto& c : candidates) {
        if (fs::exists(c / "fonts" / "Inter-Regular.ttf", ec)) return c;
    }
    return {};
}

std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

} // namespace

UIManager::UIManager()
    : wallpaper_setter_(Config::instance().get_desktop_environment()) {
    thread_pool_ = std::make_shared<ThreadPool>(4);
    texture_cache_ = std::make_unique<TextureCache>(thread_pool_);
    provider_ = std::make_shared<WallhavenProvider>();
}

UIManager::~UIManager() {
    shutdown();
}

bool UIManager::init(int width, int height, const char* title) {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        std::cerr << "Error: No se pudo inicializar GLFW." << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    }

    if (!window_) {
        std::cerr << "Error: No se pudo crear la ventana GLFW." << std::endl;
        glfwTerminate();
        return false;
    }

    glfwSetWindowSizeLimits(window_, 960, 600, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    load_window_icon();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // La disposición es fija; no ensuciar el directorio de trabajo

    load_fonts();
    setup_style();
    load_brand_logo();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    check_for_updates_async();

    trigger_search(true, true);
    return true;
}

void UIManager::load_fonts() {
    ImGuiIO& io = ImGui::GetIO();
    fs::path assets = find_assets_dir();

    // Fuente auxiliar para símbolos que Inter (subconjunto latino) no incluye
    const char* symbol_fallbacks[] = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"
    };
    const char* symbol_font = nullptr;
    for (const char* f : symbol_fallbacks) {
        if (fs::exists(f)) { symbol_font = f; break; }
    }

    // Respaldo del sistema si los recursos empaquetados no están disponibles
    const char* system_regular[] = {
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/TTF/OpenSans-Regular.ttf",
        "/usr/share/fonts/cantarell/Cantarell-VF.otf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf"
    };

    auto add_font = [&](const char* inter_file, const char* noto_file) -> ImFont* {
        ImFont* font = nullptr;
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.PixelSnapH = false;
        if (!assets.empty() && fs::exists(assets / "fonts" / inter_file)) {
            font = io.Fonts->AddFontFromFileTTF((assets / "fonts" / inter_file).string().c_str(), theme::kFontBase, &cfg);
        }
        if (!font) {
            std::string noto = std::string("/usr/share/fonts/noto/") + noto_file;
            if (fs::exists(noto)) font = io.Fonts->AddFontFromFileTTF(noto.c_str(), theme::kFontBase, &cfg);
        }
        if (!font) {
            for (const char* f : system_regular) {
                if (fs::exists(f)) { font = io.Fonts->AddFontFromFileTTF(f, theme::kFontBase, &cfg); break; }
            }
        }
        if (font && symbol_font) {
            ImFontConfig merge;
            merge.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(symbol_font, theme::kFontBase, &merge);
        }
        return font;
    };

    auto& f = fonts();
    f.regular  = add_font("Inter-Regular.ttf",  "NotoSans-Regular.ttf");
    f.medium   = add_font("Inter-Medium.ttf",   "NotoSans-Medium.ttf");
    f.semibold = add_font("Inter-SemiBold.ttf", "NotoSans-SemiBold.ttf");
    f.bold     = add_font("Inter-Bold.ttf",     "NotoSans-Bold.ttf");

    if (!f.regular) f.regular = io.Fonts->AddFontDefault();
    if (!f.medium) f.medium = f.regular;
    if (!f.semibold) f.semibold = f.medium;
    if (!f.bold) f.bold = f.semibold;
    io.FontDefault = f.regular;
}

void UIManager::shutdown() {
    if (is_monitor_fullscreen_) {
        set_monitor_fullscreen(false);
    }

    if (logo_texture_id_ != 0) {
        glDeleteTextures(1, &logo_texture_id_);
        logo_texture_id_ = 0;
    }

    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }

    if (texture_cache_) {
        texture_cache_->clear();
    }
}

void UIManager::set_monitor_fullscreen(bool enable) {
    if (!window_ || is_monitor_fullscreen_ == enable) {
        return;
    }

    if (enable) {
        was_maximized_ = (glfwGetWindowAttrib(window_, GLFW_MAXIMIZED) == GLFW_TRUE);
        glfwGetWindowPos(window_, &saved_win_x_, &saved_win_y_);
        glfwGetWindowSize(window_, &saved_win_w_, &saved_win_h_);

        // Encontrar el monitor que mejor albergue la ventana actual
        GLFWmonitor* best_monitor = nullptr;
        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        if (monitors && count > 0) {
            int best_area = 0;
            for (int i = 0; i < count; ++i) {
                int mx = 0, my = 0;
                glfwGetMonitorPos(monitors[i], &mx, &my);
                const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
                if (!mode) continue;

                int inter_w = std::max(0, std::min(saved_win_x_ + saved_win_w_, mx + mode->width) - std::max(saved_win_x_, mx));
                int inter_h = std::max(0, std::min(saved_win_y_ + saved_win_h_, my + mode->height) - std::max(saved_win_y_, my));
                int area = inter_w * inter_h;
                if (area > best_area) {
                    best_area = area;
                    best_monitor = monitors[i];
                }
            }
        }
        if (!best_monitor) {
            best_monitor = glfwGetPrimaryMonitor();
        }

        if (best_monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(best_monitor);
            if (mode) {
                glfwSetWindowMonitor(window_, best_monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                is_monitor_fullscreen_ = true;
            }
        }
    } else {
        glfwSetWindowMonitor(window_, nullptr, saved_win_x_, saved_win_y_, saved_win_w_, saved_win_h_, 0);
        if (was_maximized_) {
            glfwMaximizeWindow(window_);
        }
        is_monitor_fullscreen_ = false;
    }
}

void UIManager::load_window_icon() {
    if (!window_) return;
    fs::path assets = find_assets_dir();
    if (assets.empty()) return;

    std::vector<int> sizes = { 256, 128, 64, 48, 32, 16 };
    std::vector<GLFWimage> glfw_images;
    std::vector<unsigned char*> raw_pixels;

    for (int s : sizes) {
        fs::path p = assets / ("wallfinder-" + std::to_string(s) + ".png");
        if (!fs::exists(p)) continue;
        int w = 0, h = 0, channels = 0;
        unsigned char* px = stbi_load(p.string().c_str(), &w, &h, &channels, 4);
        if (px) {
            glfw_images.push_back({ w, h, px });
            raw_pixels.push_back(px);
        }
    }

    if (glfw_images.empty()) {
        fs::path master = assets / "wallfinder.png";
        if (fs::exists(master)) {
            int w = 0, h = 0, channels = 0;
            unsigned char* px = stbi_load(master.string().c_str(), &w, &h, &channels, 4);
            if (px) {
                glfw_images.push_back({ w, h, px });
                raw_pixels.push_back(px);
            }
        }
    }

    if (!glfw_images.empty()) {
        glfwSetWindowIcon(window_, static_cast<int>(glfw_images.size()), glfw_images.data());
    }

    for (unsigned char* px : raw_pixels) {
        stbi_image_free(px);
    }
}

void UIManager::load_brand_logo() {
    fs::path assets = find_assets_dir();
    if (assets.empty()) return;
    fs::path logo_path = assets / "wallfinder_brand.png";
    if (!fs::exists(logo_path)) return;

    int channels = 0;
    unsigned char* pixels = stbi_load(logo_path.string().c_str(), &logo_width_, &logo_height_, &channels, 4);
    if (!pixels) return;

    if (glfwGetCurrentContext() != nullptr) {
        GLuint tex_id = 0;
        glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, logo_width_, logo_height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
        logo_texture_id_ = tex_id;
    }
    stbi_image_free(pixels);
}

void UIManager::check_for_updates_async() {
    if (!thread_pool_) return;
    if (!Config::instance().check_updates_on_startup) return;

    std::string repo = Config::instance().github_repo;
    thread_pool_->enqueue([this, repo]() {
        VersionInfo info = VersionChecker::check_latest(repo, "1.0.0", 5);
        if (info.check_completed && !info.check_failed) {
            update_checked_ = true;
            if (info.update_available) {
                update_available_ = true;
                latest_version_tag_ = info.latest_version;
                release_url_ = info.release_url;
                show_toast("Nueva versión disponible en GitHub: " + info.latest_version, true, 8.0f);
            }
        }
    });
}

void UIManager::setup_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = theme::kFontBase;

    style.WindowRounding = 14.0f;
    style.ChildRounding = theme::kRadius;
    style.FrameRounding = theme::kControlH * 0.5f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 10.0f;

    style.WindowPadding = ImVec2(20.0f, 16.0f);
    style.FramePadding = ImVec2(16.0f, 9.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.CellPadding = ImVec2(18.0f, 0.0f);
    style.ScrollbarSize = 8.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.SeparatorTextBorderSize = 1.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);

    using namespace theme;
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                 = v4(kText);
    c[ImGuiCol_TextDisabled]         = v4(kTextSubtle);
    c[ImGuiCol_WindowBg]             = v4(kBg);
    c[ImGuiCol_ChildBg]              = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]              = v4(IM_COL32(24, 27, 37, 252));
    c[ImGuiCol_Border]               = v4(kBorder);
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = v4(kSurface2);
    c[ImGuiCol_FrameBgHovered]       = v4(kSurface3);
    c[ImGuiCol_FrameBgActive]        = v4(kSurface3);
    c[ImGuiCol_TitleBg]              = v4(kSurface);
    c[ImGuiCol_TitleBgActive]        = v4(kSurface);
    c[ImGuiCol_TitleBgCollapsed]     = v4(kSurface);
    c[ImGuiCol_MenuBarBg]            = v4(kSurface);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = v4(IM_COL32(255, 255, 255, 28));
    c[ImGuiCol_ScrollbarGrabHovered] = v4(IM_COL32(255, 255, 255, 52));
    c[ImGuiCol_ScrollbarGrabActive]  = v4(kAccentRing);
    c[ImGuiCol_CheckMark]            = v4(kAccent);
    c[ImGuiCol_SliderGrab]           = v4(kAccent);
    c[ImGuiCol_SliderGrabActive]     = v4(kAccentHover);
    c[ImGuiCol_Button]               = v4(kSurface2);
    c[ImGuiCol_ButtonHovered]        = v4(kSurface3);
    c[ImGuiCol_ButtonActive]         = v4(kSurface3);
    c[ImGuiCol_Header]               = v4(kAccentSoft);
    c[ImGuiCol_HeaderHovered]        = v4(IM_COL32(124, 131, 255, 48));
    c[ImGuiCol_HeaderActive]         = v4(IM_COL32(124, 131, 255, 70));
    c[ImGuiCol_Separator]            = v4(kBorder);
    c[ImGuiCol_SeparatorHovered]     = v4(kBorderHi);
    c[ImGuiCol_SeparatorActive]      = v4(kAccentRing);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered]    = v4(kAccentSoft);
    c[ImGuiCol_ResizeGripActive]     = v4(kAccentRing);
    c[ImGuiCol_TableBorderLight]     = v4(kBorder);
    c[ImGuiCol_TableBorderStrong]    = v4(kBorder);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.02f, 0.02f, 0.04f, 0.72f);
    c[ImGuiCol_NavCursor]            = v4(kAccentRing);
    c[ImGuiCol_TextSelectedBg]       = v4(IM_COL32(124, 131, 255, 90));
}

SearchQuery UIManager::build_query(int page) const {
    SearchQuery query;
    query.page = page;

    // Palabras clave base del usuario + categorías temáticas
    std::string full_keywords = search_text_;
    std::vector<std::string> thematic_tags;
    if (cat_nature_) thematic_tags.push_back("landscape");
    if (cat_gaming_) thematic_tags.push_back("gaming");
    if (cat_cyberpunk_) thematic_tags.push_back("cyberpunk");
    if (cat_digital_art_) thematic_tags.push_back("digital art");
    if (cat_space_) thematic_tags.push_back("space");
    if (cat_vehicles_) thematic_tags.push_back("vehicles");
    if (cat_fantasy_) thematic_tags.push_back("fantasy");
    if (cat_minimalism_) thematic_tags.push_back("minimalism");
    if (cat_animals_) thematic_tags.push_back("animals");
    if (cat_architecture_) thematic_tags.push_back("architecture");

    for (const auto& tag : thematic_tags) {
        if (!full_keywords.empty()) full_keywords += " ";
        full_keywords += tag;
    }
    query.keywords = full_keywords;

    query.allow_nsfw = allow_nsfw_;
    query.purity = allow_nsfw_ ? "111" : "100";

    switch (selected_resolution_idx_) {
        case 1: query.resolution = ResolutionFilter::FHD_1080P; break;
        case 2: query.resolution = ResolutionFilter::QHD_1440P; break;
        case 3: query.resolution = ResolutionFilter::UHD_4K; break;
        case 4: query.resolution = ResolutionFilter::ULTRAWIDE_21_9; break;
        case 5: query.resolution = ResolutionFilter::CUSTOM; query.custom_resolution = custom_resolution_; break;
        case 0:
        default: query.resolution = ResolutionFilter::ANY; break;
    }

    // Categorías de Wallhaven: General (100), Anime (010), Personas (001)
    // Cuando ninguna categoría está seleccionada, el comportamiento por defecto es buscar en el catálogo general ("111").
    bool has_any_cat = (cat_anime_ || cat_people_ || cat_nature_ || cat_gaming_ || 
                        cat_cyberpunk_ || cat_digital_art_ || cat_space_ || cat_vehicles_ ||
                        cat_fantasy_ || cat_minimalism_ || cat_animals_ || cat_architecture_);
    if (!has_any_cat) {
        query.categories = "111"; // Catálogo general completo por defecto
    } else {
        bool need_general = (cat_nature_ || cat_gaming_ || cat_cyberpunk_ || 
                             cat_digital_art_ || cat_space_ || cat_vehicles_ ||
                             cat_fantasy_ || cat_minimalism_ || cat_animals_ || cat_architecture_);
        bool need_anime = cat_anime_;
        bool need_people = cat_people_;

        std::string cats;
        cats += (need_general ? '1' : '0');
        cats += (need_anime ? '1' : '0');
        cats += (need_people ? '1' : '0');
        query.categories = cats;
    }

    int sort = std::clamp(selected_sort_idx_, 0, kNumSortOptions - 1);
    query.sorting = kSortOptions[sort].key;

    auto preset_hex = [](int idx) -> std::string {
        return (idx > 0 && idx < kNumColorPresets) ? kColorPresets[idx].hex : "";
    };
    std::string c1 = preset_hex(selected_color_1_idx_);
    std::string c2 = preset_hex(selected_color_2_idx_);

    if (combine_colors_ && !c1.empty() && !c2.empty()) {
        query.color = c1;              // Dominante
        query.color_secondary = c2;    // Acento
        query.combine_colors = true;
        query.color_temperature.clear();
    } else if (!c1.empty()) {
        query.color = c1;
        query.color_temperature.clear();
    } else if (combine_colors_ && !c2.empty()) {
        query.color = c2;
        query.color_temperature.clear();
    } else {
        const char* temp_keys[] = { "", "warm", "cool" };
        if (selected_temp_idx_ > 0 && selected_temp_idx_ < 3) {
            query.color_temperature = temp_keys[selected_temp_idx_];
        }
    }
    return query;
}

void UIManager::reset_filters() {
    allow_nsfw_ = false;
    selected_resolution_idx_ = 0;
    cat_anime_ = false;
    cat_people_ = false;
    cat_nature_ = false;
    cat_gaming_ = false;
    cat_cyberpunk_ = false;
    cat_digital_art_ = false;
    cat_space_ = false;
    cat_vehicles_ = false;
    cat_fantasy_ = false;
    cat_minimalism_ = false;
    cat_animals_ = false;
    cat_architecture_ = false;
    selected_sort_idx_ = 0;
    selected_temp_idx_ = 0;
    selected_color_1_idx_ = 0;
    selected_color_2_idx_ = 0;
    combine_colors_ = false;
    active_color_tab_ = ColorFilterTab::ByColor;
}

void UIManager::trigger_search(bool reset, bool immediate) {
    if (!reset) {
        load_more();
        return;
    }

    if (immediate) {
        search_pending_ = false;
        execute_search(true);
    } else {
        search_pending_reset_ = true;
        search_debounce_deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        search_pending_ = true;
        show_toast("Aplicando filtros…", true, 0.8f);
    }
}

void UIManager::process_pending_search() {
    if (!search_pending_.load()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (now >= search_debounce_deadline_) {
        search_pending_ = false;
        execute_search(search_pending_reset_.load());
    }
}

void UIManager::execute_search(bool reset) {
    if (!reset) {
        load_more();
        return;
    }

    // Cada búsqueda nueva invalida las anteriores: así ningún cambio de filtro se pierde
    unsigned gen = ++search_generation_;
    is_searching_ = true;
    is_loading_more_ = false;
    has_more_ = true;
    next_page_ = 2;
    {
        std::lock_guard<std::mutex> lock(items_mutex_);
        items_.clear();
        loaded_item_ids_.clear();
    }
    show_toast("Buscando fondos…", true, 2.0f);

    SearchQuery query = build_query(1);
    thread_pool_->enqueue([this, query, gen]() {
        // Descarte temprano si la búsqueda ya caducó antes de iniciarse en el pool
        if (gen != search_generation_.load()) {
            return;
        }

        auto is_cancelled = [this, gen]() -> bool {
            return gen != search_generation_.load();
        };

        SearchPage page = provider_->search_page(query, is_cancelled);
        if (gen != search_generation_.load()) {
            return; // Resultado obsoleto
        }

        size_t total_count = 0;
        {
            std::lock_guard<std::mutex> lock(items_mutex_);
            for (auto& item : page.items) {
                if (loaded_item_ids_.insert(item.id).second) {
                    items_.push_back(std::move(item));
                }
            }
            total_count = items_.size();
        }

        next_page_ = page.next_page;
        has_more_ = page.has_more;
        is_searching_ = false;

        if (page.http_status == 429) {
            show_toast("Límite de peticiones de Wallhaven (429). Espera unos segundos…", false, 5.0f);
        } else if (total_count > 0) {
            show_toast(std::to_string(total_count) + " fondos encontrados", true);
        } else {
            show_toast("Sin resultados: prueba con otros filtros", false);
        }
    });
}

void UIManager::load_more() {
    if (is_searching_.load() || is_loading_more_.load() || !has_more_.load()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (now - last_load_more_time_ < std::chrono::milliseconds(800)) {
        return;
    }
    last_load_more_time_ = now;

    is_loading_more_ = true;
    unsigned gen = search_generation_.load();
    SearchQuery query = build_query(next_page_.load());

    thread_pool_->enqueue([this, query, gen]() {
        if (gen != search_generation_.load()) {
            is_loading_more_ = false;
            return;
        }

        auto is_cancelled = [this, gen]() -> bool {
            return gen != search_generation_.load();
        };

        SearchPage page = provider_->search_page(query, is_cancelled);
        if (gen != search_generation_.load()) {
            is_loading_more_ = false;
            return;
        }

        size_t new_added = 0;
        {
            std::lock_guard<std::mutex> lock(items_mutex_);
            for (auto& item : page.items) {
                if (loaded_item_ids_.insert(item.id).second) {
                    items_.push_back(std::move(item));
                    ++new_added;
                }
            }
        }

        next_page_ = page.next_page;
        // Con filtros estrictos una tanda puede no aportar nada aunque queden páginas
        has_more_ = page.has_more;
        (void)new_added;
        is_loading_more_ = false;

        if (page.http_status == 429) {
            show_toast("Límite de peticiones de Wallhaven (429). Espera unos segundos…", false, 5.0f);
        }
    });
}

void UIManager::show_toast(const std::string& message, bool is_success, float duration_seconds) {
    std::lock_guard<std::mutex> lock(toast_mutex_);
    toast_message_ = message;
    toast_is_success_ = is_success;
    toast_expiry_ = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(static_cast<int>(duration_seconds * 1000.0f));
}

std::string UIManager::get_toast(bool& out_is_success) {
    std::lock_guard<std::mutex> lock(toast_mutex_);
    auto now = std::chrono::steady_clock::now();
    out_is_success = toast_is_success_;
    if (now > toast_expiry_) {
        out_is_success = true;
        return "Listo";
    }
    return toast_message_;
}

void UIManager::run() {
    if (!window_) {
        return;
    }

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        texture_cache_->process_pending_uploads(6);
        process_pending_search();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render_ui();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImVec4 bg = theme::v4(theme::kBg);
        glClearColor(bg.x, bg.y, bg.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

void UIManager::render_ui() {
    ImGuiIO& io = ImGui::GetIO();

    // Atajos de zoom estilo navegador (Ctrl + '+', Ctrl + '-', Ctrl + '0', Ctrl + rueda)
    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
            card_zoom_width_ = std::min(kZoomMax, card_zoom_width_ + kZoomStep);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
            card_zoom_width_ = std::max(kZoomMin, card_zoom_width_ - kZoomStep);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_0) || ImGui::IsKeyPressed(ImGuiKey_Keypad0)) {
            card_zoom_width_ = kZoomDefault;
        }
        if (io.MouseWheel != 0.0f) {
            card_zoom_width_ = std::clamp(card_zoom_width_ + io.MouseWheel * 20.0f, kZoomMin, kZoomMax);
        }
        // Ctrl + F: enfocar búsqueda
        if (ImGui::IsKeyPressed(ImGuiKey_F) && !preview_modal_.is_open()) {
            focus_search_ = true;
        }
    }
    // "/" enfoca la búsqueda cuando no se está escribiendo (convención de la web)
    if (!io.WantTextInput && !preview_modal_.is_open() && ImGui::IsKeyPressed(ImGuiKey_Slash)) {
        focus_search_ = true;
    }

    // Sincronizar estado de pantalla completa entre PreviewModal y la ventana física GLFW
    if (preview_modal_.is_fullscreen() != is_monitor_fullscreen_) {
        set_monitor_fullscreen(preview_modal_.is_fullscreen());
    }

    // Escape: cierra la vista previa (si está en pantalla completa, primero sale de ella)
    if (preview_modal_.is_open() && !preview_modal_.is_fullscreen() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        preview_modal_.close();
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 19.0f));

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("MainWindow", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // 1. Barra superior
    render_top_bar();

    // 2. Panel de filtros plegable
    if (show_filters_) {
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        render_filters_panel();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
    } else {
        // Cuadrícula alineada exactamente a 74.0f:
        // Margen superior = 19px, Altura de barra = 36px, Margen inferior = 19px.
        // Total = 74px exactos, manteniendo la distancia idéntica y centrando buscador y botones.
        ImGui::SetCursorPosY(74.0f);
    }

    // 3. Cuadrícula desplazable
    const float status_bar_height = 34.0f;
    float content_height = ImGui::GetContentRegionAvail().y - status_bar_height - 10.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 6.0f));
    ImGui::BeginChild("ScrollableGridRegion", ImVec2(0, content_height), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    {
        std::vector<WallpaperItem> items_copy;
        {
            std::lock_guard<std::mutex> lock(items_mutex_);
            items_copy = items_;
        }
        grid_view_.render(
            items_copy,
            *texture_cache_,
            card_zoom_width_,
            is_searching_.load(),
            is_loading_more_.load(),
            has_more_.load(),
            [this](const WallpaperItem& item) {
                preview_modal_.open(item);
            }
        );

        // Scroll infinito: precargar antes de llegar al final, o si el contenido aún
        // no llena la pantalla (filtros estrictos que devolvieron pocos fondos)
        float scroll_y = ImGui::GetScrollY();
        float scroll_max_y = ImGui::GetScrollMaxY();
        bool near_end = scroll_max_y > 40.0f && scroll_y >= scroll_max_y - 600.0f;
        bool underfilled = scroll_max_y <= 40.0f && !items_copy.empty();
        if (near_end || underfilled) {
            load_more();
        }
    }
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    // 4. Barra de estado
    render_status_bar();

    // 5. Modal de vista previa
    preview_modal_.render(*texture_cache_, wallpaper_setter_, *thread_pool_, [this](const std::string& msg, bool ok) {
        show_toast(msg, ok);
    });

    ImGui::End();
}

void UIManager::render_top_bar() {
    using namespace theme;
    const float bar_h = kControlH;
    const float window_w = ImGui::GetWindowWidth();
    const float pad_x = ImGui::GetStyle().WindowPadding.x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float row_y = ImGui::GetCursorPosY();

    // --- IZQUIERDA: Marca WallFinder + botón de filtros -------------------------
    if (logo_texture_id_ != 0 && logo_width_ > 0 && logo_height_ > 0) {
        float aspect = static_cast<float>(logo_width_) / static_cast<float>(logo_height_);
        float target_h = 28.0f;
        float target_w = target_h * aspect;
        ImVec2 p = ImGui::GetCursorScreenPos();
        float logo_y = std::round(p.y + (bar_h - target_h) * 0.5f);

        ImVec2 p0(p.x, logo_y);
        ImVec2 p1(p.x + target_w, logo_y + target_h);
        ImTextureRef tex_ref(static_cast<ImTextureID>(logo_texture_id_));
        dl->AddImage(tex_ref, p0, p1);

        ImGui::Dummy(ImVec2(target_w, bar_h));
    } else {
        FontScope fsb(fonts().bold, 18.5f);
        ImVec2 wall_sz = ImGui::CalcTextSize("Wall");
        ImVec2 find_sz = ImGui::CalcTextSize("Finder");
        float total_w = wall_sz.x + find_sz.x;
        ImVec2 p = ImGui::GetCursorScreenPos();
        float text_y = std::round(p.y + (bar_h - wall_sz.y) * 0.5f);

        // "Wall" en blanco brillante
        dl->AddText(ImVec2(p.x, text_y), IM_COL32(255, 255, 255, 255), "Wall");
        // "Finder" en azul vibrante coherente con el acento de la aplicación
        dl->AddText(ImVec2(p.x + wall_sz.x, text_y), kAccentHover, "Finder");

        ImGui::Dummy(ImVec2(total_w, bar_h));
    }

    ImGui::SameLine(0.0f, 18.0f);

    int active_filters = get_active_filter_count();
    {
        // Botón "Filtros" con contador e indicador de estado
        FontScope fs(fonts().semibold, kFontSmall + 1.0f);
        const char* lbl = "Filtros";
        ImVec2 tsz = ImGui::CalcTextSize(lbl);
        float badge_w = active_filters > 0 ? 22.0f : 0.0f;
        float w = 16.0f + 15.0f + 8.0f + tsz.x + (badge_w > 0 ? badge_w + 8.0f : 0.0f) + 8.0f + 12.0f + 14.0f;

        bool clicked = ImGui::InvisibleButton("##FilterToggle", ImVec2(w, bar_h));
        bool hovered = ImGui::IsItemHovered();
        float t_open = Animate(ImGui::GetItemID(), show_filters_ ? 1.0f : 0.0f);
        float t_hov = Animate(ImGui::GetItemID() + 7, hovered ? 1.0f : 0.0f, 18.0f);
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        ImU32 bg = Lerp(Lerp(kSurface2, kSurface3, t_hov), Lerp(kAccentSoft, IM_COL32(124, 131, 255, 60), t_hov), t_open);
        dl->AddRectFilled(a, b, bg, bar_h * 0.5f);
        dl->AddRect(a, b, Lerp(kBorder, kAccentRing, t_open), bar_h * 0.5f);

        float cy = std::round((a.y + b.y) * 0.5f);
        float x = a.x + 16.0f;
        ImU32 fg = Lerp(kText, IM_COL32(255, 255, 255, 255), t_open);
        icons::DrawFilterSliders(dl, ImVec2(x + 7.5f, cy), 15.0f, show_filters_ ? kAccentHover : kTextMuted);
        x += 15.0f + 8.0f;
        dl->AddText(ImVec2(x, std::round(cy - tsz.y * 0.5f)), fg, lbl);
        x += tsz.x + 8.0f;
        if (active_filters > 0) {
            ImVec2 ba(x, cy - 10.0f), bb(x + badge_w, cy + 10.0f);
            dl->AddRectFilled(ba, bb, kAccent, 10.0f);
            FontScope fsb(fonts().bold, kFontCaption);
            char num[16];
            std::snprintf(num, sizeof(num), "%d", active_filters);
            ImVec2 nsz = ImGui::CalcTextSize(num);
            dl->AddText(ImVec2(std::round(x + (badge_w - nsz.x) * 0.5f), std::round(cy - nsz.y * 0.5f)),
                        IM_COL32(255, 255, 255, 255), num);
            x += badge_w + 8.0f;
        }
        // Chevron que rota suavemente según el estado
        ImVec2 cc(x + 6.0f, cy);
        float s = 4.0f;
        float dir = 1.0f - 2.0f * t_open; // 1 = abajo, -1 = arriba
        dl->AddLine(ImVec2(cc.x - s, cc.y - s * 0.5f * dir), ImVec2(cc.x, cc.y + s * 0.5f * dir), kTextMuted, 1.8f);
        dl->AddLine(ImVec2(cc.x, cc.y + s * 0.5f * dir), ImVec2(cc.x + s, cc.y - s * 0.5f * dir), kTextMuted, 1.8f);

        if (clicked) show_filters_ = !show_filters_;
        if (hovered) {
            ImGui::SetTooltip(show_filters_ ? "Ocultar filtros" : "Mostrar filtros de resolución, orden, categorías, tono y color");
        }
    }
    ImGui::SameLine();
    float left_end = ImGui::GetCursorPosX();

    // --- DERECHA: Control de zoom segmentado ------------------------------------
    const float zoom_w = 132.0f;
    const float zoom_x = window_w - pad_x - zoom_w;

    // --- CENTRO: Campo de búsqueda + botón ---------------------------------------
    const float search_btn_w = 104.0f;
    const float gap = 8.0f;
    float search_w = std::clamp(window_w * 0.38f, 300.0f, 640.0f);
    float block_w = search_w + gap + search_btn_w;
    float center_x = std::round((window_w - block_w) * 0.5f);
    float min_x = left_end + 16.0f;
    float max_x = zoom_x - 16.0f - block_w;
    if (center_x < min_x) center_x = min_x;
    if (center_x > max_x) {
        center_x = std::max(min_x, max_x);
        search_w = std::max(200.0f, zoom_x - 16.0f - center_x - gap - search_btn_w);
    }

    ImGui::SetCursorPos(ImVec2(center_x, row_y));
    bool enter_pressed = false;
    {
        FontScope fs(fonts().regular, kFontBase);
        const float font_h = ImGui::GetFontSize();
        const float icon_pad = 42.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(icon_pad, std::floor((bar_h - font_h) * 0.5f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, bar_h * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, v4(kSurface2));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, v4(kSurface3));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, v4(kSurface2));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, v4(kTextSubtle));

        if (focus_search_) {
            ImGui::SetKeyboardFocusHere();
            focus_search_ = false;
        }
        ImGui::SetNextItemWidth(search_w);
        ImGui::SetNextItemAllowOverlap();
        enter_pressed = ImGui::InputTextWithHint(
            "##SearchInput",
            "Buscar: paisajes, anime, cyberpunk, minimalista…",
            search_text_, sizeof(search_text_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        bool input_active = ImGui::IsItemActive();
        bool input_hovered = ImGui::IsItemHovered();
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);

        float cy = std::round((a.y + b.y) * 0.5f);
        icons::DrawSearch(dl, ImVec2(a.x + 22.0f, cy), 15.0f, input_active ? kAccentHover : kTextMuted);
        if (input_active) {
            dl->AddRect(ImVec2(a.x - 1.0f, a.y - 1.0f), ImVec2(b.x + 1.0f, b.y + 1.0f), kAccentRing, bar_h * 0.5f + 1.0f, 0, 1.5f);
        }

        // Botón de limpiar (×) dentro del campo
        if (search_text_[0] != '\0') {
            float cs = 22.0f;
            ImVec2 cpos(b.x - cs - 9.0f, cy - cs * 0.5f);
            ImGui::SetCursorScreenPos(cpos);
            if (ImGui::InvisibleButton("##ClearSearch", ImVec2(cs, cs))) {
                search_text_[0] = '\0';
                trigger_search(true, true);
            }
            bool ch = ImGui::IsItemHovered();
            dl->AddCircleFilled(ImVec2(cpos.x + cs * 0.5f, cy), cs * 0.5f, ch ? kSurface3 : IM_COL32(0, 0, 0, 0), 20);
            icons::DrawClose(dl, ImVec2(cpos.x + cs * 0.5f, cy), 14.0f, ch ? kText : kTextMuted);
            if (ch) ImGui::SetTooltip("Limpiar búsqueda");
        } else if (!input_active && !input_hovered) {
            // Indicador de atajo de teclado
            FontScope fsk(fonts().medium, kFontCaption);
            const char* kbd = "Ctrl F";
            ImVec2 ksz = ImGui::CalcTextSize(kbd);
            ImVec2 ka(b.x - ksz.x - 26.0f, cy - 10.0f), kb(b.x - 12.0f, cy + 10.0f);
            dl->AddRectFilled(ka, kb, kSurface3, 6.0f);
            dl->AddText(ImVec2(ka.x + 7.0f, std::round(cy - ksz.y * 0.5f)), kTextSubtle, kbd);
        }
    }

    ImGui::SetCursorPos(ImVec2(center_x + search_w + gap, row_y));
    bool btn_search = theme::Button("##SearchBtn", icons::DrawSearch, "Buscar",
                                    ImVec2(search_btn_w, bar_h), ButtonKind::Primary);
    if (enter_pressed || btn_search) {
        trigger_search(true, true);
    }

    // --- Zoom: [ − | 100% | + ] dentro de una sola píldora ---------------------
    ImGui::SetCursorPos(ImVec2(zoom_x, row_y));
    {
        ImVec2 a = ImGui::GetCursorScreenPos();
        ImVec2 b(a.x + zoom_w, a.y + bar_h);
        dl->AddRectFilled(a, b, kSurface2, bar_h * 0.5f);
        dl->AddRect(a, b, kBorder, bar_h * 0.5f);

        const float seg = 36.0f;
        auto zoom_btn = [&](const char* id, IconFn icon, float x, bool enabled, const char* tip) {
            ImGui::SetCursorScreenPos(ImVec2(x, a.y));
            if (!enabled) ImGui::BeginDisabled();
            bool c = ImGui::InvisibleButton(id, ImVec2(seg, bar_h));
            if (!enabled) ImGui::EndDisabled();
            bool h = ImGui::IsItemHovered() && enabled;
            ImVec2 cc(x + seg * 0.5f, a.y + bar_h * 0.5f);
            if (h) dl->AddCircleFilled(cc, 14.0f, kSurface3, 24);
            icon(dl, cc, 15.0f, enabled ? (h ? kText : kTextMuted) : kTextSubtle);
            if (h) ImGui::SetTooltip("%s", tip);
            return c;
        };

        if (zoom_btn("##zoom_out", icons::DrawMinus, a.x + 2.0f, card_zoom_width_ > kZoomMin, "Tarjetas más pequeñas (Ctrl −)")) {
            card_zoom_width_ = std::max(kZoomMin, card_zoom_width_ - kZoomStep);
        }
        if (zoom_btn("##zoom_in", icons::DrawPlus, b.x - seg - 2.0f, card_zoom_width_ < kZoomMax, "Tarjetas más grandes (Ctrl +)")) {
            card_zoom_width_ = std::min(kZoomMax, card_zoom_width_ + kZoomStep);
        }

        // Porcentaje central (doble clic = restablecer)
        int zoom_pct = static_cast<int>(std::round((card_zoom_width_ / kZoomDefault) * 100.0f));
        char pct[16];
        std::snprintf(pct, sizeof(pct), "%d%%", zoom_pct);
        float mid_w = zoom_w - seg * 2.0f - 4.0f;
        ImGui::SetCursorScreenPos(ImVec2(a.x + 2.0f + seg, a.y));
        ImGui::InvisibleButton("##zoom_reset", ImVec2(mid_w, bar_h));
        bool mh = ImGui::IsItemHovered();
        if (mh && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) card_zoom_width_ = kZoomDefault;
        if (mh) ImGui::SetTooltip("Doble clic para restablecer (Ctrl 0)");
        FontScope fsz(fonts().semibold, kFontSmall + 0.5f);
        ImVec2 psz = ImGui::CalcTextSize(pct);
        dl->AddText(ImVec2(std::round(a.x + 2.0f + seg + (mid_w - psz.x) * 0.5f), std::round(a.y + (bar_h - psz.y) * 0.5f)),
                    mh ? kText : kTextMuted, pct);
    }

    // Restaurar cursor a la siguiente línea tras la barra
    ImGui::SetCursorPos(ImVec2(pad_x, row_y + bar_h));
}

void UIManager::render_filters_panel() {
    using namespace theme;
    const int active_filters = get_active_filter_count();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, v4(kSurface));
    ImGui::PushStyleColor(ImGuiCol_Border, v4(kBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 10.0f));

    ImGui::BeginChild("FiltersPanel", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // Espaciado vertical reducido entre elementos para mantener títulos y controles pegados y compactos
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 3.0f));

    // --- Encabezado del panel ---------------------------------------------------
    {
        float header_h = 24.0f;
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        {
            FontScope fs(fonts().semibold, 14.5f);
            ImVec2 tsz = ImGui::CalcTextSize("Filtros avanzados");
            dl->AddText(ImVec2(p.x, std::round(p.y + (header_h - tsz.y) * 0.5f)), kText, "Filtros avanzados");
            float x = p.x + tsz.x + 10.0f;
            FontScope fs2(fonts().regular, kFontSmall);
            std::string sub = active_filters == 0 ? "Sin filtros activos"
                            : (std::to_string(active_filters) + (active_filters == 1 ? " filtro activo" : " filtros activos"));
            ImVec2 ssz = ImGui::CalcTextSize(sub.c_str());
            dl->AddText(ImVec2(x, std::round(p.y + (header_h - ssz.y) * 0.5f)), active_filters ? kAccentHover : kTextSubtle, sub.c_str());
        }

        float nsfw_w = 112.0f;
        float reset_w = 120.0f;
        float gap_btn = 8.0f;
        float total_right = reset_w + gap_btn + nsfw_w;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - total_right);

        // Botón NSFW (+18) con advertencia visual
        if (theme::PillToggle("##FilterNSFW", icons::DrawAlert, "NSFW (+18)", allow_nsfw_, nsfw_w, header_h,
                              allow_nsfw_ ? IM_COL32(239, 68, 68, 255) : 0)) {
            allow_nsfw_ = !allow_nsfw_;
            trigger_search(true, false);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Permitir contenido explícito / para adultos (+18) en Reddit y Wallhaven");
        }

        ImGui::SameLine(0.0f, gap_btn);
        if (theme::Button("##ResetFilters", icons::DrawReset, "Restablecer", ImVec2(reset_w, header_h),
                          ButtonKind::Ghost, active_filters > 0)) {
            reset_filters();
            trigger_search(true, true);
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    const float avail_panel_w = ImGui::GetContentRegionAvail().x;
    const bool is_narrow = (avail_panel_w < 820.0f);

    auto styled_combo_begin = []() {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 9.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kControlH * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, v4(kSurface2));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, v4(kSurface3));
    };
    auto styled_combo_end = []() {
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(5);
    };
    // Combo con ítems de altura cómoda y marca en el seleccionado
    auto combo = [&](const char* id, int* idx, const char* const* items, int count, const char* const* hints) {
        bool changed = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        FontScope fs(fonts().medium, kFontSmall + 1.0f);
        if (ImGui::BeginCombo(id, items[*idx], ImGuiComboFlags_HeightLarge)) {
            ImGui::PopStyleVar(1); // ItemSpacing interno
            for (int i = 0; i < count; ++i) {
                bool sel = (*idx == i);
                ImGui::PushID(i);
                ImVec2 p = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##opt", sel, 0, ImVec2(0.0f, 30.0f))) {
                    *idx = i;
                    changed = true;
                }
                if (hints && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hints[i]);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 tsz = ImGui::CalcTextSize(items[i]);
                dl->AddText(ImVec2(p.x + 10.0f, std::round(p.y + (30.0f - tsz.y) * 0.5f)), sel ? kText : kTextMuted, items[i]);
                if (sel) {
                    float rx = p.x + ImGui::GetContentRegionAvail().x - 14.0f;
                    icons::DrawCheck(dl, ImVec2(rx, p.y + 15.0f), 13.0f, kAccentHover);
                }
                if (sel) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
            ImGui::EndCombo();
        }
        return changed;
    };

    auto render_section_resolution_order = [&]() {
        SectionLabel("RESOLUCIÓN");
        styled_combo_begin();
        if (combo("##ResCombo", &selected_resolution_idx_, kResolutionOptions, IM_ARRAYSIZE(kResolutionOptions), nullptr)) {
            trigger_search(true, false);
        }
        styled_combo_end();

        if (selected_resolution_idx_ == 5) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 7.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.0f);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputTextWithHint("##CustomRes", "Ancho x Alto (ej. 2560x1080) ↵", custom_resolution_,
                                         sizeof(custom_resolution_), ImGuiInputTextFlags_EnterReturnsTrue)) {
                trigger_search(true, true);
            }
            ImGui::PopStyleVar(2);
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        SectionLabel("ORDENAR POR");
        const char* sort_labels[kNumSortOptions];
        const char* sort_hints[kNumSortOptions];
        for (int i = 0; i < kNumSortOptions; ++i) {
            sort_labels[i] = kSortOptions[i].label;
            sort_hints[i] = kSortOptions[i].hint;
        }
        styled_combo_begin();
        if (combo("##SortCombo", &selected_sort_idx_, sort_labels, kNumSortOptions, sort_hints)) {
            trigger_search(true, false);
        }
        styled_combo_end();
    };

    auto render_section_categories = [&]() {
        SectionLabel("CATEGORÍAS");
        struct Cat {
            const char* id;
            const char* label;
            IconFn icon;
            bool* v;
            const char* tip;
        };
        Cat cats[12] = {
            { "##cat_a", "Anime",        icons::DrawAnime,        &cat_anime_,        "Anime, manga e ilustración japonesa" },
            { "##cat_p", "Personas",     icons::DrawUser,         &cat_people_,       "Retratos, modelos y fotografía de personas" },
            { "##cat_m", "Paisajes",     icons::DrawMountain,     &cat_nature_,       "Naturaleza, montañas, bosques y vistas panorámicas" },
            { "##cat_g", "Videojuegos",  icons::DrawGamepad,      &cat_gaming_,       "Gaming, consolas, personajes y captura de juegos" },
            { "##cat_c", "Cyberpunk",    icons::DrawBolt,         &cat_cyberpunk_,    "Sci-Fi, ciudades neón, futurismo y estética cyberpunk" },
            { "##cat_d", "Arte Digital", icons::DrawPalette,      &cat_digital_art_,  "Concept art, renders 3D e ilustraciones digitales" },
            { "##cat_s", "Espacio",      icons::DrawSpace,        &cat_space_,        "Cosmos, galaxias, nebulosas, planetas y astronomía" },
            { "##cat_v", "Vehículos",    icons::DrawVehicles,     &cat_vehicles_,     "Autos, supercars, motocicletas, aviones y transporte" },
            { "##cat_f", "Fantasía",     icons::DrawFantasy,      &cat_fantasy_,      "Magia, reinos fantásticos, mitología y castillos" },
            { "##cat_mn","Minimalista",  icons::DrawMinimalism,   &cat_minimalism_,   "Composiciones limpias, formas simples y elegancia sutil" },
            { "##cat_an","Animales",     icons::DrawAnimals,      &cat_animals_,      "Fauna silvestre, mascotas, aves y vida salvaje" },
            { "##cat_ar","Arquitectura", icons::DrawArchitecture, &cat_architecture_, "Ciudades, rascacielos, metrópolis y diseño urbano" }
        };

        const float g = 6.0f;
        float avail = ImGui::GetContentRegionAvail().x;
        int cols = std::clamp(static_cast<int>((avail + g) / (108.0f + g)), 2, 4);
        float w = std::min(116.0f, std::floor((avail - (cols - 1) * g) / cols));
        for (int i = 0; i < 12; ++i) {
            if (i % cols != 0) {
                ImGui::SameLine(0.0f, g);
            } else if (i > 0) {
                ImGui::Dummy(ImVec2(0.0f, g - ImGui::GetStyle().ItemSpacing.y));
            }

            // Cada categoría tiene su propio icono vectorial y NINGUNA palomita
            if (PillToggle(cats[i].id, cats[i].icon, cats[i].label, *cats[i].v, w, 25.0f)) {
                *cats[i].v = !*cats[i].v;
                trigger_search(true, false);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", cats[i].tip);
        }
    };

    auto render_sub_tone = [&]() {
        struct Tone { const char* id; const char* label; IconFn icon; ImU32 tint; const char* desc; };
        const Tone tones[3] = {
            { "##t_all",  "Todos",     icons::DrawSparkle,   0,                           "Sin restricción de tono" },
            { "##t_warm", "Cálidos",   icons::DrawFlame,     IM_COL32(251, 146, 60, 255), "Rojos, naranjas, amarillos y rosas" },
            { "##t_cool", "Fríos",     icons::DrawSnowflake, IM_COL32(56, 189, 248, 255), "Azules, cianes, verdes y violetas" }
        };
        const float g = 6.0f;
        float avail = ImGui::GetContentRegionAvail().x;
        int cols = 3;
        float w = std::min(116.0f, std::floor((avail - (cols - 1) * g) / cols));
        for (int i = 0; i < 3; ++i) {
            if (i % cols != 0) ImGui::SameLine(0.0f, g);
            else if (i > 0) ImGui::Dummy(ImVec2(0.0f, g - ImGui::GetStyle().ItemSpacing.y));
            bool active = (selected_temp_idx_ == i);
            if (PillToggle(tones[i].id, tones[i].icon, tones[i].label, active, w, 25.0f,
                           (active || i == 0) ? 0 : tones[i].tint)) {
                if (selected_temp_idx_ == i && i != 0) {
                    selected_temp_idx_ = 0; // Deseleccionar al hacer clic de nuevo
                } else {
                    selected_temp_idx_ = i;
                    if (i != 0) {
                        // Exclusividad mutua impecable: tono resetea color 1, color 2 y bicolor
                        selected_color_1_idx_ = 0;
                        selected_color_2_idx_ = 0;
                        combine_colors_ = false;
                    }
                }
                trigger_search(true, false);
            }
        }
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        FontScope fs(fonts().regular, kFontSmall);
        ImGui::PushStyleColor(ImGuiCol_Text, v4(kTextSubtle));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(tones[std::clamp(selected_temp_idx_, 0, 2)].desc);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    };

    auto render_sub_color = [&]() {
        const float diam = 20.0f;
        const float sgap = 4.0f;

        if (!combine_colors_) {
            if (RenderSwatchRow("c1", &selected_color_1_idx_, 22.0f, 5.0f)) {
                if (selected_color_1_idx_ != 0) selected_temp_idx_ = 0; // Exclusividad mutua impecable
                trigger_search(true, false);
            }

            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            FontScope fs(fonts().regular, kFontSmall);
            ImGui::PushStyleColor(ImGuiCol_Text, v4(kTextSubtle));
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(selected_color_1_idx_ > 0
                ? "El color elegido domina la imagen y se ordenan por mayor fidelidad perceptual."
                : "Selecciona un color o activa Bicolor para combinar dos.");
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        } else {
            // Modo Bicolor: selectores de color principal y de acento LADO A LADO
            if (ImGui::BeginTable("BicolorSideBySide", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
                ImGui::TableSetupColumn("col_c1", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("col_c2", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableNextRow();

                // Lado izquierdo: Color Principal
                ImGui::TableSetColumnIndex(0);
                {
                    FontScope fs(fonts().semibold, kFontCaption);
                    ImGui::PushStyleColor(ImGuiCol_Text, v4(kText));
                    ImGui::TextUnformatted("Color principal");
                    ImGui::PopStyleColor();
                    if (RenderSwatchRow("c1", &selected_color_1_idx_, diam, sgap)) {
                        if (selected_color_1_idx_ != 0) selected_temp_idx_ = 0;
                        trigger_search(true, false);
                    }
                }

                // Lado derecho: Color de Acento
                ImGui::TableSetColumnIndex(1);
                {
                    FontScope fs(fonts().semibold, kFontCaption);
                    ImGui::PushStyleColor(ImGuiCol_Text, v4(kText));
                    ImGui::TextUnformatted("Color de acento");
                    ImGui::PopStyleColor();
                    if (RenderSwatchRow("c2", &selected_color_2_idx_, diam, sgap)) {
                        if (selected_color_2_idx_ != 0) selected_temp_idx_ = 0;
                        trigger_search(true, false);
                    }
                }
                ImGui::EndTable();
            }

            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            FontScope fs(fonts().regular, kFontSmall);
            ImGui::PushStyleColor(ImGuiCol_Text, v4(kTextSubtle));
            ImGui::PushTextWrapPos(0.0f);
            if (selected_color_1_idx_ > 0 && selected_color_2_idx_ > 0) {
                ImGui::Text("%s dominante, %s de acento.", kColorPresets[selected_color_1_idx_].name,
                            kColorPresets[selected_color_2_idx_].name);
            } else {
                ImGui::TextUnformatted("Elige el color dominante y el color de acento.");
            }
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }
    };

    auto render_section_color_tone = [&]() {
        SectionLabel("COLOR Y TONO");

        const float tab_gap = 6.0f;
        const float tab_w = 98.0f;
        const float tab_h = 24.0f;

        bool is_color = (active_color_tab_ == ColorFilterTab::ByColor);
        bool is_tone = (active_color_tab_ == ColorFilterTab::ByTone);

        if (PillToggle("##tab_by_color", icons::DrawPalette, "Por color", is_color, tab_w, tab_h)) {
            if (active_color_tab_ != ColorFilterTab::ByColor) {
                active_color_tab_ = ColorFilterTab::ByColor;
                if (selected_temp_idx_ != 0) {
                    selected_temp_idx_ = 0;
                    trigger_search(true, false);
                }
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filtra fondos por color específico o combinación bicolor");

        ImGui::SameLine(0.0f, tab_gap);

        if (PillToggle("##tab_by_tone", icons::DrawSparkle, "Por tono", is_tone, tab_w, tab_h)) {
            if (active_color_tab_ != ColorFilterTab::ByTone) {
                active_color_tab_ = ColorFilterTab::ByTone;
                if (selected_color_1_idx_ != 0 || selected_color_2_idx_ != 0 || combine_colors_) {
                    selected_color_1_idx_ = 0;
                    selected_color_2_idx_ = 0;
                    combine_colors_ = false;
                    trigger_search(true, false);
                }
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filtra fondos por ambiente de temperatura (cálidos, fríos, monocromo)");

        // Interruptor Bicolor ubicado en la misma fila de los botones para máxima compacidad
        if (active_color_tab_ == ColorFilterTab::ByColor) {
            ImGui::SameLine(0.0f, 14.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::PushID("BicolorToggleBox");

            if (ToggleSwitch("##bicolor", &combine_colors_,
                             "Combina un color principal (dominante en la imagen) con un color de acento")) {
                if (!combine_colors_) {
                    selected_color_2_idx_ = 0;
                } else if (selected_color_1_idx_ != 0) {
                    selected_temp_idx_ = 0; // Exclusividad mutua
                }
                trigger_search(true, false);
            }

            ImGui::SameLine(0.0f, 6.0f);
            FontScope fs(fonts().medium, kFontSmall);
            ImGui::PushStyleColor(ImGuiCol_Text, v4(combine_colors_ ? kText : kTextMuted));
            ImGui::TextUnformatted("Bicolor");
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetTooltip("Haz clic para activar o desactivar la combinación de 2 colores");
            }
            if (ImGui::IsItemClicked()) {
                combine_colors_ = !combine_colors_;
                if (!combine_colors_) {
                    selected_color_2_idx_ = 0;
                } else if (selected_color_1_idx_ != 0) {
                    selected_temp_idx_ = 0; // Exclusividad mutua
                }
                trigger_search(true, false);
            }

            ImGui::PopID();
        }

        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        if (active_color_tab_ == ColorFilterTab::ByColor) {
            render_sub_color();
        } else {
            render_sub_tone();
        }
    };

    // --- Rejilla responsiva: 2 columnas en pantallas estrechas (<1000px), 3 columnas principales en pantallas estándar ---
    ImGuiTableFlags tflags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV |
                             ImGuiTableFlags_PadOuterX | ImGuiTableFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(20.0f, is_narrow ? 10.0f : 0.0f));

    if (is_narrow) {
        if (ImGui::BeginTable("FilterGridNarrow", 2, tflags)) {
            ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_WidthStretch;
            ImGui::TableSetupColumn("col_res_order", col_flags, 1.0f);
            ImGui::TableSetupColumn("col_categories", col_flags, 1.15f);

            // Fila 1: Resolución/Orden y Categorías
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            render_section_resolution_order();
            ImGui::TableSetColumnIndex(1);
            render_section_categories();

            ImGui::EndTable();
        }

        // Fila 2 (ancho completo): Color y Tono con selector por pestañas
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        render_section_color_tone();
    } else {
        if (ImGui::BeginTable("FilterGrid3Col", 3, tflags)) {
            ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_WidthStretch;
            ImGui::TableSetupColumn("col_res_order", col_flags, 0.95f);
            ImGui::TableSetupColumn("col_categories", col_flags, 1.15f);
            ImGui::TableSetupColumn("col_color_tone", col_flags, 1.60f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            render_section_resolution_order();

            ImGui::TableSetColumnIndex(1);
            render_section_categories();

            ImGui::TableSetColumnIndex(2);
            render_section_color_tone();

            ImGui::EndTable();
        }
    }
    ImGui::PopStyleVar(); // CellPadding
    ImGui::PopStyleVar(); // ItemSpacing

    ImGui::EndChild();
}

void UIManager::render_status_bar() {
    using namespace theme;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float bar_w = ImGui::GetContentRegionAvail().x;
    float bar_h = 34.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(pos, ImVec2(pos.x + bar_w, pos.y + bar_h), kSurface, bar_h * 0.5f);
    dl->AddRect(pos, ImVec2(pos.x + bar_w, pos.y + bar_h), kBorder, bar_h * 0.5f);

    FontScope fs(fonts().medium, kFontSmall);
    float line_h = ImGui::GetTextLineHeight();
    float cy = pos.y + bar_h * 0.5f;
    float text_y = std::round(cy - line_h * 0.5f);

    // IZQUIERDA: entorno de escritorio + carpeta destino (clic para abrir)
    float x = pos.x + 16.0f;
    DesktopEnvironment de = wallpaper_setter_.get_desktop_environment();
    std::string de_str = to_string(de);
    icons::DrawDesktop(dl, ImVec2(x + 7.0f, cy), 14.0f, kAccentHover);
    x += 14.0f + 7.0f;
    dl->AddText(ImVec2(x, text_y), kText, de_str.c_str());
    x += ImGui::CalcTextSize(de_str.c_str()).x + 12.0f;
    dl->AddLine(ImVec2(x, cy - 8.0f), ImVec2(x, cy + 8.0f), kBorderHi, 1.0f);
    x += 12.0f;

    std::string path_str = Config::instance().wallpaper_dir.string();
    if (const char* home = std::getenv("HOME")) {
        std::string h(home);
        if (!h.empty() && path_str.rfind(h, 0) == 0) path_str = "~" + path_str.substr(h.size());
    }
    ImVec2 path_sz = ImGui::CalcTextSize(path_str.c_str());
    ImGui::SetCursorScreenPos(ImVec2(x - 4.0f, pos.y + 4.0f));
    bool open_dir = ImGui::InvisibleButton("##OpenDir", ImVec2(14.0f + 7.0f + path_sz.x + 8.0f, bar_h - 8.0f));
    bool dir_hov = ImGui::IsItemHovered();
    if (dir_hov) {
        dl->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), kSurface2, (bar_h - 8.0f) * 0.5f);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("Abrir la carpeta de fondos descargados");
    }
    icons::DrawFolder(dl, ImVec2(x + 7.0f, cy), 14.0f, kWarning);
    dl->AddText(ImVec2(x + 21.0f, text_y), dir_hov ? kText : kTextMuted, path_str.c_str());
    if (open_dir) {
        std::error_code ec;
        fs::create_directories(Config::instance().wallpaper_dir, ec);
        std::string cmd = "xdg-open " + shell_quote(Config::instance().wallpaper_dir.string()) + " >/dev/null 2>&1 &";
        int rc = std::system(cmd.c_str());
        (void)rc;
    }

    // CENTRO: fondos cargados y badge de actualización
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(items_mutex_);
        count = items_.size();
    }
    std::string count_str = std::to_string(count) + (count == 1 ? " fondo cargado" : " fondos cargados");
    ImVec2 count_sz = ImGui::CalcTextSize(count_str.c_str());

    float cx = std::round(pos.x + (bar_w - (14.0f + 7.0f + count_sz.x)) * 0.5f);
    icons::DrawSparkle(dl, ImVec2(cx + 7.0f, cy), 14.0f, kAccentHover);
    dl->AddText(ImVec2(cx + 21.0f, text_y), kTextMuted, count_str.c_str());

    // Si hay una actualización disponible en GitHub, mostrar botón interactivo
    if (update_available_.load()) {
        std::string upd_lbl = "★ Actualización " + latest_version_tag_;
        ImVec2 upd_sz = ImGui::CalcTextSize(upd_lbl.c_str());
        float btn_w = upd_sz.x + 22.0f;
        float btn_x = cx + 21.0f + count_sz.x + 18.0f;
        ImVec2 b0(btn_x, cy - 11.0f);
        ImVec2 b1(btn_x + btn_w, cy + 11.0f);

        ImGui::SetCursorScreenPos(b0);
        bool upd_clicked = ImGui::InvisibleButton("##UpdateBadgeBtn", ImVec2(btn_w, 22.0f));
        bool upd_hover = ImGui::IsItemHovered();
        dl->AddRectFilled(b0, b1, upd_hover ? kAccentHover : kAccentSoft, 11.0f);
        dl->AddRect(b0, b1, kAccent, 11.0f);
        dl->AddText(ImVec2(b0.x + 11.0f, std::round(cy - upd_sz.y * 0.5f)),
                    IM_COL32(255, 255, 255, 255), upd_lbl.c_str());
        if (upd_hover) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("Nueva versión disponible. Haz clic para abrir GitHub Releases.");
        }
        if (upd_clicked && !release_url_.empty()) {
            std::string open_cmd = "xdg-open " + shell_quote(release_url_) + " >/dev/null 2>&1 &";
            int rc = std::system(open_cmd.c_str());
            (void)rc;
        }
    }

    // DERECHA: estado / notificación
    bool is_success = true;
    std::string toast = get_toast(is_success);
    bool busy = is_searching_.load() || is_loading_more_.load();
    ImVec2 toast_sz = ImGui::CalcTextSize(toast.c_str());
    float rx = pos.x + bar_w - 16.0f - toast_sz.x;
    ImU32 col = is_success ? kSuccess : kDanger;
    if (busy) {
        float t = static_cast<float>(ImGui::GetTime());
        ImVec2 c(rx - 13.0f, cy);
        dl->PathArcTo(c, 6.0f, t * 7.0f, t * 7.0f + 4.4f, 16);
        dl->PathStroke(kAccentHover, 0, 2.0f);
        col = kTextMuted;
    } else if (is_success) {
        icons::DrawCheck(dl, ImVec2(rx - 13.0f, cy), 13.0f, col);
    } else {
        icons::DrawAlert(dl, ImVec2(rx - 13.0f, cy), 15.0f, col);
    }
    dl->AddText(ImVec2(rx, text_y), busy ? kTextMuted : (is_success ? kTextMuted : kDanger), toast.c_str());

    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(bar_w, bar_h));
}

} // namespace wallpaper
