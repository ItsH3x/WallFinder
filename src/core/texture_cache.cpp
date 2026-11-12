#include "core/texture_cache.hpp"
#include "app/thread_pool.hpp"
#include "net/http_client.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma GCC diagnostic pop

#include <iostream>

namespace wallpaper {

TextureCache::TextureCache(std::shared_ptr<ThreadPool> thread_pool)
    : thread_pool_(std::move(thread_pool)) {
    if (!thread_pool_) {
        thread_pool_ = std::make_shared<ThreadPool>(4);
        owns_thread_pool_ = true;
    }
}

TextureCache::~TextureCache() {
    clear();
}

TextureCache::TextureCache(TextureCache&& other) noexcept {
    std::scoped_lock lock(other.cache_mutex_, other.pending_mutex_);
    thread_pool_ = std::move(other.thread_pool_);
    owns_thread_pool_ = other.owns_thread_pool_;
    textures_ = std::move(other.textures_);
    in_flight_keys_ = std::move(other.in_flight_keys_);
    pending_uploads_ = std::move(other.pending_uploads_);
}

TextureCache& TextureCache::operator=(TextureCache&& other) noexcept {
    if (this != &other) {
        clear();
        std::scoped_lock lock(cache_mutex_, other.cache_mutex_, pending_mutex_, other.pending_mutex_);
        thread_pool_ = std::move(other.thread_pool_);
        owns_thread_pool_ = other.owns_thread_pool_;
        textures_ = std::move(other.textures_);
        in_flight_keys_ = std::move(other.in_flight_keys_);
        pending_uploads_ = std::move(other.pending_uploads_);
    }
    return *this;
}

TextureInfo TextureCache::load_from_memory(const std::string& key, const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return TextureInfo{};
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(data),
        static_cast<int>(size),
        &width, &height, &channels, 4
    );

    if (!pixels) {
        return TextureInfo{};
    }

    TextureInfo info;
    info.width = width;
    info.height = height;
    info.loaded = true;

    if (glfwGetCurrentContext() != nullptr) {
        GLuint tex_id = 0;
        glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
        info.texture_id = tex_id;
    }

    stbi_image_free(pixels);

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        textures_[key] = info;
    }

    return info;
}

TextureInfo TextureCache::load_from_memory(const std::string& key, const std::string& data) {
    return load_from_memory(key, reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

TextureInfo TextureCache::load_from_memory(const std::string& key, const std::vector<uint8_t>& data) {
    return load_from_memory(key, data.data(), data.size());
}

void TextureCache::request_texture(const std::string& key, const std::string& url) {
    if (key.empty() || url.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = textures_.find(key);
        if (it != textures_.end()) {
            if (it->second.loaded || it->second.failed) {
                return; // Ya cargada exitosamente o falló definitivamente, evitar peticiones infinitas
            }
        }
    }

    if (check_in_flight(key)) {
        return;
    }

    mark_in_flight(key);

    if (!thread_pool_ || thread_pool_->is_stopped()) {
        unmark_in_flight(key);
        return;
    }

    thread_pool_->enqueue([this, key, url]() {
        HttpClient client;
        client.set_timeout(10);
        HttpResponse resp = client.get(url);

        if (!resp.success || resp.body.empty()) {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_uploads_.push(DecodedImage{key, nullptr, 0, 0, false});
            return;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(resp.body.data()),
            static_cast<int>(resp.body.size()),
            &width, &height, &channels, 4
        );

        if (!pixels) {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_uploads_.push(DecodedImage{key, nullptr, 0, 0, false});
            return;
        }

        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_uploads_.push(DecodedImage{key, pixels, width, height, true});
    });
}

void TextureCache::queue_decode_from_memory(const std::string& key, std::vector<uint8_t> data) {
    if (key.empty() || data.empty()) {
        return;
    }

    if (has_texture(key) || check_in_flight(key)) {
        return;
    }

    mark_in_flight(key);

    if (!thread_pool_ || thread_pool_->is_stopped()) {
        unmark_in_flight(key);
        return;
    }

    thread_pool_->enqueue([this, key, buf = std::move(data)]() {
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(buf.data()),
            static_cast<int>(buf.size()),
            &width, &height, &channels, 4
        );

        if (!pixels) {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_uploads_.push(DecodedImage{key, nullptr, 0, 0, false});
            return;
        }

        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_uploads_.push(DecodedImage{key, pixels, width, height, true});
    });
}

void TextureCache::process_pending_uploads(size_t max_uploads_per_frame) {
    std::vector<DecodedImage> to_upload;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        size_t count = 0;
        while (!pending_uploads_.empty() && count < max_uploads_per_frame) {
            to_upload.push_back(std::move(pending_uploads_.front()));
            pending_uploads_.pop();
            ++count;
        }
    }

    for (auto& item : to_upload) {
        unmark_in_flight(item.key);

        if (!item.success || !item.pixels) {
            TextureInfo failed_info;
            failed_info.loaded = false;
            failed_info.failed = true;
            failed_info.last_accessed = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(cache_mutex_);
            textures_[item.key] = failed_info;
            continue;
        }

        TextureInfo info;
        info.width = item.width;
        info.height = item.height;
        info.loaded = true;
        info.failed = false;
        info.last_accessed = std::chrono::steady_clock::now();

        if (glfwGetCurrentContext() != nullptr) {
            GLuint tex_id = 0;
            glGenTextures(1, &tex_id);
            glBindTexture(GL_TEXTURE_2D, tex_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, item.width, item.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, item.pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
            info.texture_id = tex_id;
        }

        stbi_image_free(item.pixels);
        item.pixels = nullptr;

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            // Control inteligente de memoria para scroll infinito:
            // Capacidad holgada (500 miniaturas). Al superar el límite, desalojar
            // exclusivamente texturas no accedidas recientemente (> 45 segundos)
            const size_t max_cached = 500;
            if (textures_.size() > max_cached) {
                auto now = std::chrono::steady_clock::now();
                std::vector<std::string> to_evict;
                for (const auto& [k, v] : textures_) {
                    if (k != item.key && (now - v.last_accessed) > std::chrono::seconds(45)) {
                        to_evict.push_back(k);
                        if (to_evict.size() >= 30) break;
                    }
                }
                for (const auto& k : to_evict) {
                    auto it = textures_.find(k);
                    if (it != textures_.end()) {
                        if (it->second.texture_id != 0 && glfwGetCurrentContext() != nullptr) {
                            glDeleteTextures(1, &it->second.texture_id);
                        }
                        textures_.erase(it);
                    }
                }
            }
            textures_[item.key] = info;
        }
    }
}

TextureInfo TextureCache::get_texture(const std::string& key) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = textures_.find(key);
    if (it != textures_.end()) {
        const_cast<TextureInfo&>(it->second).last_accessed = std::chrono::steady_clock::now();
        return it->second;
    }
    return TextureInfo{};
}

bool TextureCache::has_texture(const std::string& key) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = textures_.find(key);
    return (it != textures_.end() && it->second.loaded);
}

bool TextureCache::is_loading(const std::string& key) const {
    return check_in_flight(key);
}

void TextureCache::remove_texture(const std::string& key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = textures_.find(key);
    if (it != textures_.end()) {
        if (it->second.texture_id != 0 && glfwGetCurrentContext() != nullptr) {
            glDeleteTextures(1, &it->second.texture_id);
        }
        textures_.erase(it);
    }
}

void TextureCache::clear() {
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        while (!pending_uploads_.empty()) {
            auto& item = pending_uploads_.front();
            if (item.pixels) {
                stbi_image_free(item.pixels);
                item.pixels = nullptr;
            }
            pending_uploads_.pop();
        }
    }

    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (glfwGetCurrentContext() != nullptr) {
        for (auto& [key, info] : textures_) {
            if (info.texture_id != 0) {
                glDeleteTextures(1, &info.texture_id);
                info.texture_id = 0;
            }
        }
    }
    textures_.clear();
    in_flight_keys_.clear();
}

size_t TextureCache::size() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return textures_.size();
}

size_t TextureCache::pending_count() const {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    return pending_uploads_.size();
}

void TextureCache::mark_in_flight(const std::string& key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    in_flight_keys_.insert(key);
}

void TextureCache::unmark_in_flight(const std::string& key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    in_flight_keys_.erase(key);
}

bool TextureCache::check_in_flight(const std::string& key) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return in_flight_keys_.find(key) != in_flight_keys_.end();
}

} // namespace wallpaper
