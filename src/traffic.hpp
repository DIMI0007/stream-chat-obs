#pragma once
#include <atomic>
#include <cstdint>

namespace sc::traffic {
inline std::atomic<std::uint64_t> received{0};
inline std::atomic<std::uint64_t> sent{0};
inline void addReceived(std::uint64_t bytes){received.fetch_add(bytes,std::memory_order_relaxed);}
inline void addSent(std::uint64_t bytes){sent.fetch_add(bytes,std::memory_order_relaxed);}
}
