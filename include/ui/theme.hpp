#pragma once

// =============================================================================
// Sistema de diseño: tokens de color, tipografía y widgets base reutilizables.
// Estética: oscuro neutro con acento índigo-violeta, superficies escalonadas,
// controles en forma de píldora y jerarquía tipográfica con Inter.
// =============================================================================

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace wallpaper::theme {

// --- Tipografía ---------------------------------------------------------------
struct Fonts {
    ImFont* regular{nullptr};
    ImFont* medium{nullptr};
    ImFont* semibold{nullptr};
    ImFont* bold{nullptr};
};
inline Fonts& fonts() {
    static Fonts f;
    return f;
}

inline constexpr float kFontBase    = 15.0f;
inline constexpr float kFontSmall   = 12.5f;
inline constexpr float kFontCaption = 11.5f;
inline constexpr float kFontTitle   = 18.0f;

struct FontScope {
    FontScope(ImFont* f, float size) { ImGui::PushFont(f, size); }
    ~FontScope() { ImGui::PopFont(); }
    FontScope(const FontScope&) = delete;
    FontScope& operator=(const FontScope&) = delete;
};

// --- Paleta -------------------------------------------------------------------
inline constexpr ImU32 kBg          = IM_COL32(13, 15, 20, 255);
inline constexpr ImU32 kSurface     = IM_COL32(20, 23, 31, 255);
inline constexpr ImU32 kSurface2    = IM_COL32(27, 31, 42, 255);
inline constexpr ImU32 kSurface3    = IM_COL32(36, 41, 55, 255);
inline constexpr ImU32 kBorder      = IM_COL32(255, 255, 255, 16);
inline constexpr ImU32 kBorderHi    = IM_COL32(255, 255, 255, 34);
inline constexpr ImU32 kText        = IM_COL32(232, 235, 243, 255);
inline constexpr ImU32 kTextMuted   = IM_COL32(146, 153, 172, 255);
inline constexpr ImU32 kTextSubtle  = IM_COL32(98, 105, 124, 255);
inline constexpr ImU32 kAccent      = IM_COL32(124, 131, 255, 255);
inline constexpr ImU32 kAccentHover = IM_COL32(145, 151, 255, 255);
inline constexpr ImU32 kAccentDown  = IM_COL32(104, 110, 236, 255);
inline constexpr ImU32 kAccentSoft  = IM_COL32(124, 131, 255, 38);
inline constexpr ImU32 kAccentRing  = IM_COL32(124, 131, 255, 150);
inline constexpr ImU32 kSuccess     = IM_COL32(74, 222, 128, 255);
inline constexpr ImU32 kDanger      = IM_COL32(248, 113, 113, 255);
inline constexpr ImU32 kWarning     = IM_COL32(251, 191, 36, 255);

inline ImVec4 v4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

// --- Métricas -----------------------------------------------------------------
inline constexpr float kControlH = 36.0f;  // Altura estándar de controles
inline constexpr float kChipH    = 32.0f;  // Altura de chips / toggles
inline constexpr float kRadius   = 12.0f;  // Tarjetas y paneles

using IconFn = void (*)(ImDrawList*, ImVec2, float, ImU32);

// Etiqueta de sección: pequeña, seminegrita, con espaciado amplio y color atenuado
inline void SectionLabel(const char* text) {
    FontScope fs(fonts().semibold, kFontCaption);
    ImGui::PushStyleColor(ImGuiCol_Text, v4(kTextSubtle));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

// Interpolación suave por widget (animaciones de hover/activo)
inline float Animate(ImGuiID id, float target, float speed = 14.0f) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    float v = st->GetFloat(id, target);
    float dt = ImGui::GetIO().DeltaTime;
    v += (target - v) * std::min(1.0f, dt * speed);
    if (std::abs(v - target) < 0.002f) v = target;
    st->SetFloat(id, v);
    return v;
}

inline ImU32 Lerp(ImU32 a, ImU32 b, float t) {
    ImVec4 A = v4(a), B = v4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(A.x + (B.x - A.x) * t, A.y + (B.y - A.y) * t,
                                                 A.z + (B.z - A.z) * t, A.w + (B.w - A.w) * t));
}

// Chip/píldora seleccionable con icono opcional. Si width <= 0 se ajusta al contenido.
inline bool PillToggle(const char* id, IconFn icon, const char* label, bool active,
                       float width = 0.0f, float height = kChipH, ImU32 icon_tint = 0) {
    FontScope fs(fonts().medium, kFontSmall + 0.5f);
    const float icon_sz = 14.0f;
    const float gap = (icon && label && *label) ? 7.0f : 0.0f;
    ImVec2 tsz = (label && *label) ? ImGui::CalcTextSize(label) : ImVec2(0, 0);
    float content_w = (icon ? icon_sz : 0.0f) + gap + tsz.x;
    float w = width > 0.0f ? width : content_w + 28.0f;

    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##pill", ImVec2(w, height));
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    ImGuiID aid = ImGui::GetID("##anim");
    float t_act = Animate(aid, active ? 1.0f : 0.0f);
    float t_hov = Animate(aid + 1, hovered ? 1.0f : 0.0f, 18.0f);
    ImGui::PopID();

    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float r = height * 0.5f;

    ImU32 bg = Lerp(Lerp(kSurface2, kSurface3, t_hov), Lerp(kAccentSoft, IM_COL32(124, 131, 255, 60), t_hov), t_act);
    ImU32 border = Lerp(Lerp(kBorder, kBorderHi, t_hov), kAccentRing, t_act);
    if (held) bg = Lerp(bg, kSurface3, 0.5f);
    dl->AddRectFilled(a, b, bg, r);
    dl->AddRect(a, b, border, r, 0, 1.0f);

    ImU32 txt = Lerp(kTextMuted, kText, std::max(t_act, t_hov * 0.6f));
    float x = std::round(a.x + (w - content_w) * 0.5f);
    float cy = std::round((a.y + b.y) * 0.5f);
    if (icon) {
        ImU32 ic = icon_tint ? icon_tint : Lerp(kTextMuted, kAccentHover, t_act);
        icon(dl, ImVec2(x + icon_sz * 0.5f, cy), icon_sz, ic);
        x += icon_sz + gap;
    }
    if (tsz.x > 0.0f) {
        dl->AddText(ImVec2(x, std::round(cy - tsz.y * 0.5f)), txt, label);
    }
    return clicked;
}

// Interruptor estilo iOS/Material
inline bool ToggleSwitch(const char* id, bool* v, const char* tooltip = nullptr) {
    const float h = 20.0f, w = 36.0f;
    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##sw", ImVec2(w, h));
    if (clicked) *v = !*v;
    bool hovered = ImGui::IsItemHovered();
    float t = Animate(ImGui::GetID("##swa"), *v ? 1.0f : 0.0f, 16.0f);
    ImGui::PopID();

    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 track = Lerp(hovered ? kSurface3 : IM_COL32(44, 50, 66, 255), kAccent, t);
    dl->AddRectFilled(a, b, track, h * 0.5f);
    float knob_r = h * 0.5f - 3.0f;
    float kx = a.x + h * 0.5f + (w - h) * t;
    dl->AddCircleFilled(ImVec2(kx, (a.y + b.y) * 0.5f + 1.0f), knob_r, IM_COL32(0, 0, 0, 60), 20);
    dl->AddCircleFilled(ImVec2(kx, (a.y + b.y) * 0.5f), knob_r, IM_COL32(250, 251, 255, 255), 20);
    if (hovered && tooltip) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

// Botón principal/neutro tipo píldora con icono (estilo relleno)
enum class ButtonKind { Primary, Secondary, Ghost, Success };

inline bool Button(const char* id, IconFn icon, const char* label, ImVec2 size = ImVec2(0, 0),
                   ButtonKind kind = ButtonKind::Secondary, bool enabled = true) {
    FontScope fs(fonts().semibold, kFontSmall + 1.0f);
    const float icon_sz = 15.0f;
    bool has_label = label && *label && label[0] != '#';
    const float gap = (icon && has_label) ? 8.0f : 0.0f;
    ImVec2 tsz = has_label ? ImGui::CalcTextSize(label, nullptr, true) : ImVec2(0, 0);
    float content_w = (icon ? icon_sz : 0.0f) + gap + tsz.x;
    float w = size.x > 0.0f ? size.x : content_w + 34.0f;
    float h = size.y > 0.0f ? size.y : kControlH;

    ImGui::PushID(id);
    if (!enabled) ImGui::BeginDisabled();
    bool clicked = ImGui::InvisibleButton("##btn", ImVec2(w, h));
    if (!enabled) ImGui::EndDisabled();
    bool hovered = ImGui::IsItemHovered() && enabled;
    bool held = ImGui::IsItemActive();
    float t_hov = Animate(ImGui::GetID("##bh"), hovered ? 1.0f : 0.0f, 18.0f);
    ImGui::PopID();

    ImU32 bg_n, bg_h, bg_d, fg, border;
    switch (kind) {
        case ButtonKind::Primary:
            bg_n = kAccent; bg_h = kAccentHover; bg_d = kAccentDown; fg = IM_COL32(255, 255, 255, 255); border = 0; break;
        case ButtonKind::Success:
            bg_n = IM_COL32(34, 160, 94, 255); bg_h = IM_COL32(44, 182, 110, 255); bg_d = IM_COL32(28, 140, 82, 255);
            fg = IM_COL32(255, 255, 255, 255); border = 0; break;
        case ButtonKind::Ghost:
            bg_n = IM_COL32(0, 0, 0, 0); bg_h = kSurface3; bg_d = kSurface2; fg = kTextMuted; border = 0; break;
        case ButtonKind::Secondary:
        default:
            bg_n = kSurface2; bg_h = kSurface3; bg_d = kSurface2; fg = kText; border = kBorder; break;
    }

    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float r = h * 0.5f;
    ImU32 bg = held ? bg_d : Lerp(bg_n, bg_h, t_hov);
    if (!enabled) { bg = Lerp(bg, kSurface, 0.55f); fg = kTextSubtle; }
    if ((bg >> IM_COL32_A_SHIFT) & 0xFF) dl->AddRectFilled(a, b, bg, r);
    if (border) dl->AddRect(a, b, hovered ? kBorderHi : border, r);
    if (kind == ButtonKind::Ghost && hovered) fg = kText;

    float x = std::round(a.x + (w - content_w) * 0.5f);
    float cy = std::round((a.y + b.y) * 0.5f) + (held ? 1.0f : 0.0f);
    if (icon) {
        icon(dl, ImVec2(x + icon_sz * 0.5f, cy), icon_sz, fg);
        x += icon_sz + gap;
    }
    if (has_label) dl->AddText(ImVec2(x, std::round(cy - tsz.y * 0.5f)), fg, label, ImGui::FindRenderedTextEnd(label));
    return clicked;
}

// Panel/tarjeta con fondo de superficie y borde sutil (dibuja sobre un rect dado)
inline void DrawPanel(ImDrawList* dl, ImVec2 a, ImVec2 b, float radius = kRadius, ImU32 fill = kSurface) {
    dl->AddRectFilled(a, b, fill, radius);
    dl->AddRect(a, b, kBorder, radius);
}

} // namespace wallpaper::theme

