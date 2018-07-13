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
const char* g_passwd = "lasdjf";

void dec_func(char * buf, int len, void* arg) {
    p::rpc::StreamDecrypt* dec = (p::rpc::StreamDecrypt*)arg;
    dec->decrypt(buf, len);
}

void enc_func(char * buf, int len, void* arg) {
    p::rpc::StreamEncrypt* enc = (p::rpc::StreamEncrypt*)arg;
    enc->encrypt(buf, len);
}

class ShadowSocket;

class ClientSocket : public p::rpc::Socket {
public:
    ClientSocket(ShadowSocket* s) : ss_(s) {
        LOG_DEBUG  << this << " ClientSocket New";
    }

    ~ClientSocket() override {
        LOG_DEBUG  << this << " ClientSocket Free";
    }

    void on_msg_read(p::base::ZBuffer* zbuf) override;

    void on_connected() override;

    void on_msg_sended(int err, void* arg) override {
        LOG_DEBUG << this << " ClientSocket on on_msg_sended err=" << err;
    }

    void on_failed() override ;

    int             eof_;
    ShadowSocket*   ss_;
};

class ShadowSocket : public p::rpc::Socket {
public:
    ShadowSocket() {
        LOG_DEBUG  << this << " ShadowSocket New";
    }

    ~ShadowSocket() {
        LOG_DEBUG  << this << " ShadowSocket Free";
    }
    int try_to_decrypt() {
        std::unique_ptr<p::rpc::StreamDecrypt> ptr;
        for (int i = 0; ; ++i) {
            char buf[64];
            int len = buf_.copy_front(buf, 64);

            ptr.reset(p::rpc::StreamDecrypt::get_one(i));
            if (!ptr) {
                return -1;
            }

            int used = ptr->init(g_passwd, buf, len);
            if (used <= 0 || used + 8 >= len) {
                continue;
            }

            char* p = &buf[used];
            ptr->decrypt(p, 8);

            if (p[0] != 0x01  && p[0] !=0x03) {
                continue;
            }

            LOG_DEBUG << this << " ShadowSocket found decrypt";
            //TODO
            dec_.reset(ptr->NewStreamDecrypt());
            used = dec_->init(g_passwd, buf, len);
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
            buf_.popn(buf, host_len);
            buf[(int)host_len] = 0;
        }

        uint16_t port;
        buf_.popn((char*)&port, 2);
        port = be16toh(port);

        p::base::EndPoint addr(buf, port);
        LOG_DEBUG << "get address:" << buf << ",port=" << port << ",ep=" << addr;

        target_ = new ClientSocket(this);
        int ret = target_->try_connect(addr);
        if (ret < 0) {
            delete target_;
            target_ = nullptr;
            return -1;
        }

        if (ret > 0) {
            delete target_;
            target_ = nullptr;
            LOG_DEBUG << "retry later ";
            return 0;
        }

        LOG_DEBUG << "doing connecting";


        return 0;
    }

    virtual void on_msg_read(p::base::ZBuffer* zbuf) {
        if (zbuf == nullptr) {
            Close();
            return ;
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
        //target_->send_msg(std::move(buf_), nullptr);
    }

    virtual void on_msg_sended(int err, void* arg) {
        LOG_DEBUG << this << " on writed len=" << " :" << arg;
    }

    virtual void on_failed() {
        LOG_DEBUG << this << " on failed";
        if (target_) {
            target_->set_failed(error());
            target_ = nullptr;
        }
    }

    void on_target_connected() {
        target_->send_msg(std::move(buf_), this);
        enc_.reset(dec_->NewStreamEncrypt());
        p::base::ZBuffer iv;
        iv.append(enc_->get_ivbuf(), enc_->get_ivlen());
        // TODO
        send_msg(std::move(iv), this);
    }

    void send_reply(p::base::ZBuffer* buf) {
        buf->map(enc_func, enc_.get());
        send_msg(std::move(*buf), this);
    }

private:
    p::base::ZBuffer        buf_;

    std::unique_ptr<p::rpc::StreamEncrypt> enc_;
    std::unique_ptr<p::rpc::StreamDecrypt> dec_;

    Socket* target_ = nullptr;
};

void ClientSocket::on_connected() {
    LOG_DEBUG << this << " ClientSocket on on_connected";
    ss_->on_target_connected();
}

void ClientSocket::on_msg_read(p::base::ZBuffer* zbuf) {
    LOG_DEBUG << this << " ClientSocket on on_msg_read";
    if (zbuf == nullptr) {
        Close();
        return ;
    }

    LOG_DEBUG << this << "ClientSocket on read len=" << zbuf->size();
    ss_->send_reply(zbuf);
}

void ClientSocket::on_failed() {
    LOG_DEBUG << this << " ClientSocket on failed";
    ss_->set_failed(error());
}
