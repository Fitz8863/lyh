#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
private:
    std::vector<std::thread> threads;
    std::queue <std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable condition;
    bool stop;

public:
    ThreadPool(int numThreads);
    template<class F, class...Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;;
    ~ThreadPool();
};

inline ThreadPool::ThreadPool(int numThreads) :stop(false) {
    for (int i = 0;i < numThreads;i++) {
        threads.emplace_back(
            [this] {
                while (1) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->mtx);
                        this->condition.wait(lock,
                            [this] {
                                return this->stop || !this->tasks.empty();
                            });
                        if (this->stop && tasks.empty()) {
                            return;
                        }
                        task = std::move(this->tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
    }
}

template<class F, class...Args>
auto ThreadPool::enqueue(F && f, Args &&... args)
-> std::future<typename std::result_of<F(Args...)>::type> {

    // std::function<void()>  task;
    // task = std::bind(std::forward<F>(f), std::forward<Args>(args...));

    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared< std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(this->mtx);
        if (this->stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        // this->tasks.emplace(std::move(task));
        this->tasks.emplace([task]() { (*task)(); });
    }
    this->condition.notify_one();
    return res;
}

// 析构函数
inline ThreadPool::~ThreadPool() {
    // 这里是设置标志位为true
    {
        std::unique_lock<std::mutex> lock(mtx);
        this->stop = true;
    }
    // 让全部线程把任务堆里面的任务给完成
    condition.notify_all();
    for (auto& t : threads) {
        t.join();
    }
}


