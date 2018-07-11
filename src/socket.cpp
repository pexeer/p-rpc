// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.


#include "p/rpc/socket.h"          // Socket

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

int Socket::shutdown() {
    set_failed(1);
    int ret = Shutdown();
    if (ret) {
        LOG_DEBUG << this << " Socket shutdown failed err=" << strerror(ret);
    }

    return ret;
}

int Socket::set_failed(int err) {
    if (errno_) {
        return -1;
    }

    errno_ = err;

    for (auto& ref : sending_queue_) {
        if (ref.length) {
            ref.release();
        } else {
            on_msg_sended(errno_, (void*)ref.block);
        }
    }
    sending_queue_.clear();
    return 0;
}

void Socket::on_msg_in() {
    if (!reading_msg_) {
        return ;
    }

    while (true) {
        base::ZBuffer   zbuf;

        ssize_t ret = zbuf.read_from_fd(fd_, -1, 512 * 1024);

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
        on_msg_read(nullptr);
        break;
    }
}

void Socket::on_msg_out() {
    //int64_t lock = sending_lock_.fetch_add(1);
    //if (lock) {
        // sending data in other thread
     //   return ;
    //}

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
}


} // end namespace rpc
} // end namespace p

