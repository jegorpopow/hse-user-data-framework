#include <stdint.h>
#include <time.h>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/format.hpp>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <cstring>
#include <charconv>
#include <string_view>
#include <utility>
#include "simdjson.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
using boost::asio::ip::tcp;

const std::string login_request_template =
    R"json({"op": "login", "args": [{"apiKey": "***", "passphrase": "", "timestamp": %d, "sign": "***"}]})json";

const char subscribe_request[] = R"json({"op": "subscribe", "args": [{"channel": "orders", "instType": "ANY", "instId": "BNB-USDT-SWAP"}]})json";

const std::string host = "127.0.0.1";
const std::string port = "9999";
const std::string hostname = "127.0.0.1:9999";
const std::string serverParameters =
    "/?url=wss://ws.okx.com:8443/ws/v5/private";

constexpr std::size_t max_response_len = 10000;


std::pair<char*, std::size_t> construct_ans(std::string_view order_id,
                std::string_view side,
                std::string_view price,
                std::string_view volume,
                std::string_view state,
                std::string_view utime) {
    static char result[4096] = "{\"orderId\": ";
    std::size_t len = 12;

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

    return std::make_pair(result, len); 
}

int main() {
    while (true) {
        boost::asio::io_context io_context;
        auto const counterparty = tcp::resolver(io_context).resolve(host, port);
        websocket::stream<tcp::socket> ws(io_context);
        boost::asio::connect(ws.next_layer(), counterparty);

        ws.handshake(hostname, serverParameters);

        std::string login_request =
            (boost::format(login_request_template) % std::time(nullptr)).str();
        std::cout << "login request: " << login_request << std::endl;
        ws.write(boost::asio::buffer(login_request));
        beast::flat_buffer buffer;
        ws.read(buffer);
        std::cout << "login response:\n"
                << beast::make_printable(buffer.data()) << std::endl;
        ws.write(boost::asio::buffer(subscribe_request, sizeof(subscribe_request) - 1));
        ws.read(buffer);
        std::cout << "subscribe response:\n"
                << beast::make_printable(buffer.data()) << std::endl;

        simdjson::ondemand::parser parser;
        simdjson:simdjson::padded_string padded(max_response_len);

        try {
            while (true) {
                buffer.clear();
                ws.read(buffer);
                std::string meow = beast::buffers_to_string(buffer.data());
                if (meow[0] != '{') {
                    meow = "{" + meow;
                }

                if (meow.back() != '}') {
                    meow.push_back('}');
                }

                padded = meow;
                //std::cout << "received message:\n" << padded << std::endl;

                simdjson::ondemand::document doc; 
                parser.iterate(padded).get(doc);
                if (doc.get_object().find_field("data").error() != simdjson::SUCCESS) {
                    continue;
                }

                simdjson::ondemand::array array;
                doc["data"].get_array().get(array);
                simdjson::ondemand::object first_answer;
                simdjson::ondemand::array_iterator iter;
                array.begin().get(iter);
                (*iter).get_object().get(first_answer);

                std::string_view instId;
                std::string_view order_id;
                std::string_view side;
                std::string_view price;
                std::string_view volume;
                std::string_view state;
                std::string_view utime;

                first_answer["instId"].get(instId);

                //if (instId[0] == 'B' && instId[1] == 'N') {
                    first_answer["ordId"].get(order_id);
                    first_answer["side"].get(side);
                    first_answer["px"].get(price);
                    first_answer["sz"].get(volume);
                    first_answer["state"].get(state);
                    first_answer["uTime"].get(utime);
    /*                
                    uint64_t timestamp = std::chrono::duration_cast< std::chrono::microseconds >(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                    
                    uint64_t tm{};
                    std::string_view val;
                    first_answer["uTime"].get(val);
                    std::from_chars(val.data(), val.data() + val.size(), tm);
                    std::cout << "delay = " <<  timestamp - tm * 1000 << "\n";*/

                    auto [str, len] = construct_ans(order_id, side, price, volume, state, utime);
                    //std::cout << "response:\n" << str << std::endl;
                    ws.write(boost::asio::buffer(str, len));  
                //} 
            }
        } catch(...) {
            continue;
        }
    }
    return 0;
}