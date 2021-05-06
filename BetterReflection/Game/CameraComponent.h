#pragma once
#include <cstdint>
#include "AVCamera.h"
#include "ktgl.h"
class CameraComponent
{
public:
	AVCamera* AvCamera; //0x0000 
	ktgl::AVCCamera* AVCCamera; //0x0008 
	float CameraXOrigin; //0x0010 
	float CameraZOrigin; //0x0014 
	float CameraYOrigin; //0x0018 
	float CameraWOrigin; //0x001C 
	float CameraXLookAt; //0x0020 same as char position with diff z, since camera always points at chars head in combat zones this seems accurate
	float CameraZLookAt; //0x0024 
	float CameraYLookAt; //0x0028 
	float CameraWLookAt; //0x002C 
	char pad_0x0030[0xD0]; //0x0030

}; //Size=0x0100
