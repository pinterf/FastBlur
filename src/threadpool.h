#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <algorithm>
#include <memory>
#include <vector>

class Threadpool {
public:
  static Threadpool* GetInstance(int threads = 0) {
    static std::once_flag flag;
    std::call_once(flag, [&]() {
      instance = new Threadpool(threads);
      });
    return instance;
  }

  void Queue(std::function<void()> function);
  int GetNThreads() const { return n_threads; }
  void Wait();

private:
  struct tp_struct {
    std::thread handle;
    std::function<void()> function;
    bool free{ true };
    bool stop{ false };
    std::mutex* main_mutex{ nullptr };
    std::mutex* return_mutex{ nullptr };
    std::condition_variable* main_cond{ nullptr };
    std::condition_variable* return_cond{ nullptr };
    std::deque<std::function<void()>>* queue{ nullptr };
    int i{ 0 };
  };

  static Threadpool* instance;
  explicit Threadpool(int _threads = 0);  // constructor is private
  ~Threadpool();

  void ThreadFunction(tp_struct* param);

  std::vector<tp_struct> threads;
  std::deque<std::function<void()>> queue;
  int n_threads;
  std::mutex main_mutex;
  std::mutex return_mutex;
  std::condition_variable main_cond;
  std::condition_variable return_cond;

  // Delete copy constructor and assignment operator
  Threadpool(const Threadpool&) = delete;
  Threadpool& operator=(const Threadpool&) = delete;
  // Delete move constructor and assignment operator
  Threadpool(Threadpool&&) = delete;
  Threadpool& operator=(Threadpool&&) = delete;
};
