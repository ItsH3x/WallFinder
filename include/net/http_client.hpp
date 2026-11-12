#pragma once

#include <string>
#include <map>
#include <filesystem>
#include <functional>
#include <memory>

namespace wallpaper {

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::string error;
    bool success = false;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {});

    bool download_file(const std::string& url, 
                       const std::filesystem::path& destination, 
                       std::function<void(size_t downloaded, size_t total)> progress_cb = nullptr);

    static std::string url_encode(const std::string& value);

    void set_timeout(long seconds);
    [[nodiscard]] long get_timeout() const;

    void set_user_agent(const std::string& user_agent);
    [[nodiscard]] const std::string& get_user_agent() const;

private:
    std::string user_agent_{"WallpaperFinder/1.0"};
    long timeout_seconds_{10};
};

} // namespace wallpaper

