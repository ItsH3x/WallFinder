#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <type_traits>
#include <memory>
#include <atomic>
#include <stdexcept>

namespace wallpaper {

class ThreadPool {
public:
    explicit ThreadPool(size_t threads = 0) : stop_(false) {
        if (threads == 0) {
            threads = std::thread::hardware_concurrency();
            if (threads == 0) {
                threads = 4;
            }
        }

        workers_.reserve(threads);
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex_);
                        this->cv_.wait(lock, [this]() {
                            return this->stop_.load() || !this->tasks_.empty();
                        });

                        if (this->stop_.load() && this->tasks_.empty()) {
                            return;
                        }

                        task = std::move(this->tasks_.front());
                        this->tasks_.pop();
                    }

                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            [func = std::forward<F>(f), ...fargs = std::forward<Args>(args)]() mutable {
                return std::invoke(std::forward<decltype(func)>(func), std::forward<decltype(fargs)>(fargs)...);
            }
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_.load()) {
                throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return res;
    }

    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_.load()) {
                return;
            }
            stop_.store(true);
        }
        cv_.notify_all();

        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    size_t size() const noexcept {
        return workers_.size();
    }

    size_t pending_tasks() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    bool is_stopped() const noexcept {
        return stop_.load();
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
};

} // namespace wallpaper

