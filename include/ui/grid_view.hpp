#pragma once

#include "core/models.hpp"
#include <functional>
#include <vector>

namespace wallpaper {

class TextureCache;

class GridView {
public:
    GridView();
    ~GridView() = default;

    void render(
        const std::vector<WallpaperItem>& items,
        TextureCache& texture_cache,
        float card_target_width,
        bool is_loading,
        bool is_loading_more,
        bool has_more,
        std::function<void(const WallpaperItem& item)> on_item_click
    );

private:
    void render_card(
        const WallpaperItem& item,
        size_t item_index,
        TextureCache& texture_cache,
        float card_w,
        float thumb_h,
        std::function<void(const WallpaperItem& item)> on_item_click
    );

    void render_skeleton_card(
        size_t index,
        float card_w,
        float thumb_h
    );
};

} // namespace wallpaper
