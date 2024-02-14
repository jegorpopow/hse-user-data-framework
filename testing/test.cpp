#include "net.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdint.h>
#include <unistd.h>
#include <ctime>


#include <bitset>
#include <string>

const std::string subscribe_request = R"json({"op": "subscribe", "args": [{"channel": "orders", "instType": "ANY"}]})json";

int main() {
    std::string request =  std::string(R"json({"op":"login","args":[{"apiKey":"***","passphrase":"","timestamp":)json") 
    + std::to_string(std::time(nullptr) ) + R"json(,"sign":"***"}]})json";

    std::cout << request << std::endl;
    std::cout << request.length() + 1 << std::endl;
    int sock = websocket("127.0.0.1", 9999);

    write_buffer wb;
    std::cout << "request: " << request << std::endl; 
    std::cout << "request length" << request.length() << std::endl;
    memcpy(wb.get_data(), request.c_str(), request.length());
    wb.send_to(sock, request.length());
 
    //std::cout << "request message: " << reinterpret_cast<char*>(buffer + 6) << std::endl;
    //write(sock, buffer, to_send);
    //websocket_write(sock, login_request_template, std::strlen(login_request_template) + 1);

    std::cout << "login request send" << std::endl;

    ws_read_buffer<16> rb;
    rb.read_from(sock);
    std::cout << "login request result:" << rb.get_result() << std::endl;

    memcpy(wb.get_data(), subscribe_request.c_str(), subscribe_request.length());
    wb.send_to(sock, subscribe_request.length());

    std::cout << "subscribe request send" << std::endl;
    rb.read_from(sock);
    std::cout << "subscribe request result:" << rb.get_result() << std::endl;

    int total_received = 0;
    while (true) {
        char aboba[4096];
        //ws_read_buffer<16> rb;
        //rb.read_from(sock, 4);
        ws_slow_read(sock, aboba);
        std::cout << aboba << std::endl;
        total_received++;
        
        if (total_received % 10 == 0) {
            std::cout << total_received << std::endl;
        } 
        //std::cout << rb.get_result() << std::endl;
    }

/*
    std::uint8_t buffer[1000] = {};
    ssize_t res = read(sock, buffer, 100);
    std::cout << "response is " << res << "bytes" << std::endl;
    for (ssize_t i = 0; i < res; i++) {
        std::cout << std::bitset<8>(buffer[i]) << std::endl;
    }

    std::cout << "len = " << uint8_t(reinterpret_cast<ws_small_header*>(buffer)->len) << std::endl;

    char str[21] = {};
    std::memcpy(str, buffer + 2, 16);
    std::cout << str << std::endl;
*/
    //char buffer[MAX_BUFFER_SIZE];
    //websocket_read(sock, buffer);
    //std::cout << buffer << std::endl;
}