// Copyright (c) 2018, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#pragma once

#include <unistd.h>
#include <string>

namespace p {
namespace rpc {
namespace ss {

class StreamEncrypt {
public:
    virtual const char* get_ivbuf() = 0;

    virtual int get_ivlen() = 0;

    virtual void encrypt(char *buf, size_t len) = 0;

    virtual ~StreamEncrypt() = default;
};

class StreamDecrypt {
public:
    virtual ~StreamDecrypt() = default;

    static StreamDecrypt* get_one(int index);

    virtual int init_key(const std::string& passwd) = 0;

    virtual int init_iv(char *ivbuf, int len) = 0;

    virtual void decrypt(char *buf, size_t len) = 0;

    virtual StreamDecrypt* NewStreamDecrypt() = 0;

    virtual StreamEncrypt* NewStreamEncrypt() = 0;
};


} // end namespace ss
} // end namespace rpc
} // end namespace p
