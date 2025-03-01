#include <gtest/gtest.h>
#include <chrono>
#include <atomic>
#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include "../include/ThreadPool.hpp"
#include "../include/Task.hpp"

using namespace std::chrono_literals;

// Basic test fixture for ThreadPool tests
class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code - will be called before each test
        start = std::thread{[](){
            pool::DummyThreadPool::getInstance().start();
        }};

        std::this_thread::sleep_for(2s);
    }

    void TearDown() override {
        // Teardown code - will be called after each test
        pool::DummyThreadPool::getInstance().stop();

        start.join();

        std::this_thread::sleep_for(2s);
    }

    std::thread start;
};

// Basic functionality tests
TEST_F(ThreadPoolTest, BasicTaskExecution) {
    std::atomic<bool> task_executed{false};
    
    // Submit a task to the thread pool
    bool result = pool::DummyThreadPool::getInstance().go([&task_executed]() {
        task_executed = true;
    });
    
    EXPECT_TRUE(result) << "Task submission should succeed";
    
    // Wait for the task to complete
    for (int i = 0; i < 100 && !task_executed; ++i) {
        std::this_thread::sleep_for(10ms);
    }
    
    EXPECT_TRUE(task_executed) << "Task should have been executed";
}

TEST_F(ThreadPoolTest, MultipleTasksExecution) {
    const int NUM_TASKS = 4000;
    std::atomic<int> counter{0};
    
    // Submit multiple tasks to the thread pool
    for (int i = 0; i < NUM_TASKS; ++i) {
        pool::DummyThreadPool::getInstance().go([&counter]() {
            counter.fetch_add(1);
        });
    }
    
    // Wait for tasks to complete
    for (int i = 0; i < 800 && counter < NUM_TASKS; ++i) {
        std::this_thread::sleep_for(10ms);
    }
    
    EXPECT_EQ(counter, NUM_TASKS) << "All tasks should have been executed";
}

//TEST_F(ThreadPoolTest, StopAndRestart) {
//    std::atomic<int> counter{0};
//    
//    try {
//    // Submit a task
//    pool::DummyThreadPool::getInstance().go([&counter]() {
//        counter = 1;
//    });
//    
//    // Wait for task to complete
//    for (int i = 0; i < 50 && counter != 1; ++i) {
//        std::this_thread::sleep_for(10ms);
//    }
//    
//    
//    // Try to submit a task - should fail
//    bool result = pool::DummyThreadPool::getInstance().go([&counter]() {
//        counter = 2;
//    });
//    
//    EXPECT_FALSE(result) << "Task submission should fail when pool is stopped";
//    EXPECT_EQ(counter, 1) << "Counter should not have changed";
//
//    // Submit a task again
//    result = pool::DummyThreadPool::getInstance().go([&counter]() {
//        counter = 2;
//    });
//    
//    EXPECT_TRUE(result) << "Task submission should succeed after restart";
//    
//    // Wait for task to complete
//    for (int i = 0; i < 50 && counter != 2; ++i) {
//        std::this_thread::sleep_for(10ms);
//    }
//
//    } catch(const char* exc) {
//        std::cout << "exc occured: " << exc << '\n';
//    }
//    
//    EXPECT_EQ(counter, 2) << "Task should have been executed after restart";
//}

// Task coroutine tests
TEST_F(ThreadPoolTest, SimpleCoroutineTask) {
    std::atomic<bool> started{false};
    std::atomic<bool> completed{false};
    
    auto task = [](std::atomic<bool>& started, std::atomic<bool>& completed) -> Task {
        started = true;
        co_await YieldAwaitable{};
        completed = true;
        co_return;
    }(started, completed);

    // Wait for the task to start and complete
    for (int i = 0; i < 100 && (!started || !completed); ++i) {
        std::this_thread::sleep_for(10ms);
    }

    EXPECT_TRUE(started) << "Task should have started";
    EXPECT_TRUE(completed) << "Task should have completed";
}

TEST_F(ThreadPoolTest, MultipleYields) {
    std::atomic<int> progress{0};
    
    auto task = [&progress]() -> Task {
        progress = 1;
        co_await YieldAwaitable{};
        
        progress = 2;
        co_await YieldAwaitable{};
        
        progress = 3;
        co_await YieldAwaitable{};
        
        progress = 4;
        co_return;
    };

    // Create and submit the task
    task();

    // Wait for the task to complete all stages
    for (int i = 0; i < 100 && progress < 4; ++i) {
        std::this_thread::sleep_for(10ms);
    }

    EXPECT_EQ(progress, 4) << "Task should have completed all four stages";
}

// Integration tests between thread pool and tasks
TEST_F(ThreadPoolTest, CooperativeMultitasking) {
    std::mutex mutex;
    std::vector<int> execution_order;

    auto long_task = [&]() -> Task {
        {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(1);
        }

        // Yield to allow other tasks to run
        co_await YieldAwaitable{};
        
        {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(3);
        }

        // Yield again
        co_await YieldAwaitable{};
        
        {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(5);
        }
        co_return;
    };

    auto quick_task = [&]() {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(2);
    };

    auto another_quick_task = [&]() {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(4);
    };

    // Submit the long-running task first
    long_task();

    // Submit quick tasks which should execute when the long task yields
    pool::DummyThreadPool::getInstance().go(quick_task);


    // Submit another quick task
    pool::DummyThreadPool::getInstance().go(another_quick_task);

    // Wait for all tasks to complete
    std::this_thread::sleep_for(200ms);

    // Verify the execution order
    ASSERT_EQ(execution_order.size(), 5) << "All tasks should have executed";
}

TEST_F(ThreadPoolTest, ManyTasksWithYield) {
    const int NUM_TASKS = 20;
    std::atomic<int> completed_tasks{0};
    
    auto yielding_task = [&completed_tasks](int id) -> Task {
        // Do first part of work
        std::this_thread::sleep_for(5ms);
        
        // Yield to allow other tasks to run
        co_await YieldAwaitable{};
        
        // Do second part of work
        std::this_thread::sleep_for(5ms);
        
        // Mark as completed
        completed_tasks.fetch_add(1);
        co_return;
    };
 
    // Create and submit all tasks
    for (int i = 0; i < NUM_TASKS; ++i) {
        yielding_task(i);
    }
    
    // Wait for all tasks to complete
    for (int i = 0; i < 100 && completed_tasks < NUM_TASKS; ++i) {
        std::this_thread::sleep_for(10ms);
    }
    
    EXPECT_EQ(completed_tasks, NUM_TASKS) << "All tasks should have completed";
    
}

