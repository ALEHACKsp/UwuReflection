#pragma once
#include "d3dx11.h"
#include <cstdint>
#include "RotationComponent.h"
#include "DirectXMath.h"
static class FunctionDefinitions
{
public:
	HRESULT __stdcall hkPresent(IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags);
	using fnPresent = HRESULT(__stdcall*)(IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags);
	LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	using fnGetPlayer = std::uintptr_t(_fastcall*)();
	using fnPrintVector3 = void(_fastcall*)();
	using fnSetRotationComponentPosition = void(_fastcall*)(RotationComponent* component, DirectX::XMFLOAT4* newPosition);
	using fnGetComponentByIndex = std::uintptr_t(_fastcall*)(std::uintptr_t* componentListPtr, ULONGLONG componentIndex);
	using fnGetComponentListSize = LONGLONG(_fastcall*)(std::uintptr_t* componentListPtr);
};