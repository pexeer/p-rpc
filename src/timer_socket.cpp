// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#include "p/rpc/timer_socket.h"
#include <sys/timerfd.h>        // timerfd_xxxx

namespace p {
namespace rpc {

static void on_timer_message(void* arg) {
    TimerSocket* ts = (TimerSocket*)arg;

    uint64_t count[1024];
    ssize_t ret = ts->read(&count, sizeof(count));

    LOG_DEBUG<< "timer_=" << ts << ",wakeup and read ret=" << ret;
    ts->set_triggered(ret);
}

TimerSocket::TimerSocket() {
    fd_.reset(::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC));
    if (fd() < 0) {
        LOG_FATAL << "create timerfd failed for " << strerror(errno);
    }

    LOG_INFO << "create timerfd for timer=" << this;

    on_read_msg_ = &on_timer_message;
    on_write_msg_ = &on_timer_message;
    is_triggered_ = 0;
}

int TimerSocket::wakeup_after(int64_t interval_ns) {
    //LOG_DEBUG << "timer=" << this << ",wakeup_after " << interval_ns;
    itimerspec new_value = {
        {0,0}, // it_interval
        {interval_ns / 1000000000LL, interval_ns % 1000000000LL}};// it_value
    int ret = ::timerfd_settime(fd(), 0, &new_value, nullptr);
    if (ret < 0) {
        LOG_FATAL << "timer_=" << this << ",wakeup_after failed " << strerror(errno);
    }
    return ret;
}

void TimerSocket::run_timer() {
    if (!is_triggered_) {
        return ;    // not trigglered by timer
    }

    int64_t interval_ns = 1;

    itimerspec new_value = {
        {0,0}, // it_interval
        {interval_ns / 1000000000LL, interval_ns % 1000000000LL}};// it_value

    int ret = ::timerfd_settime(fd(), 0, &new_value, nullptr);

    if (ret < 0) {
        LOG_FATAL << "timer_=" << this << ",wakeup_after failed " << strerror(errno);
    }

    is_triggered_ = 0;
}

} // end namespace rpc
} // end namespace p

