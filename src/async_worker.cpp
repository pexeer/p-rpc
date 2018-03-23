// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#include "p/rpc/async_worker.h"

#include "p/base/time.h"

namespace p {
namespace rpc {

thread_local AsyncWorker* tls_async_worker_ptr = nullptr;

AsyncWorker::~AsyncWorker() {
    stop();
    if (this_thread_.joinable()) {
        this_thread_.join();
    }
}

bool AsyncWorker::in_worker_thread() {
    return (this == tls_async_worker_ptr);
}

void AsyncWorker::start() {
    is_running_ = true;
    poller_.add_socket(&timer_, false);
    this_thread_ = std::thread(&AsyncWorker::worker_running, this);
}

void AsyncWorker::stop() {
    if (is_running_) {
        is_running_ = false;
        timer_.wakeup_now();
    }
}

void AsyncWorker::worker_running() {
    tls_async_worker_ptr = this;
    LOG_INFO << "AsyncWorker worker_running, worker=" << this;

    int64_t wakeup_timestamp_us = 0;
    while (is_running_) {
        // do polling
        wakeup_timestamp_us = poller_.poll();
        // do all pendding async message
        run_message();

        // check this loop cost time
        wakeup_timestamp_us = base::gettimeofday_us() - wakeup_timestamp_us;
        if (wakeup_timestamp_us >= 1000000) {
            LOG_WARN << "worker=" << this << ",worker hung for " << wakeup_timestamp_us << "us";
        }
    }
}

void AsyncWorker::run_message() {
    {
        std::unique_lock<std::mutex> lock_gaurd(mutex_);
        swaped_queue_.swap(queue_);
    }

    for (auto& message : swaped_queue_) {
        message.function_(message.object_);
    }
    swaped_queue_.clear();
}

void AsyncWorker::push_message(AsyncMessage::Func func, void* object) {
    AsyncMessage msg{func, object};
    {
        std::unique_lock<std::mutex> lock_gaurd(mutex_);
        queue_.push_back(msg);
    }
    timer_.wakeup_now();
}

} // end namespace rpc
} // end namespace p

