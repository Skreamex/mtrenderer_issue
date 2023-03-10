// Dear ImGui: standalone example application for DirectX 9
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <d3d9.h>
#include <tchar.h>
#include <mutex>
#include <unordered_map>

#pragma comment(lib, "d3d9.lib")

// Data
static LPDIRECT3D9              g_pD3D = NULL;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


namespace Settings {
    bool CopyOnEnd = false;
    bool CopyOnGetScene = false;
    bool AddToDrawList = false;
    bool DrawRect = false; // To cause vertex flicks
    bool TempBufferFix = true; // To cause vertex flicks
}


ImVector<ImVec2>& ImDrawListSharedData::TempBuffer() {

    if (!Settings::TempBufferFix) {
        static ImVector<ImVec2> TB;
        return TB;
    }


    static std::unordered_map<unsigned long, ImVector<ImVec2>*> frame_buffers;
    auto thread_id = GetCurrentThreadId();
    auto it = frame_buffers.find(thread_id);

    if (it != frame_buffers.end()) {
        return *(it->second);
    }
    else {
        auto inst = new ImVector<ImVec2>();
        frame_buffers[thread_id] = inst;
        return *inst;
    }
}


void Copy(ImDrawList* dest, ImDrawList* src) {

   // *dest = *src;
   // return;

    dest->CmdBuffer = src->CmdBuffer;
    dest->IdxBuffer = src->IdxBuffer;
    dest->VtxBuffer = src->VtxBuffer;
    dest->Flags = src->Flags;

    dest->_VtxCurrentIdx = src->_VtxCurrentIdx;
    dest->_Data = src->_Data;
    dest->_OwnerName = src->_OwnerName;
    dest->_VtxWritePtr = src->_VtxWritePtr;
    dest->_IdxWritePtr = src->_IdxWritePtr;
    dest->_ClipRectStack = src->_ClipRectStack;
    dest->_TextureIdStack = src->_TextureIdStack;
    dest->_Path = src->_Path;
    dest->_CmdHeader = src->_CmdHeader;
    dest->_Splitter = src->_Splitter;
    dest->_FringeScale = src->_FringeScale;
}



class CRender {
    ImDrawList* m_draw_list;
    ImDrawList* m_draw_list_act;
    ImDrawList* m_draw_list_rendering;
    

    std::mutex render_mutex;



public:
    CRender() { }

    void Init() {
        m_draw_list = new ImDrawList(ImGui::GetDrawListSharedData());
        m_draw_list_act = new ImDrawList(ImGui::GetDrawListSharedData());
        m_draw_list_rendering = new ImDrawList(ImGui::GetDrawListSharedData());
    }

    inline  ImDrawList* GetList() { return m_draw_list; }

    void Begin() {
        m_draw_list->_ResetForNewFrame();
        m_draw_list->PushTextureID(ImGui::GetIO().Fonts->TexID);
        m_draw_list->PushClipRectFullScreen();
    }
    void End() {
        if (Settings::CopyOnEnd) {
            render_mutex.lock();

            Copy(m_draw_list_act, m_draw_list);

            render_mutex.unlock();
        }
    }

    ImDrawList* GetScene() {
        if (Settings::CopyOnGetScene) {
            if (render_mutex.try_lock()) {

                Copy(m_draw_list_rendering, m_draw_list_act);

                render_mutex.unlock();
            }
        }
        return m_draw_list_rendering;
    }


};

CRender* g_pRender;
DWORD __stdcall SeparatedThread(LPVOID param) {

    float _time = 0.f;
    while (true) {
        
        g_pRender->Begin();

        if (Settings::DrawRect) {
            for (float F = 0.f; F < 200.f; F += 5)
                g_pRender->GetList()->AddRect({ 0, F }, { 100, F + 5 }, ImColor(255, 255, 255));
        }


        g_pRender->End();

        Sleep(0);
    }
}


// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"ImGui Example", NULL };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX9 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    g_pRender = new CRender();
    g_pRender->Init();

   CreateThread(0, 0, SeparatedThread, 0, 0, 0);


    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();




        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Separated Render Controls");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Checkbox("Add data rendered in other thread to main buffer", &Settings::AddToDrawList);      
            ImGui::Checkbox("Copy to mid buffer", &Settings::CopyOnEnd);
            ImGui::Checkbox("Copy to final render buffer", &Settings::CopyOnGetScene);
            ImGui::Checkbox("Draw simple rect to cause flicks", &Settings::DrawRect);
            ImGui::Checkbox("TempBuffer quick fix", &Settings::TempBufferFix);
            ImGui::End();
        }


        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render(g_pRender->GetScene());
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

        // Handle loss of D3D9 device
        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = NULL; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            g_d3dpp.BackBufferWidth = LOWORD(lParam);
            g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
