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

    virtual ~Socket() {
        LOG_DEBUG << this << " Socket Free";
    }

    void release() {
        if (1 == ref_.fetch_sub(1)) { //, std::memory_order_release);
            delete this;
        }
    }

    int Close();

    base::EndPoint remote_side() const { return remote_side_; }

    base::EndPoint local_side() const { return local_side_; }

    int try_connect(base::EndPoint perr);

    virtual void on_msg_read(base::ZBuffer* ref) = 0;

    virtual void on_msg_sended(int err, void* arg) = 0;

    virtual void on_connected() {}

    virtual void on_failed() = 0;

    int set_failed(int err);

    int error() {
        return errno_;
    }

    int send_msg(base::ZBuffer&& zbuf, void* arg);

    void stop_read() { status_ = kStopReading; }

    void try_read() {
        if (status_ == kStopReading) {
            status_ = kConnected;
            on_msg_in();
        }
    }

    int shutdown();

    virtual void on_msg_in();

    void on_msg_out();

    struct SendCtx {
        uint32_t offset;
        uint32_t length;
        void*    arg;
    };

    friend class SocketPtr;

    void set_local_side(base::EndPoint peer) { local_side_ = peer; }

    void set_remote_side(base::EndPoint peer) { remote_side_ = peer; }

    static constexpr int kConnected     = 0;
    static constexpr int kStopReading   = 1;
    static constexpr int kReadedEOF     = 2;
    static constexpr int kDisconnecting = 3;
    static constexpr int kConnecting    = 4;
    static constexpr int kShutdown      = 5;

    void set_status(int s) { status_ = s; }
    void set_owner(void* o) { owner_ = o;}

    static void release_socket(void* s) {
        Socket* sock = (Socket*)s;
        sock->release();
    }

protected:
    base::EndPoint remote_side_;
    base::EndPoint local_side_;

    int errno_  = 0;
    int status_ = kShutdown;

    std::atomic<int64_t>                sending_lock_ = {0};
    std::deque<base::ZBuffer::BlockRef> sending_queue_;

    std::atomic<int64_t> ref_ = {1};

    void*               owner_ = nullptr;
};

class SocketPtr {
public:
    SocketPtr(Socket* s) : socket_(s) {
        socket_->ref_.fetch_add(1); //, std::memory_order_release);
    }

    SocketPtr(SocketPtr&& ptr) {
        Socket* tmp = socket_;
        socket_     = ptr.socket_;
        ptr.socket_ = tmp;
    }

    SocketPtr(const SocketPtr& ptr) {
        if (socket_) {
            if (1 == socket_->ref_.fetch_sub(1)) { //, std::memory_order_release);
                delete socket_;
            }
        }
        socket_ = ptr.socket_;
        socket_->ref_.fetch_add(1); //, std::memory_order_release);
    }

    ~SocketPtr() {
        if (1 == socket_->ref_.fetch_sub(1)) { //, std::memory_order_release);
            delete socket_;
        }
    }

private:
    Socket* socket_;
};

} // end namespace rpc
} // end namespace p

