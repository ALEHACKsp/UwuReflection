#pragma once
#include "RotationComponent.h"
#include "HealthComponent.h"
#include "Game/CameraComponent.h"
class EntityManager
{
public:
	RotationComponent* RotationComponent;
	HealthComponent* HinaHealthComponent;
	CameraComponent* CameraComponent;

};