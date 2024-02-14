#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <string_view>
#include <bitset>
#include <string_view>
#include "ws.hpp"


// Some awful C-style code 
// TODO: refactor

bool guaranteed_write(int fd, const char* src, std::size_t size) {
    ssize_t sent = 0;
    while (sent < size) {
        ssize_t last_msg = write(fd, src + sent, size - sent);
        if (last_msg < 0) {
            return false;
        }
        sent += last_msg;
    } 
    return true;
}

bool guaranteed_read(int fd, char* dest, std::size_t size) {
    ssize_t received = 0;
    while (received < size) {
        ssize_t last_msg = read(fd, dest + received, size - received);
        if (last_msg < 0) {
            return false;
        }
        received += last_msg;
    } 
    return true;
}

int tcp_connect_to(const char* ip, int port)  {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        return -1;
    }

    struct sockaddr_in counterparty;

    counterparty.sin_family = AF_INET;
    counterparty.sin_port = htons(port);
    counterparty.sin_addr.s_addr = inet_addr(ip); 

    if(connect(sock, (struct sockaddr *) &counterparty, sizeof(counterparty)) < 0) {
        return -1;
    }

    return sock;
}

const char* websocket_http_handshake =
"GET /?url=wss://ws.okx.com:8443/ws/v5/private HTTP/1.1\r\n"
"Host: localhost:10000\r\n"
"Upgrade: websocket\r\n"
"Connection: upgrade\r\n"
"Sec-WebSocket-Key: ***\r\n"
"Sec-WebSocket-Version: 13\r\n\r\n";

int websocket(const char* ip, int port) {
    int sock = tcp_connect_to(ip, port);
    guaranteed_write(sock, websocket_http_handshake, strlen(websocket_http_handshake));

    char buffer[1024];
    ssize_t response_size;
    if ((response_size = read(sock, buffer, sizeof(buffer))) < 0) {
        return -1;
    }  
    buffer[response_size] = 0;

    std::cout << buffer << std::endl;
    return sock;
}

std::uint8_t pong_message[MAX_BUFFER_SIZE] = {pong_head, 0x80, 0x00, 0x00, 0x00, 0x00};

bool handle_messages(int wsfd) {
    std::uint8_t buffer[MAX_BUFFER_SIZE * 2];
    char* char_buffer = reinterpret_cast<char*>(buffer);

    ssize_t sreceived;
    if ((sreceived = read(wsfd, buffer, MAX_BUFFER_SIZE)) <= 0) {
        std::cout << "FAIL!" << std::endl;
        return false;
    }

    std::size_t received = sreceived;
    std::size_t handled = 0;
    std::size_t index = 0;

    std::cout << "received " << received << " bytes" << std::endl;

    while (handled < received) {

        if (received - handled < 4) {
            if((sreceived = read(wsfd, buffer + received, 4)) <= 0) {
                return false;
            }
            received += sreceived;
        }
        
        uint8_t head_byte = buffer[handled++];
        uint8_t len_byte  = buffer[handled++];

        std::size_t len = 0;

        if (len_byte == 126) {
            len = 256 * buffer[handled] + buffer[handled + 1];
            handled += 2;
        } else if (len_byte == 127) {
            len = 256 * 256 * 256 * buffer[handled] +
                  256 * 256 * buffer[handled + 1] +
                  256 * buffer[handled + 2] + 
                  buffer[handled + 3]; 
            handled += 4;
        } else {
            len = len_byte;
        }

        if (received - handled < len) {
            if (!guaranteed_read(wsfd, char_buffer + received, len - (received - handled))) {
                return false;
            }
            received += len - (received - handled);
        }

        std::cout << "message " 
            << index++ 
            << " with header " 
            << "operation:" << std::bitset<8>(head_byte) 
            << " len: " << std::bitset<8>(len_byte) 
            << " len = " << len 
            << std::endl;  
 
         if (!(head_byte >> 7)) {
            std::cout << "multi-packet message" << std::endl;
        }  
 
        if (head_byte == ping_head) {
            std::memmove(reinterpret_cast<char*>(pong_message) + 6, char_buffer + handled, len);
            pong_message[1] = 0x80 | len;
            guaranteed_write(wsfd, reinterpret_cast<char*>(pong_message), 6 + len);
            std::cout << "pong" << std::endl;
        }

        std::cout << std::string_view(char_buffer + handled, len) << std::endl;
        handled += len;
    }

    return true;
}
