#ifndef NET_H_
#define NET_H_

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


bool ws_slow_read(int sock, char* dest);

struct write_buffer {
    char buffer[sizeof(ws_header_t) + MAX_BUFFER_SIZE];
    ws_write_header* header = nullptr;
    write_buffer() {
        header = new(buffer) ws_write_header;  
        header->head_byte = text_head;
        header->len_byte = 0x80;
        header->mask = 0;
    }

    char* get_data() {
        return buffer + sizeof(ws_write_header);
    }

    bool send_to(int sock, std::size_t size) {
        uint8_t* bf = reinterpret_cast<uint8_t*>(buffer);
        std::cout << "len " << std::bitset<8>(size) << std::endl; 
        header->len_byte = (0x80 | size);
/*        
        for(int i = 0; i < size + sizeof(ws_write_header); i++) {
            std::cout << std::bitset<8>(bf[i]) << std::endl;
        }
*/
        return guaranteed_write(sock, buffer, size + sizeof(ws_write_header));
    }
};



template <std::size_t Alignment = 4>
struct ws_read_buffer {
    alignas(Alignment) uint8_t buffer[MAX_BUFFER_SIZE + Alignment];
    bool read_from(int sock, std::size_t header_size = 2) {
        char* bfs = reinterpret_cast<char*>(buffer);
        uint8_t* bf = buffer + Alignment - header_size; 

        ssize_t received = read(sock, bfs + Alignment - header_size, MAX_BUFFER_SIZE);
        if (received < 0) {
            return false;
        }

        if (bf[0] == ping_head) {
            std::cout << "PING!" << std::endl;
            std::uint8_t pong_msg[] = {pong_head, 0x80, 0, 0, 0, 0};
            if (!guaranteed_write(sock, reinterpret_cast<char*>(pong_msg), sizeof(pong_msg))) {
                return false;
            }
            std::cout << "PONG!" << std::endl;
            return read_from(sock, header_size);
        }

        if (bf[0] != text_head) {
            std::cout << std::bitset<8>(bf[0]) << std::endl; 
            std::cout << "UWAGA!" << std::endl;
            return read_from(sock, header_size);
        } 

        //std::cout << std::bitset<8>(bf[0]) << std::endl;
        //std::cout << std::bitset<8>(bf[1]) << std::endl;
        if (!(bf[0] & 0x80)) {
            std::cout << "UNCLOSED\n" << std::endl;
        }

        std::size_t len = (bf[1] & 0x7F);
        std::size_t real_header_size = 2;

        if (len == 126) {
            len = bf[3] + 256 * bf[2];
            real_header_size = 4;
        }     

        if (len == 127) {
            std::cout << "VERY LONG MESSAGE" << std::endl;
        }   

        std::cout << "len = " << len << std::endl;

        if (received > real_header_size + len) {
            std::cout << "OVERFLOW: " << received << "of" << real_header_size << " +  " << len << std::endl;
        }
        
        if (received < real_header_size + len) {
            if (!guaranteed_read(sock,
                  bfs + Alignment - header_size + received,
                  real_header_size + len - received)) {
                return false;
            }
        } 

        buffer[Alignment + len] = 0;
        return true;
    }

    char* get_result() {
        return reinterpret_cast<char*>(buffer + Alignment);
    }
};

#endif