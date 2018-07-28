// Copyright (c) 2018, pexeer@gmail.com All rights reserved.
// Licensed under a BSD-style license that can be found in the LICENSE file.

#include "stream_crypt.h"

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

namespace p {
namespace rpc {
namespace ss {

inline void md5sum(const char* in, int inlen, char* buf) {
    CryptoPP::Weak::MD5 hash;
    hash.CalculateDigest((CryptoPP::byte*)buf, (const CryptoPP::byte*)in, inlen);
}

// make sure keyLen is multi of 16
void BuildKeyFromPassword(const std::string& password, char* key, int keyLen) {
    constexpr int kMD5Len = 16;

    int         buf_len = password.size() + kMD5Len;
    std::string tmp_buf(buf_len, '\0');
    char*       buf = &tmp_buf[0];
    ::memcpy(buf + kMD5Len, (const void*)password.data(), password.size());

    md5sum(buf + kMD5Len, password.size(), key);
    int len = kMD5Len;

    while (len + kMD5Len <= keyLen) {
        ::memcpy(buf, key + len - kMD5Len, kMD5Len);
        md5sum(buf, buf_len, key + len);
        len += kMD5Len;
    }
}

template <typename T, const int keyLen, const int ivLen> class Encrypter : public StreamEncrypt {
public:
    Encrypter(CryptoPP::byte* key) {
        ::memcpy(key_, key, keyLen);

        CryptoPP::AutoSeededRandomPool prng;
        prng.GenerateBlock(ivbuf_, ivLen);

        enc_.SetKeyWithIV(key_, keyLen, ivbuf_, ivLen);
    }

    const char* get_ivbuf() override { return (const char*)ivbuf_; }

    int get_ivlen() override { return ivLen; }

    void encrypt(char* buf, size_t len) override {
        CryptoPP::ArraySource((const CryptoPP::byte*)buf, len, true,
                              new CryptoPP::StreamTransformationFilter(
                                  enc_, new CryptoPP::ArraySink((CryptoPP::byte*)buf, len)));
    }

private:
    CryptoPP::byte         key_[32];
    CryptoPP::byte         ivbuf_[32];
    typename T::Encryption enc_;
};

template <typename T, const int keyLen, const int ivLen> class Decrypter : public StreamDecrypt {
public:
    Decrypter() {}

    Decrypter(CryptoPP::byte* key) { ::memcpy(key_, key, keyLen); }

    int init_key(const std::string& passwd) override {
        BuildKeyFromPassword(passwd, (char*)key_, keyLen);
        return 0;
    }


    int init_iv(char* ivbuf, int len) override {
        static_assert(keyLen <= 32 && (keyLen % 16 == 0), "error key length");

        if (len <= ivLen) {
            return -1;
        }

        dec_.SetKeyWithIV(key_, keyLen, (const CryptoPP::byte*)ivbuf, ivLen);

        return ivLen;
    }

    void decrypt(char* buf, size_t len) override {
        CryptoPP::ArraySource((const CryptoPP::byte*)buf, len, true,
                              new CryptoPP::StreamTransformationFilter(
                                  dec_, new CryptoPP::ArraySink((CryptoPP::byte*)buf, len)));
    }

    virtual StreamDecrypt* NewStreamDecrypt() override {
        return new Decrypter<T, keyLen, ivLen>(&key_[0]);
    }

    virtual StreamEncrypt* NewStreamEncrypt() override {
        return new Encrypter<T, keyLen, ivLen>(&key_[0]);
    }

private:
    CryptoPP::byte         key_[32];
    typename T::Decryption dec_;
};

typedef Decrypter<CryptoPP::CFB_Mode<CryptoPP::AES>, 16, 16> DecrypterAES_16_16;
typedef Decrypter<CryptoPP::CFB_Mode<CryptoPP::AES>, 32, 16> DecrypterAES_32_16;
typedef Decrypter<CryptoPP::ChaCha20, 32, 8>                 DecrypterChaCha_32_8;
typedef Decrypter<CryptoPP::Salsa20, 32, 8>                  DecrypterSalsa20_32_8;

StreamDecrypt* StreamDecrypt::get_one(int index) {
    switch (index) {
    case 0:
        return new DecrypterAES_32_16();
        break;
    case 1:
        return new DecrypterAES_16_16();
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

} // end namespace ss
} // end namespace rpc
} // end namespace p
