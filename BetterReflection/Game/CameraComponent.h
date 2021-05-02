#pragma once
#include <cstdint>
#include "AVCamera.h"
#include "ktgl.h"
class CameraComponent
{
public:
	AVCamera* AvCamera; //0x0000 
	ktgl::AVCCamera* AVCCamera; //0x0008 
	float CameraX; //0x0010 
	float CameraZ; //0x0014 
	float CameraY; //0x0018 
	float CameraW; //0x001C 
	float CameraXTwo; //0x0020 
	float CameraZTwo; //0x0024 
	float CameraYTwo; //0x0028 
	float CameraWTwo; //0x002C 
	char pad_0x0030[0xD0]; //0x0030

}; //Size=0x0100
