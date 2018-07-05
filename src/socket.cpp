// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.


#include "p/rpc/socket.h"          // Socket

namespace p {
namespace rpc {

const size_t kMaxReadingBufferSize = 1024 * 1024;

int Socket::send_msg(const char* buf, size_t len, void* arg) {
    if (errno_) {
        return -1;
    }

    SendCtx ctx = {buf, len, arg};
    sending_queue_.push_back(ctx);

    if (sending_queue_.size() == 1) {
        sending_buf_ = ctx.buf;
        sending_left_ = ctx.len;
        on_msg_out();
    }

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
    sending_buf_ = nullptr;
    sending_left_ = 0;

    for (auto& ctx : sending_queue_) {
        on_send_failed(ctx.buf, ctx.len, ctx.arg);
    }
    sending_queue_.clear();
    return 0;
}

void Socket::on_msg_in() {
    if (!reading_msg_) {
        return ;
    }

    char buf[kMaxReadingBufferSize];

    while (true) {
        ssize_t ret = Read(buf, sizeof(buf));

        if (ret > 0) {
            on_msg_read(buf, ret);
            continue;
        }

        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            set_failed(errno);
        }

        // ret == 0, set EOF
        //try_close();
        LOG_DEBUG << this << " Socket set EOF";
        on_msg_read(nullptr, 0);
        break;
    }
}

void Socket::on_msg_out() {
    //int64_t lock = sending_lock_.fetch_add(1);
    //if (lock) {
        // sending data in other thread
     //   return ;
    //}

    while (sending_buf_) {
        ssize_t ret = Write(sending_buf_, sending_left_);
        if (ret > 0) {
            sending_buf_ += ret;
            sending_left_ -= ret;
            if (sending_left_ == 0) {
                sending_buf_ = nullptr;
                SendCtx ctx = sending_queue_.front();
                sending_queue_.pop_front();
                on_msg_sended(ctx.buf, ctx.len, ctx.arg);
                if (!sending_queue_.empty()) {
                    ctx = sending_queue_.front();
                    sending_buf_ = ctx.buf;
                    sending_left_ = ctx.len;
                }
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

