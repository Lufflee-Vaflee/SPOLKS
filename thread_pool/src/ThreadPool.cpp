#include "ThreadPool.hpp"

#include <iostream>
#include <thread>

namespace pool {

DummyThreadPool::DummyThreadPool() {
    std::cout << "Constructing Thread Pool\n";
}

DummyThreadPool::~DummyThreadPool() {
    std::cout << "Destroying Thread Pool\n";
}

void DummyThreadPool::start() {
    //Stopped
    std::cout << "try start Thread Pool\n";

    state_t expected = state_t::Stopped;
    if(!m_state.compare_exchange_strong(expected, state_t::Launching)) {
        throw std::domain_error("called start on non-stopped thread-pool");
    }

    std::cout << "launching thread-pool";

    //Launching
    std::vector<std::thread> handlers;
    m_state = state_t::Launching;
    for(std::size_t i = 1; i < std::thread::hardware_concurrency(); i++) {
        handlers.emplace_back([this](){
            ++m_current_thread_amount;
            pool_entry();
            m_current_thread_amount--;
        });
    }

    //Started
    m_state = state_t::Started;
    pool_entry();

    //Stopping
    for(std::size_t i = 0; i < handlers.size(); ++i) {
        handlers[i].join();
    }

    while(!m_tasks.empty()) {
        m_tasks.pop();
    }

    //Stopped
    m_threads_to_start = std::thread::hardware_concurrency();
    m_state = state_t::Stopped;
}

atomic_state const& DummyThreadPool::get_state_ref() {
    return m_state;
}

void DummyThreadPool::stop() {
    auto expected = state_t::Started;
    if(!m_state.compare_exchange_strong(expected, state_t::Stopping)) {
        throw std::domain_error("try stopping server on non-started thread pool");
    }

    m_cond.notify_all();
}

void DummyThreadPool::reserve_service(task_t&& service) {
    if(m_state != state_t::Stopped) {
        throw std::domain_error("trying reserve service on non-stopped pool");
    }

    {
        std::lock_guard lock {m_mutex};
        m_tasks.push(std::move(service));
        m_threads_to_start++;
    }
}

bool DummyThreadPool::go(task_t&& task) {
    if(!task || m_state != state_t::Started) {
        return false;
    }

    {
        std::lock_guard lock {m_mutex};
        m_tasks.push(std::move(task));
        m_task_load_approximation.fetch_add(1, std::memory_order_relaxed);
    }

    m_cond.notify_one();

    return true;
}

void DummyThreadPool::pool_entry() {
    auto id = std::this_thread::get_id();

    //theoretically, not to much sence in that, but still spooky to start consuming tasks before all threads initialized
    //so in case its quite cheap...
    while(m_state == state_t::Launching) {
        std::this_thread::yield();
    }

    while(m_state == state_t::Started) {
        task_t task;
        {
            std::unique_lock lock {m_mutex};
            m_cond.wait(lock, [id, this] () {
                std::cout << "Thread with id: " << id << " woke up\n";
                return !m_tasks.empty() || m_state != state_t::Started;
            });

            if(m_state != state_t::Started) {
                return;
            }

            task = std::move(m_tasks.front());

            m_tasks.pop();
        }

        if(m_state != state_t::Started) {
            return;
        }

        try {
            task();
            m_task_load_approximation.fetch_add(-1, std::memory_order_relaxed);
        } catch(std::exception exc) {
            std::cout << "exception occured: " << exc.what() << "\n";
            continue;
        } catch(const char* err) {
            std::cout << "exception occured: " << err << "\n";
            continue;
        }
    }
}

bool DummyThreadPool::getLoad() {
    return m_task_load_approximation < m_threads_to_start;
}

}
