// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

#include "p/rpc/socket.h"       // Socket
#include <atomic>
#include <vector>
#include <mutex>

namespace p {
namespace rpc {

class TimerSocket : public Socket {
public:
    TimerSocket();

    int stop() {
        return wakeup_after(0);
    }

    int wakeup_now() {
        return wakeup_after(1); // after 1 nano second
    }

    int wakeup_after(int64_t interval_ns);

    void run_timer();

    void set_triggered(int ret) {
        is_triggered_ = ret;
    }

private:
    int                            is_triggered_;
    std::vector<int64_t>           timer_heap_;

    std::atomic<int64_t>           min_timestamp_ns_;

    std::mutex                     mutex_;
    std::vector<int64_t>           timer_list_;
};

} // end namespace rpc
} // end namespace p

