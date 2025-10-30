#include <cstdlib>
#include <gl/GL.h>
#include <mutex>
#include <pch.hpp>
#include <shared_mutex>

struct WGL_WindowData {
    HDC hDC;
};

static HGLRC g_hRC;
static WGL_WindowData g_MainWindow;
static int g_Width;
static int g_Height;

bool CreateDeviceWGL(HWND hWnd, WGL_WindowData *data);
void CleanupDeviceWGL(HWND hWnd, WGL_WindowData *data);
void ResetDeviceWGL();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool url_modal_opened = false, settings_modal_opened = false;
bool should_open_url_modal = false;
bool download_track = false;

char track_link[256] = "https://soundcloud.com/artist/song-name";
int selected_song = 0;

std::shared_mutex dl_mutex;
std::deque<std::shared_ptr<sc_mgr::downloader>> track_list = {};

void download_song_thread() {
    while (true) {
        if (download_track) {
            auto &t = track_list.back();

            t->download();

            download_track = false;
        }
    }
}

uint64_t last_tick = 0ull;
bool should_update_track_info = false, updated_track_info = false;

int main(int, char **) {
    std::jthread t1(std::move(download_song_thread));
    sc_mgr::downloader track;

    ImVec2 window_size = ImVec2(350, 300);

    ImGui_ImplWin32_EnableDpiAwareness();
    const WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Standalone", nullptr};
    ::RegisterClassEx(&wc);
    const HWND hwnd = ::CreateWindow(wc.lpszClassName, L"ImGui Standalone", WS_OVERLAPPEDWINDOW, 100, 100, window_size.x, window_size.y, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceWGL(hwnd, &g_MainWindow)) {
        CleanupDeviceWGL(hwnd, &g_MainWindow);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    wglMakeCurrent(g_MainWindow.hDC, g_hRC);

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();
    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init();

    RECT rect;

    GetWindowRect(hwnd, &rect);

    long width = rect.right - rect.left;
    long height = rect.bottom - rect.top;

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    bool done = false;
    while (!done) {
        //std::unique_lock<std::shared_mutex> lock(dl_mutex);

        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

        if (ImGui::Begin("download soundcloud music halal 2k25", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("Menu")) {
                    if (ImGui::MenuItem("Download from URL"))
                        url_modal_opened = true;

                    if (ImGui::MenuItem("Settings"))
                        settings_modal_opened = true;

                    ImGui::Separator();

                    if (ImGui::MenuItem("Exit"))
                        exit(0);

                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            if (ImGui::Button("Download"))
                url_modal_opened = true;

            ImGui::Separator();

            if (!track_list.empty()) {
                int it = 0;

                for (const auto &s : track_list) {
                    const bool track_finished = s->get_progress() == 1.0f;
                    const auto additional_cursor_pos = track_finished ? 0.0f : 20.0f;

                    if (ImGui::Selectable(fmt::format("##{}", it).c_str(), it == selected_song, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0.0f, 40.0f + additional_cursor_pos)))
                        selected_song = it;

                    const auto cursor_pos = ImGui::GetCursorPos();

                    ImGui::SetCursorPos(ImVec2(cursor_pos.x, cursor_pos.y - 40.0f - additional_cursor_pos));
                    ImGui::Image(ImTextureID((intptr_t) s->artwork_tex), s->artwork_size);
                    ImGui::SameLine();
                    ImGui::Text(s->artist_name.c_str());
                    ImGui::Text(s->title.c_str());

                    if (!track_finished) ImGui::ProgressBar(s->get_progress());

                    ImGui::SetCursorPos(cursor_pos);
                    ImGui::Separator();

                    it++;
                }
            }
        }
        ImGui::End();

        if (url_modal_opened) {
            ImGui::OpenPopup("Enter link##url_modal");
            url_modal_opened = false;
        }

        if (settings_modal_opened) {
            ImGui::OpenPopup("Settings");
            settings_modal_opened = false;
        }

        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Client ID");
            ImGui::InputText("##client_id", sc_mgr::client_id.data(), 100);

            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Enter link##url_modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Input song URL:");

            bool changed = ImGui::InputText("URL", track_link, IM_ARRAYSIZE(track_link));

            {
                // Request update for track info once when finished typing.
                if ((GetTickCount64() - last_tick) > 400ull && should_update_track_info) {
                    updated_track_info = track.get_track_info(track_link);
                    should_update_track_info = false;
                }

                if (updated_track_info) {
                    ImGui::SeparatorText("Track info");

                    ImGui::InputText("Artist", track.artist_name.data(), 50);
                    ImGui::InputText("Track name", track.title.data(), 50);
                }

                // Update last tick.
                if (changed) {
                    should_update_track_info = true;
                    last_tick = GetTickCount64();
                }
            }

            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();

            ImGui::SameLine();

            if (ImGui::Button("Download")) {
                track_list.emplace_back(std::make_shared<sc_mgr::downloader>(track));

                download_track = true;

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::Render();
        glViewport(0, 0, g_Width, g_Height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ::SwapBuffers(g_MainWindow.hDC);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceWGL(hwnd, &g_MainWindow);
    wglDeleteContext(g_hRC);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

bool CreateDeviceWGL(HWND hWnd, WGL_WindowData *data) {
    HDC hDc = ::GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(hDc, &pfd);
    if (pf == 0)
        return false;
    if (::SetPixelFormat(hDc, pf, &pfd) == FALSE)
        return false;
    ::ReleaseDC(hWnd, hDc);

    data->hDC = ::GetDC(hWnd);
    if (!g_hRC)
        g_hRC = wglCreateContext(data->hDC);
    return true;
}

void CleanupDeviceWGL(HWND hWnd, WGL_WindowData *data) {
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hWnd, data->hDC);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                g_Width = LOWORD(lParam);
                g_Height = HIWORD(lParam);
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}