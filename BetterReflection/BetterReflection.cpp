#include "pch.h"
#include "stdint.h"
#include "d3d11.h"
#include "dxgi.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <cstdint>
#include "D3D_VMT_Indices.h"
#include "EntityManager.h"
#include "RotationComponent.h"
#include "HealthComponent.h"
#include "Game/AvBtlChara.h"
#include "Game/AvBattleCharaParty.h"
#include "Game/CameraComponent.h"
#include "detours.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define safe_release(p) if (p) { p->Release(); p = nullptr; } 
#define VMT_PRESENT (UINT)IDXGISwapChainVMT::Present
#define SWP_ASYNCWINDOWPOS (INT32)0x4000
#define CameraComponentBaseOffset (INT32)0x01B6FEF8 

HRESULT __stdcall hkPresent(IDXGISwapChain * pThis, UINT SyncInterval, UINT Flags);
using fnPresent = HRESULT(__stdcall*)(IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags);
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static WNDPROC OriginalWndProcHandler = nullptr;

fnPresent oPresent;

ID3D11Device* gDevice = nullptr;
IDXGISwapChain* gSwapchain = nullptr;
ID3D11DeviceContext* gContext = nullptr;
ID3D11RenderTargetView* mainRenderTargetView;

HWND gameWindow;
bool gShowMenu = true;
bool gImguiAndDx11Initialized = false;
bool gShouldExitAndCleanup = false;

std::uintptr_t blueReflectionBase = (std::uintptr_t)GetModuleHandle(L"Blue_Reflection.exe");
std::uintptr_t _RotationComponentBaseAddress = NULL;
std::uintptr_t _HinaHealthComponentBaseAddress = NULL;
std::uintptr_t _CameraComponentBaseAddress = NULL;


RotationComponent* GetRotationComponent();
HealthComponent* GetHinaHealthComponent();
CameraComponent* GetCameraComponent();
void Unload();

static EntityManager _EntityManager = EntityManager();

LRESULT CALLBACK hWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();
	POINT mPos;
	GetCursorPos(&mPos);
	ScreenToClient(gameWindow, &mPos);
	ImGui::GetIO().MousePos.x = mPos.x;
	ImGui::GetIO().MousePos.y = mPos.y;

	if (uMsg == WM_KEYUP)
	{
		if (wParam == VK_DELETE)
		{
			gShowMenu = !gShowMenu;
		}

	}

	if (gShowMenu)
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		return true;
	}

	return CallWindowProc(OriginalWndProcHandler, hWnd, uMsg, wParam, lParam);
}

bool GetDeviceAndContext(IDXGISwapChain* pSwapchain)
{
	HRESULT hr = pSwapchain->GetDevice(__uuidof(ID3D11Device), (void**)&gDevice);

	if (FAILED(hr))
	{
		return false;
	}

	gDevice->GetImmediateContext(&gContext);
	gContext->OMGetRenderTargets(1, &mainRenderTargetView, nullptr);

	if (!mainRenderTargetView)
	{
		ID3D11Texture2D* backBuffer = nullptr;

		hr = pSwapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

		if (FAILED(hr))
			return false;

		hr = gDevice->CreateRenderTargetView(backBuffer, nullptr, &mainRenderTargetView);

		backBuffer->Release();

		if (FAILED(hr))
			return false;

		gContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);

	}

	return true;

}

bool ResizeWindow()
{
	/*DXGI_MODE_DESC dxgiMode;
DXGI_RATIONAL dxgiRational;
dxgiRational.Numerator = 120;
dxgiRational.Denominator = 2;
dxgiMode.Width = 1200;
dxgiMode.Height = 1920;
dxgiMode.RefreshRate = dxgiRational;
dxgiMode.Format = DXGI_FORMAT_UNKNOWN;
dxgiMode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
dxgiMode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

HRESULT hr = pThis->ResizeTarget(&dxgiMode);
std::cout << hr << std::endl;*/
	return true;
}

bool DummyDeviceGetPresent()
{
	HWND hWnd = GetForegroundWindow();

	//bool success = SetWindowPos(hWnd, (HWND)0, 0, 0, 1920, 1200, SWP_ASYNCWINDOWPOS);

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	DXGI_SWAP_CHAIN_DESC swapChainDesc;

	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	ID3D11Device* pDevice = nullptr;
	IDXGISwapChain* pSwapchain = nullptr;

	if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, &featureLevel, 1
		, D3D11_SDK_VERSION, &swapChainDesc, &pSwapchain, &pDevice, NULL, nullptr)))
		return false;
	std::cout << "Swapchain and device created: ";
	printf("swapchain %p device %p", pSwapchain, pDevice);


	// Get swapchain vmt
	void** pVMT = *(void***)pSwapchain;
	oPresent = (fnPresent)(pVMT[(UINT)IDXGISwapChainVMT::Present]);


	safe_release(pDevice);
	safe_release(pSwapchain);

	return true;
}

/*void RemoveHooks()
{

}*/


void UnhookPresent()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	long detach = DetourDetach((PVOID*)(&oPresent), (PVOID)hkPresent);
	DetourTransactionCommit();
}


/*void PlaceHooks()
{

}*/

void HookPresent()
{
	if (oPresent)
	{
		std::cout << "Original Present Vtable address: " << std::hex << oPresent << std::endl;
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		long attach = DetourAttach((PVOID*)(&oPresent), (PVOID)hkPresent);
		DetourTransactionCommit();

		std::cout << "Present hooked!" << std::endl;
	}
}


bool RemoveWindowBorder()
{
	return true;
}


HRESULT _stdcall hkPresent(IDXGISwapChain * pThis, UINT syncInterval, UINT flags)
{
	if (!gImguiAndDx11Initialized)
	{
		bool success = GetDeviceAndContext(pThis);
		
		if (success)
		{

			DXGI_SWAP_CHAIN_DESC realDescription;
			pThis->GetDesc(&realDescription);
			gameWindow = realDescription.OutputWindow;
			OriginalWndProcHandler = (WNDPROC)SetWindowLongPtr(gameWindow, GWLP_WNDPROC, (LONG_PTR)hWndProc);

			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			io.DisplaySize = ImVec2(1920, 1200);

			ImGui_ImplWin32_Init(gameWindow);
			ImGui_ImplDX11_Init(gDevice, gContext);
			ImGui::GetIO().ImeWindowHandle = gameWindow;

			gImguiAndDx11Initialized = true;
		}
	}

	if (!gShouldExitAndCleanup)
	{

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
		ImGui::Begin("Debug");
		std::string dxStatus = "Directx hooked/init: " + std::to_string(gImguiAndDx11Initialized);
		ImGui::Text(dxStatus.c_str());
		//fix this stringstream whackness
		std::stringstream sstream2;
		sstream2 << std::hex << oPresent;
		std::string orgPresentAddress = "Original Present Address: 0x" + sstream2.str();
		ImGui::Text(orgPresentAddress.c_str());
		if (ImGui::Button("Unload", ImVec2(100, 20)))
		{
			ImGui::End();
			ImGui::EndFrame();
			//ImGui::Render();
			gShouldExitAndCleanup = true;
			return oPresent(pThis, syncInterval, flags);
		}
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Text("Camera Position");
		if (_EntityManager.CameraComponent)
		{
			ImGui::Text("Camera Component Base Address");
			std::stringstream sstream;
			sstream << "0x" << std::hex << _CameraComponentBaseAddress;
			std::string camComponentBA = sstream.str();
			ImGui::Text(camComponentBA.c_str());
			std::string camX = "CamX: " + std::to_string(_EntityManager.CameraComponent->CameraX);
			std::string camZ = "CamZ: " + std::to_string(_EntityManager.CameraComponent->CameraZ);
			std::string camY = "CamY: " + std::to_string(_EntityManager.CameraComponent->CameraY);
			ImGui::Text(camX.c_str());
			ImGui::Text(camZ.c_str());
			ImGui::Text(camY.c_str());
		}
		ImGui::Text("Position");
		if (_EntityManager.RotationComponent)
		{
			std::stringstream sstream;
			sstream << "0x" << std::hex << _RotationComponentBaseAddress;
			std::string rotComponentHex = sstream.str();
			ImGui::Text("Rotation Component Base Address");
			ImGui::Text(rotComponentHex.c_str());
			std::string xPos = "X: " + std::to_string(_EntityManager.RotationComponent->X);
			ImGui::Text(xPos.c_str());
			std::string zPos = "Z: " + std::to_string(_EntityManager.RotationComponent->Z);
			ImGui::Text(zPos.c_str());
			std::string yPos = "Y: " + std::to_string(_EntityManager.RotationComponent->Y);
			ImGui::Text(yPos.c_str());
		}

		if (_EntityManager.HinaHealthComponent)
		{
			ImGui::Text("Hina Health Component");
			std::stringstream sstream;
			sstream << "0x" << std::hex << _HinaHealthComponentBaseAddress;

			std::string hinaHealthComponentHex = sstream.str();

			ImGui::Text("Base Address");
			ImGui::Text(hinaHealthComponentHex.c_str());
			std::string curHp = "Current HP: " + std::to_string(_EntityManager.HinaHealthComponent->CurrentHP);
			std::string curMp = "Current MP: " + std::to_string(_EntityManager.HinaHealthComponent->CurrentMP);
			std::string maxHp = "Max HP: " + std::to_string(_EntityManager.HinaHealthComponent->MaxHP);
			std::string maxMp = "Max MP: " + std::to_string(_EntityManager.HinaHealthComponent->MaxMP);

			ImGui::Text(curHp.c_str());
			ImGui::Text(curMp.c_str());
			ImGui::Text(maxHp.c_str());
			ImGui::Text(maxMp.c_str());
		

		}

		ImGui::End();

		ImGui::EndFrame();
		ImGui::Render();
		gContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	}

	return oPresent(pThis, syncInterval, flags);
}

CameraComponent* GetCameraComponent()
{
	std::uintptr_t cameraManager = *(std::uintptr_t*)(blueReflectionBase + CameraComponentBaseOffset);

	if (!cameraManager)
		return NULL;

	cameraManager = *(std::uintptr_t*)(cameraManager + 0x0E8);

	if (!cameraManager)
		return NULL;

	cameraManager = *(std::uintptr_t*)(cameraManager + 0x00);

	if (!cameraManager)
		return NULL;

	cameraManager = *(std::uintptr_t*)(cameraManager + 0x028);

	if (!cameraManager)
		return NULL;

	cameraManager = *(std::uintptr_t*)(cameraManager + 0x028);

	if (!cameraManager)
		return NULL;

	cameraManager = *(std::uintptr_t*)(cameraManager + 0x090);

	if (!cameraManager)
		return NULL;

	cameraManager = *(std::uintptr_t*)(cameraManager + 0x010);

	if (!cameraManager)
		return NULL;

	cameraManager += 0x0C0;

	CameraComponent* cameraComponent = (CameraComponent*)(cameraManager);
	_CameraComponentBaseAddress = cameraManager;
	return cameraComponent;
}

HealthComponent* GetHinaHealthComponent()
{

	std::uintptr_t healthManager = *(std::uintptr_t*)(blueReflectionBase + 0x0187F320);

	if (!healthManager)
		return NULL;

	healthManager += 0x08;

	if (!healthManager)
		return NULL;

	healthManager = *(std::uintptr_t*)(healthManager);

	if (!healthManager)
		return NULL;

	healthManager = *(std::uintptr_t*)(healthManager + 0x030);

	if (!healthManager)
		return NULL;

	healthManager = *(std::uintptr_t*)(healthManager + 0x010);

	if (!healthManager)
		return NULL;

	healthManager = *(std::uintptr_t*)(healthManager + 0x038);

	if (!healthManager)
		return NULL;

	healthManager = *(std::uintptr_t*)(healthManager + 0x040);

	if (!healthManager)
		return NULL;

	healthManager = *(std::uintptr_t*)(healthManager + 0x030);

	/*HealthComponent* healthComponent = (HealthComponent*)(*healthManager)*/
	
	_HinaHealthComponentBaseAddress = (healthManager);
	HealthComponent* healthComponent = (HealthComponent*)(healthManager);

	return healthComponent;
}




RotationComponent* GetRotationComponent()
{
	RotationComponent* rotComponent;

	std::uintptr_t rotationManager = *(std::uintptr_t*)(blueReflectionBase + 0x1B6FEE8);

	if (rotationManager != NULL)
	{
		std::cout << "Rotation component base: " << std::hex << blueReflectionBase << std::endl;

		rotationManager = (rotationManager + 0x10);

		//std::cout << "step 1: " << rotationManager << std::endl;

		rotationManager = *(std::uintptr_t*)(rotationManager);

			if (rotationManager != NULL)
			{

			//std::cout << "step 2: " << rotationManager << std::endl;
			rotationManager = *(std::uintptr_t*)(rotationManager + 0x18);
			if (!rotationManager)
				return NULL;

			//std::cout << "step 3: " << rotationManager << std::endl;

			rotationManager = *(std::uintptr_t*)(rotationManager + 0x08);
			if (!rotationManager)
				return NULL;

			//std::cout << "step 4: " << rotationManager << std::endl;

			rotationManager = *(std::uintptr_t*)(rotationManager + 0x088);
			if (!rotationManager)
				return NULL;

			//std::cout << "step 5: " << rotationManager << std::endl;

			rotationManager = *(std::uintptr_t*)(rotationManager + 0x020);
			if (!rotationManager)
				return NULL;

			//std::cout << "step 6: " << rotationManager << std::endl;

			rotationManager = *(std::uintptr_t*)(rotationManager + 0x050);
			if (!rotationManager)
				return NULL;
			//std::cout << "step 7: " << rotationManager << std::endl;

			rotationManager += 0x020;

			//std::cout << "step 8: " << rotationManager << std::endl;

			std::uintptr_t* rotationComponentPtr = (std::uintptr_t*)(rotationManager + 0x08);
			if (!rotationComponentPtr)
				return NULL;

			rotComponent = (RotationComponent*)(*rotationComponentPtr);
			_RotationComponentBaseAddress = (std::uintptr_t)*rotationComponentPtr;

			//std::cout << "Rotation component base: " << rotComponent << std::endl;

			return rotComponent;

		}

	}

	return NULL;
}

void Unload()
{
	HMODULE thisModule = GetModuleHandle(L"BetterReflection.dll");

	UnhookPresent();
	Sleep(100);
	WNDPROC restoreWindProc = (WNDPROC)SetWindowLongPtr(gameWindow, GWLP_WNDPROC, (LONG_PTR)OriginalWndProcHandler);
	Sleep(10);
	
	ImGui::DestroyContext();
	gDevice->Release();
//	gSwapchain->Release();
	gContext->Release();
	//mainRenderTargetView->Release();
	Sleep(10);
	//should check if our module isn't 0(it should never be at this point, but the compiler whines about it)
	FreeLibraryAndExitThread(thisModule, NULL);

}


void Load()
{

	/*AllocConsole();
	FILE* filePtr;

	freopen_s(&filePtr, "CONIN$", "r", stdin);
	freopen_s(&filePtr, "CONOUT$", "w", stdout);
	std::cout << "Console Successfully attached!\n";*/

	DummyDeviceGetPresent();
	Sleep(1000);
	HookPresent();



	while (true)
	{

		if (gShouldExitAndCleanup)
		{
			Unload();
			break;
		}
		
		RotationComponent* rotComponent = GetRotationComponent();
		HealthComponent* hinaHealthComponent = GetHinaHealthComponent();
		CameraComponent* cameraComponent = GetCameraComponent();
		_EntityManager.RotationComponent = rotComponent;
		_EntityManager.HinaHealthComponent = hinaHealthComponent;
		_EntityManager.CameraComponent = cameraComponent;
		if (rotComponent)
		{
			std::cout << "X: " << rotComponent->X << std::endl;
			std::cout << "Z: " << rotComponent->Z << std::endl;
			std::cout << "Y: " << rotComponent->Y << std::endl;
			std::cout << "Rotation Component Base: " << std::hex << (std::uintptr_t)(rotComponent) << std::endl;
		
		}

		Sleep(1);
		
	}

}




