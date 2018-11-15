#include <stdio.h>
#include <unistd.h>

#include "p/rpc/async_worker.h"
#include "p/rpc/acceptor.h"
#include "stream_crypt.h"
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
p::base::EndPoint g_ss_addr("pexeer.xyz", 8222);
const char* g_passwd = "3.e4e5926w";

void dec_func(char * buf, int len, void* arg) {
    p::rpc::ss::StreamDecrypt* dec = (p::rpc::ss::StreamDecrypt*)arg;
    dec->decrypt(buf, len);
}

void enc_func(char * buf, int len, void* arg) {
    p::rpc::ss::StreamEncrypt* enc = (p::rpc::ss::StreamEncrypt*)arg;
    enc->encrypt(buf, len);
}

class Socks5Proxy;
class RemoteSocket: public p::rpc::Socket {
public:
    RemoteSocket(Socks5Proxy* sp) : sp_(sp) {
        LOG_DEBUG  << this << " RemoteSocket New";
    }

    ~RemoteSocket() override {
        LOG_DEBUG  << this << " RemoteSocket Free";
    }

    void on_msg_sended(int err, void* arg) override {
        LOG_DEBUG  << this << " RemoteSocket on msg sended";
    }

    void on_msg_read(p::base::ZBuffer* zbuf) override;

    void on_connected() override;

    void on_failed() override ;

private:
    Socks5Proxy*    sp_;
};

class Socks5Proxy : public p::rpc::Socket {
public:
    Socks5Proxy() {
        LOG_DEBUG  << this << " Socks5Proxy New";
    }

    ~Socks5Proxy() override {
        LOG_DEBUG  << this << " Socks5Proxy Free";
    }

    void step0(p::base::ZBuffer* zbuf) {
        if (zbuf->size() < 2) {
            return ;
        }
        step_ = 1;

        char version = 0;
        char method_num = 0;
        zbuf->popn(&version, 1);
        zbuf->popn(&method_num, 1);

        LOG_DEBUG << this << " Socks5Proxy method_num=" << (int)method_num << ",left=" << zbuf->size();

        if ((int)version != 5 || (uint32_t)method_num != zbuf->size()) {
            set_failed(EPROTO);
            return ;
        }

        p::base::ZBuffer res;

        res.append(&version, 1);
        char method = 0;
        res.append(&method, 1);

        send_msg(std::move(res), this);

        step_ = 2;
        remote_ = new RemoteSocket(this);
        LOG_DEBUG << this << " Socks5Proxy begin to connect "  << g_ss_addr << ",rmt="
            << remote_side() << ",lcs=" << local_side();

        int ret = remote_->try_connect(g_ss_addr);
        if (ret < 0) {
            delete remote_;
            remote_ = nullptr;
            LOG_DEBUG << "connect failed";
            set_failed(EHOSTDOWN);
            return ;
        }

        if (ret > 0) {
            delete remote_;
            remote_ = nullptr;
            LOG_DEBUG << "retry later ";
            set_failed(EHOSTDOWN);
            return ;
        }

        LOG_DEBUG << "doing connecting";
        stop_read();
    }

    void step3(p::base::ZBuffer* zbuf) {
        step_ = 4;
        stop_read();

        if (zbuf->size() < 7) {
            return ;
        }

        char VER = 0;
        char CMD = 0;
        char RSV = 0;
        char ATYP = 0xFF;
        unsigned short PORT = 0;

        //ATYP   address type of following address
        // IP V4 address: X'01'
        // DOMAINNAME: X'03'
        // IP V6 address: X'04'
        zbuf->popn(&VER, 1);
        zbuf->popn(&CMD, 1);
        zbuf->popn(&RSV, 1);
        if (VER != 5 || RSV != 0) {
            set_failed(EPROTO);
            return ;
        }

        char addr_buf[1024];

        int addr_len = zbuf->copy_front(addr_buf, sizeof(addr_buf));

        zbuf->popn(&ATYP, 1);
        if (ATYP != 0x1 && ATYP != 0x3) {
            set_failed(EPROTO);
            return ;
        }

        p::base::EndPoint addr;
        char buf[255];
        if (ATYP == 0x1) {
            if (zbuf->size() < 6) {
                set_failed(EPROTO);
                return ;
            }
            in_addr_t ip;
            zbuf->popn((char*)&ip, sizeof(in_addr_t));
            addr = p::base::EndPoint(ip, 0);
            buf[0] = 0;
        } else {
            char host_len;
            zbuf->popn(&host_len, 1);
            if (zbuf->size() < ((uint32_t)host_len + 2)) {
                set_failed(EPROTO);
                return ;
            }
            zbuf->popn(buf, host_len);
            buf[(int)host_len] = 0;
            addr = p::base::EndPoint(buf, 0);
        }

        zbuf->popn((char*)&PORT, 2);
        PORT = be16toh(PORT);
        addr.set_port(PORT);

        LOG_DEBUG << this << " Socks5Proxy Get Reuquest VER=" << (int)VER
            << ",CMD=" << (int)CMD << ",RSV=" << (int)RSV << ",ATYP=" << (int)ATYP
            << ",left=" << zbuf->size();
        p::base::ZBuffer header;
        header.append(addr_buf, addr_len);
        header.map(enc_func, enc_.get());

        // send remote connect
        assert(remote_->connected());
        remote_->send_msg(std::move(header), this);

        p::base::ZBuffer reply;
        char ans[3];
        ans[0] = 0x5;
        ans[1] = 0x0;
        ans[2] = 0x0;
        reply.append(ans, 3);
        reply.append(addr_buf, addr_len);

        send_msg(std::move(reply), this);

        return ;
    }

    void on_msg_read(p::base::ZBuffer* zbuf) override {
        if (!zbuf) {
            LOG_DEBUG << this << " Socks5Proxy read EOF";
            return ;
        }

        LOG_DEBUG << this << " Socks5Proxy read len=" << zbuf->size() << ",step=" << step_;

        if (step_ == 0) {
            step0(zbuf);
            return ;
        }

        if (step_ == 1 || step_ == 2 || step_ == 4) {
            set_failed(EPROTO);
            return ;
        }

        if (step_ == 3) {
            step3(zbuf);
            return ;
        }

        if (step_ >= 5) {
            zbuf->map(enc_func, enc_.get());
            // send remote connect
            assert(remote_->connected());
            remote_->send_msg(std::move(*zbuf), this);
            return ;
        }
    }

    void on_msg_sended(int err, void* arg) override {
        LOG_DEBUG  << this << " Socks5Proxy on msg sended arg=" << arg << ",step=" << step_;
        if (step_ == 4) {
            step_ = 5;
            try_read();
        }
    }

    void on_failed() override  {
        if (remote_) {
            remote_->set_failed(EHOSTDOWN);
            remote_ = nullptr;
        }
    }

    void step2() {
        if (step_ != 2) {
            set_failed(EHOSTDOWN);
            return ;
        }
        step_ = 3;

        dec_.reset(p::rpc::ss::StreamDecrypt::get_one(0));
        dec_->init_key(g_passwd);
        enc_.reset(dec_->NewStreamEncrypt());

        p::base::ZBuffer iv;
        iv.append(enc_->get_ivbuf(), enc_->get_ivlen());
        remote_->send_msg(std::move(iv), this);

        try_read();
    }

    void got_response(p::base::ZBuffer&& zbuf) {
        if (!dec_init_) {
            char buf[64];
            int len = zbuf.copy_front(buf, sizeof(buf));
            int iv_used = dec_->init_iv(buf, len);
            if (iv_used < 0 || iv_used > 64) {
                set_failed(EPROTO);
                return ;
            }
            zbuf.popn(buf, iv_used);
        }

        zbuf.map(dec_func, dec_.get());
        send_msg(std::move(zbuf), this);
    }

private:
    int step_ = 0;
    bool dec_init_ = false;
    std::unique_ptr<p::rpc::ss::StreamEncrypt> enc_;
    std::unique_ptr<p::rpc::ss::StreamDecrypt> dec_;

    Socket* remote_ = nullptr;
};


void RemoteSocket::on_msg_read(p::base::ZBuffer* zbuf) {
    if (zbuf == nullptr) {
        return ;
    }
    LOG_DEBUG  << this << " RemoteSocket on msg read len=" << zbuf->size();
    sp_->got_response(std::move(*zbuf));
}

void RemoteSocket::on_connected() {
    LOG_DEBUG << this << " RemoteSocket on on_connected" << ",rms=" << remote_side()
         << ",lcs" << local_side();

    sp_->step2();
}

void RemoteSocket::on_failed() {

}

