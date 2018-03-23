#include <stdio.h>
#include <unistd.h>

#include "p/rpc/async_worker.h"

void x(void* arg) {
    LOG_DEBUG<< "message run chenzjiga " << (char*)arg;
    //sleep(3);
}

int main() {
    p::rpc::AsyncWorker worker;

    worker.start();

    sleep(1);
    //worker.

    //worker.wakeup_after(1000000000);
    //sleep(2);

    worker.push_message(x, (void*)"asldjfsdfasd");
    sleep(1);
    LOG_DEBUG << "add a new message";
    worker.push_message(x, (void*)"asldjfsdfasd");

    sleep(3);

    LOG_DEBUG << "in_worker_thread" << worker.in_worker_thread();

    return 0;
}
