// Copyright (c) 2018, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#include "p/rpc/stream_crypt.h"

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include "cryptopp/aes.h"
#include "cryptopp/blowfish.h"
#include "cryptopp/cast.h"
#include "cryptopp/chacha.h"
#include "cryptopp/des.h"
#include "cryptopp/hex.h"
#include "cryptopp/md5.h"
#include "cryptopp/salsa.h"
#include "cryptopp/cryptlib.h"
#include "cryptopp/filters.h"
#include "cryptopp/modes.h"
#include "cryptopp/osrng.h"

using CryptoPP::AutoSeededRandomPool;
using CryptoPP::Exception;
using CryptoPP::StringSink;
using CryptoPP::StringSource;
using CryptoPP::ArraySink;
using CryptoPP::ArraySource;
using CryptoPP::StreamTransformationFilter;
using CryptoPP::AES;
using CryptoPP::DES;
using CryptoPP::Blowfish;
using CryptoPP::CAST;
using CryptoPP::ChaCha20;
using CryptoPP::Salsa20;
using CryptoPP::CFB_Mode;
using CryptoPP::byte;

namespace p {
namespace rpc {

inline void md5sum(const char* in, int inlen, char* buf) {
    CryptoPP::Weak::MD5 hash;
    hash.CalculateDigest((byte*)buf, (const byte*)in, inlen);
}

// make sure keyLen is multi of 16
void BuildKeyFromPassword(const std::string& password, char *key, int keyLen) {
    constexpr int kMD5Len = 16;

    int buf_len = password.size() + kMD5Len;
    std::string tmp_buf(buf_len, '\0');
    char* buf = &tmp_buf[0];
    ::memcpy(buf + kMD5Len, (const void*)password.data(), password.size());

    md5sum(buf + kMD5Len, password.size(), key);
    int len = kMD5Len;

    while (len + kMD5Len <= keyLen) {
        ::memcpy(buf, key + len - kMD5Len, kMD5Len);
        md5sum(buf, buf_len, key + len);
        len += kMD5Len;
    }
}

template <typename T, const int keyLen, const int ivLen>
class Decrypter : public StreamDecrypt {
    int init(const std::string& passwd, char* ivbuf, int len) override {
        static_assert(keyLen <= 32 && (keyLen % 16 == 0), "error key length");

        if (len <= ivLen) {
            return -1;
        }

        BuildKeyFromPassword(passwd, key_, keyLen);
        dec_.SetKeyWithIV((const byte*)key_, keyLen, (const byte*)ivbuf, ivLen);

        return ivLen;
    }



    void decrypt(char *buf, size_t len) override {
        CryptoPP::ArraySource((const byte*)buf, len, true,
                new StreamTransformationFilter(dec_, new ArraySink((byte *)buf, len)));
    }

private:
    char     key_[32];
    typename T::Decryption dec_;
};

typedef Decrypter<CFB_Mode<AES>, 16, 16> DecrypterAES_16_16;
typedef Decrypter<CFB_Mode<AES>, 32, 16> DecrypterAES_32_16;
typedef Decrypter<ChaCha20, 32, 8> DecrypterChaCha_32_8;
typedef Decrypter<Salsa20, 32, 8> DecrypterSalsa20_32_8;


StreamDecrypt* StreamDecrypt::get_one(int index) {
    switch (index) {
    case 0:
        return new DecrypterAES_16_16();
        break;
    case 1:
        return new DecrypterAES_32_16();
        break;
    case 2:
        return new DecrypterChaCha_32_8();
        break;
    case 3:
        return new DecrypterSalsa20_32_8();
        break;
    }
    return nullptr;
}



} // end namespace rpc
} // end namespace p