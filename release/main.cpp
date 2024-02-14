#include <stdint.h>
#include <unistd.h>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <thread>
#include "process.hpp"
#include "ws.hpp"

int main() {
    std::thread t1(main_loop, 1);
    t1.detach();
    main_loop(3);
    return 0;
}