#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <queue>

#include <iostream>

namespace pool {

enum state_t : uint8_t {
    Stopped,
    Launching,
    Started,
    Stopping,
};

using atomic_state = std::atomic<state_t>;
using task_t = std::move_only_function<void(atomic_state const& close)>;

class DummyThreadPool {
   private:
    DummyThreadPool() {
        std::cout << "Constructing Thread Pool\n";
    }

   public:
    DummyThreadPool(DummyThreadPool const&) = delete;
    DummyThreadPool(DummyThreadPool&&) = delete;
    DummyThreadPool& operator=(DummyThreadPool const&) = delete;
    DummyThreadPool& operator=(DummyThreadPool&&) = delete;

    ~DummyThreadPool() {
        std::cout << "Destroying Thread Pool\n";
    }

   public:
    static DummyThreadPool& getInstance() {
        static DummyThreadPool pool;
        return pool;
    }

   public:
    void start() {
        //Stopped
        std::cout << "try start Thread Pool\n";
        if(m_state != state_t::Stopped) {
            throw "Called start on non-stopped pool";
        }

        std::cout << "launching thread Pool\n";

        //Launching
        m_state = state_t::Launching;
        for(std::size_t i = 1; i < std::thread::hardware_concurrency(); i++) { 
            std::thread thr([this](){
                auto id = ++m_current_thread_amount;
                pool_entry(id);
                m_current_thread_amount--;
            });

            thr.detach();
        }

        //Started
        m_state = state_t::Started;
        pool_entry(0);

        //Stopping
        while(m_current_thread_amount != 0) {
            std::this_thread::yield();
        }

        while(!m_tasks.empty()) {
            m_tasks.pop();
        }

        //Stopped
        m_threads_to_start = std::thread::hardware_concurrency();
        m_state = state_t::Stopped;
    }

    void stop() {
        if(m_state != state_t::Started) {
            throw "Called stopped on non started pool";
        }

        {
            std::lock_guard lock {m_mutex};
            m_tasks.push(task_t{});
            m_state = state_t::Stopping;
        }

        m_cond.notify_all();
    }

    void reserve_service(task_t&& service) {
        if(m_state != state_t::Stopped) {
            throw "trying reserve service on non-stopped pool";
        }

        {
            std::lock_guard lock {m_mutex};
            m_tasks.push(std::move(service));
            m_threads_to_start++;
        }
    }

    bool go(task_t&& task) {
        if(!task || m_state != state_t::Started) {
            return false;
        }

        {
            std::lock_guard lock {m_mutex};
            m_tasks.push(std::move(task));
        }
        m_cond.notify_one();

        return true;
    }

   private:
    void pool_entry(std::size_t id) {
        std::cout << "starting thread with id: " << id << " \n";

        //theoretically, not to much sence in that, but still spooky to start consuming tasks before all threads initialized
        //so in case its quite cheap...
        while(m_state == state_t::Launching) {
            std::this_thread::yield();
        }

        std::cout << "Thread with id: " << id << " waited for other threads and start to recieve tasks\n";
        while(m_state == state_t::Started) {
            task_t task;
            {
                std::cout << "Thread with id: " << id << " waiting for task on condition_variable\n";
                std::unique_lock lock {m_mutex};
                m_cond.wait(lock, [id, this] {
                    std::cout << "Thread with id: " << id << " woke up\n";
                    return !m_tasks.empty() || m_state != state_t::Started;
                });

                if(m_state != state_t::Started) {
                    std::cout << "Thread with id: " << id << " closing because of pool state change\n";
                    return;
                }

                task = std::move(m_tasks.front());

                if(!task) {
                    std::cout << "Thread with id: " << id << " closing because of stop task\n";
                    return;
                }

                std::cout << "Thread with id: " << id << " acquired task\n";
                m_tasks.pop();
            }

            if(m_state != state_t::Started) {
                std::cout << "Thread with id: " << id << " closing because of pool state change\n";
                return;
            }

            try {
                std::cout << "Thread with id: " << id << " start to execute task\n";
                task(m_state);
            } catch(std::exception) {
                continue;
            }
        }
    }


   private:
    atomic_state m_state = state_t::Stopped;
    std::atomic<std::size_t> m_current_thread_amount = 0;
    std::size_t m_threads_to_start = std::thread::hardware_concurrency();

    std::condition_variable m_cond;
    std::mutex m_mutex;
    std::queue<task_t> m_tasks;
};

}

