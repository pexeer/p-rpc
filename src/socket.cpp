// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.


#include "p/rpc/socket.h"        // Socket
#include "p/rpc/async_worker.h"  // AsyncWorker
#include "p/rpc/acceptor.h"     // AsyncWorker
#include <assert.h>

namespace p {
namespace rpc {

int Socket::send_msg(base::ZBuffer&& zbuf, void* arg) {
    //LOG_DEBUG << this << " Socket send_msg len=" << zbuf.size();
    if (errno_) {
        return -1;
    }

    bool no_pendding_msg = sending_queue_.empty();

    zbuf.dump_refs_to(&sending_queue_);

    sending_queue_.push_back(base::ZBuffer::BlockRef{0, 0, (base::ZBuffer::Block*)arg});

    if (no_pendding_msg) {
        on_msg_out();
    }

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
    }
    return err;
}

int Socket::shutdown() {
    LOG_DEBUG << this << " Socket shutdown";
    set_failed(ESHUTDOWN);
    return 0;
}

int Socket::try_close() {
    status_ = kDisconnecting;
    on_msg_out();
    // TODO
    // deal with the case
    //      has many data to send
    //      but recive EOF
    // how to release this socket
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

    reset(-1);

    return 0;
}

void Socket::on_msg_in() {
    LOG_DEBUG << this << " Socket on_msg_in doing";

    while (status_ == kConnected) {
        base::ZBuffer   zbuf;

        ssize_t ret = zbuf.read_from_fd(fd_, -1, 32 * 1024);

        if (ret > 0) {
            doing_on_msg_in_ = 1;
            on_msg_read(&zbuf);
            doing_on_msg_in_ = 0;
            continue;
        }

        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            set_failed(errno);
            return ;
        }

        LOG_DEBUG << this << " Socket set EOF";
        status_ = kReadedEOF;
        doing_on_msg_in_ = 1;
        on_msg_read(nullptr);
        doing_on_msg_in_ = 0;

        set_failed(EHOSTUNREACH);
        return ;
    }
}

void Socket::on_msg_out() {
    LOG_DEBUG << this << " Socket on_msg_out doing";
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

    if (status_ == kShutdown) {
        return ;
    }

    while (!sending_queue_.empty()) {
        auto &ref = sending_queue_.front();
        if (ref.length == 0) {
            sending_queue_.pop_front();
            doing_on_msg_out_ = 1;
            on_msg_sended(0, (void*)ref.block);
            doing_on_msg_out_ = 0;
            continue;
        }

        ssize_t ret = Write(ref.begin(), ref.length);

        if (ret > 0) {
            writed_bytes_ += ret;
            if (ref.length == ret) {
                ref.release();
                sending_queue_.pop_front();
            } else {
                ref.offset += ret;
                ref.length -= ret;
            }
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // AGAIN
            return ;
        }
        set_failed(errno);
        return ;
    }

    if (status_ == kDisconnecting) {
        set_failed(ESHUTDOWN);
    }
}

} // end namespace rpc
} // end namespace p

