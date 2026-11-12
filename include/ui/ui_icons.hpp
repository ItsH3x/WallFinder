#pragma once

#include <imgui.h>
#include <cmath>

namespace wallpaper::icons {

// Dibuja el icono de Búsqueda estilo Google Material Symbols (Lupa geométrica limpia)
inline void DrawSearch(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float r = size * 0.30f;
    ImVec2 c(center.x - size * 0.10f, center.y - size * 0.10f);
    dl->AddCircle(c, r, color, 16, 2.0f);

    float angle = 0.785398f; // 45 grados en radianes
    ImVec2 h_start(c.x + std::cos(angle) * r, c.y + std::sin(angle) * r);
    ImVec2 h_end(center.x + size * 0.42f, center.y + size * 0.42f);
    dl->AddLine(h_start, h_end, color, 2.4f);
}

// Dibuja el icono de Descarga estilo Microsoft Fluent (Flecha descendente hacia bandeja)
inline void DrawDownload(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    // Línea de la flecha
    float top_y = center.y - size * 0.35f;
    float bot_y = center.y + size * 0.08f;
    dl->AddLine(ImVec2(center.x, top_y), ImVec2(center.x, bot_y), color, 2.2f);

    // Punta de la flecha
    float tip_w = size * 0.24f;
    dl->AddTriangleFilled(
        ImVec2(center.x, bot_y + size * 0.12f),
        ImVec2(center.x - tip_w, bot_y - size * 0.04f),
        ImVec2(center.x + tip_w, bot_y - size * 0.04f),
        color
    );

    // Bandeja inferior
    float tray_y = center.y + size * 0.34f;
    float tray_hw = size * 0.40f;
    float tray_lip = size * 0.14f;
    ImVec2 tray_pts[4] = {
        ImVec2(center.x - tray_hw, tray_y - tray_lip),
        ImVec2(center.x - tray_hw, tray_y),
        ImVec2(center.x + tray_hw, tray_y),
        ImVec2(center.x + tray_hw, tray_y - tray_lip)
    };
    dl->AddPolyline(tray_pts, 4, color, 0, 2.0f);
}

// Dibuja el icono de Monitor / Escritorio estilo Fluent / Material (Pantalla con marco y base)
inline void DrawDesktop(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    // Marco de pantalla
    float hw = size * 0.44f;
    float top_y = center.y - size * 0.36f;
    float scr_h = size * 0.52f;
    dl->AddRect(
        ImVec2(center.x - hw, top_y),
        ImVec2(center.x + hw, top_y + scr_h),
        color,
        2.5f,
        0,
        1.8f
    );

    // Soporte central
    float stem_top = top_y + scr_h;
    float stem_bot = center.y + size * 0.32f;
    dl->AddLine(ImVec2(center.x, stem_top), ImVec2(center.x, stem_bot), color, 2.0f);

    // Base horizontal
    float base_w = size * 0.28f;
    dl->AddLine(ImVec2(center.x - base_w, stem_bot), ImVec2(center.x + base_w, stem_bot), color, 2.0f);
}

// Dibuja el icono de Enlace Externo estilo Google Material (Cuadro con flecha diagonal)
inline void DrawExternalLink(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float hw = size * 0.36f;
    float hh = size * 0.36f;

    // Cuadro abierto
    ImVec2 box_pts[4] = {
        ImVec2(center.x - hw, center.y - hh * 0.3f),
        ImVec2(center.x - hw, center.y + hh),
        ImVec2(center.x + hw * 0.4f, center.y + hh),
        ImVec2(center.x + hw * 0.4f, center.y - hh * 0.3f)
    };
    dl->AddPolyline(box_pts, 3, color, 0, 1.8f);

    // Flecha diagonal hacia arriba a la derecha
    ImVec2 arrow_start(center.x - hw * 0.2f, center.y + hh * 0.2f);
    ImVec2 arrow_end(center.x + hw, center.y - hh);
    dl->AddLine(arrow_start, arrow_end, color, 2.0f);

    // Puntas de flecha
    float tip_len = size * 0.25f;
    dl->AddLine(arrow_end, ImVec2(arrow_end.x - tip_len, arrow_end.y), color, 2.0f);
    dl->AddLine(arrow_end, ImVec2(arrow_end.x, arrow_end.y + tip_len), color, 2.0f);
}

// Dibuja el icono de Carpeta estilo Microsoft Fluent (Carpeta con solapa)
inline void DrawFolder(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float hw = size * 0.44f;
    float top_y = center.y - size * 0.28f;
    float bot_y = center.y + size * 0.34f;
    
    // Cuerpo principal
    dl->AddRect(
        ImVec2(center.x - hw, top_y + size * 0.12f),
        ImVec2(center.x + hw, bot_y),
        color,
        2.5f,
        0,
        1.8f
    );

    // Pestaña superior de la carpeta
    ImVec2 tab_pts[4] = {
        ImVec2(center.x - hw, top_y + size * 0.12f),
        ImVec2(center.x - hw, top_y),
        ImVec2(center.x - hw * 0.15f, top_y),
        ImVec2(center.x + hw * 0.05f, top_y + size * 0.12f)
    };
    dl->AddPolyline(tab_pts, 4, color, 0, 1.8f);
}

// Dibuja el icono de Destello / Sparkle estilo Microsoft Fluent
inline void DrawSparkle(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float r = size * 0.42f;
    float in_r = r * 0.30f;
    ImVec2 pts[8] = {
        ImVec2(center.x, center.y - r),
        ImVec2(center.x + in_r, center.y - in_r),
        ImVec2(center.x + r, center.y),
        ImVec2(center.x + in_r, center.y + in_r),
        ImVec2(center.x, center.y + r),
        ImVec2(center.x - in_r, center.y + in_r),
        ImVec2(center.x - r, center.y),
        ImVec2(center.x - in_r, center.y - in_r)
    };
    dl->AddPolyline(pts, 8, color, ImDrawFlags_Closed, 1.6f);
}

// Dibuja el icono de Checkmark estilo Material
inline void DrawCheck(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    ImVec2 p1(center.x - size * 0.35f, center.y + size * 0.02f);
    ImVec2 p2(center.x - size * 0.08f, center.y + size * 0.28f);
    ImVec2 p3(center.x + size * 0.36f, center.y - size * 0.28f);
    dl->AddLine(p1, p2, color, 2.4f);
    dl->AddLine(p2, p3, color, 2.4f);
}

// Dibuja icono de Sliders / Filtros estilo Fluent
inline void DrawFilterSliders(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float hw = size * 0.40f;
    float hh = size * 0.34f;
    float y1 = center.y - hh;
    float y2 = center.y;
    float y3 = center.y + hh;
    dl->AddLine(ImVec2(center.x - hw, y1), ImVec2(center.x + hw, y1), color, 1.8f);
    dl->AddLine(ImVec2(center.x - hw, y2), ImVec2(center.x + hw, y2), color, 1.8f);
    dl->AddLine(ImVec2(center.x - hw, y3), ImVec2(center.x + hw, y3), color, 1.8f);
    dl->AddCircleFilled(ImVec2(center.x - hw * 0.35f, y1), 2.5f, color);
    dl->AddCircleFilled(ImVec2(center.x + hw * 0.35f, y2), 2.5f, color);
    dl->AddCircleFilled(ImVec2(center.x - hw * 0.10f, y3), 2.5f, color);
}

// Chevron / Flecha hacia abajo
inline void DrawChevronDown(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float hw = size * 0.32f;
    float hh = size * 0.18f;
    dl->AddLine(ImVec2(center.x - hw, center.y - hh), ImVec2(center.x, center.y + hh), color, 2.0f);
    dl->AddLine(ImVec2(center.x, center.y + hh), ImVec2(center.x + hw, center.y - hh), color, 2.0f);
}

// Chevron / Flecha hacia arriba
inline void DrawChevronUp(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float hw = size * 0.32f;
    float hh = size * 0.18f;
    dl->AddLine(ImVec2(center.x - hw, center.y + hh), ImVec2(center.x, center.y - hh), color, 2.0f);
    dl->AddLine(ImVec2(center.x, center.y - hh), ImVec2(center.x + hw, center.y + hh), color, 2.0f);
}

// Icono de Llama / Fuego ("Caliente" / "Cálidos") estilo Fluent / Material Symbols
inline void DrawFlame(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float hw = size * 0.32f;
    float hh = size * 0.40f;
    ImVec2 p_top(center.x, center.y - hh);
    ImVec2 p_bot(center.x, center.y + hh);
    dl->AddBezierQuadratic(p_top, ImVec2(center.x - hw * 1.35f, center.y + hh * 0.25f), p_bot, color, 1.8f);
    dl->AddBezierQuadratic(p_bot, ImVec2(center.x + hw * 1.35f, center.y + hh * 0.25f), p_top, color, 1.8f);

    // Pequeño núcleo de llama interior
    float in_hh = hh * 0.42f;
    float in_hw = hw * 0.45f;
    ImVec2 in_top(center.x, center.y + hh * 0.15f - in_hh);
    ImVec2 in_bot(center.x, center.y + hh * 0.75f);
    dl->AddBezierQuadratic(in_top, ImVec2(center.x - in_hw * 1.25f, center.y + hh * 0.45f), in_bot, color, 1.4f);
    dl->AddBezierQuadratic(in_bot, ImVec2(center.x + in_hw * 1.25f, center.y + hh * 0.45f), in_top, color, 1.4f);
}

// Icono de Copo de Nieve ("Frío" / "Fríos") estilo Fluent / Material Symbols
inline void DrawSnowflake(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float r = size * 0.38f;
    float branch = r * 0.40f;
    float angles[3] = { 0.0f, 1.047197f, 2.094395f }; // 0°, 60°, 120°
    for (float a : angles) {
        float dx = std::cos(a) * r;
        float dy = std::sin(a) * r;
        dl->AddLine(ImVec2(center.x - dx, center.y - dy), ImVec2(center.x + dx, center.y + dy), color, 1.8f);
        // Ramas laterales simétricas
        float px = -std::sin(a) * branch;
        float py = std::cos(a) * branch;
        float mx1 = center.x + dx * 0.65f;
        float my1 = center.y + dy * 0.65f;
        dl->AddLine(ImVec2(mx1, my1), ImVec2(center.x + dx * 0.9f + px * 0.6f, center.y + dy * 0.9f + py * 0.6f), color, 1.3f);
        dl->AddLine(ImVec2(mx1, my1), ImVec2(center.x + dx * 0.9f - px * 0.6f, center.y + dy * 0.9f - py * 0.6f), color, 1.3f);

        float mx2 = center.x - dx * 0.65f;
        float my2 = center.y - dy * 0.65f;
        dl->AddLine(ImVec2(mx2, my2), ImVec2(center.x - dx * 0.9f + px * 0.6f, center.y - dy * 0.9f + py * 0.6f), color, 1.3f);
        dl->AddLine(ImVec2(mx2, my2), ImVec2(center.x - dx * 0.9f - px * 0.6f, center.y - dy * 0.9f - py * 0.6f), color, 1.3f);
    }
    dl->AddCircleFilled(center, 2.0f, color);
}

// Icono de Contraste / Monocromático (B/N) estilo Material Symbols
inline void DrawContrast(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float r = size * 0.38f;
    dl->AddCircle(center, r, color, 24, 1.8f);
    // Mitad derecha rellena
    dl->PathArcTo(center, r - 1.0f, -1.570796f, 1.570796f, 16);
    dl->PathLineTo(center);
    dl->PathFillConvex(color);
}

// Icono de Pantalla Completa / Expandir estilo Material / Fluent (4 esquinas hacia afuera)
inline void DrawFullscreen(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    float hw = size * 0.38f;
    float hh = size * 0.38f;
    float corner_len = size * 0.22f;

    // Arriba-Izquierda
    dl->AddLine(ImVec2(center.x - hw, center.y - hh + corner_len), ImVec2(center.x - hw, center.y - hh), color, 1.8f);
    dl->AddLine(ImVec2(center.x - hw, center.y - hh), ImVec2(center.x - hw + corner_len, center.y - hh), color, 1.8f);

    // Arriba-Derecha
    dl->AddLine(ImVec2(center.x + hw - corner_len, center.y - hh), ImVec2(center.x + hw, center.y - hh), color, 1.8f);
    dl->AddLine(ImVec2(center.x + hw, center.y - hh), ImVec2(center.x + hw, center.y - hh + corner_len), color, 1.8f);

    // Abajo-Izquierda
    dl->AddLine(ImVec2(center.x - hw, center.y + hh - corner_len), ImVec2(center.x - hw, center.y + hh), color, 1.8f);
    dl->AddLine(ImVec2(center.x - hw, center.y + hh), ImVec2(center.x - hw + corner_len, center.y + hh), color, 1.8f);

    // Abajo-Derecha
    dl->AddLine(ImVec2(center.x + hw - corner_len, center.y + hh), ImVec2(center.x + hw, center.y + hh), color, 1.8f);
    dl->AddLine(ImVec2(center.x + hw, center.y + hh), ImVec2(center.x + hw, center.y + hh - corner_len), color, 1.8f);
}

// Dibuja un icono en el espacio en línea actual (perfectamente centrado con la línea de texto)
inline void DrawPlus(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float h = size * 0.34f;
    dl->AddLine(ImVec2(c.x - h, c.y), ImVec2(c.x + h, c.y), color, 1.9f);
    dl->AddLine(ImVec2(c.x, c.y - h), ImVec2(c.x, c.y + h), color, 1.9f);
}

inline void DrawMinus(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float h = size * 0.34f;
    dl->AddLine(ImVec2(c.x - h, c.y), ImVec2(c.x + h, c.y), color, 1.9f);
}

inline void DrawClose(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float h = size * 0.28f;
    dl->AddLine(ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h), color, 1.8f);
    dl->AddLine(ImVec2(c.x + h, c.y - h), ImVec2(c.x - h, c.y + h), color, 1.8f);
}

// Flecha circular de "restablecer" (Material: restart_alt)
inline void DrawReset(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float r = size * 0.34f;
    dl->PathArcTo(c, r, -2.6f, 2.0f, 20);
    dl->PathStroke(color, 0, 1.7f);
    ImVec2 tip(c.x + std::cos(-2.6f) * r, c.y + std::sin(-2.6f) * r);
    dl->AddTriangleFilled(ImVec2(tip.x - size * 0.02f, tip.y - size * 0.22f),
                          ImVec2(tip.x + size * 0.20f, tip.y + size * 0.02f),
                          ImVec2(tip.x - size * 0.12f, tip.y + size * 0.10f), color);
}

// Alerta circular (error)
inline void DrawAlert(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float r = size * 0.40f;
    dl->AddCircle(c, r, color, 20, 1.6f);
    dl->AddLine(ImVec2(c.x, c.y - r * 0.50f), ImVec2(c.x, c.y + r * 0.10f), color, 1.8f);
    dl->AddCircleFilled(ImVec2(c.x, c.y + r * 0.45f), 1.3f, color);
}

// Logotipo de la app: marco de imagen con montaña y sol, con degradado índigo-violeta
inline void DrawLogo(ImDrawList* dl, ImVec2 c, float size) {
    float h = size * 0.5f;
    ImVec2 a(c.x - h, c.y - h), b(c.x + h, c.y + h);
    dl->AddRectFilled(a, b, IM_COL32(124, 131, 255, 255), size * 0.30f);
    dl->AddRectFilled(ImVec2(a.x, c.y), b, IM_COL32(150, 100, 250, 120), size * 0.30f, ImDrawFlags_RoundCornersBottom);
    ImU32 w = IM_COL32(255, 255, 255, 235);
    dl->AddTriangleFilled(ImVec2(c.x - h * 0.62f, c.y + h * 0.48f), ImVec2(c.x - h * 0.08f, c.y - h * 0.18f),
                          ImVec2(c.x + h * 0.40f, c.y + h * 0.48f), w);
    dl->AddTriangleFilled(ImVec2(c.x + h * 0.02f, c.y + h * 0.48f), ImVec2(c.x + h * 0.32f, c.y + h * 0.10f),
                          ImVec2(c.x + h * 0.64f, c.y + h * 0.48f), IM_COL32(255, 255, 255, 170));
    dl->AddCircleFilled(ImVec2(c.x + h * 0.32f, c.y - h * 0.34f), h * 0.15f, w, 14);
}

inline void RenderIconInline(void (*draw_fn)(ImDrawList*, ImVec2, float, ImU32), float size = 15.0f, ImU32 color = IM_COL32(200, 210, 230, 255)) {
    float line_h = ImGui::GetTextLineHeight();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center(std::round(pos.x + size * 0.5f), std::round(pos.y + line_h * 0.5f));
    if (draw_fn) {
        draw_fn(ImGui::GetWindowDrawList(), center, size, color);
    }
    ImGui::Dummy(ImVec2(size, line_h));
}

// Icono de Persona / Usuario / Retratos (silueta cabeza y hombros)
inline void DrawUser(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float head_r = size * 0.22f;
    ImVec2 head_c(c.x, c.y - size * 0.16f);
    dl->AddCircle(head_c, head_r, color, 16, 1.8f);

    float sh_w = size * 0.38f;
    float bot_y = c.y + size * 0.38f;
    float top_sh_y = c.y + size * 0.16f;
    ImVec2 pts[4] = {
        ImVec2(c.x - sh_w, bot_y),
        ImVec2(c.x - sh_w * 0.65f, top_sh_y),
        ImVec2(c.x + sh_w * 0.65f, top_sh_y),
        ImVec2(c.x + sh_w, bot_y)
    };
    dl->AddPolyline(pts, 4, color, 0, 1.8f);
}

inline void DrawPerson(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    DrawUser(dl, c, size, color);
}

// Icono de Anime / Manga (estrella centelleante estilizada estilo manga)
inline void DrawAnime(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float r = size * 0.40f;
    float in_r = r * 0.28f;
    ImVec2 pts[8] = {
        ImVec2(c.x, c.y - r),
        ImVec2(c.x + in_r, c.y - in_r),
        ImVec2(c.x + r, c.y),
        ImVec2(c.x + in_r, c.y + in_r),
        ImVec2(c.x, c.y + r),
        ImVec2(c.x - in_r, c.y + in_r),
        ImVec2(c.x - r, c.y),
        ImVec2(c.x - in_r, c.y - in_r)
    };
    dl->AddPolyline(pts, 8, color, ImDrawFlags_Closed, 1.8f);
    dl->AddCircleFilled(c, 1.8f, color);
}

// Icono de Paisajes / Montaña / Naturaleza (montañas con sol radiante)
inline void DrawMountain(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float bot_y = c.y + size * 0.34f;
    float hw = size * 0.42f;

    // Montaña principal
    ImVec2 m1[3] = {
        ImVec2(c.x - hw, bot_y),
        ImVec2(c.x - hw * 0.15f, c.y - size * 0.25f),
        ImVec2(c.x + hw * 0.6f, bot_y)
    };
    dl->AddPolyline(m1, 3, color, 0, 1.8f);

    // Montaña secundaria
    ImVec2 m2[3] = {
        ImVec2(c.x + hw * 0.05f, bot_y),
        ImVec2(c.x + hw * 0.55f, c.y - size * 0.10f),
        ImVec2(c.x + hw, bot_y)
    };
    dl->AddPolyline(m2, 3, color, 0, 1.6f);

    // Línea base
    dl->AddLine(ImVec2(c.x - hw, bot_y), ImVec2(c.x + hw, bot_y), color, 1.8f);

    // Sol
    dl->AddCircleFilled(ImVec2(c.x + hw * 0.45f, c.y - size * 0.24f), size * 0.10f, color, 12);
}

inline void DrawLandscape(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    DrawMountain(dl, c, size, color);
}

// Icono de Videojuegos / Gaming (mando con cruceta y botones)
inline void DrawGamepad(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float hw = size * 0.44f;
    float hh = size * 0.26f;
    dl->AddRect(ImVec2(c.x - hw, c.y - hh), ImVec2(c.x + hw, c.y + hh), color, hh * 0.75f, 0, 1.8f);

    // Cruceta izquierda (+)
    float cross_r = size * 0.11f;
    float left_x = c.x - hw * 0.45f;
    dl->AddLine(ImVec2(left_x - cross_r, c.y), ImVec2(left_x + cross_r, c.y), color, 1.8f);
    dl->AddLine(ImVec2(left_x, c.y - cross_r), ImVec2(left_x, c.y + cross_r), color, 1.8f);

    // Botones derechos (4 puntos)
    float right_x = c.x + hw * 0.45f;
    float b_dist = size * 0.09f;
    float b_r = 1.3f;
    dl->AddCircleFilled(ImVec2(right_x, c.y - b_dist), b_r, color);
    dl->AddCircleFilled(ImVec2(right_x, c.y + b_dist), b_r, color);
    dl->AddCircleFilled(ImVec2(right_x - b_dist, c.y), b_r, color);
    dl->AddCircleFilled(ImVec2(right_x + b_dist, c.y), b_r, color);
}

// Icono de Rayo / Cyberpunk / Futurista (rayo geométrico nítido)
inline void DrawBolt(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    ImVec2 p1(c.x + size * 0.08f, c.y - size * 0.40f);
    ImVec2 p2(c.x - size * 0.14f, c.y - size * 0.02f);
    ImVec2 p3(c.x + size * 0.12f, c.y + size * 0.02f);
    ImVec2 p4(c.x - size * 0.08f, c.y + size * 0.40f);
    dl->AddLine(p1, p2, color, 2.2f);
    dl->AddLine(p2, p3, color, 2.2f);
    dl->AddLine(p3, p4, color, 2.2f);
}

inline void DrawCyberpunk(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    DrawBolt(dl, c, size, color);
}

// Icono de Arte Digital / Pintura (Paleta de artista con manchas de color y orificio)
inline void DrawPalette(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float r = size * 0.38f;
    dl->AddCircle(c, r, color, 24, 1.8f);
    // Orificio para pulgar
    dl->AddCircle(ImVec2(c.x + size * 0.16f, c.y + size * 0.12f), size * 0.09f, color, 12, 1.5f);
    // Manchas de pintura
    dl->AddCircleFilled(ImVec2(c.x - size * 0.16f, c.y - size * 0.16f), size * 0.06f, color, 8);
    dl->AddCircleFilled(ImVec2(c.x + size * 0.06f, c.y - size * 0.22f), size * 0.06f, color, 8);
    dl->AddCircleFilled(ImVec2(c.x - size * 0.22f, c.y + size * 0.06f), size * 0.06f, color, 8);
}

// Icono de Espacio / Cosmos / Astronomía (Planeta Saturno con anillo elíptico inclinado)
inline void DrawSaturn(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float r = size * 0.22f;

    // Anillo elíptico inclinado (~ -22 grados)
    constexpr float angle = -0.384f; // rad
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    float a = size * 0.44f;
    float b = size * 0.14f;

    constexpr int kPts = 28;
    ImVec2 ring_pts[kPts];
    for (int i = 0; i < kPts; ++i) {
        float t = (2.0f * 3.14159265f * i) / (kPts - 1);
        float ct = std::cos(t);
        float st = std::sin(t);
        float rx = a * ct;
        float ry = b * st;
        ring_pts[i] = ImVec2(
            c.x + rx * cos_a - ry * sin_a,
            c.y + rx * sin_a + ry * cos_a
        );
    }

    // Mitad trasera del anillo (detrás del planeta)
    dl->AddPolyline(ring_pts + (kPts / 2), kPts - (kPts / 2), color, 0, 1.6f);

    // Esfera del planeta
    dl->AddCircle(c, r, color, 20, 1.8f);

    // Mitad delantera del anillo (por delante del planeta)
    dl->AddPolyline(ring_pts, (kPts / 2) + 1, color, 0, 1.8f);

    // Estrella guía lejana
    dl->AddCircleFilled(ImVec2(c.x + size * 0.34f, c.y - size * 0.28f), 1.4f, color);
}

inline void DrawSpace(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    DrawSaturn(dl, c, size, color);
}

// Icono de Vehículos / Automóviles / Transporte (Silueta aerodinámica con ruedas y ventanilla)
inline void DrawCar(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float wheel_y = c.y + size * 0.18f;
    float r_wheel = size * 0.12f;
    float w1_x = c.x - size * 0.25f;
    float w2_x = c.x + size * 0.25f;

    // Rueda trasera y delantera
    dl->AddCircle(ImVec2(w1_x, wheel_y), r_wheel, color, 14, 1.6f);
    dl->AddCircle(ImVec2(w2_x, wheel_y), r_wheel, color, 14, 1.6f);
    dl->AddCircleFilled(ImVec2(w1_x, wheel_y), 1.3f, color);
    dl->AddCircleFilled(ImVec2(w2_x, wheel_y), 1.3f, color);

    // Carrocería deportiva (silueta exterior)
    ImVec2 body_pts[9] = {
        ImVec2(c.x - size * 0.44f, wheel_y - size * 0.02f),         // Trasera baja
        ImVec2(c.x - size * 0.42f, c.y - size * 0.02f),             // Alerón trasero
        ImVec2(c.x - size * 0.26f, c.y - size * 0.02f),             // Maletero
        ImVec2(c.x - size * 0.12f, c.y - size * 0.20f),             // Parabrisas trasero
        ImVec2(c.x + size * 0.06f, c.y - size * 0.20f),             // Techo
        ImVec2(c.x + size * 0.22f, c.y + size * 0.00f),             // Parabrisas delantero
        ImVec2(c.x + size * 0.38f, c.y + size * 0.04f),             // Capó
        ImVec2(c.x + size * 0.44f, wheel_y - size * 0.02f),         // Morro delantero
        ImVec2(w2_x + r_wheel, wheel_y)                             // Paso de rueda delantero
    };
    dl->AddPolyline(body_pts, 9, color, 0, 1.8f);

    // Faldón lateral entre ruedas
    dl->AddLine(ImVec2(w1_x + r_wheel, wheel_y), ImVec2(w2_x - r_wheel, wheel_y), color, 1.6f);
    // Trasera bajo parachoques hasta rueda
    dl->AddLine(ImVec2(c.x - size * 0.44f, wheel_y - size * 0.02f), ImVec2(w1_x - r_wheel, wheel_y), color, 1.6f);

    // Ventanilla lateral
    ImVec2 win_pts[4] = {
        ImVec2(c.x - size * 0.10f, c.y - size * 0.16f),
        ImVec2(c.x + size * 0.04f, c.y - size * 0.16f),
        ImVec2(c.x + size * 0.16f, c.y - size * 0.02f),
        ImVec2(c.x - size * 0.10f, c.y - size * 0.02f)
    };
    dl->AddPolyline(win_pts, 4, color, ImDrawFlags_Closed, 1.2f);
}

inline void DrawVehicles(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    DrawCar(dl, c, size, color);
}

// Icono de Fantasía / Magia (Varita mágica con destellos/estrellas)
inline void DrawFantasy(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    ImVec2 p1(c.x - size * 0.35f, c.y + size * 0.35f);
    ImVec2 p2(c.x + size * 0.12f, c.y - size * 0.12f);
    dl->AddLine(p1, p2, color, 2.0f);
    dl->AddCircleFilled(p2, 1.8f, color);

    ImVec2 tip(c.x + size * 0.22f, c.y - size * 0.22f);
    float r = size * 0.15f;
    dl->AddLine(ImVec2(tip.x - r, tip.y), ImVec2(tip.x + r, tip.y), color, 1.6f);
    dl->AddLine(ImVec2(tip.x, tip.y - r), ImVec2(tip.x, tip.y + r), color, 1.6f);
    dl->AddCircleFilled(ImVec2(c.x + size * 0.35f, c.y - size * 0.05f), 1.2f, color);
    dl->AddCircleFilled(ImVec2(c.x + size * 0.05f, c.y - size * 0.35f), 1.2f, color);
}

// Icono de Minimalismo (Composición geométrica pura y limpia)
inline void DrawMinimalism(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float r = size * 0.36f;
    dl->AddCircle(c, r, color, 24, 1.6f);
    float hr = size * 0.16f;
    dl->AddRect(ImVec2(c.x - hr, c.y - hr), ImVec2(c.x + hr, c.y + hr), color, 1.5f, 0, 1.6f);
}

// Icono de Animales / Fauna (Huella de pata estilizada)
inline void DrawAnimals(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    ImVec2 pad(c.x, c.y + size * 0.08f);
    float pr = size * 0.18f;
    dl->AddCircleFilled(pad, pr, color, 16);

    float toe_r = size * 0.075f;
    dl->AddCircleFilled(ImVec2(c.x - size * 0.24f, c.y - size * 0.04f), toe_r, color, 10);
    dl->AddCircleFilled(ImVec2(c.x - size * 0.10f, c.y - size * 0.24f), toe_r, color, 10);
    dl->AddCircleFilled(ImVec2(c.x + size * 0.10f, c.y - size * 0.24f), toe_r, color, 10);
    dl->AddCircleFilled(ImVec2(c.x + size * 0.24f, c.y - size * 0.04f), toe_r, color, 10);
}

// Icono de Arquitectura / Ciudad (Siluetas de edificios y metrópolis)
inline void DrawArchitecture(ImDrawList* dl, ImVec2 c, float size, ImU32 color) {
    float bot_y = c.y + size * 0.36f;
    dl->AddRect(ImVec2(c.x - size * 0.38f, c.y - size * 0.10f), ImVec2(c.x - size * 0.14f, bot_y), color, 1.0f, 0, 1.6f);
    dl->AddRect(ImVec2(c.x - size * 0.12f, c.y - size * 0.30f), ImVec2(c.x + size * 0.14f, bot_y), color, 1.0f, 0, 1.6f);
    dl->AddLine(ImVec2(c.x + size * 0.01f, c.y - size * 0.30f), ImVec2(c.x + size * 0.01f, c.y - size * 0.40f), color, 1.6f);
    dl->AddRect(ImVec2(c.x + size * 0.16f, c.y + size * 0.02f), ImVec2(c.x + size * 0.38f, bot_y), color, 1.0f, 0, 1.6f);
    dl->AddLine(ImVec2(c.x - size * 0.42f, bot_y), ImVec2(c.x + size * 0.42f, bot_y), color, 1.6f);
}

// Botón elegante con icono vectorial garantizado (centrado con precisión milimétrica y soporte para estilos redondeados)
inline bool ButtonWithIcon(
    const char* str_id,
    void (*draw_fn)(ImDrawList*, ImVec2, float, ImU32),
    const char* label,
    ImVec2 size = ImVec2(0.0f, 0.0f),
    ImVec4 custom_bg = ImVec4(0, 0, 0, 0),
    ImVec4 custom_hover = ImVec4(0, 0, 0, 0),
    ImU32 custom_icon_color = 0
) {
    bool has_custom_col = (custom_bg.w > 0.0f);
    if (has_custom_col) {
        ImGui::PushStyleColor(ImGuiCol_Button, custom_bg);
        if (custom_hover.w > 0.0f) {
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, custom_hover);
        } else {
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(custom_bg.x * 1.18f, custom_bg.y * 1.18f, custom_bg.z * 1.18f, custom_bg.w));
        }
    }

    float icon_size = 14.0f;
    bool has_label = (label && label[0] != '\0');
    float icon_gap = (draw_fn && has_label) ? 8.0f : 0.0f;
    ImVec2 text_sz = has_label ? ImGui::CalcTextSize(label, nullptr, true) : ImVec2(0.0f, 0.0f);
    
    float auto_w = (draw_fn ? icon_size : 0.0f) + icon_gap + text_sz.x + ImGui::GetStyle().FramePadding.x * 2.4f;
    float auto_h = ImGui::GetFrameHeight();

    ImVec2 final_sz(size.x > 0.0f ? size.x : auto_w, size.y > 0.0f ? size.y : auto_h);

    bool clicked = ImGui::Button(str_id, final_sz);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    if (has_custom_col) {
        ImGui::PopStyleColor(2);
    }

    ImVec2 bb_min = ImGui::GetItemRectMin();
    ImVec2 bb_max = ImGui::GetItemRectMax();
    float btn_w = bb_max.x - bb_min.x;
    float btn_h = bb_max.y - bb_min.y;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
    ImU32 icon_color = (custom_icon_color != 0 && !active) 
        ? custom_icon_color 
        : (active ? IM_COL32(255, 255, 255, 255) : (hovered ? IM_COL32(245, 250, 255, 255) : text_col));

    float total_content_w = (draw_fn ? icon_size : 0.0f) + icon_gap + text_sz.x;
    float start_x = std::round(bb_min.x + (btn_w - total_content_w) * 0.5f);
    float center_y = std::round(bb_min.y + btn_h * 0.5f);

    if (active) {
        center_y += 1.0f;
        start_x += 1.0f;
    }

    if (draw_fn) {
        ImVec2 icon_center(start_x + icon_size * 0.5f, center_y);
        draw_fn(dl, icon_center, icon_size, icon_color);
        start_x += icon_size + icon_gap;
    }

    if (has_label) {
        float text_y = std::round(bb_min.y + (btn_h - text_sz.y) * 0.5f);
        if (active) {
            text_y += 1.0f;
        }
        dl->AddText(ImVec2(start_x, text_y), icon_color, label);
    }

    return clicked;
}

} // namespace wallpaper::icons

