// ImGui_FullscreenBox.cpp
// Minimal ImGui + DirectX11 example (fullscreen, borderless, transparent)

#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "server.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static ServerCmd serverCmd;

// buffer untuk edit string (panjang 256 cukup)
static char urlBuf[256] = "http://default-url.com";
static char proxyBuf[256] = "127.0.0.1:8080";

// buffer untuk integer
static int connectionAlive = 0;
static int connectionCount = 0;
static int connectionmuch = 0;

static std::string proxyStatus = "Idle";

// Globals
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

// Forward declare
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Window setup
HWND CreateAppWindow(HINSTANCE hInstance, const wchar_t* title)
{
    const wchar_t* className = L"ImGuiOverlayClass";

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc,
        0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
        className, NULL };
    RegisterClassEx(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    DWORD style = WS_POPUP;
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST;

    HWND hWnd = CreateWindowExW(
        exStyle, wc.lpszClassName, title, style,
        0, 0, screenWidth, screenHeight,
        NULL, NULL, hInstance, NULL);

    // Transparansi hitam colorkey
    SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    return hWnd;
}

int main()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND hWnd = CreateAppWindow(hInstance, L"Vortenix");

    if (!hWnd || !CreateDeviceD3D(hWnd))
        return 1;

    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Start frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.1f, 1.0f)); // hampir biru hitam

        // Window ImGui utama
        ImGui::Begin("Vortenix");

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 0, 0, 0.3f));
        if (ImGui::Button("Close This Program"))
        {
            exit(0);
        }
        ImGui::PopStyleColor(1);

        ImGui::Dummy(ImVec2(0.0f, 0.0f));

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float paddingX = 0.0f;
        float paddingY = 0.0f;

        ImGui::BeginChild("BoxChild",
            ImVec2(avail.x - paddingX, avail.y - paddingY),
            true
        );

        ImGui::Text("Server Settings");

        ImGui::InputText("URL", urlBuf, IM_ARRAYSIZE(urlBuf));
        ImGui::SameLine();
        if (ImGui::Button("Update URL")) {
            serverCmd.url = urlBuf;
        }

        ImGui::InputText("Proxy", proxyBuf, IM_ARRAYSIZE(proxyBuf));
        ImGui::SameLine();
        if (ImGui::Button("Update Proxy")) {
            serverCmd.proxy = proxyBuf;
        }

        ImGui::InputInt("Connection Alive", &connectionAlive);
        ImGui::SameLine();
        if (ImGui::Button("Update Alive")) {
            serverCmd.connectionalive = connectionAlive;
        }

        ImGui::InputInt("Connection Count", &connectionCount);
        ImGui::SameLine();
        if (ImGui::Button("Update Count")) {
            serverCmd.connectioncount = connectionCount;
        }
        ImGui::InputInt("Connection Much", &connectionmuch);
        ImGui::SameLine();
        if (ImGui::Button("Update Much")) {
            serverCmd.howmuchconnect = connectionmuch;
        }

        if (ImGui::Button("Generate Proxy")) {
            proxyStatus = "Generating...";
            try {
                serverCmd.generateProxy();
                if (!serverCmd.proxylist.empty()) {
                    proxyStatus = "Success";
                }
                else {
                    proxyStatus = "Failed";
                }
            }
            catch (...) {
                proxyStatus = "Failed";
            }
        }

        ImGui::SameLine();
        ImGui::Text("%s", proxyStatus.c_str());

        if (ImGui::Button("Start")) {
            serverCmd.makeconnection();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            serverCmd.stop();
        }

        ImGui::Separator();
        ImGui::BeginChild("ThreadStatusBox", ImVec2(0, 200), true);

        for (auto& st : serverCmd.threadStatuses) {
            ImGui::Text("Thread %d | Proxy: %s | %s | %d sec",
                st.id, st.proxy.c_str(), st.status.c_str(), st.duration);
        }

        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::End();


        // Render
        ImGui::Render();
        float clear_col[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_col);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    UnregisterClass(L"ImGuiOverlayClass", hInstance);

    return 0;
}

// DX11 helpers
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
        0, levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext)))
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = NULL;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer)
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// WndProc
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam),
                DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
