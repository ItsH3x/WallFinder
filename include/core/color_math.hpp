#pragma once

// =============================================================================
// Motor de análisis cromático perceptual (CIELAB + CIEDE2000)
// -----------------------------------------------------------------------------
// Wallhaven devuelve para cada fondo una paleta de hasta 5 colores ordenados por
// dominancia (el índice 0 es el tono que más superficie ocupa). Este módulo
// evalúa esa paleta con distancias perceptuales reales para:
//   * Exigir que el Color 1 sea el tono dominante de la imagen (modo bicolor).
//   * Exigir que el Color 2 aparezca como acento secundario.
//   * Descartar fondos con colores "parásitos" muy ajenos a los seleccionados.
//   * Detectar fondos verdaderamente monocromáticos (escala de grises).
//   * Validar la temperatura cromática (cálidos / fríos).
// Umbrales calibrados contra la paleta oficial de 29 colores de Wallhaven.
// =============================================================================

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wallpaper::color {

struct Rgb {
    float r{0}, g{0}, b{0}; // 0..255
};

struct Lab {
    float L{0}, a{0}, b{0};
};

// --- Umbrales (escala CIEDE2000) ---------------------------------------------
inline constexpr float kMatchDeltaE   = 19.5f; // Mismo "familia" de color (ej. rojo ~ rojo oscuro)
inline constexpr float kForeignDeltaE = 30.0f; // Más allá de esto, un color es ajeno a la selección
inline constexpr float kNeutralChroma = 12.5f; // Por debajo: gris/negro/blanco (sin tono real)
inline constexpr float kVividChroma   = 18.0f; // Por encima: color con tono claramente perceptible

inline std::optional<Rgb> parse_hex(std::string_view hex) {
    if (!hex.empty() && hex.front() == '#') hex.remove_prefix(1);
    if (hex.size() != 6) return std::nullopt;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    int v[6];
    for (int i = 0; i < 6; ++i) {
        v[i] = nib(hex[static_cast<size_t>(i)]);
        if (v[i] < 0) return std::nullopt;
    }
    return Rgb{static_cast<float>(v[0] * 16 + v[1]),
               static_cast<float>(v[2] * 16 + v[3]),
               static_cast<float>(v[4] * 16 + v[5])};
}

inline Lab to_lab(const Rgb& c) {
    auto lin = [](float u) {
        u /= 255.0f;
        return u <= 0.04045f ? u / 12.92f : std::pow((u + 0.055f) / 1.055f, 2.4f);
    };
    float r = lin(c.r), g = lin(c.g), b = lin(c.b);
    float X = (0.4124f * r + 0.3576f * g + 0.1805f * b) / 0.95047f;
    float Y = (0.2126f * r + 0.7152f * g + 0.0722f * b);
    float Z = (0.0193f * r + 0.1192f * g + 0.9505f * b) / 1.08883f;
    auto f = [](float t) { return t > 0.008856f ? std::cbrt(t) : 7.787f * t + 16.0f / 116.0f; };
    float fx = f(X), fy = f(Y), fz = f(Z);
    return Lab{116.0f * fy - 16.0f, 500.0f * (fx - fy), 200.0f * (fy - fz)};
}

inline float chroma(const Lab& c) { return std::hypot(c.a, c.b); }

// Ángulo de tono en grados [0, 360)
inline float hue_deg(const Lab& c) {
    float h = std::atan2(c.b, c.a) * 57.2957795f;
    return h < 0.0f ? h + 360.0f : h;
}

inline float hue_dist(float h1, float h2) {
    float d = std::abs(h1 - h2);
    return d > 180.0f ? 360.0f - d : d;
}

inline bool is_neutral(const Lab& c) { return chroma(c) <= kNeutralChroma; }

// Diferencia de color perceptual CIEDE2000
inline float delta_e(const Lab& l1, const Lab& l2) {
    constexpr float kDeg = 57.2957795f;
    constexpr float kRad = 0.0174532925f;
    float C1 = std::hypot(l1.a, l1.b), C2 = std::hypot(l2.a, l2.b);
    float Cm = (C1 + C2) * 0.5f;
    float Cm7 = std::pow(Cm, 7.0f);
    float G = 0.5f * (1.0f - std::sqrt(Cm7 / (Cm7 + 6103515625.0f)));
    float a1p = (1.0f + G) * l1.a, a2p = (1.0f + G) * l2.a;
    float C1p = std::hypot(a1p, l1.b), C2p = std::hypot(a2p, l2.b);
    auto hp = [&](float bb, float ap) {
        if (bb == 0.0f && ap == 0.0f) return 0.0f;
        float h = std::atan2(bb, ap) * kDeg;
        return h < 0.0f ? h + 360.0f : h;
    };
    float h1 = hp(l1.b, a1p), h2 = hp(l2.b, a2p);
    float dL = l2.L - l1.L, dC = C2p - C1p;
    float dh = 0.0f;
    if (C1p * C2p != 0.0f) {
        dh = h2 - h1;
        if (dh > 180.0f) dh -= 360.0f;
        else if (dh < -180.0f) dh += 360.0f;
    }
    float dH = 2.0f * std::sqrt(C1p * C2p) * std::sin(dh * 0.5f * kRad);
    float Lm = (l1.L + l2.L) * 0.5f, Cmp = (C1p + C2p) * 0.5f;
    float hm = h1 + h2;
    if (C1p * C2p != 0.0f) {
        if (std::abs(h1 - h2) <= 180.0f) hm = (h1 + h2) * 0.5f;
        else hm = (h1 + h2 < 360.0f) ? (h1 + h2 + 360.0f) * 0.5f : (h1 + h2 - 360.0f) * 0.5f;
    }
    float T = 1.0f - 0.17f * std::cos((hm - 30.0f) * kRad) + 0.24f * std::cos(2.0f * hm * kRad) +
              0.32f * std::cos((3.0f * hm + 6.0f) * kRad) - 0.20f * std::cos((4.0f * hm - 63.0f) * kRad);
    float dt = 30.0f * std::exp(-std::pow((hm - 275.0f) / 25.0f, 2.0f));
    float Cmp7 = std::pow(Cmp, 7.0f);
    float Rc = 2.0f * std::sqrt(Cmp7 / (Cmp7 + 6103515625.0f));
    float Sl = 1.0f + 0.015f * (Lm - 50.0f) * (Lm - 50.0f) / std::sqrt(20.0f + (Lm - 50.0f) * (Lm - 50.0f));
    float Sc = 1.0f + 0.045f * Cmp;
    float Sh = 1.0f + 0.015f * Cmp * T;
    float Rt = -std::sin(2.0f * dt * kRad) * Rc;
    float tL = dL / Sl, tC = dC / Sc, tH = dH / Sh;
    return std::sqrt(tL * tL + tC * tC + tH * tH + Rt * tC * tH);
}

// Evaluación perceptual de concordancia cromática estricta calibrada:
// - Blanco no capta amarillo (chroma alto), pero sí gris claro.
// - Negro capta gris muy oscuro, pero no gris medio ni colores vivos.
// - Amarillo capta amarillo puro o amarillo muy claro, pero no blanco ni naranja.
// - Rojo capta tinto (vino) o rojo anaranjado, pero no naranja directo ni rosa.
inline bool is_color_match(const Lab& target, const Lab& cand) {
    float tc = chroma(target);
    float cc = chroma(cand);
    float th = hue_deg(target);
    float ch = hue_deg(cand);
    float de = delta_e(target, cand);

    // 1. Blanco (ffffff): L >= 90, c <= 12
    if (target.L >= 90.0f && tc <= 12.0f) {
        // Exige neutralidad (c <= 14) y luminosidad alta (L >= 65): capta gris claro y blanco, rechaza amarillo
        return (cc <= 14.0f) && (cand.L >= 65.0f);
    }

    // 2. Negro (000000): L <= 15, c <= 12
    if (target.L <= 15.0f && tc <= 12.0f) {
        // Exige neutralidad (c <= 16) y oscuridad profunda (L <= 28): capta gris muy oscuro y negro
        return (cc <= 16.0f) && (cand.L <= 28.0f);
    }

    // 3. Colores cromáticos: el candidato DEBE tener saturación real (cc >= 14).
    // Esto asegura que amarillo NUNCA capte blanco ni grises neutros.
    if (cc < 14.0f) {
        return false;
    }

    // 3.1. Rojo (cc0000, th ~ 40)
    if (th >= 25.0f && th <= 48.0f) {
        // Capta tinto/vino (h ~ 25, L baja), rojo puro (h ~ 40), rojo anaranjado (h <= 48).
        // Rechaza Naranja directo (h >= 55) y Rosa (h <= 18 o >= 335).
        bool is_red_hue = (ch >= 345.0f || ch <= 48.0f);
        return is_red_hue && (cc >= 18.0f) && (de <= 26.0f || cand.L <= 55.0f);
    }

    // 3.2. Naranja (ff9900, th ~ 68.7)
    if (th >= 50.0f && th <= 76.0f) {
        // Rango de naranja estricto: rechaza rojo (<= 48) y amarillo (>= 78)
        return (ch >= 52.0f && ch <= 76.0f) && (cc >= 18.0f) && (de <= 22.0f);
    }

    // 3.3. Amarillo (ffff00, th ~ 102.9)
    if (th >= 85.0f && th <= 112.0f) {
        // Capta amarillo puro y amarillo muy claro (L alta, cc >= 16, h entre 78 y 115).
        // Rechaza Naranja (< 76) y Verde (> 118).
        return (ch >= 78.0f && ch <= 115.0f) && (cc >= 16.0f) && (de <= 28.0f || cand.L >= 70.0f);
    }

    // 3.4. Verde (669900, th ~ 121.2)
    if (th >= 115.0f && th <= 165.0f) {
        return (ch >= 115.0f && ch <= 165.0f) && (cc >= 16.0f) && (de <= 24.0f);
    }

    // 3.5. Cian (66cccc, th ~ 197.2)
    if (th >= 170.0f && th <= 225.0f) {
        return (ch >= 170.0f && ch <= 225.0f) && (cc >= 14.0f) && (de <= 24.0f);
    }

    // 3.6. Azul (0066cc, th ~ 284.4)
    if (th >= 235.0f && th <= 298.0f) {
        return (ch >= 235.0f && ch <= 298.0f) && (cc >= 16.0f) && (de <= 24.0f);
    }

    // 3.7. Morado (663399, th ~ 312.3)
    if (th >= 298.0f && th <= 335.0f) {
        return (ch >= 298.0f && ch <= 335.0f) && (cc >= 16.0f) && (de <= 24.0f);
    }

    // 3.8. Rosa (ea4c88, th ~ 1.1)
    if (th >= 335.0f || th <= 15.0f) {
        bool is_pink_hue = (ch >= 335.0f || ch <= 18.0f);
        return is_pink_hue && (cc >= 16.0f) && (de <= 24.0f);
    }

    return de <= 20.0f;
}

inline std::vector<Lab> palette_to_lab(const std::vector<std::string>& palette) {
    std::vector<Lab> out;
    out.reserve(palette.size());
    for (const auto& hex : palette) {
        if (auto rgb = parse_hex(hex)) out.push_back(to_lab(*rgb));
    }
    return out;
}

// --- Evaluadores ---------------------------------------------------------------

struct MatchResult {
    bool accepted{false};
    float score{1e9f}; // Menor = mejor coincidencia
};

// Cuenta colores con tono vivo que están lejos de TODOS los objetivos.
// Los neutros (sombras negras, luces blancas) se toleran, salvo que el usuario
// haya pedido exclusivamente colores con tono (en cuyo caso siguen tolerándose:
// prácticamente toda imagen tiene sombras o luces).
inline int count_foreign(const std::vector<Lab>& pal, size_t from, const std::vector<Lab>& targets) {
    int foreign = 0;
    for (size_t i = from; i < pal.size(); ++i) {
        if (chroma(pal[i]) <= kVividChroma) continue;
        float best = 1e9f;
        for (const auto& t : targets) best = std::min(best, delta_e(pal[i], t));
        if (best > kForeignDeltaE) ++foreign;
    }
    return foreign;
}

// Un solo color: se evalúan TODOS los colores de la paleta devuelta por Wallhaven.
// Se exige que al menos un color representativo de la paleta coincida perceptualmente
// con el tono seleccionado según las tolerancias estrictas calibradas.
inline MatchResult evaluate_single(const std::vector<std::string>& palette, const std::string& target_hex) {
    auto t = parse_hex(target_hex);
    auto pal = palette_to_lab(palette);
    if (!t) return {};
    if (pal.empty()) {
        // Si no hay paleta detallada pero Wallhaven lo devolvió para colors=HEX, se acepta
        return {true, 50.0f};
    }
    Lab tl = to_lab(*t);

    float best = 1e9f;
    bool found_match = false;
    for (size_t i = 0; i < pal.size(); ++i) {
        if (is_color_match(tl, pal[i])) {
            found_match = true;
            float d = delta_e(pal[i], tl);
            if (is_neutral(tl)) {
                if (tl.L >= 90.0f) {
                    d = std::abs(pal[i].L - 100.0f) + chroma(pal[i]) * 1.5f;
                } else {
                    d = pal[i].L + chroma(pal[i]) * 1.5f;
                }
            }
            float rank_penalty = static_cast<float>(i) * 2.5f;
            best = std::min(best, d + rank_penalty);
        }
    }

    if (!found_match) {
        return {false, 1e9f};
    }

    int foreign = count_foreign(pal, 0, {tl});
    return {true, best + static_cast<float>(foreign) * 4.0f};
}

// Bicolor: Color 1 = tono dominante (índice 0 o índice 1 si el fondo es neutro),
// Color 2 = acento presente en la paleta, tolerando fondos oscuros/claros habituales.
inline MatchResult evaluate_dual(const std::vector<std::string>& palette,
                                 const std::string& primary_hex,
                                 const std::string& secondary_hex) {
    auto p = parse_hex(primary_hex);
    auto s = parse_hex(secondary_hex);
    auto pal = palette_to_lab(palette);
    if (!p || !s || pal.size() < 2) return {};
    Lab pl = to_lab(*p), sl = to_lab(*s);

    bool prim_matched = false;
    float d_primary = 1e9f;
    if (is_color_match(pl, pal[0])) {
        prim_matched = true;
        d_primary = delta_e(pal[0], pl);
    } else if (is_neutral(pal[0]) && pal.size() >= 2 && is_color_match(pl, pal[1])) {
        prim_matched = true;
        d_primary = delta_e(pal[1], pl) + 2.0f;
    }

    if (!prim_matched) return {};

    float d_secondary = 1e9f;
    bool sec_matched = false;
    for (size_t i = 1; i < pal.size(); ++i) {
        if (is_color_match(sl, pal[i])) {
            sec_matched = true;
            d_secondary = std::min(d_secondary, delta_e(pal[i], sl) + static_cast<float>(i - 1) * 2.0f);
        }
    }
    if (!sec_matched) return {};

    int foreign = count_foreign(pal, 1, {pl, sl});
    if (foreign >= 2) return {};
    return {true, d_primary * 1.5f + d_secondary + static_cast<float>(foreign) * 10.0f};
}

// Umbral ultra-estricto para fondos verdaderamente monocromáticos (escala de grises pura, sin ningún tinte)
inline constexpr float kStrictMonochromeChroma = 5.0f;

// Monocromo estricto: TODOS los colores de la paleta deben carecer de tono perceptible (croma <= 5.0).
inline bool is_strict_monochrome(const std::vector<std::string>& palette) {
    auto pal = palette_to_lab(palette);
    if (pal.empty()) return false;
    return std::all_of(pal.begin(), pal.end(), [](const Lab& c) { return chroma(c) <= kStrictMonochromeChroma; });
}

enum class Temperature { Warm, Cool };

// Rojos, naranjas, amarillos y rosas = cálidos; verdes, cianes, azules y violetas = fríos.
inline bool is_warm_hue(float h) { return h < 108.0f || h >= 340.0f; }

// Temperatura: ponderación por dominancia. Se eliminó la trampa de descarte inmediato
// 'first_vivid_checked' para no rechazar fondos fríos por un pequeño destello cálido
// (y viceversa). Se exige que el tono objetivo sea representativo en la composición.
inline bool matches_temperature(const std::vector<std::string>& palette, Temperature want) {
    auto pal = palette_to_lab(palette);
    if (pal.empty()) return true;

    float warm_w = 0.0f, cool_w = 0.0f;
    for (size_t i = 0; i < pal.size(); ++i) {
        float c = chroma(pal[i]);
        if (c <= kNeutralChroma) continue;
        float w = static_cast<float>(5 - std::min<size_t>(i, 4)) * std::min(1.0f, c / 30.0f);
        bool warm = is_warm_hue(hue_deg(pal[i]));
        (warm ? warm_w : cool_w) += w;
    }
    float total = warm_w + cool_w;
    if (total <= 0.0f) return true; // Fondo esencialmente neutro/monocromo: tolerar
    float target = (want == Temperature::Warm) ? warm_w : cool_w;
    float other  = (want == Temperature::Warm) ? cool_w : warm_w;

    return (target >= other * 0.6f) || (target / total >= 0.35f);
}

} // namespace wallpaper::color

