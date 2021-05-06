#pragma once
#include "DirectXMath.h"
class Renderer
{
public:
	std::uintptr_t* vTable; //0x0000 
	char pad_0x0008[0x218]; //0x0008
	DirectX::XMVECTOR CameraOrigin; //0x0220 
	DirectX::XMVECTOR CameraLookAt; //0x0230 
	char pad_0x0240[0x10]; //0x0240
	DirectX::XMMATRIX ViewMatrix; //0x0250 
	DirectX::XMMATRIX ViewMatrix2; //0x0290 
	DirectX::XMMATRIX ProjectionMatrix; //0x02D0 
	DirectX::XMMATRIX WorldViewProjectionMatrix; //0x0310 

}; //Size=0x0E80