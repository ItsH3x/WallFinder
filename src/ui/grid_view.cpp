#include "ui/grid_view.hpp"
#include "ui/theme.hpp"
#include "core/texture_cache.hpp"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace wallpaper {

namespace {

using theme::FontScope;
using theme::fonts;

std::string format_size_short(uint64_t bytes) {
    if (bytes == 0) return "";
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (bytes >= 1024 * 1024) {
        ss << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
    } else if (bytes >= 1024) {
        ss << (static_cast<double>(bytes) / 1024.0) << " KB";
    } else {
        ss << bytes << " B";
    }
    return ss.str();
}

std::string format_aspect_ratio(const std::string& ratio_str, int width, int height) {
    float r = 0.0f;
    if (!ratio_str.empty()) {
        try { r = std::stof(ratio_str); } catch (...) {}
    }
    if (r <= 0.0f && width > 0 && height > 0) {
        r = static_cast<float>(width) / static_cast<float>(height);
    }
    if (r <= 0.0f) return "";

    if (std::abs(r - (16.0f / 9.0f)) < 0.035f) return "16:9";
    if (std::abs(r - (21.0f / 9.0f)) < 0.07f || std::abs(r - 2.37f) < 0.07f) return "21:9";
    if (std::abs(r - (16.0f / 10.0f)) < 0.035f) return "16:10";
    if (std::abs(r - (4.0f / 3.0f)) < 0.035f) return "4:3";
    if (std::abs(r - (9.0f / 16.0f)) < 0.035f) return "9:16";
    if (std::abs(r - 1.0f) < 0.035f) return "1:1";
    if (std::abs(r - (32.0f / 9.0f)) < 0.08f) return "32:9";

    return "";
}

} // namespace

GridView::GridView() = default;

void GridView::render(
    const std::vector<WallpaperItem>& items,
    TextureCache& texture_cache,
    float card_target_width,
    bool is_loading,
    bool is_loading_more,
    bool has_more,
    std::function<void(const WallpaperItem& item)> on_item_click
) {
    float avail_w = ImGui::GetContentRegionAvail().x;
    if (avail_w < 100.0f) {
        avail_w = 100.0f;
    }

    // Ancho dinámico de tarjeta con atajos de zoom (Ctrl +/-/rueda, por defecto 220px compacto)
    float min_card_w = std::clamp(card_target_width, 160.0f, 480.0f);
    const float gap = 14.0f;
    int cols = std::max(1, static_cast<int>((avail_w + gap) / (min_card_w + gap)));
    float total_gaps = static_cast<float>(cols - 1) * gap;
    float card_w = (avail_w - total_gaps) / static_cast<float>(cols);
    float thumb_h = card_w * (9.0f / 16.0f);

    if (items.empty()) {
        if (is_loading) {
            // Rejilla de cuadros gris con animación de latido secuencial en todos los cuadros no cargados
            int skeleton_count = std::max(cols * 2, 8);
            for (int s = 0; s < skeleton_count; ++s) {
                if (s % cols != 0) {
                    ImGui::SameLine(0.0f, gap);
                }
                render_skeleton_card(static_cast<size_t>(s), card_w, thumb_h);
            }
            return;
        }

        ImVec2 region = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + region.y * 0.35f);
        const char* msg1 = "No se encontraron fondos de pantalla";
        const char* msg2 = "Prueba buscando con otros términos o ajustando los filtros de resolución y categorías.";
        
        float tw1 = ImGui::CalcTextSize(msg1).x;
        float tw2 = ImGui::CalcTextSize(msg2).x;
        
        ImGui::SetCursorPosX((avail_w - tw1) * 0.5f);
        ImGui::TextColored(ImVec4(0.9f, 0.92f, 0.96f, 1.0f), "%s", msg1);
        
        ImGui::SetCursorPosX((avail_w - tw2) * 0.5f);
        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.70f, 1.0f), "%s", msg2);
        return;
    }

    for (size_t i = 0; i < items.size(); ++i) {
        if (i % cols != 0) {
            ImGui::SameLine(0.0f, gap);
        }
        render_card(items[i], i, texture_cache, card_w, thumb_h, on_item_click);
    }

    // Indicador inferior de carga para scroll infinito: fila de skeletons con latido secuencial
    if (is_loading_more) {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        size_t base_idx = items.size();
        for (int s = 0; s < cols; ++s) {
            if (s % cols != 0) {
                ImGui::SameLine(0.0f, gap);
            }
            render_skeleton_card(base_idx + static_cast<size_t>(s), card_w, thumb_h);
        }
    } else if (!has_more && !items.empty()) {
        ImGui::Spacing();
        ImGui::Spacing();
        const char* end_msg = "✓ Has llegado al final de los resultados";
        float tw = ImGui::CalcTextSize(end_msg).x;
        ImGui::SetCursorPosX((avail_w - tw) * 0.5f);
        ImGui::TextColored(ImVec4(0.50f, 0.55f, 0.68f, 0.9f), "%s", end_msg);
        ImGui::Spacing();
    }
}

void GridView::render_card(
    const WallpaperItem& item,
    size_t item_index,
    TextureCache& texture_cache,
    float card_w,
    float thumb_h,
    std::function<void(const WallpaperItem& item)> on_item_click
) {
    ImGui::PushID(item.id.c_str());

    float footer_h = 48.0f;
    float card_h = thumb_h + footer_h;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + card_w, p0.y + card_h);

    // Frustum Culling / Virtualización robusta:
    // ImGui::IsRectVisible tiene en cuenta el clip rect y el scroll del child window.
    bool is_in_view = ImGui::IsRectVisible(
        ImVec2(p0.x, p0.y - 350.0f),
        ImVec2(p1.x, p1.y + 350.0f)
    );

    if (!is_in_view) {
        ImGui::Dummy(ImVec2(card_w, card_h));
        ImGui::PopID();
        return;
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Capturar interacción con botón invisible
    bool clicked = ImGui::InvisibleButton("##card_interactive", ImVec2(card_w, card_h));
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    // Fondo y borde de tarjeta redondeados y minimalistas según el sistema de diseño
    ImU32 card_bg = hovered ? theme::kSurface2 : theme::kSurface;
    ImU32 border_col = active ? theme::kAccentHover 
                              : (hovered ? theme::kAccentRing : theme::kBorder);

    // Dibujar fondo de tarjeta redondeado (12px)
    draw_list->AddRectFilled(p0, p1, card_bg, theme::kRadius);

    // Área de miniatura
    ImVec2 thumb_p0 = p0;
    ImVec2 thumb_p1 = ImVec2(p0.x + card_w, p0.y + thumb_h);

    TextureInfo tex = texture_cache.get_texture(item.id);
    if (!tex.loaded && !tex.failed) {
        if (!texture_cache.is_loading(item.id)) {
            texture_cache.request_texture(item.id, item.preview_url);
        }
    }

    // Animación de latido secuencial armónico para cuadros no cargados
    float t = static_cast<float>(ImGui::GetTime());
    float phase = t * 3.6f - static_cast<float>(item_index) * 0.22f;
    float s1 = std::sin(phase);
    float s2 = std::sin(phase * 2.0f);
    float pulse = std::max(0.0f, s1 * 0.7f + s2 * 0.3f);
    pulse = pulse * pulse;

    // Animación de fade-in sincronizada al completarse la carga
    ImGuiID fade_id = ImGui::GetID((item.id + "_fade_anim").c_str());
    float fade_t = 0.0f;
    if (tex.loaded && tex.texture_id != 0) {
        fade_t = theme::Animate(fade_id, 1.0f, 5.0f);
    } else {
        ImGui::GetStateStorage()->SetFloat(fade_id, 0.0f);
    }

    // Cuadro completamente gris con baja opacidad y latido secuencial mientras no esté al 100%
    if (fade_t < 1.0f) {
        float remaining = 1.0f - fade_t;
        int alpha = static_cast<int>((18.0f + 36.0f * pulse) * remaining);
        if (alpha > 0) {
            draw_list->AddRectFilled(thumb_p0, thumb_p1, IM_COL32(135, 140, 150, alpha),
                                     theme::kRadius, ImDrawFlags_RoundCornersTop);
            int border_a = static_cast<int>((10.0f + 20.0f * pulse) * remaining);
            if (border_a > 0) {
                draw_list->AddRect(thumb_p0, thumb_p1, IM_COL32(160, 165, 180, border_a),
                                   theme::kRadius, ImDrawFlags_RoundCornersTop, 1.0f);
            }
        }
    }

    if (tex.failed) {
        draw_list->AddRectFilled(thumb_p0, thumb_p1, theme::kSurface2, theme::kRadius, ImDrawFlags_RoundCornersTop);
        const char* fail_txt = "Vista previa no disponible";
        ImVec2 ft_sz = ImGui::CalcTextSize(fail_txt);
        draw_list->AddText(ImVec2(thumb_p0.x + (card_w - ft_sz.x) * 0.5f, thumb_p0.y + (thumb_h - ft_sz.y) * 0.5f),
                           theme::kTextSubtle, fail_txt);
    } else if (tex.loaded && tex.texture_id != 0) {
        int img_alpha = static_cast<int>(fade_t * 255.0f);
        if (img_alpha > 0) {
            ImTextureRef tex_ref(static_cast<ImTextureID>(tex.texture_id));
            draw_list->AddImageRounded(
                tex_ref,
                thumb_p0, thumb_p1,
                ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32(255, 255, 255, img_alpha),
                theme::kRadius,
                ImDrawFlags_RoundCornersTop
            );
        }
    }

    // Badges en imagen con desvanecimiento (fade-in) coordinado con el ritmo de carga
    if (fade_t > 0.05f) {
        float badge_pad_x = 8.0f;
        float badge_pad_y = 3.5f;
        float badge_margin = 7.0f;

        std::string res_str = std::to_string(item.width) + "x" + std::to_string(item.height);
        ImVec2 badge_sz;
        {
            FontScope fs(fonts().medium, theme::kFontCaption);
            badge_sz = ImGui::CalcTextSize(res_str.c_str());
        }
        ImVec2 b_min = ImVec2(thumb_p1.x - badge_sz.x - badge_pad_x * 2.0f - badge_margin, thumb_p1.y - badge_sz.y - badge_pad_y * 2.0f - badge_margin);
        ImVec2 b_max = ImVec2(thumb_p1.x - badge_margin, thumb_p1.y - badge_margin);
        float res_pill_r = (b_max.y - b_min.y) * 0.5f;
        draw_list->AddRectFilled(b_min, b_max, IM_COL32(10, 12, 18, static_cast<int>(225.0f * fade_t)), res_pill_r);
        draw_list->AddRect(b_min, b_max, IM_COL32(80, 85, 105, static_cast<int>(180.0f * fade_t)), res_pill_r);
        {
            FontScope fs(fonts().medium, theme::kFontCaption);
            draw_list->AddText(ImVec2(b_min.x + badge_pad_x, b_min.y + badge_pad_y),
                               IM_COL32(255, 255, 255, static_cast<int>(255.0f * fade_t)), res_str.c_str());
        }

        // Badge de aspecto/ratio en esquina inferior izquierda si existe
        std::string ratio_str = format_aspect_ratio(item.aspect_ratio, item.width, item.height);
        if (!ratio_str.empty()) {
            ImVec2 r_sz;
            {
                FontScope fs(fonts().medium, theme::kFontCaption);
                r_sz = ImGui::CalcTextSize(ratio_str.c_str());
            }
            ImVec2 rb_min = ImVec2(thumb_p0.x + badge_margin, thumb_p1.y - r_sz.y - badge_pad_y * 2.0f - badge_margin);
            ImVec2 rb_max = ImVec2(thumb_p0.x + badge_margin + r_sz.x + badge_pad_x * 2.0f, thumb_p1.y - badge_margin);
            float ratio_pill_r = (rb_max.y - rb_min.y) * 0.5f;
            draw_list->AddRectFilled(rb_min, rb_max, IM_COL32(10, 12, 18, static_cast<int>(215.0f * fade_t)), ratio_pill_r);
            draw_list->AddRect(rb_min, rb_max, IM_COL32(80, 85, 105, static_cast<int>(180.0f * fade_t)), ratio_pill_r);
            {
                FontScope fs(fonts().medium, theme::kFontCaption);
                draw_list->AddText(ImVec2(rb_min.x + badge_pad_x, rb_min.y + badge_pad_y),
                                   IM_COL32(180, 190, 210, static_cast<int>(230.0f * fade_t)), ratio_str.c_str());
            }
        }
    }

    // Pie de tarjeta simétrico y equilibrado
    float footer_y = thumb_p1.y + 7.0f;
    float footer_x = p0.x + 10.0f;

    std::string title_str = "#" + item.id;
    {
        FontScope fs(fonts().semibold, theme::kFontSmall + 1.0f);
        draw_list->AddText(ImVec2(footer_x, footer_y), theme::kText, title_str.c_str());
    }

    std::string sz_str = format_size_short(item.file_size);
    if (!sz_str.empty()) {
        FontScope fs(fonts().medium, theme::kFontCaption);
        ImVec2 sz_dim = ImGui::CalcTextSize(sz_str.c_str());
        draw_list->AddText(ImVec2(p1.x - sz_dim.x - 10.0f, footer_y + 1.0f), theme::kTextMuted, sz_str.c_str());
    }

    // Subtítulo con tag o source
    float sub_y = footer_y + 18.0f;
    std::string tag_hint = !item.tags.empty() ? ("#" + item.tags[0]) : (item.source.empty() ? "Wallhaven" : item.source);
    {
        FontScope fs(fonts().regular, theme::kFontCaption);
        draw_list->AddText(ImVec2(footer_x, sub_y), theme::kTextSubtle, tag_hint.c_str());
    }

    // Borde de la tarjeta (12px redondeado)
    draw_list->AddRect(p0, p1, border_col, theme::kRadius, 0, hovered ? 1.5f : 1.0f);

    // Tooltip al pasar el cursor
    if (hovered) {
        std::string res_tip = std::to_string(item.width) + "x" + std::to_string(item.height);
        ImGui::SetTooltip("Fondo #%s (%s)\nHaz clic para ver detalles y aplicar", item.id.c_str(), res_tip.c_str());
    }

    // Disparar callback al hacer clic
    if (clicked && on_item_click) {
        on_item_click(item);
    }

    ImGui::PopID();
}

void GridView::render_skeleton_card(size_t index, float card_w, float thumb_h) {
    float footer_h = 48.0f;
    float card_h = thumb_h + footer_h;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + card_w, p0.y + card_h);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Fondo base de tarjeta
    dl->AddRectFilled(p0, p1, theme::kSurface, theme::kRadius);

    // Animación de latido secuencial: cuadro completamente gris con baja opacidad
    float t = static_cast<float>(ImGui::GetTime());
    float phase = t * 3.6f - static_cast<float>(index) * 0.22f;
    float s1 = std::sin(phase);
    float s2 = std::sin(phase * 2.0f);
    float pulse = std::max(0.0f, s1 * 0.7f + s2 * 0.3f);
    pulse = pulse * pulse;

    int alpha = static_cast<int>(18.0f + 36.0f * pulse);
    ImVec2 thumb_p1 = ImVec2(p0.x + card_w, p0.y + thumb_h);
    dl->AddRectFilled(p0, thumb_p1, IM_COL32(135, 140, 150, alpha), theme::kRadius, ImDrawFlags_RoundCornersTop);

    int border_a = static_cast<int>(10.0f + 20.0f * pulse);
    dl->AddRect(p0, thumb_p1, IM_COL32(160, 165, 180, border_a), theme::kRadius, ImDrawFlags_RoundCornersTop, 1.0f);

    // Líneas sutiles de skeleton en el footer con el mismo latido secuencial
    float f_alpha = 14.0f + 22.0f * pulse;
    dl->AddRectFilled(ImVec2(p0.x + 10.0f, thumb_p1.y + 10.0f),
                      ImVec2(p0.x + card_w * 0.45f, thumb_p1.y + 20.0f),
                      IM_COL32(135, 140, 150, static_cast<int>(f_alpha)), 4.0f);
    dl->AddRectFilled(ImVec2(p0.x + 10.0f, thumb_p1.y + 26.0f),
                      ImVec2(p0.x + card_w * 0.28f, thumb_p1.y + 34.0f),
                      IM_COL32(135, 140, 150, static_cast<int>(f_alpha * 0.65f)), 3.0f);

    dl->AddRect(p0, p1, theme::kBorder, theme::kRadius);

    ImGui::Dummy(ImVec2(card_w, card_h));
}

} // namespace wallpaper
