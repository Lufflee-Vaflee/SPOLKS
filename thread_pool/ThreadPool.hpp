#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <deque>

namespace tcp {

using task = std::function<void(std::atomic<bool>& close)>;


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

        for(std::size_t i = 0; i < m_concurrency - 1; i++) { 
            m_pool.emplace_back([this](){
                pool_entry();
            });
        }

        //add_task(initital_task)
        //return;

        pool_entry();
    }

    void stop() {
        if(!started) {
            throw "AAA";
        }

        initiate_close = true;

    }

   private:
    void pool_entry() {
        
    }


   private:
    std::atomic<bool> started = false;
    std::atomic<bool> initiate_close = false;

    std::vector<std::thread> m_pool;
    std::size_t const m_concurrency = std::thread::hardware_concurrency();

    std::condition_variable cond;
    std::deque<task> m_tasks;
};

void signal_handler(int SIGNUM) {
    try {
        DummyThreadPool::getInstance().stop();
    } catch(...) {
        // nothing
    }
}


}
