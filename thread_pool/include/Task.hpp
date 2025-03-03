#include <coroutine>
#include <exception>

#include "ThreadPool.hpp"

struct PoolReshedule {
    PoolReshedule(bool force = false) :
        m_force(force) {}

    bool await_ready() const noexcept {
        return !m_force && m_pool.getLoad();
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        m_pool.go([handle]() mutable {
            handle.resume();
        });
    }

    void await_resume() noexcept {}

    private:
    bool m_force;
    pool::DummyThreadPool& m_pool = pool::DummyThreadPool::getInstance();
};

// Task class that represents a coroutine that can yield execution back to the thread pool
class Task {
public:
    struct promise_type;
    using handler_type = std::coroutine_handle<promise_type>;

    // Promise type for the Task coroutine
    struct promise_type {
        std::exception_ptr exception = nullptr;

        Task get_return_object() noexcept {
            return Task{handler_type::from_promise(*this)};
        }

        auto initial_suspend() noexcept {
            return PoolReshedule{true};
        }

        std::suspend_never final_suspend() noexcept {
            return {};
        }

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }

        void return_void() noexcept {}

        bool setAwaiter() {
        
        }

        private:
        std::coroutine_handle<> m_awaiter = nullptr;
    };

    class AwaitableTask {
       friend class Task;
       private:
        AwaitableTask(handler_type awaitedTask) :
            m_awaitedTask(awaitedTask) {}

       public:
        bool await_ready() const noexcept {
            
        }

        bool await_suspend(std::coroutine_handle<> handle) noexcept {
            
        }

        void await_resume() noexcept {

        }

       private:
        handler_type m_awaitedTask;
    };

    auto operator co_await() {
        return AwaitableTask{this->handle_};
    }

    explicit Task(handler_type handle) : 
        handle_(handle) {}

    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            handle_ = other.handle_;
        }

        return *this;
    }

    // Non-copyable
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task() { }

    // Check if the task is done
    bool is_done() const noexcept {
        return !handle_ || handle_.done();
    }

    // Resume the task execution
    void resume() {
        if (handle_ && !handle_.done()) {
            handle_();
        }
    }

   private:
    handler_type handle_ = nullptr;
};

