#pragma once
#include <cstdint>
class RotationComponent
{
public:
	std::uintptr_t* unkPtr; //0x0000 
	__int8 flag1; //0x0008 
	char pad_0x0009[0x7]; //0x0009
	float VelocityX; //0x0010 
	float VelocityZ; //0x0014 
	float VelocityY; //0x0018 
	float VelocityW; //0x001C 
	char pad_0x0020[0x8]; //0x0020
	float X; //0x0028 
	float Z; //0x002C 
	float Y; //0x0030 
	float W; //0x0034 
	char pad_0x0038[0x88]; //0x0038


}; //Size=0x0640
