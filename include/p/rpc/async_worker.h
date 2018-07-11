// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

#include "p/base/macros.h" // P_DISALLOW_COPY
#include "p/base/endpoint.h" // P_DISALLOW_COPY

#include <mutex>
#include <thread>
#include <unordered_map>
#include <deque>

#include "p/rpc/poller.h"

namespace p {
namespace rpc {

struct AsyncMessage {
    typedef void (*Func)(void*);
    Func        function;
    void*       object;
};

class AsyncWorker : public Poller {
public:
  AsyncWorker() {}

  ~AsyncWorker();

  void start();

  void stop();

  int join();

  // current thread is loop thread
  bool in_worker_thread();

  template<typename Object>
  void push_message(void (Object::*func)(void), Object* object) {
          AsyncMessage::Func* f = (AsyncMessage::Func*)(&func);
          push_message(*f, (void*)object);
  }

  // push a async message in the back of msg queue
  void push_message(AsyncMessage::Func func, void *object);

  // insert a async message in the front of msg queue
  void insert_message(AsyncMessage::Func func, void *object);

  void stop_func();

protected:
  void worker_running();

  void run_message();

private:
  bool is_running_;
  std::thread this_thread_;

  //TimerSocket timer_;

  // tasks waiting to run
  std::mutex mutex_;
  std::deque<AsyncMessage> queue_;

  std::deque<AsyncMessage> swaped_queue_;


  std::unordered_map<int64_t, Socket*>    socket_map_;
  //int32_t                        idle_timeout_sec_;
private:
  P_DISALLOW_COPY(AsyncWorker);
};

} // end namespace rpc
} // end namespace p

