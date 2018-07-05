#include <stdio.h>
#include <unistd.h>

#include "p/rpc/async_worker.h"
#include "p/rpc/acceptor.h"

class MySocket : public p::rpc::Socket {
public:
    virtual void on_msg_read(char* buf, size_t len) {
        LOG_DEBUG << this << " on read len=" << len << " :" << buf;
        if (buf == nullptr) {
            eof_ = true;
        }
        send_msg("hello,world\r\n", 13, nullptr);
    }

    virtual void on_msg_sended(const char* buf, int64_t len, void* arg) {
        LOG_DEBUG << this << " on writed len=" << len << " :" << buf;
        if (eof_) {
            shutdown();
        }
    }

    virtual void on_send_failed(const char* buf, int64_t len, void* arg) {
        LOG_DEBUG << this << " on failed";
    }

    bool eof_ = false;
};

class MyAcceptor : public p::rpc::Acceptor {
public:
    MyAcceptor(p::rpc::AsyncWorker* work) : Acceptor(work) {}

    virtual p::rpc::Socket* new_socket() {
        return new MySocket();
    }
};

void x(void* arg) {
    LOG_DEBUG<< "message run chenzjiga " << (char*)arg;
}

int main() {
    p::rpc::AsyncWorker worker;

    MyAcceptor ac(&worker);

    int ret = ac.listen(p::base::EndPoint("127.0.0.1:8001"));

    LOG_DEBUG << "listen ret=" << ret;

    worker.start();

    worker.join();


    // worker.push_message(x, (void*)"asldjfsdfasd");
    // sleep(1);
    // LOG_DEBUG << "add a new message";
    worker.push_message(x, (void*)"asldjfsdfasd");

    LOG_DEBUG << "in_worker_thread " << worker.in_worker_thread();

    return 0;
}
