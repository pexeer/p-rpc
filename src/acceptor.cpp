// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#include "p/rpc/acceptor.h"
#include "p/rpc/async_worker.h"

namespace p {
namespace rpc {

static void on_read_msg(void* arg) {
    Acceptor* acceptor  = (Acceptor*)arg;

    while (acceptor->accept()) {
        ;
    }
}

Acceptor::Acceptor(AsyncWorker* worker) : worker_(worker) {
    on_read_msg_ = &on_read_msg;
    on_write_msg_ = nullptr;
}

Socket* Acceptor::accept() {
    struct sockaddr new_addr;
    socklen_t addrlen = static_cast<socklen_t>(sizeof(new_addr));

    int new_fd = -1;
    new_fd = ::accept4(fd(), &new_addr, &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (new_fd < 0) {
        if (errno != EAGAIN) {
            // Delay
            worker_->push_message(&on_read_msg, this);
        }
        return nullptr;
    }

    new_socket_->fd_.reset(new_fd);
    new_socket_->local_side_ = local_side_;
    new_socket_->remote_side_ = base::EndPoint((struct sockaddr_in*)&new_addr);

    add_socket(new_socket_);

    Socket* ret = new_socket_;
    new_socket_ = socket_generaotr_();
    return ret;
}

void Acceptor::add_socket(Socket* s) {
    socket_map_.insert(s);
}

void Acceptor::del_socket(Socket* s) {
    socket_map_.erase(s);
}

} // end namespace rpc
} // end namespace p

