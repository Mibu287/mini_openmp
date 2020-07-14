#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

class ThreadPoolInterface
{
  public:
    // register task to be executed by thread pool
    virtual void schedule(std::function<void()>) = 0;

    // Returns the number of threads in the pool.
    virtual int num_threads() const = 0;

    // Returns a logical thread index between 0 and NumThreads() - 1 if called
    // from one of the threads in the pool. Returns -1 otherwise.
    virtual int thread_id() const = 0;

    virtual ~ThreadPoolInterface() {}
};

class SimpleThreadPool : public ThreadPoolInterface
{
  public:
    SimpleThreadPool(unsigned int n_threads = 1);
    SimpleThreadPool(const SimpleThreadPool &) = delete;

    void schedule(std::function<void()> fn) override;
    int num_threads() const override;
    int thread_id() const override;

    ~SimpleThreadPool();

  private:
    void worker_loop();
    struct Task
    {
        std::function<void()> fn;
    };

    mutable std::mutex mu_{};
    const unsigned int n_threads_;
    std::vector<std::thread> threads_;
    std::deque<Task> queue_{};
    std::condition_variable queue_cv_{};
    bool exiting_ = false;
};

SimpleThreadPool::SimpleThreadPool(unsigned int n_threads)
    : n_threads_{n_threads}, threads_{n_threads}
{
    if (n_threads < 1)
    {
        throw std::invalid_argument("n_threads must be greater than 0");
    }

    for (auto &t : threads_)
    {
        t = std::thread{[this] { this->worker_loop(); }};
    }
}

SimpleThreadPool::~SimpleThreadPool()
{
    mu_.lock();
    exiting_ = true;
    mu_.unlock();
    queue_cv_.notify_all();

    for (auto &t : threads_)
        t.join();
}

int
SimpleThreadPool::num_threads() const
{
    return n_threads_;
}

int
SimpleThreadPool::thread_id() const
{
    std::thread::id current_thread_id = std::this_thread::get_id();

    for (unsigned int i = 0; i < threads_.size(); ++i)
    {
        const std::thread &t = threads_[i];
        if (t.get_id() == current_thread_id)
            return i;
    }

    return -1;
}

void
SimpleThreadPool::schedule(std::function<void()> fn)
{
    std::unique_lock<std::mutex> lck{mu_};
    queue_.push_back(Task{fn});
    queue_cv_.notify_one();
}

void
SimpleThreadPool::worker_loop()
{
    std::unique_lock<std::mutex> lck{mu_};
    while (!exiting_)
    {
        // wait for new task
        while (queue_.empty())
        {
            queue_cv_.wait(lck);
            if (exiting_)
                return;
        }

        // queue_ not empty, lck has been acquired
        // run new task
        Task task = queue_.front();
        queue_.pop_front();

        lck.unlock();
        task.fn();
        lck.lock();
    }
}

#endif // THREAD_POOL_HPP_
