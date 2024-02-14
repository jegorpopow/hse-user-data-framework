#include "ws.hpp"
#include "process.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdint.h>
#include <unistd.h>
#include <ctime>


#include <bitset>
#include <string>

int main() {
    while (true) {
        int sock = websocket("127.0.0.1", 9999);
        
        ws_write_buffer write_buffer;

        std::string login_request =  std::string(R"json({"op":"login","args":[{"apiKey":"***","passphrase":"","timestamp":)json") 
        + std::to_string(std::time(nullptr) ) + R"json(,"sign":"***"}]})json";
        const std::string subscribe_request = R"json({"op": "subscribe", "args": [{"channel": "orders", "instType": "ANY"}]})json";

        std::cout << "sending " << login_request << std::endl;

        memcpy(write_buffer.data_buffer(), login_request.c_str(), login_request.length());
        write_buffer.send_to(sock, login_request.length());

        std::cout << "waiting for response" << std::endl;

        handle_messages(sock);

        std::cout << "sending " << login_request << std::endl;

        memcpy(write_buffer.data_buffer(), subscribe_request.c_str(), subscribe_request.length());
        write_buffer.send_to(sock, subscribe_request.length());

        std::cout << "waiting for response" << std::endl;

        handle_messages(sock);
        process_data(sock);
    }

    return 0;
}