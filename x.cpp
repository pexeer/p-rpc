
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
    p::rpc::AsyncWorker worker;

    ShadowAcceptor ac(&worker);

    int ret = ac.listen(p::base::EndPoint(argv[1]));

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
