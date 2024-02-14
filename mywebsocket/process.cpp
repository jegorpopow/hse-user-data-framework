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
#include "process.hpp"
#include "simdjson.h"

std::size_t construct_ans(
                char* result,
                std::string_view order_id,
                std::string_view side,
                std::string_view price,
                std::string_view volume,
                std::string_view state,
                std::string_view utime) {
    std::size_t len = 0;

    std::memmove(result + len, "{\"orderId\": ", 12);
    len += 11;

    std::memmove(result + len, order_id.data(), order_id.size());
    len += order_id.size();

    std::memmove(result + len, ", \"side\": \"", 11);
    len += 11;

    std::memmove(result + len, side.data(), side.size());
    len += side.size();

    std::memmove(result + len, "\", \"price\": \"", 13);
    len += 13;

    std::memmove(result + len, price.data(), price.size());
    len += price.size();

    std::memmove(result + len, "\", \"volume\": \"", 14);
    len += 14;

    std::memmove(result + len, volume.data(), volume.size());
    len += volume.size();

    std::memmove(result + len, "\", \"state\": \"", 14);
    len += 13;

    std::memmove(result + len, state.data(), state.size());
    len += state.size();

    std::memmove(result + len, "\", \"uTime\": \"", 14);
    len += 13;

    std::memmove(result + len, utime.data(), utime.size());
    len += utime.size();

    const char epilogue[] = "\", \"apiKey\": \"***\", \"sign\": \"***\"}";
    std::memmove(result + len, epilogue, sizeof(epilogue));
    len += sizeof(epilogue) - 1;

    return len; 
}

void process_data(int wsfd) {
    std::uint8_t buffer[MAX_BUFFER_SIZE * 2];
    char* char_buffer = reinterpret_cast<char*>(buffer);

    ws_write_buffer write_buffer;
    simdjson::ondemand::parser parser;

    while (true) {
        ssize_t sreceived;
        if ((sreceived = read(wsfd, buffer, MAX_BUFFER_SIZE)) <= 0) {
            std::cout << "FAIL!" << std::endl;
            return;
        }

        std::size_t received = sreceived;
        std::size_t handled = 0;
        std::size_t index = 0;

        while (handled < received) {

            // Processing one message

            if (received - handled < 4) {
                // if message header is cut, read it
                if((sreceived = read(wsfd, buffer + received, 4)) <= 0) {
                    return;
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
                // this should never happen
                len = 256 * 256 * 256 * buffer[handled] +
                    256 * 256 * buffer[handled + 1] +
                    256 * buffer[handled + 2] + 
                    buffer[handled + 3]; 
                handled += 4;
            } else {
                len = len_byte;
            }

            if (received - handled < len) {
                // reading missing tail if needed
                std::cout << "read tail" << std::endl;
                if (!guaranteed_read(wsfd, char_buffer + received, len - (received - handled))) {
                    return;
                }
                received += len - (received - handled);
            }

/*             if (!(head_byte >> 7)) {
                std::cout << "multi-packet message" << std::endl;
                return;
            }  
 */
            std::size_t message_start = handled;
            std::size_t message_end = handled + len; 
            std::uint8_t next_head{};

            if (head_byte == ping_head) {
                std::memmove(reinterpret_cast<char*>(pong_message) + 6, char_buffer + handled, len);
                pong_message[1] = 0x80 | len;
                guaranteed_write(wsfd, reinterpret_cast<char*>(pong_message), 6 + len);
                //std::cout << "pong" << std::endl;
            } else if (head_byte == text_head) {
                if (char_buffer[message_start] != '{') {
                    char_buffer[--message_start] = '}';
                }

                if (char_buffer[message_end - 1] != '}') {
                    next_head = buffer[message_end]; 
                    char_buffer[message_end++] = '}';
                }

                std::size_t real_len = message_end - message_start;

                std::string_view message(char_buffer + message_start, real_len);
                //std::cout << "message: " << message << std::endl;

                simdjson::ondemand::document doc; 

                parser.iterate(char_buffer + message_start, real_len, 2 * MAX_BUFFER_SIZE - message_start).get(doc);
                
                if (doc.get_object().find_field("data").error() == simdjson::SUCCESS) {
                    simdjson::ondemand::array array;
                    doc["data"].get_array().get(array);
                    simdjson::ondemand::object first_answer;
                    simdjson::ondemand::array_iterator iter;
                    array.begin().get(iter);
                    (*iter).get_object().get(first_answer);

                    std::string_view order_id;
                    std::string_view side;
                    std::string_view price;
                    std::string_view volume;
                    std::string_view state;
                    std::string_view utime;

                    first_answer["ordId"].get(order_id);
                    first_answer["side"].get(side);
                    first_answer["px"].get(price);
                    first_answer["sz"].get(volume);
                    first_answer["state"].get(state);
                    first_answer["uTime"].get(utime);

                    std::size_t response_len = construct_ans(write_buffer.data_buffer(), order_id, side, price, volume, state, utime);
                    //std::string_view response(write_buffer.data_buffer(), response_len);
                    //std::cout << "response: " << response << std::endl;
                    //std::cout << "response len = " << response_len << std::endl;
                    write_buffer.send_to(wsfd, response_len);   
                }
            } else {
                //std::cout << "Unknown message type" << std::endl;
            }
            handled += len;
            
            if (handled != message_end) {
                buffer[handled] = next_head; 
            }
        }
    }
}

void main_loop() {
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
        close(sock);
    }
}
