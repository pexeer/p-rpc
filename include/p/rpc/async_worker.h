// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

#include "p/base/macros.h"          // P_DISALLOW_COPY

#include <thread>
#include <mutex>
#include <vector>
#include <unordered_set>

#include "p/rpc/poller.h"           // Poller
#include "p/rpc/timer_socket.h"     // TimerSocket
#include "p/rpc/acceptor.h"

namespace p {
namespace rpc {

class Acceptor;

class AsyncWorker {
public:
  ~AsyncWorker();

  void start();

  void stop();

  bool in_worker_thread();

  void push_message(AsyncMessage::Func func, void* object);

  template<typename Object>
  void push_message(void (Object::*func)(void), Object* object) {
      AsyncMessage::Func* f = (AsyncMessage::Func*)(&func);
      push_message(*f, (void*)object);
  }

  void worker_running();

  void run_message();

  int wakeup_after(int64_t interval_ns) {
      return timer_.wakeup_after(interval_ns);
  }

private:
    bool                          is_running_;
    std::thread                   this_thread_;

    // event epoll
    Poller                        poller_;
    TimerSocket                   timer_;
    std::vector<AsyncMessage>     swaped_queue_;

    // tasks waiting to run
    std::mutex                    mutex_;
    std::vector<AsyncMessage>     queue_;

    std::unordered_set<Socket*>      socket_map_;
    std::unordered_set<Acceptor*>    acceptor_map_;
private:
    P_DISALLOW_COPY(AsyncWorker);
};

} // end namespace rpc
} // end namespace p

