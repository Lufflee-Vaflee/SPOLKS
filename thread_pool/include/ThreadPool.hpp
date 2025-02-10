#pragma once

#include <atomic>
#include <functional>
#include <condition_variable>
#include <queue>

namespace pool {

enum state_t : int {
    Stopped,
    Launching,
    Started,
    Stopping,
};

using atomic_state = std::atomic<state_t>;
using task_t = std::move_only_function<void()>;

class DummyThreadPool {
   private:
    DummyThreadPool();
    DummyThreadPool(DummyThreadPool const&) = delete;
    DummyThreadPool(DummyThreadPool&&) = delete;
    DummyThreadPool& operator=(DummyThreadPool const&) = delete;
    DummyThreadPool& operator=(DummyThreadPool&&) = delete;
    ~DummyThreadPool();

   public:
    static DummyThreadPool& getInstance() {
        static DummyThreadPool pool;
        return pool;
    }

    void start();
    void stop();
    void reserve_service(task_t&& service);
    bool go(task_t&& service);
    atomic_state const& get_state_ref();


   private:
    void pool_entry();

   private:
    atomic_state m_state = state_t::Stopped;
    std::atomic<std::size_t> m_current_thread_amount = 0;
    std::size_t m_threads_to_start = std::thread::hardware_concurrency();

    std::condition_variable m_cond;
    std::mutex m_mutex;
    std::queue<task_t> m_tasks;
};

}

