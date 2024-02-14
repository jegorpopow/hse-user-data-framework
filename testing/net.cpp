#include <cstdint>
#include <iostream>
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

#include <bitset>
#include "net.hpp"


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

struct ws_write_buffer {
    char inner_buffer[sizeof(ws_header_t) + MAX_BUFFER_SIZE];
    std::size_t inner_size;
    ws_header_t* header_m = nullptr;

    ws_write_buffer() {
        ws_header_t header;
        header.fin =  1;
        header.rsv1 = 0;
        header.rsv2 = 0;
        header.rsv3 = 0;
        header.opcode = 1;
        header.masked = 1;
        header.mask = 0;

        header_m = new(inner_buffer) ws_header_t{}; 
        memcpy(inner_buffer, reinterpret_cast<char*>(&header), sizeof(header));
    }

    void save_data(const char* data, std::size_t size) {
        header_m->len = size;
        inner_size = sizeof(ws_header_t) + size;
        memcpy(inner_buffer + sizeof(ws_header_t), data, size);
    }
};

bool ws_slow_read(int sock, char* dest) {
    uint8_t header[4];
    if (!guaranteed_read(sock, reinterpret_cast<char*>(header), 4)) {
        return false;
    }

    if ((header[0] & 0x0f) == 1) {
        std::size_t raw_len = header[1] & 0x7F;
        std::size_t len = raw_len == 126 ? header[3] + 256 * header[2] : raw_len;
    
        if (len == raw_len) {
            std::memcpy(dest, reinterpret_cast<char*>(header + 2), 2);
            guaranteed_read(sock, dest + 2, len - 2);
            dest[len] = 0;
        } else {
            guaranteed_read(sock, dest,len);
            dest[len] = 0;
        }
    }

    return true;
} 
