// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#include "p/rpc/async_worker.h"
#include "p/rpc/socket.h"

#include "p/base/logging.h"
#include "p/base/time.h"

namespace p {
namespace rpc {

thread_local AsyncWorker* tls_async_worker_ptr = nullptr;

AsyncWorker* AsyncWorker::current_worker() {
    return tls_async_worker_ptr;
}

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
    this_thread_ = std::thread(&AsyncWorker::worker_running, this);
}

int AsyncWorker::join() {
    if (this_thread_.joinable()) {
        this_thread_.join();
        return 0;
    }
    return -1;
}

void AsyncWorker::stop_func() {
    is_running_ = false;
}

void AsyncWorker::stop() {
    push_message(&AsyncWorker::stop_func, this);
}

void AsyncWorker::worker_running() {
    tls_async_worker_ptr = this;
    LOG_INFO << "AsyncWorker worker_running, worker=" << this;

    int64_t wakeup_timestamp_us = 0;
    while (is_running_) {
        run_message();

        // do polling
        wakeup_timestamp_us = poll();

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
        message.function(message.object);
    }

    swaped_queue_.clear();
}

void AsyncWorker::push_message(AsyncMessage::Func func, void* object) {
    AsyncMessage msg{func, object};
    {
        std::unique_lock<std::mutex> lock_gaurd(mutex_);
        queue_.push_back(msg);
    }
}

void AsyncWorker::insert_message(AsyncMessage::Func func, void* object) {
    AsyncMessage msg{func, object};
    {
        std::unique_lock<std::mutex> lock_gaurd(mutex_);
        queue_.push_front(msg);
    }
}

void AsyncWorker::insert_connect(Socket* s) {
    s->set_owner(this);
    add_socket(s, true);
    socket_map_.insert(s);
}

void AsyncWorker::release_connect(Socket* s) {
    del_socket(s);
    socket_map_.erase(s);

    push_message(&Socket::release_socket, s);
}

#include <signal.h>

class IgnoreSignalPipe {
public:
    IgnoreSignalPipe() {
        struct sigaction act;
        act.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &act, NULL);
    }
} dummy_ignore_signal_pipe;

} // end namespace rpc
} // end namespace p

