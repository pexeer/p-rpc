// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

#include <stdint.h>

namespace p {
namespace rpc {

class Socket;

class Poller {
public:
  Poller();

  ~Poller();

  // return epoll_wait wakeup timestamp_us point
  int64_t poll();

  //int stop();

  int add_socket(Socket* s, bool out);

  int mod_socket(Socket* s, bool add);

  int del_socket(Socket* s);

protected:
  int       poll_fd_ = -1;
};

} // end namespace rpc
} // end namespace p

