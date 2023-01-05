#include "connection.h"

#include <iostream>
#include <thread>

#include <unistd.h>

using namespace std;

void server(){
    Server server_obj("127.0.0.1", 8080);
    SocketTask task;
    server_obj.run(task);
}

void client(){
    Client client_obj("127.0.0.1", 8080);
    SocketTask task;
    client_obj.run(task);
}

int main(){
    auto th1 = thread(server);
    sleep(1);
    auto th2 = thread(client);
    th1.join();
    th2.join();
};
