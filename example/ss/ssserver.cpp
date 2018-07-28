
#include "shadowsocket.h"

class ShadowAcceptor : public p::rpc::Acceptor {
public:
    ShadowAcceptor(p::rpc::AsyncWorker* work) : Acceptor(work) {}

    virtual p::rpc::Socket* new_socket() {
        return new ShadowSocket();
    }
};

int main(int argc, char* argv[]) {
    //p::base::Log::set_log_level(p::base::LogLevel::kTrace);
    //p::base::g_log_level = p::base::LogLevel::kFatal;

    p::rpc::AsyncWorker worker;

    ShadowAcceptor ac(&worker);

    int port = 8333;
    if (argc >= 2) {
        port = atol(argv[1]);
    }

    int ret = ac.listen(p::base::EndPoint("0.0.0.0", port));

    LOG_DEBUG << "listen ret=" << ret;

    worker.start();

    worker.join();

    return 0;
}
