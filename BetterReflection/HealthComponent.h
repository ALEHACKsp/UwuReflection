#pragma once
#include <cstdint>
#include "Game/AvBattleCharaParty.h"
#include "Game/AvBtlChara.h"
class HealthComponent
{
public:
	AvBattleCharaParty::AvBtlChara BattleChar; //0x0000 
	char pad_0x0008[0x6C]; //0x0008
	__int32 CurrentHP; //0x0070 
	__int32 CurrentMP; //0x0074 
	__int32 MaxHP; //0x0078 
	__int32 MaxMP; //0x007C 
	__int32 _unknownint1; //0x0080 
	__int32 _unknownint2; //0x0084 
	__int32 _unknownint3; //0x0088 
	__int32 _unkonwnint4; //0x008C 
	char pad_0x0090[0x8]; //0x0090

}; //Size=0x0098