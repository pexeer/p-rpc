// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

#include <unordered_set>

#include "p/rpc/socket.h"           // Socket
#include "p/rpc/async_worker.h"     // AsyncWorker

namespace p {
namespace rpc {

class AsyncWorker;

class Acceptor : public Socket {
public:
    Acceptor(AsyncWorker* worker);

    ~Acceptor() {}

    int listen(const base::EndPoint& ep);

    virtual Socket* new_socket() = 0;

    Socket* accept();

private:
    int stop();

    int join(bool wait_doing);

    void add_socket(Socket* s);

    void del_socket(Socket* s);

    virtual void on_msg_in();

    virtual void on_msg_read(base::ZBuffer* ref) {}

    virtual void on_msg_sended(int err, void* arg) {}

    virtual void on_failed() {}

    friend class AsyncWorker;

    AsyncWorker*                   worker_;

    std::unordered_set<Socket*>    socket_map_;
    int32_t                        idle_timeout_sec_;
};

} // end namespace rpc
} // end namespace p

