#include <stdio.h>
#include <unistd.h>

#include "p/rpc/async_worker.h"
#include "p/rpc/acceptor.h"
#include "p/rpc/stream_crypt.h"
#include <assert.h>
#include <endian.h>


// The following address types are defined:
// 0x01: host is a 4-byte IPv4 address.
// 0x03: host is a variable length string, starting with a 1-byte length, followed by up to 255-byte domain name.
// 0x04: host is a 16-byte IPv6 address.

constexpr int kAddressTypeIpv4 = 0x01;
constexpr int kAddressTypeString = 0x03;
constexpr int kAddressTypeIpv6= 0x04;

void dec_func(char * buf, int len, void* arg) {
    p::rpc::StreamDecrypt* dec = (p::rpc::StreamDecrypt*)arg;
    dec->decrypt(buf, len);
}

class ShadowSocket : public p::rpc::Socket {
public:
    int try_to_decrypt() {
        char buf[64];
        int len = buf_.copy_front(buf, 64);

        std::unique_ptr<p::rpc::StreamDecrypt> ptr;

        for (int i = 0; ; ++i) {
            ptr.reset(p::rpc::StreamDecrypt::get_one(i));
            if (!ptr) {
                return -1;
            }

            int used = ptr->init("3.e4e5926w", buf, len);
            if (used <= 0 || used + 8 <= len) {
                continue;
            }

            char* p = &buf[used];
            ptr->decrypt(p, 8);

            if (p[0] != 0x01  && p[0] !=0x03) {
                continue;
            }

            //TODO
            dec_.reset(ptr.release());
            buf_.popn(buf, used);
            buf_.map(dec_func, dec_.get());
            return 0;
        }

        return 0;
    }

    int connect_taget() {
        char ip_type;
        buf_.popn(&ip_type, 1);
        char buf[255];
        if (ip_type == 0x01) {
            // fuck
            assert(0);
        } else {
            char host_len;
            buf_.popn(&host_len, 1);
            buf_.popn(&buf, host_len);
        }

        uint16_t port;
        buf_.popn(&port, 2);
        port = be16toh(port);
    }

    virtual void on_msg_read(p::base::ZBuffer* zbuf) {
        if (zbuf == nullptr) {
            eof_ = true;
        }

        if (!enc_) {
            LOG_DEBUG << this << "on read len=" << zbuf->size();
            buf_.append(std::move(*zbuf));
            if (try_to_decrypt() == 0) {
                connect_taget();
            }
        } else {
            zbuf->map(dec_func, dec_.get());
            buf_.append(std::move(*zbuf));
        }

        LOG_DEBUG << this << " on read raw msg len=" << buf_.size();
        target_->send_msg(std::move(buf_), nullptr);
    }

    virtual void on_msg_sended(int err, void* arg) {
        LOG_DEBUG << this << " on writed len=" << " :" << arg;
        if (eof_) {
            shutdown();
        }
    }

    virtual void on_failed() {
        LOG_DEBUG << this << " on failed";
    }

private:
    bool eof_ = false;
    p::base::ZBuffer        buf_;

    std::unique_ptr<p::rpc::StreamEncrypt> enc_;
    std::unique_ptr<p::rpc::StreamDecrypt> dec_;

    ShadowSocket* target_ = nullptr;
};

