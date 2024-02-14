#include "process.hpp"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <bitset>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <charconv>
#include <set>
#include <string_view>
#include "simdjson.h"
#include "ws.hpp"

std::mutex m;
std::size_t construct_ans(char *result,
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

    const char epilogue[] =
        "\", \"apiKey\": \"***\", \"sign\": \"***\"}";
    std::memmove(result + len, epilogue, sizeof(epilogue));
    len += sizeof(epilogue) - 1;

    return len;
}

void process_data(int wsfd) {
    std::uint8_t buffer[MAX_BUFFER_SIZE * 2];
    char *char_buffer = reinterpret_cast<char *>(buffer);

    ws_write_buffer write_buffer;
    simdjson::ondemand::parser parser;

    while (true) {
        ssize_t sreceived;
        std::unique_lock l{m};

        if ((sreceived = read(wsfd, buffer, MAX_BUFFER_SIZE)) <= 0) {
            std::cout << "FAIL!" << std::endl;
            return;
        }

        std::size_t received = sreceived;
        std::size_t handled = 0;
        std::size_t index = 0;

        while (handled < received) {
            // Processing one message
            std::cout << "read try" << std::endl;

            if (received - handled < 4) {
                // if message header is cut, read it
                if ((sreceived = read(wsfd, buffer + received, 4)) <= 0) {
                    return;
                }
                received += sreceived;
            }

            uint8_t head_byte = buffer[handled++];
            uint8_t len_byte = buffer[handled++];

            std::size_t len = 0;

            if (len_byte == 126) {
                len = 256 * buffer[handled] + buffer[handled + 1];
                handled += 2;
            } else if (len_byte == 127) {
                // this should never happen
                len = 256 * 256 * 256 * buffer[handled] +
                      256 * 256 * buffer[handled + 1] +
                      256 * buffer[handled + 2] + buffer[handled + 3];
                handled += 4;
            } else {
                len = len_byte;
            }

            if (received - handled < len) {
                // reading missing tail if needed
                std::cout << "read tail" << std::endl;
                if (!guaranteed_read(wsfd, char_buffer + received,
                                     len - (received - handled))) {
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
                std::memmove(reinterpret_cast<char *>(pong_message) + 6,
                             char_buffer + handled, len);
                pong_message[1] = 0x80 | len;
                guaranteed_write(wsfd, reinterpret_cast<char *>(pong_message),
                                 6 + len);
                // std::cout << "pong" << std::endl;
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
                // std::cout << "message: " << message << std::endl;

                simdjson::ondemand::document doc;

                parser
                    .iterate(char_buffer + message_start, real_len,
                             2 * MAX_BUFFER_SIZE - message_start)
                    .get(doc);

                if (doc.get_object().find_field("data").error() ==
                    simdjson::SUCCESS) {
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

                    std::size_t response_len =
                        construct_ans(write_buffer.data_buffer(), order_id,
                                      side, price, volume, state, utime);
                    // std::string_view response(write_buffer.data_buffer(),
                    // response_len); std::cout << "response: " << response <<
                    // std::endl; std::cout << "response len = " << response_len
                    // << std::endl;
                    write_buffer.send_to(wsfd, response_len);
                }
            } else {
                // std::cout << "Unknown message type" << std::endl;
            }
            handled += len;

            if (handled != message_end) {
                buffer[handled] = next_head;
            }
        }
    }
}

constexpr std::uint64_t COUNT_THLD = 1000;
constexpr std::uint64_t TIME_THLD = 120000;

void process_single_data(int wsfd, int thid = 0) {
    std::uint64_t sum = 0;
    std::uint64_t count = 0;


    std::uint8_t buffer[MAX_BUFFER_SIZE * 2];
    char *char_buffer = reinterpret_cast<char *>(buffer);

    ws_write_buffer write_buffer;
    simdjson::ondemand::parser parser;

    while (true) {
        // assume that single read reads only one message,
        // if it doesn't work use `process_data()`
        ssize_t sreceived;
        if ((sreceived = read(wsfd, buffer, MAX_BUFFER_SIZE)) <= 0) {
            std::cout << "FAIL!" << std::endl;
            return;
        }

        std::size_t handled = 0;

        uint8_t head_byte = buffer[handled++];
        uint8_t len_byte = buffer[handled++];

        std::size_t received = sreceived;
        std::size_t len = 0;
        std::size_t header_size = 2;

        if (len_byte == 126) {
            len = 256 * buffer[handled] + buffer[handled + 1];
            handled += 2;
            header_size = 4;
        } else if (len_byte == 127) {
            // this should never happen, except for some ill-formed messages
            len = 256 * 256 * 256 * buffer[handled] +
                  256 * 256 * buffer[handled + 1] + 256 * buffer[handled + 2] +
                  buffer[handled + 3];
            handled += 4;
            header_size = 8;
        } else {
            len = len_byte;
        }

        // std::cout << "read message of len " << len << std::endl;

        if (len + header_size != received) {
            std::cout << "bad read!" << std::endl;
            return;
        }

        if (head_byte == ping_head) {
            std::memmove(reinterpret_cast<char *>(pong_message) + 6,
                         char_buffer + handled, len);
            pong_message[1] = 0x80 | len;
            guaranteed_write(wsfd, reinterpret_cast<char *>(pong_message),
                             6 + len);
        } else if (head_byte == text_head) {
            std::size_t message_start = handled;
            std::size_t message_end = received;

            if (char_buffer[message_start] != '{') {
                char_buffer[--message_start] = '{';
            }

            if (char_buffer[message_end - 1] != '}') {
                char_buffer[message_end++] = '}';
            }

            std::size_t real_len = message_end - message_start;

            std::string_view message(char_buffer + message_start, real_len);
            //std::cout << "message: " << message << std::endl;

            simdjson::ondemand::document doc;

            parser
                .iterate(char_buffer + message_start, real_len,
                         2 * MAX_BUFFER_SIZE - message_start)
                .get(doc);

            if (doc.get_object().find_field("data").error() ==
                simdjson::SUCCESS) {
                simdjson::ondemand::array array;
                doc["data"].get_array().get(array);
                simdjson::ondemand::object first_answer;
                simdjson::ondemand::array_iterator iter;
                array.begin().get(iter);
                (*iter).get_object().get(first_answer);

                std::string_view instType;
                std::string_view order_id;
                std::string_view side;
                std::string_view price;
                std::string_view volume;
                std::string_view state;
                std::string_view utime;

                first_answer["instId"].get(instType);

                first_answer["ordId"].get(order_id);
                first_answer["side"].get(side);
                first_answer["px"].get(price);
                first_answer["sz"].get(volume);
                first_answer["state"].get(state);
                first_answer["uTime"].get(utime);

                std::size_t response_len =
                    construct_ans(write_buffer.data_buffer(), order_id, side,
                                  price, volume, state, utime);
                std::string_view response(write_buffer.data_buffer(),
                                          response_len);
                //std::cout << "response" << response[response_len - 1] << std::endl;
                //std::cout << "response: " << response << std::endl;
/*
                uint64_t timestamp =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

                uint64_t tm{};
                std::from_chars(utime.data(), utime.data() + utime.size(), tm);
                */
                //std::cout << "delay = " << timestamp - tm * 1000 << "\n";
                sum += timestamp - tm * 1000;
                count++;
                if (count > COUNT_THLD && sum / count > LAT_THLD) {
                   return; 
                }

                //std::cout << "thid = " << thid << " , mean delay = " << sum / count << std::endl;
                // std::cout << "response len = " << response_len <<
                // std::endl;

//
//   DO NOT FORGET UNCOMMENT!!!!!!!!
// 
                write_buffer.send_to(wsfd, response_len);
            } else {
                // std::cout << "corrupted_message" << message << std::endl;
            }
        }
    }
}

void main_loop(int thid) {
    while (true) {
        int sock = websocket("127.0.0.1", 9999);

        ws_write_buffer write_buffer;

        std::string login_request =
            std::string(
                R"json({"op":     "login",   "args": [{"apiKey" : "***", "passphrase" : "","timestamp": )json") +
            std::to_string(std::time(nullptr)) +
            R"json(, "sign": "***"}]})json";
        const std::string subscribe_request =
            R"json({"op": "subscribe", "args": [{"channel": "orders", "instType": "ANY", "instId": "BNB-USDT-SWAP"}]})json";

        const std::string subscribe_request_all =
            R"json({"op": "subscribe", "args": [{"channel": "orders", "instType": "ANY"}]})json";

        std::cout << "sending " << login_request << " " <<login_request.length() << std::endl;

        memcpy(write_buffer.data_buffer(), login_request.c_str(),
               login_request.length());
        write_buffer.send_to(sock, login_request.length());

        std::cout << "waiting for response" << std::endl;

        uint64_t timestamp1 =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();

        handle_messages(sock);

        uint64_t timestamp2 =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
            
        std::cout << timestamp2 - timestamp1 << std::endl;

        std::cout << "sending " << login_request << std::endl;

        memcpy(write_buffer.data_buffer(), subscribe_request.c_str(),
               subscribe_request.length());
        write_buffer.send_to(sock, subscribe_request.length());

        std::cout << "waiting for response" << std::endl;

        handle_messages(sock);
        process_single_data(sock, thid);
        std::cout << "mean = " << 1.0 * sum / count << std::endl;
        close(sock);
    }
}
