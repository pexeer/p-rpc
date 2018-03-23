// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

#include <unordered_set>

#include "p/rpc/socket.h"       // Socket

namespace p {
namespace rpc {

class AsyncWorker;

class Acceptor : public Socket {
public:
    typedef Socket* (*SocketGeneratorFunc)(void);

public:
    Acceptor(AsyncWorker* worker);

    int listen(const base::EndPoint& ep) {
        local_side_ = ep;
        return fd_.Listen(local_side_);
    }

    Socket* accept();

    int stop();

    int join(bool wait_doing);

    void set_socket_generaotr(SocketGeneratorFunc f) {
        socket_generaotr_ = f;
    }

    void add_socket(Socket* s);

    void del_socket(Socket* s);

protected:
    AsyncWorker*                   worker_;
    SocketGeneratorFunc            socket_generaotr_;
    std::unordered_set<Socket*>    socket_map_;
    int32_t                        idle_timeout_sec_;
    Socket*                        new_socket_ = nullptr;
};

} // end namespace rpc
} // end namespace p

