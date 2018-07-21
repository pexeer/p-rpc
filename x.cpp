
#include "shadowsocket.h"

class ShadowAcceptor : public p::rpc::Acceptor {
public:
    ShadowAcceptor(p::rpc::AsyncWorker* work) : Acceptor(work) {}

    virtual p::rpc::Socket* new_socket() {
        return new ShadowSocket();
    }
};

void x(void* arg) {
    LOG_DEBUG<< "message run chenzjiga " << (char*)arg;
}

int main(int argc, char* argv[]) {
    p::base::Log::set_ef_log_level(p::base::LogLevel::kTrace);
    //p::base::g_log_level = p::base::LogLevel::kFatal;

    p::rpc::AsyncWorker worker;

    ShadowAcceptor ac(&worker);

    p::base::EndPoint test("ip.cn", 433);

    LOG_DEBUG << "test is " << test;

    int port = 8333;
    if (argc >= 2) {
        port = atol(argv[1]);
    }

    int ret = ac.listen(p::base::EndPoint("0.0.0.0", port));

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
