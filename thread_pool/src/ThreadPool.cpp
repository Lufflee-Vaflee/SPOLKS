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
        throw "called start on non-stopped thread-pool";
    }

    std::cout << "launching thread-pool";

    //Launching
    std::vector<std::thread> handlers;
    m_state = state_t::Launching;
    for(std::size_t i = 1; i < std::thread::hardware_concurrency(); i++) {
        handlers.emplace_back([this](){
            auto id = ++m_current_thread_amount;
            pool_entry(id);
            m_current_thread_amount--;
        });
    }

    //Started
    m_state = state_t::Started;
    pool_entry(0);

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

void DummyThreadPool::stop() {
    auto expected = state_t::Started;
    if(!m_state.compare_exchange_strong(expected, state_t::Stopping)) {
        throw "Called stopped on non started pool";
    }

    m_cond.notify_all();
}

void DummyThreadPool::reserve_service(task_t&& service) {
    if(m_state != state_t::Stopped) {
        throw "trying reserve service on non-stopped pool";
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

    m_cond.notify_one();

    return true;
}

void DummyThreadPool::pool_entry(std::size_t id) {
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

}
