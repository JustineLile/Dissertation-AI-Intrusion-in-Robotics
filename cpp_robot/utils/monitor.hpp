#ifndef MONITOR_HPP
#define MONITOR_HPP
#include <cstdint>
namespace monitor {

#if defined(__x86_64__)

struct Registers {
	//general registers
	std::uint64_t rax;
	std::uint64_t rbx;
	std::uint64_t rcx;
	std::uint64_t rdx;
	std::uint64_t rsi;
	std::uint64_t rdi;
	//pointers
	std::uint64_t rip;
	std::uint64_t rsp;
	std::uint64_t rbp;
};

//calls the cpp func
Registers capture();
#endif

}
#endif
