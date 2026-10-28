#include "../src/usage.hpp"
#include <chrono>
#include <iostream>

int main() {
    std::uint64_t before=0,after=0;
    if(!sc::threadCpuNs(before))return 1;
    const auto start=sc::measuredCpuNs.load(std::memory_order_relaxed);
    {
        sc::CpuScope work;
        volatile std::uint64_t value=1;
        const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(80);
        while(std::chrono::steady_clock::now()<until)value=value*1664525u+1013904223u;
        (void)value;
    }
    if(!sc::threadCpuNs(after)||after<=before)return 2;
    if(sc::measuredCpuNs.load(std::memory_order_relaxed)<=start)return 3;
    std::cout<<"Chat measured CPU counter advances during plugin work\n";
}
