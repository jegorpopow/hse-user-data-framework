#ifndef WS_H_
#define WS_H_

#include <cstdint>
#include <cstring>
#include <new>
#include <cstddef>
#include <stdint.h>
#include <bitset>
#include <iostream>
#include <type_traits>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

constexpr inline std::size_t MAX_BUFFER_SIZE = 4096;
constexpr inline std::size_t HEADER_SIZE = 8;


struct ws_header_t {
    uint8_t opcode : 4;
    uint8_t rsv3 : 1;
    uint8_t rsv2 : 1;
    uint8_t rsv1 : 1;
    uint8_t fin : 1;
    uint8_t len : 7;
    uint8_t masked : 1;
    uint32_t mask;
};

#pragma pack(push, 1)
struct ws_write_header {
    uint8_t head_byte = 0x81;
    uint8_t len_byte  = 0x80;
    uint32_t mask = 0;
};
#pragma pack(pop)

constexpr std::uint8_t text_head = 0x81;
constexpr std::uint8_t ping_head = 0x89;
constexpr std::uint8_t pong_head = 0x8A;

#pragma pack(push, 1)
struct ws_small_header {
    uint8_t opcode : 4;
    uint8_t rsv3 : 1;
    uint8_t rsv2 : 1;
    uint8_t rsv1 : 1;
    uint8_t fin : 1;
    uint8_t len : 7;
    uint8_t masked : 1;
};
#pragma pack(pop)

bool guaranteed_write(int fd, const char* src, std::size_t size);
bool guaranteed_read(int fd, char* dest, std::size_t size);
int tcp_connect_to(const char* ip, int port);
int websocket(const char* ip, int port);

bool handle_messages(int wsfd);
extern std::uint8_t pong_message[MAX_BUFFER_SIZE];

struct ws_write_buffer {
private:    
    uint8_t buffer[MAX_BUFFER_SIZE + HEADER_SIZE];
public:
    char* data_buffer() {
        return reinterpret_cast<char*>(buffer) + HEADER_SIZE;
    }

    bool send_to(int wsfd, std::size_t len, uint8_t head_byte = text_head) {
        //std::cout << "len = " << len << std::endl;
        std::memset(reinterpret_cast<char*>(buffer), 0, HEADER_SIZE);
        if (len < 126) {
            buffer[3] = 0x80 | len;
            buffer[2] = head_byte;
            return guaranteed_write(wsfd, reinterpret_cast<char*>(buffer) + 2, len + 6);
        } else {
            buffer[3] = len % 256;
            buffer[2] = len / 256;
            buffer[1] = 0x80 | 126;
            buffer[0] = head_byte;
            return guaranteed_write(wsfd, reinterpret_cast<char*>(buffer), len + 8);
        }
    }

    ws_write_buffer() : buffer{} {
        std::cout << "created buffer" << std::endl;
    } 
};

#endif