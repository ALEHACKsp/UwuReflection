#include "pch.h"
#include "stdint.h"
#include "d3d11.h"
#include "DirectXMath.h"
#include "dxgi.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <cstdint>
#include "D3D_VMT_Indices.h"
#include "EntityManager.h"
#include "RotationComponent.h"
#include "RotationContainer.h"
#include "HealthComponent.h"
#include "Game/AvBtlChara.h"
#include "Game/AvBattleCharaParty.h"
#include "Game/CameraComponent.h"
#include "Game/Renderer.h"
#include "detours.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <vector>
#include "Hooking/FunctionDefinitions.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define safe_release(p) if (p) { p->Release(); p = nullptr; }
#define VMT_PRESENT (UINT)IDXGISwapChainVMT::Present
#define SWP_ASYNCWINDOWPOS (INT32)0x4000
#define CameraComponentBaseOffset (INT32)0x01B6FEF8
#define RotationComponentListOffset (INT32)0x0187F2A0
#define RendererBaseOffset (INT32)0x01B9E0F0

std::uintptr_t blueReflectionBase = (std::uintptr_t)GetModuleHandle(L"Blue_Reflection.exe");

HRESULT __stdcall hkPresent(IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags);
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//game functions, replace offsets with sig scans
FunctionDefinitions::fnGetPlayer oGetPlayer = reinterpret_cast<FunctionDefinitions::fnGetPlayer>(blueReflectionBase + 0x76D910);
FunctionDefinitions::fnPrintVector3 oPrintVector3 = reinterpret_cast<FunctionDefinitions::fnPrintVector3>(blueReflectionBase + 0x76DB20);
FunctionDefinitions::fnSetRotationComponentPosition oSetPosition = reinterpret_cast<FunctionDefinitions::fnSetRotationComponentPosition>(blueReflectionBase + 0xAEBE30);
FunctionDefinitions::fnGetComponentByIndex oGetComponentByIndex = reinterpret_cast<FunctionDefinitions::fnGetComponentByIndex>(blueReflectionBase + 0x6B80B0);
FunctionDefinitions::fnGetComponentListSize oGetComponentListSize = reinterpret_cast<FunctionDefinitions::fnGetComponentListSize>(blueReflectionBase + 0x6B8950);

std::uintptr_t _fastcall hkGetPlayer();
std::uintptr_t _fastcall hkGetComponentByIndex(std::uintptr_t* componentListPtr, ULONGLONG componentIndex);

static WNDPROC OriginalWndProcHandler = nullptr;

FunctionDefinitions::fnPresent oPresent;

ID3D11Device* gDevice = nullptr;
IDXGISwapChain* gSwapchain = nullptr;
ID3D11DeviceContext* gContext = nullptr;
ID3D11RenderTargetView* mainRenderTargetView;

HWND gameWindow;
bool gShowMenu = true;
bool gImguiAndDx11Initialized = false;
bool gShouldExitAndCleanup = false;
bool gFreeCam = false;
bool gNoClip = false;
bool gGetPlayerHookCalled = false;
bool gGetComponentByIndexHookCalled = false;

LONGLONG gRotationComponentListSize = 0;

Renderer* _GameRenderer;

std::uintptr_t _RotationComponentBaseAddress = NULL;
std::uintptr_t _HinaHealthComponentBaseAddress = NULL;
std::uintptr_t _CameraComponentBaseAddress = NULL;
std::uintptr_t _RotationComponentListBaseAddress = NULL;
std::uintptr_t _RendererBaseAddress = NULL;

RotationComponent* GetRotationComponent();
HealthComponent* GetHinaHealthComponent();
CameraComponent* GetCameraComponent();
Renderer* GetRenderer();

std::uintptr_t GetRotationComponentListAddress();
std::vector<RotationComponent*> BuildRotationComponentList();

void Unload();

static EntityManager _EntityManager = EntityManager();
std::vector<RotationComponent*> _RotationComponentList = std::vector<RotationComponent*>();

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

bool DummyDeviceGetPresent()
{
	HWND hWnd = GetForegroundWindow();

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
	oPresent = (FunctionDefinitions::fnPresent)(pVMT[(UINT)IDXGISwapChainVMT::Present]);

	safe_release(pDevice);
	safe_release(pSwapchain);

	return true;
}

void RemoveHooks()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	long attach = DetourDetach((PVOID*)(&oGetPlayer), (PVOID)hkGetPlayer);
	attach = DetourDetach((PVOID*)&oGetComponentByIndex, (PVOID)hkGetComponentByIndex);
	DetourTransactionCommit();
}

void UnhookPresent()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	long detach = DetourDetach((PVOID*)(&oPresent), (PVOID)hkPresent);
	DetourTransactionCommit();
}

void PlaceHooks()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	long attach = DetourAttach((PVOID*)(&oGetPlayer), (PVOID)hkGetPlayer);
	attach = DetourAttach((PVOID*)&oGetComponentByIndex, (PVOID)hkGetComponentByIndex);
	DetourTransactionCommit();
}

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

std::uintptr_t _fastcall hkGetPlayer()
{
	gGetPlayerHookCalled = true;

	return oGetPlayer();
}

std::uintptr_t _fastcall hkGetComponentByIndex(std::uintptr_t* componentListPtr, ULONGLONG componentIndex)
{
	ULONGLONG listSize;

	listSize = oGetComponentListSize(componentListPtr);
	gRotationComponentListSize = listSize;
	gGetComponentByIndexHookCalled = true;
	_InterlockedExchange64((LONGLONG*)&_RotationComponentListBaseAddress, (LONGLONG)*componentListPtr);

	return oGetComponentByIndex(componentListPtr, componentIndex);
}

HRESULT _stdcall hkPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags)
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
		//std::uintptr_t getPlayerResult = oGetPlayer();
		if (gGetPlayerHookCalled) //|| getPlayerResult != NULL)
		{
			std::string playerHookStatus = "GetPlayer hooked: " + std::to_string(gGetPlayerHookCalled);
			ImGui::Text(playerHookStatus.c_str());
		}
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
		if (_EntityManager.CameraComponent && _EntityManager.RotationComponent)
		{
			ImGui::Checkbox("FreeCam", &gFreeCam);
			if (gFreeCam)
			{
			}

			ImGui::Checkbox("NoClip", &gNoClip);
			if (gNoClip)
			{
				if (GetAsyncKeyState(0x057) & 0x01)
				{
					_EntityManager.RotationComponent->X += 50.0f;
				}

				if (GetAsyncKeyState(0x44) & 0x01)
				{
					_EntityManager.RotationComponent->Y += 50.0f;
				}

				if (GetAsyncKeyState(0x053) & 0x01)
				{
					_EntityManager.RotationComponent->X -= 50.0f;
				}

				if (GetAsyncKeyState(0x041) & 0x01)
				{
					_EntityManager.RotationComponent->Y -= 50.0f;
				}

				if (GetAsyncKeyState(0x20) & 0x01)
				{
					_EntityManager.RotationComponent->Z += 50.0f;
				}
			}

			if (_GameRenderer)
			{
				std::stringstream sstream;
				sstream << "Renderer Base: " << std::hex << _RendererBaseAddress;
				std::string rendString = sstream.str();
				ImGui::Text(rendString.c_str());
				std::string componentHookCalled = "Component Hook Called: " + std::to_string(gGetComponentByIndexHookCalled);
				ImGui::Text(componentHookCalled.c_str());
				DirectX::XMMATRIX viewMatrix;
				DirectX::XMMATRIX projMatrix;
				DirectX::XMMATRIX worldMatrix;

				worldMatrix = DirectX::XMMatrixIdentity();
				projMatrix = _GameRenderer->ProjectionMatrix;	//*(DirectX::XMMATRIX*)(0x026892730);
				viewMatrix = _GameRenderer->ViewMatrix;	//*(DirectX::XMMATRIX*)(0x0268926B0);

				viewMatrix = DirectX::XMMatrixMultiply(worldMatrix, viewMatrix);

				if (!DirectX::XMMatrixIsNaN(viewMatrix) || !DirectX::XMMatrixIsInfinite(viewMatrix))
				{
					DirectX::XMFLOAT4X4 sVm;
					DirectX::XMFLOAT4X4 sPm;

					DirectX::XMStoreFloat4x4(&sVm, viewMatrix);
					DirectX::XMStoreFloat4x4(&sPm, projMatrix);

					std::string firstRow = std::to_string(sVm._11) + " " + std::to_string(sVm._12) + " " + std::to_string(sVm._13) + " " + std::to_string(sVm._14);
					std::string secondRow = std::to_string(sVm._21) + " " + std::to_string(sVm._22) + " " + std::to_string(sVm._23) + " " + std::to_string(sVm._24);
					std::string thirdRow = std::to_string(sVm._31) + " " + std::to_string(sVm._32) + " " + std::to_string(sVm._33) + " " + std::to_string(sVm._34);
					std::string fourthRow = std::to_string(sVm._41) + " " + std::to_string(sVm._42) + " " + std::to_string(sVm._43) + " " + std::to_string(sVm._44);

					std::string pFirstRow = std::to_string(sPm._11) + " " + std::to_string(sPm._12) + " " + std::to_string(sPm._13) + " " + std::to_string(sPm._14);
					std::string pSecondRow = std::to_string(sPm._21) + " " + std::to_string(sPm._22) + " " + std::to_string(sPm._23) + " " + std::to_string(sPm._24);
					std::string pThirdRow = std::to_string(sPm._31) + " " + std::to_string(sPm._32) + " " + std::to_string(sPm._33) + " " + std::to_string(sPm._34);
					std::string pFourthRow = std::to_string(sPm._41) + " " + std::to_string(sPm._42) + " " + std::to_string(sPm._43) + " " + std::to_string(sPm._44);

					ImGui::Text("VM");
					ImGui::Text(firstRow.c_str());
					ImGui::Text(secondRow.c_str());
					ImGui::Text(thirdRow.c_str());
					ImGui::Text(fourthRow.c_str());

					ImGui::Text("PM");
					ImGui::Text(pFirstRow.c_str());
					ImGui::Text(pSecondRow.c_str());
					ImGui::Text(pThirdRow.c_str());
					ImGui::Text(pFourthRow.c_str());

					DirectX::XMVECTOR curPos = { _EntityManager.RotationComponent->X, _EntityManager.RotationComponent->Y, _EntityManager.RotationComponent->Z };
					DirectX::XMVECTOR screenPoint = DirectX::XMVector3Project(curPos, 0, 0, 1920, 1200, 0.0f, 1.0f, projMatrix, viewMatrix, worldMatrix);
					DirectX::XMFLOAT3 wts;
					DirectX::XMStoreFloat3(&wts, screenPoint);

					auto drawList = ImGui::GetBackgroundDrawList();
					ImVec4 color = ImVec4(60.0f, -20.0f, 100.0f, 255.0f);
					ImU32 color2 = ImGui::ColorConvertFloat4ToU32(color);

					if (_RotationComponentListBaseAddress && _RotationComponentBaseAddress)
					{
						try
						{
							std::vector<RotationComponent*> rotComponentList = BuildRotationComponentList();

							if (rotComponentList.size() > 0)
							{
								for (int i = 0; i < rotComponentList.size(); i++)
								{
									if (rotComponentList[i])
									{
										if (rotComponentList[i]->X == 0 && rotComponentList[i]->Y == 0 && rotComponentList[i]->Z == 0)
											continue;

										if (!_RotationComponentListBaseAddress)
											break;

										DirectX::XMVECTOR entPos = { rotComponentList[i]->X, rotComponentList[i]->Z, rotComponentList[i]->Y };
										DirectX::XMVECTOR entToScreen = DirectX::XMVector3Project(entPos, 0, 0, 1920, 1200, 0.0f, 1.0f, projMatrix, viewMatrix, worldMatrix);
										DirectX::XMFLOAT4 screenP;
										DirectX::XMStoreFloat4(&screenP, entToScreen);

										if (screenP.x > 0 && screenP.y > 0)
										{
											drawList->AddCircle(ImVec2(screenP.x, screenP.y - 50), 35, color2);
											std::string myPos = "I'm at X: " + std::to_string(rotComponentList[i]->X) + " Y: " + std::to_string(rotComponentList[i]->Y) + " Z: " + std::to_string(rotComponentList[i]->Z);
											drawList->AddText(ImVec2(screenP.x, screenP.y - 100), color2, myPos.c_str());
										}
									}
								}
							}
						}
						catch (...)
						{
							std::cout << "stuff";
						}
					}
				}
			}

			ImGui::Text("Camera Component Base Address");
			std::stringstream sstream;
			sstream << "0x" << std::hex << _CameraComponentBaseAddress;
			std::string camComponentBA = sstream.str();
			ImGui::Text(camComponentBA.c_str());
			std::string camX = "CamX: " + std::to_string(_EntityManager.CameraComponent->CameraXOrigin);
			std::string camZ = "CamZ: " + std::to_string(_EntityManager.CameraComponent->CameraZOrigin);
			std::string camY = "CamY: " + std::to_string(_EntityManager.CameraComponent->CameraYOrigin);
			std::string lookatX = "LookAtX: " + std::to_string(_EntityManager.CameraComponent->CameraXLookAt);
			std::string lookatZ = "LookAtZ: " + std::to_string(_EntityManager.CameraComponent->CameraZLookAt);
			std::string lookatY = "LookAtY: " + std::to_string(_EntityManager.CameraComponent->CameraYLookAt);
			ImGui::Text("Origin?");
			ImGui::Text(camX.c_str());
			ImGui::Text(camZ.c_str());
			ImGui::Text(camY.c_str());
			ImGui::Text("Looking at?");
			ImGui::Text(lookatX.c_str());
			ImGui::Text(lookatZ.c_str());
			ImGui::Text(lookatY.c_str());
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

		if (_RotationComponentListBaseAddress)
		{
			std::stringstream sstream;

			sstream << "Rotation ComponentList Base Address: 0x" << std::hex << _RotationComponentListBaseAddress;
			std::string adrString = sstream.str();
			ImGui::Text(adrString.c_str());
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

		if (_RotationComponentListBaseAddress && _RotationComponentBaseAddress && gRotationComponentListSize > 0)
		{
			ImGui::Begin("Rotation Component List");
			std::vector<RotationComponent*> rotComponentList = BuildRotationComponentList();
			if (!_RotationComponentBaseAddress)
			{
				rotComponentList = std::vector<RotationComponent*>();
			}

			for (int i = 0; i < rotComponentList.size(); i++)
			{
				if (rotComponentList[i])
				{
					std::stringstream sstream;

					sstream << "0x" << std::hex << (std::uintptr_t)(rotComponentList[i]);
					try
					{
						if ((std::uintptr_t)(rotComponentList[i]) != 0xFFFFFFFFFFFFFFFF && (std::uintptr_t)(rotComponentList[i]) != NULL)
						{
							if (!_RotationComponentBaseAddress)
							{
								break;
							}
							std::string componentAddress = "Component Address: " + sstream.str();
							if ((std::uintptr_t)rotComponentList[i] > 0x00000000A0000000 || (std::uintptr_t)rotComponentList[i] < 0x0000000020000000)
							{
								rotComponentList.clear();
								break;
							}
							std::string componentPosition = "X: " + std::to_string(rotComponentList[i]->X) + " Y: " + std::to_string(rotComponentList[i]->Y) + " Z: " + std::to_string(rotComponentList[i]->Z);

							ImGui::Text(componentAddress.c_str());
							ImGui::Text(componentPosition.c_str());
						}
					}
					catch (...)
					{
						rotComponentList = std::vector<RotationComponent*>();
						break;
					}
				}
			}

			ImGui::End();
		}

		ImGui::EndFrame();
		ImGui::Render();
		gContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	return oPresent(pThis, syncInterval, flags);
}

std::vector<RotationComponent*> BuildRotationComponentList()
{
	std::vector<RotationComponent*> rotComponentVector = std::vector<RotationComponent*>();

	std::uintptr_t rotComponentListBase = _RotationComponentListBaseAddress;

	if (!rotComponentListBase)
		return std::vector<RotationComponent*>();

	int32_t componentCount = (int32_t)gRotationComponentListSize;

	if (componentCount == 0)
		return std::vector<RotationComponent*>();

	for (int i = 0; i < componentCount; i++)
	{
		if (_RotationComponentListBaseAddress)
		{
			RotationContainer* rotationContainer = (RotationContainer*)(*(std::uintptr_t*)(rotComponentListBase + (i * 8)));
			try
			{
				if ((uintptr_t)rotationContainer > 0x000000000001000000)//terrible check, occasionally you'll get an object with an addr of like 0x00000000000021 while changing map
				{
					if (rotationContainer->rotComponent);
					{
						rotComponentVector.push_back(rotationContainer->rotComponent);
					}
				}
			}
			catch (...)
			{
				_RotationComponentListBaseAddress = NULL;
				break;
			}
		}
		else
			return std::vector<RotationComponent*>();
	}

	return rotComponentVector;
}

std::uintptr_t GetRotationComponentListAddress()
{
	std::uintptr_t rotationComponentListBase = *(std::uintptr_t*)(blueReflectionBase + RotationComponentListOffset);

	if (!rotationComponentListBase)
		return NULL;

	rotationComponentListBase = *(std::uintptr_t*)(rotationComponentListBase + 0x20);

	if (!rotationComponentListBase)
		return NULL;

	rotationComponentListBase = *(std::uintptr_t*)(rotationComponentListBase + 0x58);

	if (!rotationComponentListBase)
		return NULL;

	rotationComponentListBase = *(std::uintptr_t*)(rotationComponentListBase + 0x08);

	if (!rotationComponentListBase)
		return NULL;

	rotationComponentListBase = *(std::uintptr_t*)(rotationComponentListBase + 0x08);

	if (!rotationComponentListBase)
		return NULL;

	rotationComponentListBase = *(std::uintptr_t*)(rotationComponentListBase + 0x040);

	if (!rotationComponentListBase)
		return NULL;

	rotationComponentListBase = *(std::uintptr_t*)(rotationComponentListBase + 0x0);

	if (!rotationComponentListBase)
		return NULL;

	rotationComponentListBase += 0x20;
	_RotationComponentListBaseAddress = rotationComponentListBase;
	return rotationComponentListBase;
}

Renderer* GetRenderer()
{
	std::uintptr_t rendererBase = *(std::uintptr_t*)(blueReflectionBase + RendererBaseOffset);

	if (!rendererBase)
		return NULL;

	rendererBase = *(std::uintptr_t*)(rendererBase + 0x0);

	if (!rendererBase)
		return NULL;

	rendererBase = *(std::uintptr_t*)(rendererBase + 0x218);

	if (!rendererBase)
		return NULL;

	rendererBase = *(std::uintptr_t*)(rendererBase + 0x058);

	if (!rendererBase)
		return NULL;

	rendererBase = *(std::uintptr_t*)(rendererBase + 0x030);

	if (!rendererBase)
		return NULL;

	rendererBase = *(std::uintptr_t*)(rendererBase + 0x0);

	if (!rendererBase)
		return NULL;

	rendererBase = *(std::uintptr_t*)(rendererBase + 0x80);

	if (!rendererBase)
		return NULL;

	Renderer* renderer = (Renderer*)rendererBase;

	_RendererBaseAddress = rendererBase;
	return renderer;
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
	RemoveHooks();
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
	PlaceHooks();

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
		_GameRenderer = GetRenderer();
		//GetRotationComponentListAddress();
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