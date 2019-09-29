/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __LATCH_HPP__
#define __LATCH_HPP__

#include <atomic>
#include <cassert>
#include <cstddef>
#include <future>
#include <thread>

#include "time.hpp"

namespace rocketmq {

class latch {
 public:
  explicit latch(ptrdiff_t value) : count_(value), promise_(), future_(promise_.get_future()), waiting_(0) {
    assert(count_ >= 0);
  }

  ~latch() {
    if (!is_ready()) {
      cancel_wait("latch is destructed");
    }

    while (waiting_ > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }

  latch(const latch&) = delete;
  latch& operator=(const latch&) = delete;

  void count_down_and_wait() {
    if (try_count_down(1)) {
      awake_waiter();
    } else {
      wait();
    }
  }

  void count_down(ptrdiff_t n = 1) {
    if (try_count_down(n)) {
      awake_waiter();
    }
  }

  bool is_ready() const noexcept { return count_ <= 0; }

  void wait() const {
    if (count_ > 0) {
      waiting_++;
      future_.wait();
      waiting_--;
    }
  }

  void wait(long timeout, time_unit unit) const {
    if (count_ > 0) {
      waiting_++;
      auto time_point = until_time_point(timeout, unit);
      future_.wait_until(time_point);
      waiting_--;
    }
  }

 private:
  bool try_count_down(ptrdiff_t n) {
    for (;;) {
      auto c = count_.load();
      if (c <= 0) {
        return false;
      }
      auto nextc = c - n;
      if (count_.compare_exchange_weak(c, nextc)) {
        return nextc <= 0;
      }
    }
  }

  void awake_waiter() {
    try {
      promise_.set_value();
    } catch (...) {
    }
  }

  void cancel_wait(const char* reason) {
    try {
      promise_.set_value();
    } catch (...) {
    }
  }

 private:
  std::atomic<ptrdiff_t> count_;
  std::promise<void> promise_;
  std::future<void> future_;

  mutable std::atomic<int> waiting_;
};

}  // namespace rocketmq

#endif  // __LATCH_HPP__
