#pragma once

#ifndef _TASK_QUEUE_H_
#define _TASK_QUEUE_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <utility>

class TaskQueue {
 public:
  TaskQueue(const std::string& name, const uint32_t stack_depth, UBaseType_t priority)
      : stack_buffer_(new StackType_t[stack_depth]),
        task_handle_(xTaskCreateStatic(&Loop, name.c_str(), stack_depth, this, priority, stack_buffer_, &task_buffer_)) {
    assert(stack_buffer_ != nullptr && task_handle_ != nullptr);
    if (stack_buffer_ == nullptr || task_handle_ == nullptr) {
      abort();
    }
  }

  ~TaskQueue() {
    const auto termination_sem = xSemaphoreCreateBinary();
    Enqueue([termination_sem]() {
      xSemaphoreGive(termination_sem);
      vTaskDelay(portMAX_DELAY);
    });
    xSemaphoreTake(termination_sem, portMAX_DELAY);
    vSemaphoreDelete(termination_sem);
    vTaskDelete(task_handle_);
    delete[] stack_buffer_;
  }

  template <class F, class... Args>
  void Enqueue(F&& f, Args&&... args) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks_.emplace(Task{
          id_++,
          std::chrono::steady_clock::now(),
          [f = std::forward<F>(f), ... args = std::forward<Args>(args)]() mutable { f(std::forward<Args>(args)...); },
      });
    }
    condition_.notify_one();
  }

  template <class F, class... Args>
  void EnqueueAt(std::chrono::time_point<std::chrono::steady_clock> time_point, F&& f, Args&&... args) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks_.emplace(Task{
          id_++,
          std::move(time_point),
          [f = std::forward<F>(f), ... args = std::forward<Args>(args)]() mutable { f(std::forward<Args>(args)...); },
      });
    }
    condition_.notify_one();
  }

  TaskQueue(const TaskQueue&) = delete;
  TaskQueue& operator=(const TaskQueue&) = delete;

 private:
  struct Task {
    uint64_t id;
    std::chrono::time_point<std::chrono::steady_clock> scheduled_time;
    std::function<void()> task;

    bool operator>(const Task& other) const {
      return scheduled_time == other.scheduled_time ? id > other.id : scheduled_time > other.scheduled_time;
    }
  };

  static void Loop(void* self) {
    reinterpret_cast<TaskQueue*>(self)->Loop();
  }

  void Loop() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !tasks_.empty(); });

        auto scheduled_time = tasks_.top().scheduled_time;

        condition_.wait_until(lock, scheduled_time, [this, &scheduled_time] {
          return std::chrono::steady_clock::now() >= tasks_.top().scheduled_time ? true : (scheduled_time = tasks_.top().scheduled_time, false);
        });

        task = std::move(const_cast<Task&>(tasks_.top()).task);
        tasks_.pop();
      }
      task();
    }
  }

  std::mutex mutex_;
  std::condition_variable condition_;
  std::priority_queue<Task, std::vector<Task>, std::greater<>> tasks_;
  StackType_t* stack_buffer_ = nullptr;
  StaticTask_t task_buffer_;
  TaskHandle_t task_handle_ = nullptr;
  uint64_t id_ = 0;
};

#endif