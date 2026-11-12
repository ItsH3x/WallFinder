#pragma once

#include "core/models.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace wallpaper {

class TextureCache;
class WallpaperSetter;
class ThreadPool;

class PreviewModal {
public:
    PreviewModal();
    ~PreviewModal() = default;

    void open(const WallpaperItem& item);
    void close();
    [[nodiscard]] bool is_open() const noexcept { return is_open_; }
    [[nodiscard]] bool is_fullscreen() const noexcept { return is_fullscreen_; }
    void close_fullscreen() noexcept { is_fullscreen_ = false; }

    void render(
        TextureCache& texture_cache,
        WallpaperSetter& wallpaper_setter,
        ThreadPool& thread_pool,
        std::function<void(const std::string& message, bool is_success)> toast_cb
    );

private:
    bool is_open_{false};
    bool request_open_{false};
    bool just_opened_{false};
    bool is_fullscreen_{false};
    bool is_fullscreen_just_opened_{false};
    WallpaperItem item_;

    std::atomic<bool> is_action_in_progress_{false};
    std::atomic<float> action_progress_{0.0f};
    mutable std::mutex status_mutex_;
    std::string action_status_text_;

    void set_status(const std::string& text);
    std::string get_status() const;
    void render_fullscreen_overlay(TextureCache& texture_cache);
};

} // namespace wallpaper
