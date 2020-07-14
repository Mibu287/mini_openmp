#ifndef PARALLEL_FOR_HPP_
#define PARALLEL_FOR_HPP_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>

#define MIN(x, y) ((x) < (y) ? (x) : (y))

enum class Strategy
{
    static_,
};

template <typename ThreadPool, Strategy>
void
parallel_for(ThreadPool &pool, long start, long stop, long step,
             std::function<void(long)> fn)
{
    const unsigned int n_threads = pool.num_threads();
    long chunk = (stop - start + step - 1) / step; // get ceiling division
    chunk = (chunk + n_threads - 1) / n_threads;   // get ceiling division

    if (chunk <= 0)
    {
        throw std::invalid_argument(
            "invalid start, stop, step arguments, (stop - start) and step must "
            "have same sign");
    }

    std::mutex mu{};
    std::condition_variable cv{};
    std::atomic_long ctr{0};

    for (long i = start; i < stop; i += chunk)
    {
        ctr.fetch_add(2, std::memory_order_relaxed);
        auto task = [i, stop, chunk, fn, &mu, &cv, &ctr]() -> void {
            for (long j = i; j < MIN(i + chunk, stop); ++j)
                fn(j);

            if (ctr.fetch_sub(2, std::memory_order_acq_rel) - 2 == 1)
            {
                mu.lock();
                mu.unlock();
                cv.notify_all();
            }
        };

        pool.schedule(task);
    }

    std::unique_lock<std::mutex> lck{mu};

    if (ctr.fetch_add(1, std::memory_order_relaxed) == 0)
        return;

    cv.wait(lck, [&]() { return ctr.load() == 1; });
};

#endif // PARALLEL_FOR_HPP_
