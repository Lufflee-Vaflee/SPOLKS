#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <queue>

namespace tcp {

using task_t = std::move_only_function<void(std::atomic<bool>& close)>;


class DummyThreadPool {
    DummyThreadPool() {
        
    }

    DummyThreadPool(DummyThreadPool const&) = delete;
    DummyThreadPool(DummyThreadPool&&) = delete;
    DummyThreadPool& operator=(DummyThreadPool const&) = delete;
    DummyThreadPool& operator=(DummyThreadPool&&) = delete;

    ~DummyThreadPool() {
    }

   public:
    static DummyThreadPool& getInstance() {
        static DummyThreadPool pool;
        return pool;
    }

   void start() {
        if(started) {
            throw "AAAAAAAAA";
        }
        started = true;

        for(std::size_t i = 0; i < std::thread::hardware_concurrency() - 1; i++) { 
            std::thread thr([this](){
                pool_entry();
            });

            thr.detach();
        }

        pool_entry();
    }

    void stop() {
        if(!started) {
            throw "AAA";
        }

        m_cond.notify_all();
    }

    void go(task_t&& task) {
        {
            std::lock_guard lock {m_mutex};
            m_tasks.push(std::move(task));
        }
        m_cond.notify_one();
    }

   private:
    void pool_entry() {
        m_current_thread_amount++;

        while(true) {
            std::move_only_function<void(std::atomic<bool>&)> task;
            {
                std::unique_lock lock {m_mutex};
                m_cond.wait(lock, [this] {
                    return !m_tasks.empty() || initiate_close;
                });

                task = std::move(m_tasks.front());

                if(!task) {
                    return;
                }
            }

            try {

            } catch(std::exception) {
                continue;
            }

        }
    }


   private:
    std::atomic<bool> started = false;
    std::atomic<bool> initiate_close = false;

    std::atomic<std::size_t> m_current_thread_amount = 1;
    std::atomic<std::size_t> m_currently_used = 0;

    std::condition_variable m_cond;
    std::mutex m_mutex;
    std::queue<task_t> m_tasks;
};

void signal_handler(int SIGNUM) {
    try {
        DummyThreadPool::getInstance().stop();
    } catch(...) {
        // nothing
    }
}


}
