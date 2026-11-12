#include "ui/preview_modal.hpp"
#include "ui/theme.hpp"
#include "ui/ui_icons.hpp"
#include "app/config.hpp"
#include "app/thread_pool.hpp"
#include "core/texture_cache.hpp"
#include "core/wallpaper_setter.hpp"
#include "net/http_client.hpp"

#include <imgui.h>
#include <iomanip>
#include <sstream>

namespace wallpaper {

namespace {

using theme::FontScope;
using theme::fonts;

std::string format_size(uint64_t bytes) {
    if (bytes == 0) return "Desconocido";
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

} // namespace

PreviewModal::PreviewModal() = default;

void PreviewModal::open(const WallpaperItem& item) {
    item_ = item;
    is_open_ = true;
    request_open_ = true;
    just_opened_ = true;
    is_fullscreen_ = false;
    is_fullscreen_just_opened_ = false;
    is_action_in_progress_ = false;
    action_progress_ = 0.0f;
    set_status("");
}

void PreviewModal::close() {
    is_open_ = false;
    request_open_ = false;
    just_opened_ = false;
    is_fullscreen_ = false;
    is_fullscreen_just_opened_ = false;
}

void PreviewModal::set_status(const std::string& text) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    action_status_text_ = text;
}

std::string PreviewModal::get_status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return action_status_text_;
}

void PreviewModal::render(
    TextureCache& texture_cache,
    WallpaperSetter& wallpaper_setter,
    ThreadPool& thread_pool,
    std::function<void(const std::string& message, bool is_success)> toast_cb
) {
    if (!is_open_) {
        return;
    }

    // Si el modo pantalla completa está activo, renderizar ÚNICAMENTE el visor inmersivo
    // a pantalla completa para que la ventana de detalles no se superponga ni la tape.
    if (is_fullscreen_) {
        render_fullscreen_overlay(texture_cache);
        return;
    }

    if (request_open_) {
        ImGui::OpenPopup("Detalle del Fondo de Pantalla");
        request_open_ = false;
    }

    ImVec2 viewport_sz = ImGui::GetMainViewport()->WorkSize;
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    float modal_w = std::min(1040.0f, viewport_sz.x * 0.92f);
    float modal_h = std::min(780.0f, viewport_sz.y * 0.92f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(modal_w, modal_h), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Detalle del Fondo de Pantalla", &is_open_, ImGuiWindowFlags_NoCollapse)) {
        if (!is_open_) {
            close();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        // Permitir cerrar modal con la tecla Escape
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            close();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        // Cerrar modal al presionar fuera de la ventana de detalles (en el fondo atenuado / backdrop)
        if (just_opened_) {
            just_opened_ = false;
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2 mpos = ImGui::GetIO().MousePos;
            ImVec2 wmin = ImGui::GetWindowPos();
            ImVec2 wmax = ImVec2(wmin.x + ImGui::GetWindowSize().x, wmin.y + ImGui::GetWindowSize().y);
            if (mpos.x < wmin.x || mpos.x > wmax.x || mpos.y < wmin.y || mpos.y > wmax.y) {
                close();
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
        }

        ImVec2 content_avail = ImGui::GetContentRegionAvail();

        // 1. Imagen de vista previa de máxima calidad (Full Resolution con Thumbnail como fallback inmediato)
        std::string full_key = item_.id + "_full";
        TextureInfo full_tex = texture_cache.get_texture(full_key);
        TextureInfo thumb_tex = texture_cache.get_texture(item_.id);

        // Solicitar imagen en alta resolución original
        if (!full_tex.loaded && !full_tex.failed && !texture_cache.is_loading(full_key) && !item_.full_url.empty()) {
            texture_cache.request_texture(full_key, item_.full_url);
        }
        // Solicitar miniatura si aún no está
        if (!thumb_tex.loaded && !thumb_tex.failed && !texture_cache.is_loading(item_.id) && !item_.preview_url.empty()) {
            texture_cache.request_texture(item_.id, item_.preview_url);
        }

        bool is_full_res = (full_tex.loaded && full_tex.texture_id != 0);
        TextureInfo active_tex = is_full_res ? full_tex : thumb_tex;

        float max_preview_w = content_avail.x;
        float max_preview_h = std::min(460.0f, modal_h * 0.54f);
        float aspect = 16.0f / 9.0f;
        if (item_.width > 0 && item_.height > 0) {
            aspect = static_cast<float>(item_.width) / static_cast<float>(item_.height);
        }

        float preview_w = max_preview_w;
        float preview_h = preview_w / aspect;
        if (preview_h > max_preview_h) {
            preview_h = max_preview_h;
            preview_w = preview_h * aspect;
        }

        // Centrado de la imagen en el modal
        float offset_x = (content_avail.x - preview_w) * 0.5f;
        if (offset_x > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
        }

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + preview_w, p0.y + preview_h);

        if (active_tex.loaded && active_tex.texture_id != 0) {
            ImTextureRef tex_ref(static_cast<ImTextureID>(active_tex.texture_id));
            ImGui::Image(
                tex_ref,
                ImVec2(preview_w, preview_h)
            );

            bool img_hovered = ImGui::IsItemHovered();
            bool img_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            if (img_hovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            if (img_clicked) {
                is_fullscreen_ = true;
                is_fullscreen_just_opened_ = true;
                ImGui::CloseCurrentPopup();
            }

            ImDrawList* draw_list = ImGui::GetWindowDrawList();

            // Píldora interactiva para expandir a pantalla completa cuando se posa el cursor
            if (img_hovered) {
                const char* fs_hint = "Clic para Pantalla Completa";
                ImVec2 fs_sz = ImGui::CalcTextSize(fs_hint);
                float pill_w = fs_sz.x + 36.0f;
                float pill_h = 30.0f;
                ImVec2 pill_min(p0.x + (preview_w - pill_w) * 0.5f, p0.y + (preview_h - pill_h) * 0.5f);
                ImVec2 pill_max(pill_min.x + pill_w, pill_min.y + pill_h);

                draw_list->AddRectFilled(pill_min, pill_max, IM_COL32(12, 16, 26, 230), pill_h * 0.5f);
                draw_list->AddRect(pill_min, pill_max, IM_COL32(255, 255, 255, 70), pill_h * 0.5f, 0, 1.4f);

                ImVec2 icon_c(pill_min.x + 16.0f, pill_min.y + pill_h * 0.5f);
                icons::DrawFullscreen(draw_list, icon_c, 13.0f, IM_COL32(255, 255, 255, 240));
                draw_list->AddText(ImVec2(pill_min.x + 27.0f, pill_min.y + (pill_h - fs_sz.y) * 0.5f), IM_COL32(255, 255, 255, 245), fs_hint);
            }

            // Badge indicador de calidad en esquina superior derecha (cápsula redondeada)
            std::string badge_str = is_full_res ? "HD Original" : "Cargando Alta Calidad...";
            ImVec2 b_sz = ImGui::CalcTextSize(badge_str.c_str());
            float pad_x = 10.0f;
            float pad_y = 5.0f;
            ImVec2 b_min(p1.x - b_sz.x - pad_x * 2.0f - 12.0f, p0.y + 12.0f);
            ImVec2 b_max(p1.x - 12.0f, p0.y + 12.0f + b_sz.y + pad_y * 2.0f);
            float b_h = b_max.y - b_min.y;
            draw_list->AddRectFilled(b_min, b_max, IM_COL32(12, 14, 20, 225), b_h * 0.5f);
            draw_list->AddRect(b_min, b_max, IM_COL32(255, 255, 255, 35), b_h * 0.5f, 0, 1.0f);
            ImU32 badge_color = is_full_res ? IM_COL32(72, 187, 120, 255) : IM_COL32(236, 201, 75, 255);
            draw_list->AddText(ImVec2(b_min.x + pad_x, b_min.y + pad_y), badge_color, badge_str.c_str());
        } else {
            // Placeholder estilizado
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(p0, p1, IM_COL32(28, 30, 40, 255), 6.0f);
            draw_list->AddRect(p0, p1, IM_COL32(50, 54, 72, 255), 6.0f);

            const char* loading_text = "Cargando vista previa en alta definición...";
            ImVec2 text_size = ImGui::CalcTextSize(loading_text);
            draw_list->AddText(
                ImVec2(p0.x + (preview_w - text_size.x) * 0.5f, p0.y + (preview_h - text_size.y) * 0.5f),
                IM_COL32(160, 165, 185, 255),
                loading_text
            );
            ImGui::Dummy(ImVec2(preview_w, preview_h));
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                is_fullscreen_ = true;
                is_fullscreen_just_opened_ = true;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 2. Metadatos del fondo
        ImGui::Columns(2, "MetaColumns", false);
        ImGui::SetColumnWidth(0, 360.0f);

        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "ID:");
        ImGui::SameLine();
        ImGui::Text("#%s (%s)", item_.id.c_str(), item_.source.empty() ? "Wallhaven" : item_.source.c_str());

        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Resolución:");
        ImGui::SameLine();
        ImGui::Text("%dx%d", item_.width, item_.height);

        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Aspect Ratio:");
        ImGui::SameLine();
        ImGui::Text("%s", item_.aspect_ratio.empty() ? "Desconocido" : item_.aspect_ratio.c_str());

        ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Tamaño:");
        ImGui::SameLine();
        ImGui::Text("%s", format_size(item_.file_size).c_str());

        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Formato:");
        ImGui::SameLine();
        ImGui::Text("%s", item_.file_type.empty() ? "image/jpeg" : item_.file_type.c_str());

        ImGui::Columns(1);

        // Tags en formato chip
        if (!item_.tags.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), "Etiquetas:");
            ImGui::SameLine();
            for (size_t i = 0; i < item_.tags.size() && i < 8; ++i) {
                std::string tag_label = "#" + item_.tags[i];
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.20f, 0.28f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
                ImGui::SmallButton(tag_label.c_str());
                ImGui::PopStyleColor(2);
                if (i + 1 < item_.tags.size() && i + 1 < 8) {
                    ImGui::SameLine();
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 3. Barra de progreso si hay acción en curso
        if (is_action_in_progress_.load()) {
            float prog = action_progress_.load();
            std::string st = get_status();
            ImGui::ProgressBar(prog, ImVec2(-1, 24), st.c_str());
            ImGui::Spacing();
        }

        // 4. Botones de acción con estilo consistente según el sistema de diseño
        bool busy = is_action_in_progress_.load();

        // Botón Guardar en Carpeta (botón secundario estilizado con icono de descarga)
        if (theme::Button(
                "##btn_save",
                icons::DrawDownload,
                "Guardar en Carpeta",
                ImVec2(220, 42),
                theme::ButtonKind::Secondary,
                !busy
            )) {
            is_action_in_progress_ = true;
            action_progress_ = 0.0f;
            set_status("Descargando fondo original...");

            WallpaperItem target_item = item_;
            std::filesystem::path dest_dir = Config::instance().wallpaper_dir;

            thread_pool.enqueue([this, target_item, dest_dir, &wallpaper_setter, toast_cb]() {
                std::filesystem::path dest_file = wallpaper_setter.build_filename(target_item, dest_dir);
                HttpClient client;
                client.set_timeout(30);

                bool ok = client.download_file(
                    target_item.full_url,
                    dest_file,
                    [this](size_t downloaded, size_t total) {
                        if (total > 0) {
                            action_progress_ = static_cast<float>(downloaded) / static_cast<float>(total);
                        }
                    }
                );

                is_action_in_progress_ = false;
                if (ok) {
                    wallpaper_setter.register_in_system_wallpapers(dest_file, target_item);
                    set_status("¡Guardado y añadido a los fondos del sistema!");
                    if (toast_cb) toast_cb("¡Fondo guardado y añadido a la lista del sistema!", true);
                } else {
                    set_status("Error en la descarga.");
                    if (toast_cb) toast_cb("Error al descargar el fondo de pantalla.", false);
                }
            });
        }

        ImGui::SameLine(0.0f, 12.0f);

        // Botón Aplicar como Fondo (botón principal destacado con estilo éxito)
        if (theme::Button(
                "##btn_apply",
                icons::DrawDesktop,
                "Aplicar como Fondo",
                ImVec2(220, 42),
                theme::ButtonKind::Success,
                !busy
            )) {
            is_action_in_progress_ = true;
            action_progress_ = 0.0f;
            set_status("Descargando y aplicando fondo...");

            WallpaperItem target_item = item_;
            std::filesystem::path dest_dir = Config::instance().wallpaper_dir;

            thread_pool.enqueue([this, target_item, dest_dir, &wallpaper_setter, toast_cb]() {
                auto res = wallpaper_setter.download_and_set(
                    target_item,
                    dest_dir,
                    [this](size_t downloaded, size_t total) {
                        if (total > 0) {
                            action_progress_ = static_cast<float>(downloaded) / static_cast<float>(total);
                        }
                    }
                );

                is_action_in_progress_ = false;
                if (res.success) {
                    std::filesystem::path final_path = res.saved_path.empty() ? (dest_dir / wallpaper_setter.build_filename(target_item, dest_dir).filename()) : res.saved_path;
                    wallpaper_setter.register_in_system_wallpapers(final_path, target_item);
                    set_status("¡Fondo aplicado y añadido al sistema!");
                    if (toast_cb) toast_cb("¡Fondo aplicado con éxito al escritorio y añadido a la lista del sistema!", true);
                } else {
                    set_status("Error: " + res.message);
                    if (toast_cb) toast_cb("Error al aplicar fondo: " + res.message, false);
                }
            });
        }

        ImGui::SameLine();
        float btn_close_x = ImGui::GetWindowWidth() - 130.0f;
        if (btn_close_x > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(btn_close_x);
        }

        if (theme::Button("##btn_close", nullptr, "Cerrar", ImVec2(100, 42), theme::ButtonKind::Ghost)) {
            close();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void PreviewModal::render_fullscreen_overlay(TextureCache& texture_cache) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.025f, 0.04f, 0.98f)); // Fondo casi negro ultra inmersivo

    ImGuiWindowFlags fs_flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##FullscreenWallpaperPreview", &is_fullscreen_, fs_flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 screen_sz = vp->Size;
        ImVec2 screen_pos = vp->Pos;

        // Textura en alta resolución o miniatura como fallback
        std::string full_key = item_.id + "_full";
        TextureInfo full_tex = texture_cache.get_texture(full_key);
        TextureInfo thumb_tex = texture_cache.get_texture(item_.id);
        TextureInfo active_tex = (full_tex.loaded && full_tex.texture_id != 0) ? full_tex : thumb_tex;

        float aspect = 16.0f / 9.0f;
        if (item_.width > 0 && item_.height > 0) {
            aspect = static_cast<float>(item_.width) / static_cast<float>(item_.height);
        }

        // Ajustar imagen a toda la pantalla conservando proporción
        float img_w = screen_sz.x;
        float img_h = img_w / aspect;
        if (img_h > screen_sz.y) {
            img_h = screen_sz.y;
            img_w = img_h * aspect;
        }

        float img_x = screen_pos.x + (screen_sz.x - img_w) * 0.5f;
        float img_y = screen_pos.y + (screen_sz.y - img_h) * 0.5f;

        if (active_tex.loaded && active_tex.texture_id != 0) {
            ImTextureRef tex_ref(static_cast<ImTextureID>(active_tex.texture_id));
            dl->AddImage(tex_ref, ImVec2(img_x, img_y), ImVec2(img_x + img_w, img_y + img_h));
        } else {
            const char* loading_txt = "Cargando imagen en resolución completa...";
            ImVec2 txt_sz = ImGui::CalcTextSize(loading_txt);
            dl->AddText(
                ImVec2(screen_pos.x + (screen_sz.x - txt_sz.x) * 0.5f, screen_pos.y + (screen_sz.y - txt_sz.y) * 0.5f),
                IM_COL32(180, 190, 210, 255),
                loading_txt
            );
        }

        // Píldora flotante superior informativa con icono
        const char* hint_txt = "Haz clic o presiona cualquier tecla para salir de pantalla completa";
        ImVec2 hint_sz;
        {
            FontScope fs(fonts().medium, theme::kFontSmall + 1.0f);
            hint_sz = ImGui::CalcTextSize(hint_txt);
        }
        float pad_x = 18.0f;
        float pad_y = 9.0f;
        float icon_sz = 14.0f;
        float pill_w = hint_sz.x + icon_sz + 10.0f + pad_x * 2.0f;
        float pill_h = hint_sz.y + pad_y * 2.0f;
        ImVec2 pill_min(screen_pos.x + (screen_sz.x - pill_w) * 0.5f, screen_pos.y + 24.0f);
        ImVec2 pill_max(pill_min.x + pill_w, pill_min.y + pill_h);

        dl->AddRectFilled(pill_min, pill_max, IM_COL32(10, 12, 20, 225), pill_h * 0.5f);
        dl->AddRect(pill_min, pill_max, IM_COL32(255, 255, 255, 45), pill_h * 0.5f, 0, 1.2f);

        ImVec2 icon_pos(pill_min.x + pad_x + icon_sz * 0.5f, pill_min.y + pill_h * 0.5f);
        icons::DrawFullscreen(dl, icon_pos, icon_sz, IM_COL32(220, 230, 250, 230));
        {
            FontScope fs(fonts().medium, theme::kFontSmall + 1.0f);
            dl->AddText(
                ImVec2(pill_min.x + pad_x + icon_sz + 10.0f, pill_min.y + pad_y),
                IM_COL32(220, 230, 250, 235),
                hint_txt
            );
        }

        // Control de cierre: un clic en cualquier parte o cualquier tecla presionada
        if (is_fullscreen_just_opened_) {
            // Evitar que el clic de apertura cierre inmediatamente en el mismo fotograma
            is_fullscreen_just_opened_ = false;
        } else {
            bool mouse_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                                 ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
                                 ImGui::IsMouseClicked(ImGuiMouseButton_Middle);

            bool any_key_pressed = false;
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
                if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k), false)) {
                    any_key_pressed = true;
                    break;
                }
            }

            if (mouse_clicked || any_key_pressed) {
                is_fullscreen_ = false;
                request_open_ = true;
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

} // namespace wallpaper
