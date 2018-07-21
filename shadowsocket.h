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
constexpr int kMaxHeaderLen = 256 + 64;
const char* g_passwd = "3.e4e5926w";

void dec_func(char * buf, int len, void* arg) {
    p::rpc::StreamDecrypt* dec = (p::rpc::StreamDecrypt*)arg;
    dec->decrypt(buf, len);
}

void enc_func(char * buf, int len, void* arg) {
    p::rpc::StreamEncrypt* enc = (p::rpc::StreamEncrypt*)arg;
    enc->encrypt(buf, len);
}

class ShadowSocket;

class RemoteSocket : public p::rpc::Socket {
public:
    RemoteSocket(ShadowSocket* s) : ss_(s) {
        LOG_DEBUG  << this << " RemoteSocket New";
    }

    ~RemoteSocket() override {
        LOG_DEBUG  << this << " RemoteSocket Free";
    }

    void on_msg_read(p::base::ZBuffer* zbuf) override;

    void on_connected() override;

    void on_msg_sended(int err, void* arg) override {
        LOG_DEBUG << this << " RemoteSocket on on_msg_sended err=" << err;
    }

    void on_failed() override ;

    int             eof_;
    ShadowSocket*   ss_;
};

class ShadowSocket : public p::rpc::Socket {
public:
    ShadowSocket() {
        LOG_DEBUG  << this << " ShadowSocket New, rmt=" << remote_side()
            << ",lcs=" << local_side();
    }

    ~ShadowSocket() {
        LOG_DEBUG  << this << " ShadowSocket Free";
    }

    int try_to_decrypt() {
        std::unique_ptr<p::rpc::StreamDecrypt> ptr;
        for (int i = 0; ; ++i) {
            char buf[kMaxHeaderLen];
            int len = buf_.copy_front(buf, kMaxHeaderLen);

            ptr.reset(p::rpc::StreamDecrypt::get_one(i));
            if (!ptr) {
                // all method failed
                return -1;
            }

            int used = ptr->init(g_passwd, buf, len);
            if (used <= 0) {
                continue;
            }

            char* p = &buf[used];
            char* end = &buf[len];
            if (end <= p) {
                continue;
            }

            ptr->decrypt(p, end - p);

            char addr_type = p[0];
            ++p;


            if (kAddressTypeIpv6 == addr_type) {
                continue;
            }

            if (kAddressTypeIpv4 == addr_type) {
                ;
            } else if (kAddressTypeString == addr_type) {
                int16_t host_len = p[0];
                ++p;

                if (host_len <= 0 || host_len > 253 || p + host_len + 2 > end) {
                    continue;
                }
            } else {
                continue;
            }
            LOG_DEBUG << this << " ShadowSocket found decrypt, i=" << i;

            dec_.reset(ptr->NewStreamDecrypt());
            used = dec_->init(g_passwd, buf, len);
            buf_.popn(buf, used);
            buf_.map(dec_func, dec_.get());
            return 0;
        }

        return 0;
    }

    int connect_remote() {
        char ip_type;
        buf_.popn(&ip_type, 1);
        char buf[255];
        p::base::EndPoint addr;
        if (ip_type == 0x01) {
            in_addr_t ip;
            buf_.popn((char*)&ip, sizeof(in_addr_t));
            addr = p::base::EndPoint(ip, 0);
            buf[0] = 0;
        } else {
            char host_len;
            buf_.popn(&host_len, 1);
            buf_.popn(buf, host_len);
            buf[(int)host_len] = 0;
            addr = p::base::EndPoint(buf, 0);
        }

        uint16_t port;
        buf_.popn((char*)&port, 2);
        addr.set_port(be16toh(port));

        remote_ = new RemoteSocket(this);

        LOG_DEBUG << this << " ShadowSocket get address:" << buf << ",port="
            << port << ",ep=" << addr << "remote=" << remote_
            << ",rmt=" << remote_side() << ",lcs=" << local_side();

        int ret = remote_->try_connect(addr);
        if (ret < 0) {
            delete remote_;
            remote_ = nullptr;
            return -1;
        }

        if (ret > 0) {
            delete remote_;
            remote_ = nullptr;
            LOG_DEBUG << "retry later ";
            return 0;
        }

        LOG_DEBUG << "doing connecting";

        return 0;
    }

    virtual void on_msg_read(p::base::ZBuffer* zbuf) {
        if (zbuf == nullptr) {
            eof_ = true;
            set_failed(3456);
            return ;
        }

        if (xxx_) {
            xxx_ = false;
        LOG_DEBUG  << this << " ShadowSocket New, rmt=" << remote_side()
            << ",lcs=" << local_side();
        }

        LOG_DEBUG << this << " ShadowSocket on read len=" << zbuf->size();

        if (!enc_) {
            buf_.append(std::move(*zbuf));
            if (try_to_decrypt() == 0) {
                connect_remote();
            }

            return ;
        }


        if (!remote_->connected()) {
            zbuf->map(dec_func, dec_.get());
            buf_.append(std::move(*zbuf));
            return ;
        }

        zbuf->map(dec_func, dec_.get());
        remote_->send_msg(std::move(*zbuf), (void*)1234);
    }

    virtual void on_msg_sended(int err, void* arg) {
        LOG_DEBUG << this << " ShadowSocket on writed " << " : " << arg << ",err=" << err;
        if (arg == nullptr) {
            assert(0);
        }

        if (err) {
            on_failed();
            return ;
        }

        if (remote_ && remote_->connected()) {
            LOG_DEBUG << this << " ShadowSocket start read RemoteSocket";
            remote_->try_read();
        }
    }

    virtual void on_failed() {
        LOG_DEBUG << this << " on failed";
        if (remote_) {
            remote_->set_failed(error());
            remote_ = nullptr;
        }
    }

    void on_remote_connected() {
        remote_->send_msg(std::move(buf_), this);
        enc_.reset(dec_->NewStreamEncrypt());
        p::base::ZBuffer iv;
        iv.append(enc_->get_ivbuf(), enc_->get_ivlen());
        // TODO
        send_msg(std::move(iv), this);

        if (eof_) {
            //remote_->close();
        }
    }

    void send_reply(p::base::ZBuffer* buf) {
        buf->map(enc_func, enc_.get());
        LOG_DEBUG << this << " ShadowSocket send len=" << buf->size();
        send_msg(std::move(*buf), this);
    }

private:
    p::base::ZBuffer        buf_;

    std::unique_ptr<p::rpc::StreamEncrypt> enc_;
    std::unique_ptr<p::rpc::StreamDecrypt> dec_;

    Socket* remote_ = nullptr;
    bool    eof_ = false;
    bool    xxx_ = true;
};

void RemoteSocket::on_connected() {
    LOG_DEBUG << this << " RemoteSocket on on_connected" << ",rms=" << remote_side()
         << ",lcs" << local_side();
    ss_->on_remote_connected();
}

void RemoteSocket::on_msg_read(p::base::ZBuffer* zbuf) {
    if (zbuf == nullptr) {
        LOG_DEBUG << this << " RemoteSocket on on_msg_readn EOF";
        set_failed(8888);
        return ;
    }

    LOG_DEBUG << this << " RemoteSocket on on_msg_read len=" << zbuf->size();
    LOG_DEBUG << this << " RemoteSocket stop read";
    stop_read();
    ss_->send_reply(zbuf);
}

void RemoteSocket::on_failed() {
    LOG_DEBUG << this << " RemoteSocket on failed";
    //ss_->set_failed(error());
    ss_->set_failed(9999);
}

