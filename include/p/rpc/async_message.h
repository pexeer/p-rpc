// Copyright (c) 2017, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

namespace p {
namespace rpc {

struct AsyncMessage {
public:
    typedef void (*Func)(void*);
    Func        function_;
    void*       object_;
};

} // end namespace rpc
} // end namespace p

