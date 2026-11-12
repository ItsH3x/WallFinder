#pragma once

#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wallpaper {

class ThreadPool;
class HttpClient;

struct TextureInfo {
    GLuint texture_id = 0;
    int width = 0;
    int height = 0;
    bool loaded = false;
    bool failed = false;
    std::chrono::steady_clock::time_point last_accessed{};
};

class TextureCache {
public:
    explicit TextureCache(std::shared_ptr<ThreadPool> thread_pool = nullptr);
    ~TextureCache();

    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;
    TextureCache(TextureCache&&) noexcept;
    TextureCache& operator=(TextureCache&&) noexcept;

    // Synchronous memory decode & upload (works directly if GL context is current)
    TextureInfo load_from_memory(const std::string& key, const uint8_t* data, size_t size);
    TextureInfo load_from_memory(const std::string& key, const std::string& data);
    TextureInfo load_from_memory(const std::string& key, const std::vector<uint8_t>& data);

    // Asynchronous background download and decode for thumbnails / full images
    void request_texture(const std::string& key, const std::string& url);

    // Queue in-memory raw buffer to be decoded in worker thread
    void queue_decode_from_memory(const std::string& key, std::vector<uint8_t> data);

    // Upload pending textures to OpenGL (must be called from main render thread each frame)
    void process_pending_uploads(size_t max_uploads_per_frame = 6);

    // Query status
    [[nodiscard]] TextureInfo get_texture(const std::string& key) const;
    [[nodiscard]] bool has_texture(const std::string& key) const;
    [[nodiscard]] bool is_loading(const std::string& key) const;

    // Cache management
    void remove_texture(const std::string& key);
    void clear();

    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t pending_count() const;

private:
    struct DecodedImage {
        std::string key;
        unsigned char* pixels{nullptr};
        int width{0};
        int height{0};
        bool success{false};
    };

    std::shared_ptr<ThreadPool> thread_pool_;
    bool owns_thread_pool_{false};

    mutable std::mutex cache_mutex_;
    std::unordered_map<std::string, TextureInfo> textures_;
    std::unordered_set<std::string> in_flight_keys_;

    mutable std::mutex pending_mutex_;
    std::queue<DecodedImage> pending_uploads_;

    void mark_in_flight(const std::string& key);
    void unmark_in_flight(const std::string& key);
    bool check_in_flight(const std::string& key) const;
};

} // namespace wallpaper
