#pragma once
#include <cstdint>
class Link
{
public:
	std::uintptr_t* UnknownPtr; //0x0000 some kind of next - linked list?
	char pad_0x0008[0x8]; //0x0008
	std::uintptr_t* UnknownPtr2; //0x0010 looks like previous, entries are 0x50 - what are these? 

}; //Size=0x0018