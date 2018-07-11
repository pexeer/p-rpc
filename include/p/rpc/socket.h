// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

#include "p/base/macros.h"          // P_DISALLOW_COPY
#include "p/base/socket.h"          // SocketFd
#include "p/base/zbuffer.h"         // ZBuffer
#include <deque>

namespace p {
namespace rpc {

class SocketUser;

class SocketPtr;

class Socket : public base::SocketFd {
public:
    Socket() {}

    Socket(int fd) : SocketFd(fd) {}

    virtual ~Socket() {}

    int connect(base::EndPoint peer);

    base::EndPoint remote_side() const {
        return remote_side_;
    }

    base::EndPoint local_side() const {
        return local_side_;
    }

    virtual void on_msg_read(base::ZBuffer* ref) = 0;

    virtual void on_msg_sended(int err, void* arg) = 0;

    virtual void on_failed() = 0;

    int set_failed(int err);

    int send_msg(base::ZBuffer&& zbuf, void* arg);

    void stop_read() {
        reading_msg_ = 0;
    }

    void try_read() {
        reading_msg_  = 1;
        on_msg_in();
    }

    int shutdown();

    virtual void on_msg_in();

    void on_msg_out();

    struct SendCtx {
        uint32_t offset;
        uint32_t length;
        void     *arg;
    };

    friend class SocketPtr;

    void set_local_side(base::EndPoint peer) {
        local_side_ = peer;
    }

    void set_remote_side(base::EndPoint peer) {
        remote_side_ = peer;
    }

protected:
    base::EndPoint          remote_side_;
    base::EndPoint          local_side_;

    int                     errno_ = 0;
    int                     reading_msg_ = 1;

    std::atomic<int64_t>            sending_lock_ = {0};
    std::deque<base::ZBuffer::BlockRef>     sending_queue_;

    std::atomic<int64_t>    ref_ = {0};
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

