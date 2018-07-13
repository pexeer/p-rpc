// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#include "p/rpc/poller.h"

#include <unistd.h>
#include <errno.h>              // errno
#include <sys/epoll.h>          // epoll_create

#include "p/base/logging.h"
#include "p/base/time.h"
#include "p/rpc/socket.h"

namespace p {
namespace rpc {

Poller::Poller() {
    poll_fd_ = ::epoll_create(/*ignore*/ 102400);
    if (poll_fd_ < 0) {
        LOG_FATAL << this << " Poller create epoll failed for " << strerror(errno);
    }
}

Poller::~Poller() {
    if (poll_fd_ >= 0) {
        ::close(poll_fd_);
    }
}

int64_t Poller::poll() {
    const int kMaxEventNum = 2048;
    struct epoll_event evts[kMaxEventNum];

    LOG_INFO << this << " Poller epoll_wait begin";
    const int n = ::epoll_wait(poll_fd_, evts, kMaxEventNum, -1);
    int64_t wakeup_timestamp_us = base::gettimeofday_us();
    LOG_INFO << this << " Poller epoll_wait return " << n;

    for (int i = 0; i< n; ++i) {
        Socket* s = (Socket*)(evts[i].data.u64);

        if (evts[i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
            LOG_DEBUG << this << " Poller do on_msg_out Socket=" << s;
            s->on_msg_out();
        }
        if (evts[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
            LOG_DEBUG << this << " Poller do on_msg_in Socket=" << s;
            s->on_msg_in();
        }
    }

    return wakeup_timestamp_us;
}

int Poller::add_socket(Socket* s, bool out) {
    struct epoll_event evt;

    evt.data.u64 = (uint64_t)s;
    evt.events = EPOLLET | EPOLLIN;

    if (out) {
        evt.events |= EPOLLOUT;
    }

    LOG_DEBUG << this << " Poller add_socket Socket=" << s << ",out=" << out;

    return ::epoll_ctl(poll_fd_, EPOLL_CTL_ADD, s->fd(), &evt);
}

int Poller::mod_socket(Socket* s, bool add) {
    struct epoll_event evt;

    evt.data.u64 = (uint64_t)s;
    evt.events = EPOLLET | EPOLLIN;
    if (add) {
        evt.events |= EPOLLOUT;
    }

    LOG_DEBUG << this << " Poller mod_socket Socket=" << s << ",add=" << add;
    return ::epoll_ctl(poll_fd_, EPOLL_CTL_MOD, s->fd(), &evt);
}

int Poller::del_socket(Socket* s) {
    return ::epoll_ctl(poll_fd_, EPOLL_CTL_DEL, s->fd(), nullptr);
}

} // end namespace rpc
} // end namespace p

