// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.


#include "p/rpc/socket.h"          // Socket
#include "p/rpc/async_worker.h"     // AsyncWorker
#include "p/rpc/acceptor.h"     // AsyncWorker
#include <assert.h>

namespace p {
namespace rpc {

int Socket::send_msg(base::ZBuffer&& zbuf, void* arg) {
    if (errno_) {
        return -1;
    }

    zbuf.dump(&sending_queue_);

    sending_queue_.push_back(base::ZBuffer::BlockRef{0, 0, (base::ZBuffer::Block*)arg});

    on_msg_out();

    return 0;
}
int Socket::try_connect(base::EndPoint peer) {
    assert(fd_ < 0);
    int err = SocketFd::Connect(peer);
    LOG_DEBUG << this << " Socket try Connect " << peer << ", err=" << strerror(err);
    if (err) {
        reset(-1);
        status_ = kShutdown;
    } else {
        status_ = kConnecting;
        auto worker = p::rpc::AsyncWorker::current_worker();
        worker->insert_connect(this);
        set_owner(worker);
    }
    return err;
}

int Socket::shutdown() {
    LOG_DEBUG << this << " Socket shutdown";
    set_failed(ESHUTDOWN);
    return 0;
}

int Socket::Close() {
    status_ = kDisconnecting;
    on_msg_out();
    return 0;
}

int Socket::set_failed(int err) {
    LOG_DEBUG << this << " Socket set failed err=" << strerror(err);
    if (errno_) {
        return -1;
    }

    errno_ = err;
    status_ = kShutdown;

    for (auto& ref : sending_queue_) {
        if (ref.length) {
            ref.release();
        } else {
            on_msg_sended(errno_, (void*)ref.block);
        }
    }
    sending_queue_.clear();

    on_failed();

    LOG_DEBUG << this << " Socket try to release connect for err=" << strerror(errno_);
    if (owner_ == nullptr || owner_ == AsyncWorker::current_worker()) {
        AsyncWorker::current_worker()->release_connect(this);
    } else {
        Acceptor* ac = (Acceptor*)owner_;
        ac->release_connect(this);
    }

    int ret = SocketFd::Shutdown();
    if (ret) {
        LOG_DEBUG << this << " Socket shutdown failed err=" << strerror(ret);
    }

    return 0;
}

void Socket::on_msg_in() {
    if (status_ !=  kConnected) {
        return ;
    }

    while (true) {
        base::ZBuffer   zbuf;

        ssize_t ret = zbuf.read_from_fd(fd_, -1, 16 * 1024);

        if (ret > 0) {
            on_msg_read(&zbuf);
            continue;
        }

        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            set_failed(errno);
        }

        LOG_DEBUG << this << " Socket set EOF";
        status_ = kReadedEOF;
        on_msg_read(nullptr);
        break;
    }
}

void Socket::on_msg_out() {
    if (status_ == kConnecting) {
        int err = GetSocketErr();
        LOG_DEBUG << this << " Socket is connecting, get options, err=" << strerror(err);
        if (err) {
            set_failed(err);
            return ;
        }
        status_ = kConnected;
        LOG_DEBUG << this << " Socket on conneced";
        on_connected();
        return ;
    }

    if (status_ >= kConnecting) {
        return ;
    }

    while (!sending_queue_.empty()) {
        auto &ref = sending_queue_.front();
        if (ref.length == 0) {
            on_msg_sended(0, (void*)ref.block);
            sending_queue_.pop_front();
            continue;
        }

        ssize_t ret = Write(ref.begin(), ref.length);
        if (ret > 0) {
            if (ref.length == ret) {
                ref.release();
                sending_queue_.pop_front();
            } else {
                ref.length -= ret;
            }
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // AGAIN
            break;
        }
        set_failed(errno);
    }

    if (status_ == kDisconnecting) {
        set_failed(ESHUTDOWN);
    }
}

} // end namespace rpc
} // end namespace p

