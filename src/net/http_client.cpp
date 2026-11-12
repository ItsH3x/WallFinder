#include "net/http_client.hpp"

#include <curl/curl.h>
#include <fstream>
#include <system_error>
#include <iostream>

namespace wallpaper {

namespace {

struct CurlGlobalGuard {
    CurlGlobalGuard() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    ~CurlGlobalGuard() {
        curl_global_cleanup();
    }
};

void ensure_curl_init() {
    static CurlGlobalGuard guard;
}

struct ProgressContext {
    const std::function<void(size_t, size_t)>* callback{nullptr};
};

int curl_xferinfo_callback(void* clientp, 
                           curl_off_t dltotal, 
                           curl_off_t dlnow,
                           curl_off_t /*ultotal*/, 
                           curl_off_t /*ulnow*/) {
    auto* ctx = static_cast<ProgressContext*>(clientp);
    if (ctx && ctx->callback && *(ctx->callback)) {
        (*(ctx->callback))(static_cast<size_t>(dlnow), static_cast<size_t>(dltotal));
    }
    return 0;
}

size_t curl_string_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    size_t total_bytes = size * nmemb;
    body->append(ptr, total_bytes);
    return total_bytes;
}

size_t curl_file_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* file = static_cast<std::ofstream*>(userdata);
    size_t total_bytes = size * nmemb;
    file->write(ptr, static_cast<std::streamsize>(total_bytes));
    return file->good() ? total_bytes : 0;
}

} // namespace

HttpClient::HttpClient() {
    ensure_curl_init();
}

HttpClient::~HttpClient() = default;

HttpClient::HttpClient(HttpClient&&) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

void HttpClient::set_timeout(long seconds) {
    timeout_seconds_ = seconds;
}

long HttpClient::get_timeout() const {
    return timeout_seconds_;
}

void HttpClient::set_user_agent(const std::string& user_agent) {
    user_agent_ = user_agent;
}

const std::string& HttpClient::get_user_agent() const {
    return user_agent_;
}

std::string HttpClient::url_encode(const std::string& value) {
    ensure_curl_init();
    CURL* curl = curl_easy_init();
    if (!curl) {
        return value;
    }

    char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.length()));
    std::string result = encoded ? encoded : "";
    if (encoded) {
        curl_free(encoded);
    }
    curl_easy_cleanup(curl);
    return result;
}

HttpResponse HttpClient::get(const std::string& url, const std::map<std::string, std::string>& headers) {
    ensure_curl_init();
    HttpResponse response;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "No se pudo inicializar el manejador de libcurl";
        response.success = false;
        return response;
    }

    struct curl_slist* header_list = nullptr;
    for (const auto& [header_name, header_value] : headers) {
        std::string line = header_name + ": " + header_value;
        header_list = curl_slist_append(header_list, line.c_str());
    }

    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    char errbuf[CURL_ERROR_SIZE] = {0};
    std::string response_body;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent_.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_ * 3);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_string_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.error = (errbuf[0] != '\0') ? errbuf : curl_easy_strerror(res);
        response.success = false;
    } else {
        long status_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
        response.status_code = static_cast<int>(status_code);
        response.body = std::move(response_body);
        response.success = (response.status_code >= 200 && response.status_code < 300);
        if (!response.success && response.error.empty()) {
            response.error = "Error HTTP " + std::to_string(response.status_code);
        }
    }

    if (header_list) {
        curl_slist_free_all(header_list);
    }
    curl_easy_cleanup(curl);

    return response;
}

bool HttpClient::download_file(const std::string& url, 
                              const std::filesystem::path& destination, 
                              std::function<void(size_t, size_t)> progress_cb) {
    ensure_curl_init();

    std::error_code ec;
    if (destination.has_parent_path()) {
        std::filesystem::create_directories(destination.parent_path(), ec);
        if (ec) {
            return false;
        }
    }

    std::filesystem::path temp_destination = destination;
    temp_destination += ".part";

    std::ofstream out_file(temp_destination, std::ios::binary | std::ios::trunc);
    if (!out_file.is_open()) {
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        out_file.close();
        std::filesystem::remove(temp_destination, ec);
        return false;
    }

    char errbuf[CURL_ERROR_SIZE] = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent_.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_seconds_);
    // Do not restrict large downloads to 30s; use low speed limit (1KB/s for 30s) to abort stale connections
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_file_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out_file);

    ProgressContext prog_ctx;
    if (progress_cb) {
        prog_ctx.callback = &progress_cb;
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_xferinfo_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog_ctx);
    } else {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    }

    CURLcode res = curl_easy_perform(curl);
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    out_file.close();
    curl_easy_cleanup(curl);

    bool success = (res == CURLE_OK) && (status_code >= 200 && status_code < 300);
    if (success) {
        std::filesystem::rename(temp_destination, destination, ec);
        if (ec) {
            // Fallback for cross-device moves
            std::filesystem::copy_file(temp_destination, destination, 
                                       std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(temp_destination, ec);
        }
        return true;
    } else {
        std::filesystem::remove(temp_destination, ec);
        return false;
    }
}

} // namespace wallpaper

