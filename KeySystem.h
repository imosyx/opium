#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <fstream>
#include "Utils.h"

namespace KeySystem {

    inline bool g_authenticated = false;
    inline bool g_checking = false;
    inline bool g_checkFailed = false;
    inline bool g_autoVerified = false;
    inline char g_keyInput[128] = {};
    inline std::string g_statusMsg = "";


    // Scattered around so no single search hits the full URL or your name
    static const char* _proto = "ht" "tp";           // "http"
    static const char  _colon = ':';
    static const char* _slashes = "//";              // "//"
    static const char* _sub = "hidden-wildflower-4db0.";
    static const char  _name[] = { 'a','l','e','x','e','x','u','m','2','1','.',0 }; // alexexum21.
    static const char* _host = "workers" "." "dev";  // "workers.dev"
    static const char* _path = "/validate";

    static const char _s1[] = { 'S','D','A','O','I','J',0 };
    static const char _s2[] = { 'S','O','D','I','2','3',0 };
    static const char _s3[] = { '4','8','2','3','0','9',0 };
    static const char _s4[] = { '4','3','2','o','S','D',0 };
    static const char _s5[] = { 'H','O','A','S',0 };

    static std::string BuildApiUrl() {
        std::string url;
        url.reserve(64);
        url += _proto;
        url += _colon;
        url += _slashes;
        url += _sub;
        url += _name;
        url += _host;
        url += _path;
        return url;
    }

    static std::string BuildSecret() {
        std::string sec;
        sec.reserve(32);
        sec += _s1; sec += _s2; sec += _s3; sec += _s4; sec += _s5;
        return sec;
    }
    // -----------------------------

    // get the DLL's directory path
    static std::string GetDllDirectory() {
        char dllPath[MAX_PATH] = {};
        HMODULE hModule = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)GetDllDirectory, &hModule)) {
            GetModuleFileNameA(hModule, dllPath, MAX_PATH);
            std::string path(dllPath);
            size_t lastSlash = path.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                return path.substr(0, lastSlash + 1);
            }
        }
        return "";
    }

    // read key from key.txt in DLL folder
    static std::string ReadKeyFromFile() {
        std::string dllDir = GetDllDirectory();
        if (dllDir.empty()) return "";

        std::string keyPath = dllDir + "key.txt";
        std::ifstream file(keyPath);
        if (!file.is_open()) return "";

        std::string key;
        std::getline(file, key);
        size_t start = key.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = key.find_last_not_of(" \t\r\n");
        key = key.substr(start, end - start + 1);
        return key;
    }

    static DWORD WINAPI ValidateKeyThread(LPVOID param) {
        std::string key = *(std::string*)param;
        delete (std::string*)param;

        std::string apiUrl = BuildApiUrl();
        std::string clientSecret = BuildSecret();
        std::string body = "{\"key\":\"" + key + "\"}";

        URL_COMPONENTS uc = {};
        uc.dwStructSize = sizeof(uc);
        wchar_t host[256] = {}, path[256] = {};
        uc.lpszHostName = host; uc.dwHostNameLength = 256;
        uc.lpszUrlPath = path; uc.dwUrlPathLength = 256;

        wchar_t wurl[512];
        MultiByteToWideChar(CP_UTF8, 0, apiUrl.c_str(), -1, wurl, 512);
        WinHttpCrackUrl(wurl, 0, 0, &uc);

        HINTERNET hSession = WinHttpOpen(L"OPIUM/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
        if (!hSession) { g_statusMsg = "network error"; g_checking = false; g_checkFailed = true; return 0; }

        HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); g_statusMsg = "connect failed"; g_checking = false; g_checkFailed = true; return 0; }

        DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path, NULL, NULL, NULL, flags);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); g_statusMsg = "request error"; g_checking = false; g_checkFailed = true; return 0; }

        std::wstring headers = L"Content-Type: application/json\r\nx-client-secret: ";
        wchar_t wsecret[128];
        MultiByteToWideChar(CP_UTF8, 0, clientSecret.c_str(), -1, wsecret, 128);
        headers += wsecret;

        WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)headers.size(), WINHTTP_ADDREQ_FLAG_ADD);

        BOOL sent = WinHttpSendRequest(hRequest, NULL, 0, (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
        if (!sent) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); g_statusMsg = "send failed"; g_checking = false; g_checkFailed = true; return 0; }

        WinHttpReceiveResponse(hRequest, NULL);

        std::string response;
        DWORD bytesRead = 0;
        char buf[1024];
        while (WinHttpReadData(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
            buf[bytesRead] = 0;
            response += buf;
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (response.find("\"valid\":true") != std::string::npos) {
            g_authenticated = true;
            g_statusMsg = "authenticated";
        }
        else {
            g_checkFailed = true;
            g_statusMsg = "invalid key";
        }

        g_checking = false;
        return 0;
    }

    static void SubmitKey(const std::string& key = "") {
        if (g_checking) return;
        std::string k = key.empty() ? std::string(g_keyInput) : key;
        if (k.empty()) return;

        g_checking = true;
        g_checkFailed = false;
        g_statusMsg = "checking...";

        CreateThread(nullptr, 0, ValidateKeyThread, new std::string(k), 0, nullptr);
    }

    static void TryAutoVerify() {
        if (g_authenticated || g_checking) return;

        std::string key = ReadKeyFromFile();
        if (!key.empty()) {
            g_autoVerified = true;
            strncpy_s(g_keyInput, sizeof(g_keyInput), key.c_str(), _TRUNCATE);
            SubmitKey(key);
        }
    }

    static bool DrawKeyScreen() {
        static bool autoVerifyAttempted = false;
        if (!autoVerifyAttempted) {
            autoVerifyAttempted = true;
            TryAutoVerify();
        }

        if (g_authenticated) return false;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(250, 300), ImGuiCond_Always);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, currentSidebarColor);
        ImGui::Begin("##keyscreen", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse);

        ImGui::SetCursorPosX((380 - ImGui::CalcTextSize("OPIUM v4").x) * 0.5f);
        ImGui::TextColored(ImVec4(0.74f, 0.58f, 0.98f, 1.0f), "OPIUM v4");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Enter your key:");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(-1);
        bool enterPressed = ImGui::InputText("##keyinput", g_keyInput, sizeof(g_keyInput),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_Password);

        ImGui::Spacing();

        if (!g_statusMsg.empty()) {
            ImVec4 col = g_checkFailed
                ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f)
                : g_checking
                ? ImVec4(0.9f, 0.9f, 0.3f, 1.0f)
                : ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
            ImGui::TextColored(col, "%s", g_statusMsg.c_str());
        }

        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 36);

        bool disabled = g_checking;
        if (disabled) ImGui::BeginDisabled();

        if (ImGui::Button(g_checking ? "Checking..." : "Authenticate", ImVec2(-1, 26)) || enterPressed) {
            SubmitKey();
        }

        if (disabled) ImGui::EndDisabled();

        ImGui::PopStyleColor();
        ImGui::End();

        return true;
    }
}