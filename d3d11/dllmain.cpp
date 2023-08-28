#include <Windows.h>

#include <d3d11.h>
#include <winuser.h>

#include "../deps/detours/detours.h"
#include "../deps/imgui/imgui.h"
#include "../deps/imgui/imgui_impl_dx11.h"
#include "../deps/imgui/imgui_impl_win32.h"
#include "../deps/imgui/imgui_stdlib.h"

#pragma comment(lib, "d3d11.lib")

#if defined _M_X64 || defined __x86_64__
#pragma comment(lib, "detours.x64.lib")
#elif defined _M_IX86 || defined __i386__
#pragma comment(lib, "detours.x86.lib")
#endif

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

typedef HRESULT(WINAPI *IDXGISwapChainPresent)(IDXGISwapChain *, UINT, UINT);

typedef HRESULT(WINAPI *IDXGISwapChainResizeBuffers)(
    IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT);

bool IsInitialized = false;

WNDPROC OriginalWindowProcedure = NULL;
HWND    OutputWindow            = NULL;

ID3D11Device           *pDevice           = NULL;
ID3D11DeviceContext    *pContext          = NULL;
ID3D11RenderTargetView *pRenderTargetView = NULL;
IDXGISwapChain         *pSwapChain        = NULL;

DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};

IDXGISwapChainPresent       PresentFunc;
IDXGISwapChainResizeBuffers ResizeBuffersFunc;

LRESULT WINAPI NewWindowProcedure(const HWND _hWnd, UINT _uMsg, WPARAM _wParam, LPARAM _lParam) {

    if(ImGui_ImplWin32_WndProcHandler(_hWnd, _uMsg, _wParam, _lParam))
        return true;

    return CallWindowProc(OriginalWindowProcedure, _hWnd, _uMsg, _wParam, _lParam);
}

HRESULT WINAPI NewPresent(IDXGISwapChain *_pSwapChain, UINT _SyncInterval, UINT _Flags) {

    if(!IsInitialized) {

        HRESULT _Result = _pSwapChain->GetDevice(IID_PPV_ARGS(&pDevice));

        if(_Result)
            return PresentFunc(_pSwapChain, _SyncInterval, _Flags);

        pDevice->GetImmediateContext(&pContext);

        _Result = _pSwapChain->GetDesc(&SwapChainDesc);

        if(_Result)
            return PresentFunc(_pSwapChain, _SyncInterval, _Flags);

        OutputWindow = SwapChainDesc.OutputWindow;

        ID3D11Texture2D *_pBackBuffer;

        _Result = _pSwapChain->GetBuffer(0, IID_PPV_ARGS(&_pBackBuffer));

        if(_Result)
            return PresentFunc(_pSwapChain, _SyncInterval, _Flags);

        _Result = pDevice->CreateRenderTargetView(_pBackBuffer, nullptr, &pRenderTargetView);

        if(_Result)
            return PresentFunc(_pSwapChain, _SyncInterval, _Flags);

        _pBackBuffer->Release();

        ImGui::CreateContext();

        ImGuiIO &_Io = ImGui::GetIO();

        ImGui::StyleColorsDark();

        _Io.IniFilename = NULL;

        _Io.Fonts->AddFontFromFileTTF(
            "C:\\windows\\fonts\\simhei.ttf", 20.0f, NULL, _Io.Fonts->GetGlyphRangesChineseFull());

        _Io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

        ImGui_ImplWin32_Init(OutputWindow);

        ImGui_ImplDX11_Init(pDevice, pContext);

        OriginalWindowProcedure =
            (WNDPROC)SetWindowLongPtrA(OutputWindow, GWLP_WNDPROC, (LONG_PTR)NewWindowProcedure);

        IsInitialized = true;
    }

    ImGui_ImplWin32_NewFrame();

    ImGui_ImplDX11_NewFrame();

    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::EndFrame();

    ImGui::Render();

    pContext->OMSetRenderTargets(1, &pRenderTargetView, NULL);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return PresentFunc(_pSwapChain, _SyncInterval, _Flags);
}

HRESULT WINAPI NewResizeBuffers(IDXGISwapChain *_pSwapChain,
                                UINT            _BufferCount,
                                UINT            _Width,
                                UINT            _Height,
                                DXGI_FORMAT     _NewFormat,
                                UINT            _SwapChainFlags) {

    if(pRenderTargetView) {
        pContext->OMSetRenderTargets(0, 0, 0);
        pRenderTargetView->Release();
    }

    HRESULT _Result =
        ResizeBuffersFunc(_pSwapChain, _BufferCount, _Width, _Height, _NewFormat, _SwapChainFlags);

    ID3D11Texture2D *_pBuffer;

    _pSwapChain->GetBuffer(0, IID_PPV_ARGS(&_pBuffer));

    if(pDevice->CreateRenderTargetView(_pBuffer, nullptr, &pRenderTargetView))
        return 0;

    _pBuffer->Release();

    pContext->OMSetRenderTargets(1, &pRenderTargetView, NULL);

    D3D11_VIEWPORT _ViewPort;
    ZeroMemory(&_ViewPort, sizeof(_ViewPort));
    _ViewPort.Width    = (float)_Width;
    _ViewPort.Height   = (float)_Height;
    _ViewPort.MinDepth = 0.0f;
    _ViewPort.MaxDepth = 1.0f;
    _ViewPort.TopLeftX = 0;
    _ViewPort.TopLeftY = 0;

    pContext->RSSetViewports(1, &_ViewPort);

    return _Result;
}

void EnableHook() {

    ZeroMemory(&SwapChainDesc, sizeof(SwapChainDesc));

    SwapChainDesc.BufferDesc.Width                   = NULL;
    SwapChainDesc.BufferDesc.Height                  = NULL;
    SwapChainDesc.BufferDesc.RefreshRate.Numerator   = 60;
    SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    SwapChainDesc.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    SwapChainDesc.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
    SwapChainDesc.SampleDesc.Count                   = 1;
    SwapChainDesc.SampleDesc.Quality                 = 0;
    SwapChainDesc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount                        = 2;
    SwapChainDesc.OutputWindow                       = GetForegroundWindow();
    SwapChainDesc.Windowed                           = true;
    SwapChainDesc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDesc.Flags                              = NULL;

    if(D3D11CreateDeviceAndSwapChain(NULL,
                                     D3D_DRIVER_TYPE_HARDWARE,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     D3D11_SDK_VERSION,
                                     &SwapChainDesc,
                                     &pSwapChain,
                                     &pDevice,
                                     NULL,
                                     NULL))
        return;

    void **pp_SwapChainVTable = *reinterpret_cast<void ***>(pSwapChain);

    PresentFunc = (IDXGISwapChainPresent)pp_SwapChainVTable[8];

    ResizeBuffersFunc = (IDXGISwapChainResizeBuffers)pp_SwapChainVTable[13];

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID &)PresentFunc, (void *)NewPresent);
    DetourAttach(&(PVOID &)ResizeBuffersFunc, (void *)NewResizeBuffers);
    DetourTransactionCommit();
}

void DisableHook() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID &)PresentFunc, (void *)NewPresent);
    DetourDetach(&(PVOID &)ResizeBuffersFunc, (void *)NewResizeBuffers);
    DetourTransactionCommit();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    pRenderTargetView->Release();
    pSwapChain->Release();
    pContext->Release();
    pDevice->Release();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {

    if(ul_reason_for_call == 1) {
        CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)(void *)EnableHook, NULL, NULL, NULL);
    } else if(ul_reason_for_call == 0) {
        CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)(void *)DisableHook, NULL, NULL, NULL);
    }
    return 1;
}
