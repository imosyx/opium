#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <functional>
#include <vector>
#include "Icons.h"
#include <cmath>

#include <string>


static bool g_menuWasOpen = false;
static bool g_customCursorEnabled = false;
static RECT g_savedClipRect = {};
static bool g_hadClipRect = false;
inline bool enableSnowEffect = false;


enum class MenuCursor { Arrow, Hand, Crosshair, Diamond };
static MenuCursor g_cursorStyle = MenuCursor::Arrow;

//hooking

// Structure to hold hook configuration
struct HookConfig {
    const char* name;
    uintptr_t offset;
    void* hookFunction;
    void** originalFunction;
};

// this to resolve function pointer with fallback
template<typename T>
T ResolveFunctionPointer(uintptr_t baseAddr, const char* className, const char* methodName,
    uintptr_t fallbackOffset, const char* friendlyName) {
    T functionPtr = nullptr;

    // Try dynamic resolution first
    try {
        auto targetClass = IL2CPP::Class::Find(className);
        if (targetClass) {
            auto method = IL2CPP::Class::Utils::GetMethodPointer(targetClass, methodName);
            if (method) {
                functionPtr = (T)method;
                printf("[INFO] Found %s via resolver: 0x%llX\n", friendlyName, (uintptr_t)method);
                return functionPtr;
            }
        }
    }
    catch (...) {
        printf("[INFO] Dynamic resolution failed for %s, using static address\n", friendlyName);
    }

    // Fallback to static address
    uintptr_t staticAddr = baseAddr + fallbackOffset;
    functionPtr = (T)staticAddr;
    printf("[INFO] Using static %s address: 0x%llX\n", friendlyName, staticAddr);

    return functionPtr;
}

bool SetupHook(uintptr_t addr, void* hook, void** original, const char* name) {
    if (MH_CreateHook((void*)addr, hook, original) != MH_OK) {
        printf("[!] couldn't hook %s\n", name);
        return false;
    }
    if (MH_EnableHook((void*)addr) != MH_OK) {
        printf("[!] failed enabling %s\n", name);
        return false;
    }
    // Success - no log needed
    return true;
}

template<typename T>
void SafeWrite(uintptr_t addr, T value) {
    if (addr == 0) return;
    DWORD oldProtect;
    if (VirtualProtect((LPVOID)addr, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        *(T*)addr = value;
        VirtualProtect((LPVOID)addr, sizeof(T), oldProtect, &oldProtect);
    }
}

template<typename T>
void UpdateGameValue(const char* className, const char* fieldName, T value) {
    auto offset = IL2CPP::Class::Utils::GetFieldOffset(className, fieldName);
    auto instances = Unity::Object::FindObjectsOfType<Unity::CGameObject>(className);

    if (instances && instances->m_uMaxLength > 0) {
        for (unsigned int i = 0; i < instances->m_uMaxLength; i++) {
            Unity::CGameObject* obj = instances->operator[](i);
            if (obj == nullptr) continue;

            uintptr_t targetAddress = reinterpret_cast<uintptr_t>(obj) + offset;
            if (!IsBadWritePtr((void*)targetAddress, sizeof(T))) {
                SafeWrite<T>(targetAddress, value);
            }
        }
    }
}

void SetStringField(const char* className, const char* fieldName, const char* value) {
    if (!value) return;

    auto offset = IL2CPP::Class::Utils::GetFieldOffset(className, fieldName);
    auto instances = Unity::Object::FindObjectsOfType<Unity::CGameObject>(className);

    if (!instances || instances->m_uMaxLength == 0) return;

    // Use void* instead of Il2CppString*
    void* str = IL2CPP::String::New(value);

    for (unsigned int i = 0; i < instances->m_uMaxLength; i++) {
        auto obj = instances->operator[](i);
        if (!obj) continue;

        uintptr_t addr = (uintptr_t)obj + offset;
        SafeWrite<void*>(addr, str);
    }
}

void SetCameraClipPlanes(float nearPlane, float farPlane) {
    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto cameras = Unity::Object::FindObjectsOfType<Unity::CComponent>("UnityEngine.Camera");

        if (cameras && cameras->m_uMaxLength > 0) {
            for (int i = 0; i < cameras->m_uMaxLength; i++) {
                auto cam = cameras->operator[](i);
                if (cam) {
                    cam->CallMethodSafe<void*>("set_nearClipPlane", nearPlane);
                    cam->CallMethodSafe<void*>("set_farClipPlane", farPlane);
                }
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
}

void SetOrthographic(bool enable, float size = 5.0f) {
    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto cameras = Unity::Object::FindObjectsOfType<Unity::CComponent>("UnityEngine.Camera");

        if (cameras && cameras->m_uMaxLength > 0) {
            for (int i = 0; i < cameras->m_uMaxLength; i++) {
                auto cam = cameras->operator[](i);
                if (cam) {
                    cam->CallMethodSafe<void*>("set_orthographic", enable);
                    if (enable) {
                        cam->CallMethodSafe<void*>("set_orthographicSize", size);
                    }
                }
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
}

void SetCameraAspect(float aspect) {
    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto cameras = Unity::Object::FindObjectsOfType<Unity::CComponent>("UnityEngine.Camera");

        if (cameras && cameras->m_uMaxLength > 0) {
            for (int i = 0; i < cameras->m_uMaxLength; i++) {
                auto cam = cameras->operator[](i);
                if (cam) {
                    cam->CallMethodSafe<void*>("set_aspect", aspect);
                }
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
}

void SetCameraClearFlags(int mode) {
    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto cameras = Unity::Object::FindObjectsOfType<Unity::CComponent>("UnityEngine.Camera");

        if (cameras && cameras->m_uMaxLength > 0) {
            for (int i = 0; i < cameras->m_uMaxLength; i++) {
                auto cam = cameras->operator[](i);
                if (cam) {
                    // CameraClearFlags: Skybox=1, SolidColor=2, Depth=3, Nothing=4
                    cam->CallMethodSafe<void*>("set_clearFlags", mode);
                }
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
}

void SetCameraBackgroundColor(float r, float g, float b, float a) {
    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto cameras = Unity::Object::FindObjectsOfType<Unity::CComponent>("UnityEngine.Camera");

        if (cameras && cameras->m_uMaxLength > 0) {
            for (int i = 0; i < cameras->m_uMaxLength; i++) {
                auto cam = cameras->operator[](i);
                if (cam) {
                    // Create Unity Color struct
                    struct UnityColor { float r, g, b, a; };
                    UnityColor color = { r, g, b, a };
                    cam->CallMethodSafe<void*>("set_backgroundColor", color);
                }
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
}

void SetCullingMask(int mask) {
    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto cameras = Unity::Object::FindObjectsOfType<Unity::CComponent>("UnityEngine.Camera");

        if (cameras && cameras->m_uMaxLength > 0) {
            for (int i = 0; i < cameras->m_uMaxLength; i++) {
                auto cam = cameras->operator[](i);
                if (cam) {
                    cam->CallMethodSafe<void*>("set_cullingMask", mask);
                }
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
}

void ImSmoothTabTransition()
{
    static float anim = 1.0f;
    static double startTime = 0.0;
    const float duration = 0.25f; // speed of animation (seconds)
    const float distance = 50.0f; // how far it slides in

    // When called the first frame after switching
    if (ImGui::IsWindowAppearing()) {
        anim = 0.0f;
        startTime = ImGui::GetTime();
    }

    // Progress 0→1
    float elapsed = (float)(ImGui::GetTime() - startTime);
    anim = ImClamp(elapsed / duration, 0.0f, 1.0f);

    // Ease out (smoother curve)
    float eased = 1.0f - powf(1.0f - anim, 3.0f);

    // Apply transform
    ImVec2 offset(0, (1.0f - eased) * distance);
    ImVec2 pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(pos.x + offset.x, pos.y + offset.y));


    // Optional: fade in
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, eased);
}
void ImSmoothTabEnd()
{
    ImGui::PopStyleVar();
}

static float UI_Scale = 1.0f; // default scale (1.0 = normal size)

// Theme structures
struct ThemePreset {
    std::string name;
    std::string description;
    ImVec4 accentColor;
    ImVec4 backgroundColor;
    ImVec4 sidebarColor;
    ImVec4 textColor;
    ImVec4 borderColor;
    ImVec4 buttonColor;
    ImVec4 buttonHover;
    ImVec4 buttonActive;
    ImVec4 headerColor;
    ImVec4 checkmarkColor;
    ImVec4 sliderGrabColor;
    ImVec4 frameColor;
    ImVec4 tabColor;
    ImVec4 tabActive;
};

std::vector<ThemePreset> themePresets = {

   {
        "Obsidian Dark",
        "aesthetic dark purple",
        ImVec4(0.547f, 0.139f, 0.837f, 1.0f),  // accent        #8C23D5
        ImVec4(0.10f,  0.10f,  0.13f,  1.0f),  // background    #19191F
        ImVec4(0.07f,  0.07f,  0.10f,  1.0f),  // sidebar       #111119 (darker)
        ImVec4(0.95f,  0.92f,  1.00f,  1.0f),  // text          slight purple tint
        ImVec4(0.07f,  0.07f,  0.10f,  1.0f), // border        accent @ 55% opacity
        ImVec4(0.14f,  0.13f,  0.18f,  1.0f),  // button        #231F2E
        ImVec4(0.22f,  0.17f,  0.32f,  1.0f),  // button hover  accent tinted
        ImVec4(0.30f,  0.22f,  0.42f,  1.0f),  // button active brighter tint
        ImVec4(0.10f,  0.09f,  0.14f,  1.0f),  // header        near sidebar
        ImVec4(0.74f,  0.58f,  0.98f,  1.0f),  // checkmark     #BC94FA
        ImVec4(0.547f, 0.139f, 0.837f, 0.70f), // slider        accent 70%
        ImVec4(0.12f,  0.11f,  0.16f,  1.0f),  // frame         #1E1C29
        ImVec4(0.14f,  0.13f,  0.19f,  1.0f),  // tab           #231F30
        ImVec4(0.26f,  0.20f,  0.38f,  1.0f),  // tab active    accent tinted
   },

    {
        "Monochrome Soft",
        "Dark but not crushed, subtle contrast",

        ImVec4(0.95f, 0.95f, 0.95f, 1.0f),  // accent        #F2F2F2 (slightly softer white)
        ImVec4(0.11f, 0.11f, 0.11f, 1.0f),  // background    #1C1C1C
        ImVec4(0.09f, 0.09f, 0.09f, 1.0f),  // sidebar       #171717
        ImVec4(0.90f, 0.90f, 0.90f, 1.0f),  // text          #E6E6E6
        ImVec4(0.25f, 0.25f, 0.25f, 0.35f), // border        soft gray w/ alpha
        ImVec4(0.16f, 0.16f, 0.16f, 1.0f),  // button        #292929
        ImVec4(0.22f, 0.22f, 0.22f, 1.0f),  // button hover  #383838
        ImVec4(0.30f, 0.30f, 0.30f, 1.0f),  // button active #4D4D4D
        ImVec4(0.12f, 0.12f, 0.12f, 1.0f),  // header        #1F1F1F
        ImVec4(0.95f, 0.95f, 0.95f, 1.0f),  // checkmark
        ImVec4(0.75f, 0.75f, 0.75f, 1.0f),  // slider
        ImVec4(0.14f, 0.14f, 0.14f, 1.0f),  // frame         #242424
        ImVec4(0.15f, 0.15f, 0.15f, 1.0f),  // tab
        ImVec4(0.28f, 0.28f, 0.28f, 1.0f),  // tab active
    },

    {
        "purple",
        "nigga purple",
        ImVec4(0.694f, 0.600f, 1.000f, 1.0f),  // accent        #99C8FF
        ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // background    #1F1F1F
        ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // sidebar       #2F2F2F
        ImVec4(1.00f,  1.00f,  1.00f,  1.0f),  // text          #FFFFFF
        ImVec4(0.153f, 0.153f, 0.153f, 1.0f),  // border        #272727
        ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // button        #1F1F1F
        ImVec4(0.122f, 0.122f, 0.122f, 0.7f),  // button hover  #1F1F1F 70%
        ImVec4(0.122f, 0.122f, 0.122f, 0.5f),  // button active #1F1F1F 50%
        ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // header        #2F2F2F
        ImVec4(1.000f, 1.000f, 1.000f, 1.0f),  // checkmark     #FFFFFF
        ImVec4(0.600f, 0.784f, 1.000f, 1.0f),  // slider        #99C8FF
        ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // frame         #1F1F1F
        ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // tab           #1F1F1F
        ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // tab active    #2F2F2F
    },

{
    "Dark",
    "Classic dark gray",
    ImVec4(0.600f, 0.784f, 1.000f, 1.0f),  // accent        #99C8FF
    ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // background    #1F1F1F
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // sidebar       #2F2F2F
    ImVec4(1.00f,  1.00f,  1.00f,  1.0f),  // text          #FFFFFF
    ImVec4(0.153f, 0.153f, 0.153f, 1.0f),  // border        #272727
    ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // button        #1F1F1F
    ImVec4(0.122f, 0.122f, 0.122f, 0.7f),  // button hover  #1F1F1F 70%
    ImVec4(0.122f, 0.122f, 0.122f, 0.5f),  // button active #1F1F1F 50%
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // header        #2F2F2F
    ImVec4(1.000f, 1.000f, 1.000f, 1.0f),  // checkmark     #FFFFFF
    ImVec4(0.600f, 0.784f, 1.000f, 1.0f),  // slider        #99C8FF
    ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // frame         #1F1F1F
    ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // tab           #1F1F1F
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // tab active    #2F2F2F
},

{
    "Mocha Dark",
    "Neutral dark with purple accents",
    ImVec4(0.847f, 0.631f, 0.988f, 1.0f),  // accent        #D8A1FC
    ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // background    #1F1F1F
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // sidebar       #2F2F2F
    ImVec4(1.000f, 1.000f, 1.000f, 1.0f),  // text          #FFFFFF
    ImVec4(0.153f, 0.153f, 0.153f, 1.0f),  // border        #272727
    ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // button        #1F1F1F
    ImVec4(0.122f, 0.122f, 0.122f, 0.7f),  // button hover  #1F1F1F 70%
    ImVec4(0.122f, 0.122f, 0.122f, 0.5f),  // button active #1F1F1F 50%
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // header        #2F2F2F
    ImVec4(1.000f, 1.000f, 1.000f, 1.0f),  // checkmark     #FFFFFF
    ImVec4(0.847f, 0.631f, 0.988f, 1.0f),  // slider        #D8A1FC
    ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // frame         #1F1F1F
    ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // tab           #1F1F1F
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // tab active    #2F2F2F
},

    {
        "greehn",
        "nigga purple",
        ImVec4(0.550f, 0.900f, 0.650f, 1.0f),  // accent        #99C8FF
        ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // background    #1F1F1F
        ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // sidebar       #2F2F2F
        ImVec4(1.00f,  1.00f,  1.00f,  1.0f),  // text          #FFFFFF
        ImVec4(0.153f, 0.153f, 0.153f, 1.0f),  // border        #272727
        ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // button        #1F1F1F
        ImVec4(0.122f, 0.122f, 0.122f, 0.7f),  // button hover  #1F1F1F 70%
        ImVec4(0.122f, 0.122f, 0.122f, 0.5f),  // button active #1F1F1F 50%
        ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // header        #2F2F2F
        ImVec4(1.000f, 1.000f, 1.000f, 1.0f),  // checkmark     #FFFFFF
        ImVec4(0.550f, 0.900f, 0.650f, 1.0f),  // slider        #99C8FF
        ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // frame         #1F1F1F
        ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // tab           #1F1F1F
        ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // tab active    #2F2F2F
    },

    {
         "orange type shi",
         "nigga",
         ImVec4(0.910f, 0.420f, 0.420f, 1.0f),  // accent        #99C8FF
         ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // background    #1F1F1F
         ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // sidebar       #2F2F2F
         ImVec4(1.00f,  1.00f,  1.00f,  1.0f),  // text          #FFFFFF
         ImVec4(0.153f, 0.153f, 0.153f, 1.0f),  // border        #272727
         ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // button        #1F1F1F
         ImVec4(0.122f, 0.122f, 0.122f, 0.7f),  // button hover  #1F1F1F 70%
         ImVec4(0.122f, 0.122f, 0.122f, 0.5f),  // button active #1F1F1F 50%
         ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // header        #2F2F2F
         ImVec4(1.000f, 1.000f, 1.000f, 1.0f),  // checkmark     #FFFFFF
         ImVec4(0.910f, 0.420f, 0.420f, 1.0f),  // slider        #99C8FF
         ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // frame         #1F1F1F
         ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // tab           #1F1F1F
         ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // tab active    #2F2F2F
    },

{
    "mochaaaa",
    "Neutral dark with no purpl",
    ImVec4(0.341f, 0.525f, 0.459f, 1.0f),  // accent        #578675
    ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // background    #1F1F1F
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // sidebar       #2F2F2F
    ImVec4(1.000f, 1.000f, 1.000f, 1.0f),  // text          #FFFFFF
    ImVec4(0.153f, 0.153f, 0.153f, 1.0f),  // border        #272727
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // button        #1F1F1F
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // button hover
    ImVec4(0.122f, 0.122f, 0.122f, 0.5f),  // button active
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // header        #2F2F2F
    ImVec4(1.000f, 1.000f, 1.000f, 1.0f),  // checkmark     #FFFFFF
    ImVec4(0.341f, 0.525f, 0.459f, 1.0f),  // slider        #578675
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // frame         #1F1F1F
    ImVec4(0.122f, 0.122f, 0.122f, 1.0f),  // tab           #1F1F1F
    ImVec4(0.184f, 0.184f, 0.184f, 1.0f),  // tab active    #2F2F2F
},
};

// Current theme variables
static int currentThemeIndex = 7;
static ThemePreset customTheme = themePresets[7];
static bool useCustomTheme = false;

// Global theme variables
ImVec4 currentAccentColor = themePresets[7].accentColor;
ImVec4 currentSidebarColor = themePresets[7].sidebarColor;
ImVec4 currentFrameColor = themePresets[7].frameColor;
ImVec4 currentBackgroundColor = themePresets[7].backgroundColor;

// Function to apply a theme
void ApplyTheme(const ThemePreset& theme) {
    auto& colors = ImGui::GetStyle().Colors;

    currentAccentColor = theme.accentColor;
    currentSidebarColor = theme.backgroundColor;
    currentBackgroundColor = theme.sidebarColor;
    currentFrameColor = theme.frameColor;

    colors[ImGuiCol_WindowBg] = theme.sidebarColor;
    colors[ImGuiCol_ChildBg] = theme.backgroundColor;
    colors[ImGuiCol_Text] = theme.textColor;

    // Borders are now accent-colored for visibility
    colors[ImGuiCol_Border] = theme.borderColor;
    colors[ImGuiCol_BorderShadow] = ImVec4(theme.accentColor.x, theme.accentColor.y, theme.accentColor.z, 0.10f);

    colors[ImGuiCol_Button] = theme.buttonColor;
    colors[ImGuiCol_ButtonHovered] = theme.buttonHover;
    colors[ImGuiCol_ButtonActive] = theme.buttonActive;
    colors[ImGuiCol_Header] = theme.headerColor;
    colors[ImGuiCol_HeaderHovered] = theme.buttonHover;
    colors[ImGuiCol_HeaderActive] = theme.buttonActive;
    colors[ImGuiCol_CheckMark] = theme.checkmarkColor;
    colors[ImGuiCol_SliderGrab] = theme.sliderGrabColor;
    colors[ImGuiCol_SliderGrabActive] = theme.accentColor;
    colors[ImGuiCol_FrameBg] = theme.frameColor;
    colors[ImGuiCol_FrameBgHovered] = theme.buttonHover;
    colors[ImGuiCol_FrameBgActive] = theme.buttonActive;
    colors[ImGuiCol_Tab] = theme.tabColor;
    colors[ImGuiCol_TabHovered] = theme.buttonHover;
    colors[ImGuiCol_TabActive] = theme.tabActive;
    colors[ImGuiCol_TabUnfocused] = theme.tabColor;
    colors[ImGuiCol_TabUnfocusedActive] = theme.tabColor;

    // Accent-tinted separators so they're visible
    colors[ImGuiCol_Separator] = ImVec4(theme.accentColor.x, theme.accentColor.y, theme.accentColor.z, 0.40f);
    colors[ImGuiCol_SeparatorHovered] = theme.accentColor;
    colors[ImGuiCol_SeparatorActive] = theme.accentColor;

    colors[ImGuiCol_MenuBarBg] = theme.headerColor;
    colors[ImGuiCol_PopupBg] = theme.backgroundColor;
    colors[ImGuiCol_ScrollbarBg] = theme.backgroundColor;
    colors[ImGuiCol_ScrollbarGrab] = theme.buttonHover;
    colors[ImGuiCol_ScrollbarGrabHovered] = theme.buttonHover;
    colors[ImGuiCol_ScrollbarGrabActive] = theme.buttonHover;

    colors[ImGuiCol_ResizeGrip] = ImVec4(theme.accentColor.x, theme.accentColor.y, theme.accentColor.z, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = theme.accentColor;
    colors[ImGuiCol_ResizeGripActive] = theme.accentColor;

    colors[ImGuiCol_TitleBg] = theme.sidebarColor;
    colors[ImGuiCol_TitleBgActive] = theme.headerColor;
    colors[ImGuiCol_TitleBgCollapsed] = theme.sidebarColor;

    // Plots use accent for visual pop
    colors[ImGuiCol_PlotLines] = theme.accentColor;
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(theme.accentColor.x, theme.accentColor.y, theme.accentColor.z, 0.75f);
    colors[ImGuiCol_PlotHistogram] = theme.accentColor;
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(theme.accentColor.x, theme.accentColor.y, theme.accentColor.z, 0.75f);
}
// Forward declarations for UI functions
void DrawMainContent();
void DrawSidebar();
//void DrawESPPage();
void DrawWeaponsPage();

void DrawExploitsPage();
void DrawSettingsPage();

inline void SeparatorText(const char* text, float thickness = 2.0f)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 start = ImGui::GetCursorScreenPos();
    float full_width = ImGui::GetContentRegionAvail().x;
    ImVec2 text_size = ImGui::CalcTextSize(text);

    ImU32 color = ImGui::ColorConvertFloat4ToU32(currentAccentColor);

    float spacing = 8.0f;
    float center_y = start.y + text_size.y * 0.5f;

    float left_width = (full_width - text_size.x - spacing * 2.0f) * 0.5f;
    float right_start = start.x + left_width + spacing * 2.0f + text_size.x;

    // Left line
    draw_list->AddLine(
        ImVec2(start.x, center_y),
        ImVec2(start.x + left_width, center_y),
        color,
        thickness
    );

    // Text
    ImGui::SetCursorScreenPos(ImVec2(start.x + left_width + spacing, start.y));
    ImGui::TextUnformatted(text);

    // Right line
    draw_list->AddLine(
        ImVec2(right_start, center_y),
        ImVec2(start.x + full_width, center_y),
        color,
        thickness
    );

    // Reset cursor manually to avoid extra spacing
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + text_size.y));
}

inline void Separator(float thickness = 2.0f)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;

    ImU32 color = ImGui::ColorConvertFloat4ToU32(currentAccentColor);

    draw_list->AddLine(
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + width, pos.y),
        color,
        thickness
    );

    ImGui::Dummy(ImVec2(0.0f, thickness));
}

void SetupModernStyle() {
    auto& style = ImGui::GetStyle();

    style.WindowRounding = 8.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.WindowPadding = ImVec2(6, 6);
    style.FramePadding = ImVec2(4, 3);
    style.ItemSpacing = ImVec2(4, 3);
    style.ItemInnerSpacing = ImVec2(3, 3);
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 9.0f;
    style.SeparatorTextBorderSize = 2.0f; // explicitly keep > 0

}

void DrawStatusHeader() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, currentSidebarColor);
    ImGui::PushStyleColor(ImGuiCol_Border, currentSidebarColor);
    ImGui::BeginChild("StatusHeader", ImVec2(0, 35), true, ImGuiWindowFlags_NoScrollbar);

    const float padding = 10.0f;
    const float verticalCenter = 5.0f; // Vertical centering offset

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + verticalCenter);
    ImGui::SetCursorPosX(padding);

    // === LEFT SECTION - Developer Info ===
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.7f, 1.0f), "Developer:");
    ImGui::SameLine(0.0f, 5.0f); // Small spacing
    ImGui::TextColored(currentAccentColor, "imosyx");
    ImGui::EndGroup();


    const char* fpsText = "FPS: 999"; // Max width estimation
    float fpsTextWidth = ImGui::CalcTextSize(fpsText).x;
    float rightSectionPos = ImGui::GetWindowWidth() - fpsTextWidth - padding;

    float iconPos = rightSectionPos - 80.0f;
    ImGui::SameLine(iconPos);
    ImGui::BeginGroup();


    ImGui::EndGroup();

    // === RIGHT SECTION - Performance Info ===
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SameLine(rightSectionPos);
    ImGui::BeginGroup();

    ImGui::Text("FPS:");
    ImGui::SameLine(0.0f, 5.0f);

    // Color-coded FPS display
    ImVec4 fpsColor;
    if (io.Framerate > 60.0f) {
        fpsColor = ImVec4(0.2f, 0.9f, 0.2f, 1.0f); // Green
    }
    else if (io.Framerate > 30.0f) {
        fpsColor = ImVec4(0.9f, 0.9f, 0.2f, 1.0f); // Yellow
    }
    else {
        fpsColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // Red
    }

    ImGui::TextColored(fpsColor, "%.0f", io.Framerate);

    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}


// InterpolateColor must come first
ImVec4 InterpolateColor(const ImVec4& color1, const ImVec4& color2, float t)
{
    float inverse_t = 1.0f - t;
    return ImVec4(
        inverse_t * color1.x + t * color2.x,
        inverse_t * color1.y + t * color2.y,
        inverse_t * color1.z + t * color2.z,
        color1.w
    );
}

void GradientText(const char* text, float speed, const ImVec4& color1, const ImVec4& color2, ImVec2* position = nullptr)
{
    if (position) ImGui::SetCursorPos(*position);

    float time = ImGui::GetTime() * speed;
    float gradientStep = 1.0f / strlen(text);

    for (int i = 0; i < strlen(text); ++i)
    {
        float t = fmodf(time - i * gradientStep, 2.0f);
        if (t > 1.0f) t = 2.0f - t;

        ImVec4 color = InterpolateColor(color1, color2, t);

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%c", text[i]);
        ImGui::SameLine(0, 0);
        ImGui::PopStyleColor();
    }
    ImGui::NewLine();
}

void DrawWatermark() {
    if (!Config::showWatermark) return;
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiStyle backup_style = style;
    ImVec4 backup_colors[ImGuiCol_COUNT];
    memcpy(backup_colors, style.Colors, sizeof(backup_colors));
    ImVec2 screen_size = ImGui::GetIO().DisplaySize;

    ImVec2 pos = ImVec2(screen_size.x - 10.0f, 10.0f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));

    style.WindowPadding = ImVec2(12, 8);
    style.FramePadding = ImVec2(8, 5);
    style.WindowRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.WindowBorderSize = 0.0f; // we draw border manually
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("WatermarkHybrid", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 wpos = ImGui::GetWindowPos();
    ImVec2 wsize = ImGui::GetWindowSize();
    const float rounding = 6.0f;

    // Subtle gradient: very slightly tinted on left, dark on right
    ImU32 colLeft = IM_COL32(
        (int)(18 + currentAccentColor.x * 18),
        (int)(18 + currentAccentColor.y * 18),
        (int)(18 + currentAccentColor.z * 18),
        240
    );
    ImU32 colRight = IM_COL32(22, 22, 22, 240);

    draw_list->AddRectFilledMultiColor(
        ImVec2(wpos.x + rounding, wpos.y),
        ImVec2(wpos.x + wsize.x - rounding, wpos.y + wsize.y),
        colLeft, colRight, colRight, colLeft
    );
    // Fill rounded caps separately so corners aren't clipped
    draw_list->AddRectFilled(wpos, ImVec2(wpos.x + rounding * 2, wpos.y + wsize.y), colLeft, rounding);
    draw_list->AddRectFilled(ImVec2(wpos.x + wsize.x - rounding * 2, wpos.y), ImVec2(wpos.x + wsize.x, wpos.y + wsize.y), colRight, rounding);

    // Accent border
    draw_list->AddRect(
        wpos,
        ImVec2(wpos.x + wsize.x, wpos.y + wsize.y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(
            currentAccentColor.x, currentAccentColor.y, currentAccentColor.z, 0.25f)),
        rounding, 0, 1.0f
    );

    ImGuiIO& io = ImGui::GetIO();

    ImGui::TextColored(ImVec4(0.95f, 0.92f, 1.00f, 1.0f), "OPIUM v4");
    ImGui::SameLine(0.0f, 10.0f);
    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "|");
    ImGui::SameLine(0.0f, 10.0f);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "FPS:");
    ImGui::SameLine(0.0f, 4.0f);

    ImVec4 fpsColor;
    if (io.Framerate > 60.0f)      fpsColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    else if (io.Framerate > 30.0f) fpsColor = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
    else                           fpsColor = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

    ImGui::TextColored(fpsColor, "%.0f", io.Framerate);

    ImGui::End();

    memcpy(style.Colors, backup_colors, sizeof(backup_colors));
    style = backup_style;
}

void CreateConsole() {
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    SetConsoleTitle("OPIUM CLIENT v4");

    // Resize console window
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd) {
        MoveWindow(consoleWnd, 100, 100, 600, 300, TRUE); // x, y, width, height
    }

    // Resize console buffer
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole) {
        SMALL_RECT windowSize = { 0, 0, 79, 19 }; // 80 cols x 20 rows
        SetConsoleWindowInfo(hConsole, TRUE, &windowSize);

        COORD bufferSize = { 80, 200 };
        SetConsoleScreenBufferSize(hConsole, bufferSize);
    }

    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
}

struct Snowflake {
    float x, y, speed, radius, wobble, phase;
};

static std::vector<Snowflake> g_snowflakes;
static bool g_snowInit = false;

void InitSnow(float screenW, float screenH) {
    g_snowflakes.clear();
    for (int i = 0; i < 120; i++) {
        Snowflake s;
        s.x = (float)(rand() % (int)screenW);
        s.y = (float)(rand() % (int)screenH);
        s.speed = 35.0f + (rand() % 80);
        s.radius = 1.2f + (rand() % 100) / 100.0f * 2.5f;

        s.wobble = 20.0f + (rand() % 40);
        s.phase = (float)(rand() % 628) / 100.0f;
        g_snowflakes.push_back(s);
    }
    g_snowInit = true;
}

void DrawSnowEffect() {
    if (!Config::enableSnowEffect) return;

    ImGuiIO& io = ImGui::GetIO();
    float sw = io.DisplaySize.x;
    float sh = io.DisplaySize.y;
    float dt = io.DeltaTime;

    if (!g_snowInit || g_snowflakes.empty())
        InitSnow(sw, sh);

    // Use BACKGROUND draw list so snow is BEHIND menu but above game
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    // Darkness overlay — drawn first so snow sits on top of it
    draw->AddRectFilled(
        ImVec2(0, 0),
        ImVec2(sw, sh),
        IM_COL32(0, 0, 0, 120)   // adjust alpha for darkness (0-255)
    );

    float time = (float)ImGui::GetTime();

    for (auto& s : g_snowflakes) {
        // Update position
        s.y += s.speed * dt;
        float wobbleX = sinf(time * 0.8f + s.phase) * s.wobble;

        if (s.y > sh + 10.0f) {
            s.y = -10.0f;
            s.x = (float)(rand() % (int)sw);
        }

        // Fade near edges for softness
        float alpha = 200.0f;
        float ex = s.x + wobbleX;
        if (ex < 20.0f)      alpha *= ex / 20.0f;
        if (ex > sw - 20.0f) alpha *= (sw - ex) / 20.0f;

        draw->AddCircleFilled(
            ImVec2(ex, s.y),
            s.radius,
            IM_COL32(255, 255, 255, (int)alpha)
        );
    }
}

enum class MenuEffect {
    None,
    Snow,
    MatrixRain,
    FloatingParticles,
    GlitchLines,
    Fireflies,
    COUNT
};

static const char* MenuEffectNames[] = {
    "None",
    "Snow",
    "Matrix Rain",
    "Floating Particles",
    "Glitch Lines",
    "Fireflies"
};

struct MenuEffectState {
    MenuEffect current = MenuEffect::Snow;
    bool initialized = false;
    float time = 0.f;

    // Snow
    struct SnowFlake { float x, y, speed, size, drift; };
    std::vector<SnowFlake> snow;

    // Matrix
    struct MatrixCol { float x; std::vector<float> y; float speed; float timer; };
    std::vector<MatrixCol> matrix;

    // Particles
    struct Particle { float x, y, vx, vy, life, maxLife, size; };
    std::vector<Particle> particles;

    // Glitch
    struct GlitchLine { float y, height, alpha, timer; };
    std::vector<GlitchLine> glitches;

    // Fireflies
    struct Firefly { float x, y, tx, ty, speed, size, pulse; };
    std::vector<Firefly> fireflies;
};

static MenuEffectState g_menuEffect;

void InitMenuEffect(ImVec2 windowSize) {
    auto& fx = g_menuEffect;
    fx.initialized = true;
    fx.time = 0.f;

    float w = windowSize.x, h = windowSize.y;

    // Snow
    fx.snow.clear();
    for (int i = 0; i < 30; i++) { // number of snow particles 
        fx.snow.push_back({
            (float)(rand() % (int)w),   // X position
            (float)(rand() % (int)h),   // Y position
            13.f + (rand() % 40),        // SIZE of the snowflake
            0.8f + (rand() % 100) / 120.f, // FALL SPEED 
            (rand() % 100 - 50) / 60.f  // HORIZONTAL DRIFT
            });
    }

    // Matrix
    fx.matrix.clear();
    int cols = (int)(w / 12.f);
    for (int i = 0; i < cols; i++) {
        std::vector<float> drops;
        int numDrops = 3 + rand() % 5;
        for (int j = 0; j < numDrops; j++) drops.push_back(-(rand() % (int)h));
        fx.matrix.push_back({ (float)i * 12.f + 6.f, drops, 40.f + (rand() % 80), 0.f });
    }

    fx.particles.clear();
    for (int i = 0; i < 40; i++) {
        fx.particles.push_back({
            (float)(rand() % (int)w),
            (float)(rand() % (int)h),
            (rand() % 100 - 50) / 200.f,
            (rand() % 100 - 50) / 200.f,
            5.f,
            5.f,
            1.5f
            });
    }

    // Glitch
    fx.glitches.clear();
    for (int i = 0; i < 8; i++) {
        fx.glitches.push_back({
            (float)(rand() % (int)h),
            2.f + (rand() % 20),
            0.3f + (rand() % 100) / 200.f,
            (float)(rand() % 100) / 100.f * 3.f
            });
    }

    // Fireflies
    fx.fireflies.clear();
    for (int i = 0; i < 25; i++) {
        fx.fireflies.push_back({
            (float)(rand() % (int)w),
            (float)(rand() % (int)h),
            (float)(rand() % (int)w),
            (float)(rand() % (int)h),
            15.f + (rand() % 40),
            1.5f + (rand() % 100) / 50.f,
            (float)(rand() % 628) / 100.f
            });
    }
}

void DrawMenuEffect(ImDrawList* draw, ImVec2 pos, ImVec2 size, float dt) {
    auto& fx = g_menuEffect;
    if (fx.current == MenuEffect::None) return;

    if (!fx.initialized) InitMenuEffect(size);
    fx.time += dt;

    ImU32 accent = ImGui::ColorConvertFloat4ToU32(currentAccentColor);
    ImU32 accentDim = IM_COL32(
        (int)(currentAccentColor.x * 255),
        (int)(currentAccentColor.y * 255),
        (int)(currentAccentColor.z * 255),
        120
    );
    ImU32 white = IM_COL32(255, 255, 255, 180);
    ImU32 whiteDim = IM_COL32(255, 255, 255, 80);

    switch (fx.current) {

        // ========== SNOW ==========
    case MenuEffect::Snow: {
        for (auto& s : fx.snow) {
            s.y += s.speed * dt;
            s.x += s.drift * dt;
            if (s.y > size.y + 5) { s.y = -5; s.x = (float)(rand() % (int)size.x); }
            if (s.x < -5) s.x = size.x + 5;
            if (s.x > size.x + 5) s.x = -5;

            float alpha = 0.6f + sinf(fx.time * 2.f + s.x) * 0.3f;
            ImU32 col = IM_COL32(255, 255, 255, (int)(alpha * 200));
            draw->AddCircleFilled(ImVec2(pos.x + s.x, pos.y + s.y), s.size, col);
        }
        break;
    }

                         // ========== MATRIX RAIN ==========
    case MenuEffect::MatrixRain: {
        ImU32 green = IM_COL32(0, 255, 70, 200);
        ImU32 greenDim = IM_COL32(0, 180, 50, 100);
        ImU32 greenTail = IM_COL32(0, 100, 30, 60);

        for (auto& col : fx.matrix) {
            col.timer += dt;
            for (auto& drop : col.y) {
                drop += col.speed * dt;
                if (drop > size.y + 50) drop = -(rand() % 100);

                // Head
                draw->AddRectFilled(
                    ImVec2(pos.x + col.x - 3, pos.y + drop),
                    ImVec2(pos.x + col.x + 3, pos.y + drop + 10),
                    green
                );
                // Tail
                for (int t = 1; t < 6; t++) {
                    float ty = drop - t * 12;
                    if (ty < 0) break;
                    ImU32 c = t < 3 ? greenDim : greenTail;
                    draw->AddRectFilled(
                        ImVec2(pos.x + col.x - 2, pos.y + ty),
                        ImVec2(pos.x + col.x + 2, pos.y + ty + 8),
                        c
                    );
                }
            }
        }
        break;
    }

                               // ========== FLOATING PARTICLES ==========
    case MenuEffect::FloatingParticles: {

        for (auto& p : fx.particles) {
            p.x += p.vx * dt * 20; // was 10
            p.y += p.vy * dt * 20;

            // wrap instead of reset
            if (p.x < 0) p.x += size.x;
            if (p.x > size.x) p.x -= size.x;
            if (p.y < 0) p.y += size.y;
            if (p.y > size.y) p.y -= size.y;
        }

        // ===== CONNECTIONS =====
        float maxDist = 100.f;

        for (int i = 0; i < fx.particles.size(); i++) {
            for (int j = i + 1; j < fx.particles.size(); j++) {

                float dx = fx.particles[i].x - fx.particles[j].x;
                float dy = fx.particles[i].y - fx.particles[j].y;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist < maxDist) {
                    float alpha = 1.0f - (dist / maxDist);

                    ImU32 col = IM_COL32(
                        (int)(currentAccentColor.x * 255),
                        (int)(currentAccentColor.y * 255),
                        (int)(currentAccentColor.z * 255),
                        (int)(alpha * 80) // subtle lines
                    );

                    draw->AddLine(
                        ImVec2(pos.x + fx.particles[i].x, pos.y + fx.particles[i].y),
                        ImVec2(pos.x + fx.particles[j].x, pos.y + fx.particles[j].y),
                        col,
                        1.0f
                    );
                }
            }
        }

        // ===== DRAW POINTS =====
        for (auto& p : fx.particles) {
            ImU32 col = IM_COL32(
                (int)(currentAccentColor.x * 255),
                (int)(currentAccentColor.y * 255),
                (int)(currentAccentColor.z * 255),
                180
            );

            draw->AddCircleFilled(
                ImVec2(pos.x + p.x, pos.y + p.y),
                1.5f, // small points
                col
            );
        }

        break;
    }
                                      // ========== GLITCH LINES ==========
    case MenuEffect::GlitchLines: {
        for (auto& g : fx.glitches) {
            g.timer -= dt;
            if (g.timer <= 0) {
                g.y = (float)(rand() % (int)size.y);
                g.height = 2.f + (rand() % 15);
                g.alpha = 0.2f + (rand() % 100) / 200.f;
                g.timer = 0.2f + (rand() % 100) / 100.f * 2.f;
            }

            float flicker = sinf(fx.time * 20.f + g.y) * 0.3f + 0.7f;
            ImU32 col = IM_COL32(
                (int)(currentAccentColor.x * 255),
                (int)(currentAccentColor.y * 255),
                (int)(currentAccentColor.z * 255),
                (int)(g.alpha * flicker * 150)
            );

            float shift = sinf(fx.time * 5.f + g.y * 0.1f) * 3.f;
            draw->AddRectFilled(
                ImVec2(pos.x + shift, pos.y + g.y),
                ImVec2(pos.x + size.x - shift, pos.y + g.y + g.height),
                col
            );
        }
        break;
    }

                                // ========== FIREFLIES ==========
    case MenuEffect::Fireflies: {
        for (auto& f : fx.fireflies) {
            float dx = f.tx - f.x;
            float dy = f.ty - f.y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist < 5.f || f.x < 0 || f.x > size.x || f.y < 0 || f.y > size.y) {
                f.tx = (float)(rand() % (int)size.x);
                f.ty = (float)(rand() % (int)size.y);
            }

            f.x += (dx / dist) * f.speed * dt;
            f.y += (dy / dist) * f.speed * dt;
            f.pulse += dt * 3.f;

            float glow = 0.4f + sinf(f.pulse) * 0.4f;
            float radius = f.size * (0.8f + sinf(f.pulse) * 0.3f);

            ImU32 core = IM_COL32(
                (int)(currentAccentColor.x * 255),
                (int)(currentAccentColor.y * 255),
                (int)(currentAccentColor.z * 255),
                (int)(glow * 255)
            );
            ImU32 glowCol = IM_COL32(
                (int)(currentAccentColor.x * 255),
                (int)(currentAccentColor.y * 255),
                (int)(currentAccentColor.z * 255),
                (int)(glow * 60)
            );

            draw->AddCircleFilled(ImVec2(pos.x + f.x, pos.y + f.y), radius * 3.f, glowCol);
            draw->AddCircleFilled(ImVec2(pos.x + f.x, pos.y + f.y), radius, core);
        }
        break;
    }

    default: break;
    }
}

void ResetMenuEffect() {
    g_menuEffect.initialized = false;
}

void DrawCustomCursor() {
    if (!g_customCursorEnabled) return;

    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImVec2 mp = ImGui::GetIO().MousePos;

    ImU32 fill = ImGui::ColorConvertFloat4ToU32(currentAccentColor);
    ImU32 outline = IM_COL32(0, 0, 0, 200);

    switch (g_cursorStyle) {
    case MenuCursor::Arrow: {
        ImVec2 pts[7] = {
            { mp.x,        mp.y        },
            { mp.x,        mp.y + 14.f },
            { mp.x + 3.5f, mp.y + 10.f },
            { mp.x + 6.f,  mp.y + 15.f },
            { mp.x + 8.f,  mp.y + 14.f },
            { mp.x + 5.5f, mp.y + 9.f  },
            { mp.x + 9.f,  mp.y + 9.f  },
        };
        fg->AddConvexPolyFilled(pts, 7, fill);
        fg->AddPolyline(pts, 7, outline, true, 1.2f);
        break;
    }
    case MenuCursor::Hand: {
        fg->AddCircleFilled({ mp.x, mp.y }, 4.5f, fill);
        fg->AddCircle({ mp.x, mp.y }, 4.5f, outline, 0, 1.2f);
        fg->AddLine({ mp.x,       mp.y + 4.f }, { mp.x,       mp.y + 14.f }, fill, 3.f);
        fg->AddLine({ mp.x - 3.f, mp.y + 7.f }, { mp.x - 3.f, mp.y + 14.f }, fill, 2.5f);
        fg->AddLine({ mp.x + 3.f, mp.y + 7.f }, { mp.x + 3.f, mp.y + 14.f }, fill, 2.5f);
        fg->AddLine({ mp.x - 6.f, mp.y + 10.f }, { mp.x - 6.f, mp.y + 14.f }, fill, 2.f);
        fg->AddRect({ mp.x - 6.5f, mp.y + 13.f }, { mp.x + 3.5f, mp.y + 15.f }, outline, 1.f);
        break;
    }
    case MenuCursor::Crosshair: {
        const float R = 9.f, gap = 3.f;
        fg->AddLine({ mp.x - R,   mp.y }, { mp.x - gap, mp.y }, fill, 1.8f);
        fg->AddLine({ mp.x + gap, mp.y }, { mp.x + R,   mp.y }, fill, 1.8f);
        fg->AddLine({ mp.x,       mp.y - R }, { mp.x,       mp.y - gap }, fill, 1.8f);
        fg->AddLine({ mp.x,       mp.y + gap }, { mp.x,       mp.y + R }, fill, 1.8f);
        fg->AddCircle({ mp.x, mp.y }, 3.f, fill, 0, 1.5f);
        fg->AddCircle({ mp.x, mp.y }, 3.f, outline, 0, 0.7f);
        break;
    }
    case MenuCursor::Diamond: {
        const float s = 7.f;
        ImVec2 pts[4] = {
            { mp.x,     mp.y - s },
            { mp.x + s, mp.y     },
            { mp.x,     mp.y + s },
            { mp.x - s, mp.y     },
        };
        fg->AddConvexPolyFilled(pts, 4, fill);
        fg->AddPolyline(pts, 4, outline, true, 1.3f);
        ImVec2 shine[4] = {
            { mp.x,             mp.y - s * 0.45f },
            { mp.x + s * 0.45f, mp.y             },
            { mp.x,             mp.y + s * 0.45f },
            { mp.x - s * 0.45f, mp.y             },
        };
        fg->AddPolyline(shine, 4, IM_COL32(255, 255, 255,1), true, 0.8f);
        break;
    }
    }
}

void HandleMenuMouse(bool menuOpen) {
    static bool prevCursorEnabled = true;

    if (menuOpen && g_customCursorEnabled) {
        ClipCursor(nullptr);

        // Toggled custom cursor ON mid-menu
        if (g_customCursorEnabled && !prevCursorEnabled)
            while (ShowCursor(FALSE) >= 0);

        // Toggled custom cursor OFF mid-menu
        if (!g_customCursorEnabled && prevCursorEnabled)
            while (ShowCursor(TRUE) < 0);

        // Hide real cursor if custom is on
        if (g_customCursorEnabled)
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);

        DrawCustomCursor();
    }

    // Menu just closed - restore
    if (!menuOpen && g_menuWasOpen) {
        while (ShowCursor(TRUE) < 0);
        ClipCursor(nullptr); // let game re-lock naturally
        g_menuWasOpen = false;
    }

    if (menuOpen && !g_menuWasOpen)
        g_menuWasOpen = true;

    prevCursorEnabled = g_customCursorEnabled;
}

void DrawModernInterface() {
    HandleMenuMouse(show_menu);
    if (!show_menu) return;
    static bool wasMenuOpen = false;
    if (!wasMenuOpen && show_menu) {
        ResetMenuEffect(); // reinit when menu opens
    }
    wasMenuOpen = show_menu;
    {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        // Try to get window pos/size from last frame
        ImGuiWindow* win = ImGui::FindWindowByName("dih2");
        if (win) {
            ImVec2 wpos = win->Pos;
            ImVec2 wsize = win->Size;
            const float rounding = 8.0f;

            for (int i = 8; i >= 1; i--) {
                float offset = (float)i * 1.2f;
                float alpha = 35.0f * (1.0f - (float)i / 8.0f);
                draw->AddRectFilled(
                    ImVec2(wpos.x - offset, wpos.y - offset + 10.f),
                    ImVec2(wpos.x + wsize.x + offset, wpos.y + wsize.y + offset),
                    IM_COL32(0, 0, 0, (int)alpha),
                    rounding + offset * 0.5f
                );
            }
        }
    }

    float dpi_scale = ImGui::GetIO().DisplayFramebufferScale.x;
    ImGui::SetNextWindowSize(ImVec2(489.6 * dpi_scale, 390 * dpi_scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("dih2", &show_menu,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar)) {

        DrawStatusHeader();

        ImGui::BeginChild("MainLayout", ImVec2(0, 0), false);
        {
            DrawSidebar();
            ImGui::SameLine();

            // Only apply custom colors if theme is NOT index 7
            bool pushedColors = false;
            if (currentThemeIndex != 7) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, currentBackgroundColor);
                ImGui::PushStyleColor(ImGuiCol_Border, currentSidebarColor);
                pushedColors = true;
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

            ImGui::BeginChild("ContentArea", ImVec2(0, 0), true);
            {
                DrawMainContent();
            }
            ImGui::EndChild();

            ImGui::PopStyleVar();

            // Only pop if we pushed
            if (pushedColors) {
                ImGui::PopStyleColor(2);
            }
        }
        ImGui::EndChild();
    }

    if (show_menu) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        float dt = ImGui::GetIO().DeltaTime;

        fg->PushClipRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), true);
        DrawMenuEffect(fg, winPos, winSize, dt);
        fg->PopClipRect();
    }

    ImGui::End();
}

void ApplyUIScale() {
    auto& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(12.f * UI_Scale, 12.f * UI_Scale);
    style.FramePadding = ImVec2(8.f * UI_Scale, 6.f * UI_Scale);
    style.ItemSpacing = ImVec2(8.f * UI_Scale, 6.f * UI_Scale);
    style.ItemInnerSpacing = ImVec2(6.f * UI_Scale, 6.f * UI_Scale);
    style.ScrollbarSize = 14.f * UI_Scale;
    style.GrabMinSize = 12.f * UI_Scale;
}

static float scale = 1.0f;
static char scale_input[10] = "1.0";
// Helper function to scale ImVec2
// Helper function to scale ImVec2
ImVec2 ScaleImVec2(const ImVec2& v, float scale) {
    return ImVec2(v.x * scale, v.y * scale);
}

// Function to apply a global scale to all style parameters
void ApplyImGuiScale(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    // Set global font scale
    io.FontGlobalScale = scale;

    // Scale style parameters that affect sizes
    style.WindowPadding = ScaleImVec2(style.WindowPadding, scale);
    style.FramePadding = ScaleImVec2(style.FramePadding, scale);
    style.ItemSpacing = ScaleImVec2(style.ItemSpacing, scale);
    style.ItemInnerSpacing = ScaleImVec2(style.ItemInnerSpacing, scale);
    style.ScrollbarSize = style.ScrollbarSize * scale;
    style.GrabMinSize = style.GrabMinSize * scale;

    style.TabRounding = 5;

    style.WindowRounding = style.WindowRounding * scale;
    style.ChildRounding = style.ChildRounding * scale;
    style.FrameRounding = style.FrameRounding * scale;
    style.PopupRounding = style.PopupRounding * scale;
    style.ScrollbarRounding = style.ScrollbarRounding * scale;
    style.GrabRounding = style.GrabRounding * scale;

    // You might want to scale font sizes explicitly if using custom fonts
    // But generally, setting FontGlobalScale handles font sizes
}

bool ToggleButton(const char* label, bool* v, const ImVec2& size = ImVec2(32, 16), float animationSpeed = 7.0f) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImGuiID id = ImGui::GetID(label);
    static std::unordered_map<ImGuiID, float> toggleAnimMap;
    float& toggleAnim = toggleAnimMap[id];
    ImVec2 pos = ImGui::GetCursorScreenPos();
    pos.y += 0.5f;
    ImVec2 buttonSize = size;
    float radius = buttonSize.y * 0.42f;
    toggleAnim = ImLerp(toggleAnim, *v ? 1.0f : 0.0f, animationSpeed * io.DeltaTime);
    float togglePosX = pos.x + radius + (buttonSize.x - 2.2f * radius) * toggleAnim;
    ImVec4 offColor = ImVec4(
        ImClamp(currentBackgroundColor.x + 0.04f, 0.0f, 1.0f),
        ImClamp(currentBackgroundColor.y + 0.04f, 0.0f, 1.0f),
        ImClamp(currentBackgroundColor.z + 0.04f, 0.0f, 1.0f),
        1.0f
    );
    ImVec4 bgColor = ImLerp(offColor, currentAccentColor, toggleAnim);
    float accentBrightness = currentAccentColor.x * 0.299f + currentAccentColor.y * 0.587f + currentAccentColor.z * 0.114f;
    float avg = (currentAccentColor.x + currentAccentColor.y + currentAccentColor.z) / 3.0f;
    ImVec4 circleColorOff = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
    bool accentIsWhite = currentAccentColor.x > 0.9f && currentAccentColor.y > 0.9f && currentAccentColor.z > 0.9f;
    ImVec4 circleColorOn = accentIsWhite ? ImVec4(0.10f, 0.10f, 0.10f, 1.0f) : ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    ImVec4 circleColor = ImLerp(circleColorOff, circleColorOn, toggleAnim);
    ImGui::InvisibleButton(label, buttonSize);
    bool isToggled = ImGui::IsItemClicked();
    if (isToggled) *v = !*v;
    draw_list->AddRectFilled(pos, ImVec2(pos.x + buttonSize.x, pos.y + buttonSize.y), ImGui::GetColorU32(bgColor), radius);
    draw_list->AddCircleFilled(ImVec2(togglePosX, pos.y + buttonSize.y * 0.5f), radius - 2.0f, ImGui::GetColorU32(circleColor));
    return isToggled;
}

void DrawSettingsPage() {
    ImGui::BeginChild("SettingsContent", ImVec2(0, 0), false);
    ImSmoothTabTransition();

    ImGui::Text("deleted the whole page cus i broke it :p ill add stuff later");
    // Input for scale factor
    if (ImGui::InputText("UI Scale", scale_input, IM_ARRAYSIZE(scale_input))) {
        // optional real-time update
    }

    if (ImGui::Button("Apply Scale")) {
        float new_scale = 1.0f;
        if (sscanf_s(scale_input, "%f", &new_scale) == 1) {
            if (new_scale < 0.1f) new_scale = 0.1f;
            if (new_scale > 3.0f) new_scale = 3.0f;
            scale = new_scale;
            ApplyImGuiScale(scale);
        }
    }


    if (ImGui::Button("Print Window Size")) {
        ImVec2 size = ImGui::GetWindowSize();
        printf("Window Size: %.1f x %.1f\n", size.x, size.y);
    }


    ImGui::Text("cursor unlock");
    ImGui::SameLine(150);
    ToggleButton("##customcursor", &g_customCursorEnabled);

    if (g_customCursorEnabled) {
        ImGui::Spacing();
        ImGui::Text("Style");
        ImGui::SameLine(150);
        ImGui::RadioButton("Arrow", (int*)&g_cursorStyle, (int)MenuCursor::Arrow);
        ImGui::SameLine();
        ImGui::RadioButton("Hand", (int*)&g_cursorStyle, (int)MenuCursor::Hand);
        ImGui::SameLine();
        ImGui::RadioButton("Crosshair", (int*)&g_cursorStyle, (int)MenuCursor::Crosshair);
        ImGui::SameLine();
        ImGui::RadioButton("Diamond", (int*)&g_cursorStyle, (int)MenuCursor::Diamond);
    }

    ImGui::Text("snow effect");
    ImGui::SameLine(150);
    if (ToggleButton("##snow", &Config::enableSnowEffect)) {
        if (!Config::enableSnowEffect)
            g_snowInit = false;  // reset on disable so it reinits fresh next time
    }

    ImSmoothTabEnd();
    ImGui::EndChild();
}


namespace ImAdd
{
    static ImVec4 HexToColorVec4(unsigned int hex, float alpha = 1.0f)
    {
        float r = ((hex >> 16) & 0xFF) / 255.0f;
        float g = ((hex >> 8) & 0xFF) / 255.0f;
        float b = (hex & 0xFF) / 255.0f;
        return ImVec4(r, g, b, alpha);
    }

    static bool RadioFrameColor(const char* label, int* v, int v_button,
        const ImVec4& color, const ImVec2& size = ImVec2(60, 24)) // smaller + more square
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 rectMax = ImVec2(pos.x + size.x, pos.y + size.y);

        bool hovered = ImGui::IsMouseHoveringRect(pos, rectMax);
        bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(color);

        // Square look (very small rounding or 0.0f for sharp corners)
        draw->AddRectFilled(pos, rectMax, fillCol, 3.0f);

        if (clicked)
            *v = v_button;

        ImGui::Dummy(size);
        return clicked;
    }



}

void DrawThemesPage() {
    ImGui::BeginChild("ThemesContent", ImVec2(0, 0), false);
    ImSmoothTabTransition();

    ImGui::Text("INTERFACE THEMES");
    ImGui::Separator();
    ImGui::Text("Select a preset theme or customize your own below.");
    ImGui::Spacing();

    float boxWidth = 28.0f;
    float boxHeight = 28.0f;

    // Track if theme should change AFTER this frame
    static int pendingThemeIndex = -1;
    static bool pendingCustomTheme = false;

    // All boxes in a row
    ImAdd::RadioFrameColor("", &currentThemeIndex, 0,
        ImVec4(0.547f, 0.139f, 0.837f, 1.0f), ImVec2(boxWidth, boxHeight));
    ImGui::SameLine();
    ImAdd::RadioFrameColor("", &currentThemeIndex, 1,
        ImVec4(0.75f, 0.75f, 0.75f, 1.0f), ImVec2(boxWidth, boxHeight));
    ImGui::SameLine();
    ImAdd::RadioFrameColor("", &currentThemeIndex, 2,
        ImVec4(0.694f, 0.6f, 1.0f, 1.0f), ImVec2(boxWidth, boxHeight));
    ImGui::SameLine();
    ImAdd::RadioFrameColor("", &currentThemeIndex, 3,
        ImVec4(0.6f, 0.784f, 1.0f, 1.0f), ImVec2(boxWidth, boxHeight));
    ImGui::SameLine();
    ImAdd::RadioFrameColor("", &currentThemeIndex, 4,
        ImVec4(0.847f, 0.631f, 0.988f, 1.0f), ImVec2(boxWidth, boxHeight));
 
    ImAdd::RadioFrameColor("", &currentThemeIndex, 5,
        ImVec4(0.55f, 0.90f, 0.65f, 1.0f), ImVec2(boxWidth, boxHeight));
    ImGui::SameLine();
    ImAdd::RadioFrameColor("", &currentThemeIndex, 6,
        ImVec4(0.910f, 0.420f, 0.420f, 1.0f), ImVec2(boxWidth, boxHeight));
    ImGui::SameLine();
    ImAdd::RadioFrameColor("", &currentThemeIndex, 7,
        ImVec4(0.847f, 0.631f, 0.988f, 1.0f), ImVec2(boxWidth, boxHeight));

    // When selection changes, SCHEDULE theme change for next frame
    static int prevThemeIndex = -1;
    if (currentThemeIndex != prevThemeIndex) {
        pendingThemeIndex = currentThemeIndex;
        pendingCustomTheme = false;
        prevThemeIndex = currentThemeIndex;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Custom theme toggle + editor
    ImGui::Text("CUSTOM THEME EDITOR");
    ImGui::Spacing();

    if (ImGui::Checkbox("Use Custom Theme", &useCustomTheme)) {
        pendingCustomTheme = true;
        pendingThemeIndex = -1;
    }

    if (useCustomTheme) {
        bool themeChanged = false;
        ImGui::Columns(2, "CustomThemeColumns", true);

        // Left side - base colors
        ImGui::Text("MAIN COLORS");
        themeChanged |= ImGui::ColorEdit3("Accent", (float*)&customTheme.accentColor);
        themeChanged |= ImGui::ColorEdit3("Background", (float*)&customTheme.backgroundColor);
        themeChanged |= ImGui::ColorEdit3("Sidebar", (float*)&customTheme.sidebarColor);
        themeChanged |= ImGui::ColorEdit3("Text", (float*)&customTheme.textColor);
        themeChanged |= ImGui::ColorEdit3("Border", (float*)&customTheme.borderColor);

        ImGui::NextColumn();

        // Right side - interactive
        ImGui::Text("INTERACTIVE");
        themeChanged |= ImGui::ColorEdit3("Button", (float*)&customTheme.buttonColor);
        themeChanged |= ImGui::ColorEdit3("Hover", (float*)&customTheme.buttonHover);
        themeChanged |= ImGui::ColorEdit3("Active", (float*)&customTheme.buttonActive);
        themeChanged |= ImGui::ColorEdit3("Header", (float*)&customTheme.headerColor);
        themeChanged |= ImGui::ColorEdit3("Checkmark", (float*)&customTheme.checkmarkColor);

        ImGui::Columns(1);

        // Schedule theme change if colors were edited
        if (themeChanged) {
            pendingCustomTheme = true;
            pendingThemeIndex = -1;
        }

        ImGui::Spacing();
        if (ImGui::Button("Save", ImVec2(120, 30))) {
            customTheme.name = "Custom Theme";
            customTheme.description = "User created";
            themePresets.push_back(customTheme);
            printf("[INFO] Custom theme saved.\n");
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(120, 30))) {
            customTheme = themePresets[0];
            pendingCustomTheme = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Base", ImVec2(120, 30))) {
            if (currentThemeIndex >= 0 && currentThemeIndex < themePresets.size()) {
                customTheme = themePresets[currentThemeIndex];
                pendingCustomTheme = true;
            }
        }
    }

    // ===== MENU EFFECTS SELECTOR =====
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("MENU EFFECTS");
    ImGui::Spacing();

    int currentFx = (int)g_menuEffect.current;
    if (ImGui::BeginCombo("Effect", MenuEffectNames[currentFx])) {
        for (int i = 0; i < (int)MenuEffect::COUNT; i++) {
            bool selected = (currentFx == i);
            if (ImGui::Selectable(MenuEffectNames[i], selected)) {
                g_menuEffect.current = (MenuEffect)i;
                ResetMenuEffect(); // reinit for new effect
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (g_menuEffect.current != MenuEffect::None) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset##fx")) ResetMenuEffect();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("CURRENT THEME: %s",
        useCustomTheme ? "Custom Theme" : themePresets[currentThemeIndex].name.c_str());

    ImSmoothTabEnd();
    ImGui::EndChild();

    // APPLY THEME CHANGES AFTER CHILD ENDS (safer timing)
    if (pendingThemeIndex != -1) {
        if (pendingThemeIndex >= 0 && pendingThemeIndex < themePresets.size()) {
            ApplyTheme(themePresets[pendingThemeIndex]);
            useCustomTheme = false;
        }
        pendingThemeIndex = -1;
    }
    else if (pendingCustomTheme) {
        ApplyTheme(customTheme);
        pendingCustomTheme = false;
    }
}
