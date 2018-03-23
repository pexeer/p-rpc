// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

#include "p/base/macros.h"          // P_DISALLOW_COPY
#include "p/base/socket.h"          // SocketFd
#include "p/rpc/async_message.h"    // AsyncMessage

namespace p {
namespace rpc {

class SocketUser;

class SocketPtr;

class Socket {
public:
    Socket() {}

    virtual ~Socket() {}

    base::EndPoint remote_side() const {
        return remote_side_;
    }

    base::EndPoint local_side() const {
        return local_side_;
    }

    ssize_t write(const void *buf, size_t count) { return fd_.Write(buf, count); }

    ssize_t read(void *buf, size_t count) { return fd_.Read(buf, count); }

    int fd() const {
        return fd_.fd();
    }

public:
    friend class Poller;
    friend class Acceptor;
    friend class SocketPtr;

protected:
    p::base::SocketFd       fd_;
    int                     errno_ = 0;
    AsyncMessage::Func      on_write_msg_;
    AsyncMessage::Func      on_read_msg_;
    base::EndPoint          remote_side_;
    base::EndPoint          local_side_;

    SocketUser*             user_  = nullptr;

    std::atomic<int64_t>    ref_ = {0};

private:
    P_DISALLOW_COPY(Socket);
};

class SocketPtr {
public:
    SocketPtr(Socket* s) : socket_(s) {
        socket_->ref_.fetch_add(1);//, std::memory_order_release);
    }

    SocketPtr(SocketPtr&& ptr) {
        Socket* tmp = socket_;
        socket_ = ptr.socket_;
        ptr.socket_ = tmp;
    }

    SocketPtr(const SocketPtr& ptr) {
        if (socket_) {
            if (1 == socket_->ref_.fetch_sub(1)) {//, std::memory_order_release);
                delete socket_;
            }
        }
        socket_ = ptr.socket_;
        socket_->ref_.fetch_add(1);//, std::memory_order_release);
    }

    ~SocketPtr() {
        if (1 == socket_->ref_.fetch_sub(1)) {//, std::memory_order_release);
            delete socket_;
        }
    }

private:
    Socket* socket_;
};

} // end namespace rpc
} // end namespace p

