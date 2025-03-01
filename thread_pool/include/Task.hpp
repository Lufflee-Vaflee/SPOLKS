#include <coroutine>
#include <exception>
#include <functional>

#include "ThreadPool.hpp"

// Awaitable that yields control back to the thread pool
struct YieldAwaitable {
    bool await_ready() const noexcept {
        return false; // Never ready, always suspend
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        // Queue this task back into the thread pool
        m_pool.go([handle]() mutable {
            handle.resume();
        });
    }

    void await_resume() noexcept {}


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

        std::suspend_always initial_suspend() noexcept {
            m_pool.go([handler = handler_type::from_promise(*this)]() mutable {
                handler();
            });

            return {};
        }

        std::suspend_never final_suspend() noexcept {
            return {};
        }

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }

        void return_void() noexcept {}

        pool::DummyThreadPool& m_pool = pool::DummyThreadPool::getInstance();
    };



    // Constructor from a coroutine handle
    explicit Task(handler_type handle) : handle_(handle) {}

    // Move constructor and assignment
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            handle_ = other.handle_;
        }

        return *this;
    }

    // Destructor to clean up the coroutine handle
    ~Task() {
        // No need to manually destroy the handle anymore
        // The final_suspend returning std::suspend_never will handle destruction
    }

    // Non-copyable
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

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

    // Awaitable interface to allow direct co_await on Task objects
    bool await_ready() noexcept {
        // If the task is already done, we don't need to suspend
        return is_done();
    }

    bool await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept {
        if (is_done()) {
            // If already done, don't suspend
            return false;
        }

        try {
            // Execute the awaited task directly in the current thread
            resume();
            
            // If task is now done, resume awaiting coroutine
            if (is_done()) {
                return false; // Don't suspend
            }
            
            // If not done, the task must have scheduled itself elsewhere
            // Store the awaiting coroutine to resume it when this task completes
            // This would require additional machinery to store and resume waiting coroutines
            // which isn't implemented in this version
            return true; // Suspend
        } catch (...) {
            // Something went wrong - don't suspend
            return false;
        }
    }

    void await_resume() {
        // Check if there was an exception in the promise
        if (handle_ && handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
    }

    // Conversion operator to std::move_only_function<void()> for compatibility with ThreadPool
    operator std::move_only_function<void()>() && {
        // Capture the handle by value in the lambda
        auto h = handle_;
        return [h]() mutable {
            if (h && !h.done()) {
                h();
            }
        };
    }

   private:
    handler_type handle_ = nullptr;
};

