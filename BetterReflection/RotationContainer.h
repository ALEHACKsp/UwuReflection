#pragma once
#include "RotationComponent.h"
#include "Link.h"
class RotationContainer
{
public:
	Link* ObjectList; //0x0000 
	RotationComponent* rotComponent; //0x0008 
	char pad_0x0010[0x28]; //0x0010

}; //Size=0x0038