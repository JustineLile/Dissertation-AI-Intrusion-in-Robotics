#include "monitor.hpp"
#include <cstdint>

namespace monitor {

#if defined(__x86_64__)

Registers capture(){
	Registers regs{};
	//use inline assembly to get registers
	__asm__ volatile (
		"mov %%rax, %0\n\t"
		"mov %%rbx, %1\n\t" 
		"mov %%rcx, %2\n\t"
		"mov %%rdx, %3\n\t" 
 		"mov %%rsi, %4\n\t" 
		"mov %%rdi, %5\n\t" 
		//pointers
		"mov %%rsp, %7\n\t" 
		"mov %%rbp, %8\n\t" 

		
		//assign regs to struct
		: "=regs"(regs.rax),
		  "=regs"(regs.rbx),
		  "=regs"(regs.rcx),
		  "=regs"(regs.rdx),
                  "=regs"(regs.rsi),
                  "=regs"(regs.rdi),
                  "=regs"(regs.rip),
                  "=regs"(regs.rsp),
		  "=regs"(regs.rbp)

		:
		//tells compiler not to optimise memory access
		: "memory"
	);
	
	//gets the insruction pointers address safely
	regs.rip = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
	return regs;
}
#endif
}
