#pragma once
#include <cstdint>
class RotationComponent
{
public:
	std::uintptr_t* unknownPtr; //0x0000 --this may be the rotation manager?
	char pad_0x0008[0x8]; //0x0008
	float VelocityX; //0x0010 
	float VelocityZ; //0x0014 
	float VelocityY; //0x0018 
	char pad_0x001C[0xC]; //0x001C
	float X; //0x0028 
	float Z; //0x002C 
	float Y; //0x0030 
	float W; //0x0034 
	double N00000011; //0x0038 
	char pad_0x0040[0x70]; //0x0040
	__int8 N0000002E; //0x00B0 
	char pad_0x00B1[0x7]; //0x00B1
	float N0000002F; //0x00B8 these 4 floats are somehow related to rotation
	float N00000041; //0x00BC 
	float N00000030; //0x00C0 
	float N00000044; //0x00C4 
	char pad_0x00C8[0x578]; //0x00C8

}; //Size=0x0640
