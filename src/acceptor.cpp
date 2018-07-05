// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#include "p/rpc/acceptor.h"
#include "p/rpc/async_worker.h"

namespace p {
namespace rpc {

Acceptor::Acceptor(AsyncWorker* worker) : worker_(worker) {
}

int Acceptor::listen(const base::EndPoint& ep) {
    local_side_ = ep;
    int ret = Listen(local_side_);

    LOG_INFO << this << " listen on " << ep << " :" << strerror(ret);

    if (ret) {
        return ret;
    }

    worker_->add_socket(this, false);

    return 0;
}

Socket* Acceptor::accept() {
    struct sockaddr new_addr;
    socklen_t addrlen = static_cast<socklen_t>(sizeof(new_addr));

    int new_fd = -1;
    new_fd = ::accept4(fd(), &new_addr, &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (new_fd < 0) {
        return nullptr;
    }

    Socket* ret = new_socket();
    ret->reset(new_fd);
    ret->set_local_side(local_side_);
    ret->set_remote_side(base::EndPoint((struct sockaddr_in*)&new_addr));

    add_socket(ret);

    return ret;
}

void Acceptor::on_msg_in() {
    while (accept()) {
        ;
    }
}

void Acceptor::add_socket(Socket* s) {
    worker_->add_socket(s, true);
    socket_map_.insert(s);
}

void Acceptor::del_socket(Socket* s) {
    worker_->del_socket(s);
    socket_map_.erase(s);
}

} // end namespace rpc
} // end namespace p

