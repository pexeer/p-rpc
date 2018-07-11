// Copyright (c) 2018, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

#include <unistd.h>
#include <string>

namespace p {
namespace rpc {

class StreamEncrypt {
public:
    virtual void encrypt(char *buf, size_t len) = 0;

    ~StreamEncrypt() = default;
};

class StreamDecrypt {
public:
    //virtual const char* name();
    ~StreamDecrypt() = default;

    static StreamDecrypt* get_one(int index);

    virtual int init(const std::string& passwd, char *ivbuf, int len) = 0;

    virtual void decrypt(char *buf, size_t len) = 0;
};


} // end namespace rpc
} // end namespace p
