#pragma once
#include "esp.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>


inline Unity::Vector3 operator-(const Unity::Vector3& a, const Unity::Vector3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

namespace Aimbot
{
    bool& Enable() { static bool v = false; return v; }
    bool& IsLocked() { static bool v = false; return v; }
    float& FovCircle() { static float v = 120.0f; return v; }
    float* AimbotColor() { static float c[3] = { 0.9f, 0.9f, 1.0f }; return c; }
    void*& LockedTarget() { static void* ptr = nullptr; return ptr; }
    bool& WasKeyPressed() { static bool v = false; return v; }

    struct TargetInfo
    {
        void* avatar = nullptr;
        Unity::Vector3 worldPos{};
        Unity::Vector3 bodyPos{};
        ImVec2 screenPos{};
        float distanceToCenter = 999999.f;
        bool valid = false;
        TargetInfo() = default;
    };

    namespace Cache
    {
        static bool initialized = false;
        static void* get_Position = nullptr;
        static void* get_IsLocal = nullptr;
        static void* get_Transform = nullptr;

        static void Init()
        {
            if (initialized) return;
            auto* avatarClass = IL2CPP::Class::Find("Avatar");
            if (!avatarClass) return;
            get_Position = IL2CPP::Class::Utils::GetMethodPointer(avatarClass, "get_Position");
            get_IsLocal = IL2CPP::Class::Utils::GetMethodPointer(avatarClass, "get_IsLocal");
            get_Transform = IL2CPP::Class::Utils::GetMethodPointer(avatarClass, "get_transform");
            initialized = get_Position && get_IsLocal && get_Transform;
        }
    }

    static std::vector<TargetInfo> targets;
    static void* localAvatar = nullptr;
    static Unity::Vector3 localPos{};

    static void* fpCamera = nullptr;
    static int camRefreshCounter = 0;

    static ImVec2 GetScreenCenter()
    {
        ImGuiIO& io = ImGui::GetIO();
        return { io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f };
    }

    static float Distance2D(const ImVec2& a, const ImVec2& b)
    {
        float dx = a.x - b.x, dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    static bool IsValidPointer(const void* p)
    {
        if (!p) return false;
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        return addr > 0x10000ULL && addr < 0x7FFFFFFFFFFFULL;
    }

    static bool SafeRead(void* addr, void* out, size_t sz)
    {
        __try {
            memcpy(out, addr, sz);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool PointerLooksAlive(void* obj)
    {
        if (!IsValidPointer(obj)) return false;
        uintptr_t vtable = 0;
        if (!SafeRead(obj, &vtable, sizeof(vtable))) return false;
        if (!IsValidPointer((void*)vtable)) return false;
        uintptr_t fn = 0;
        if (!SafeRead((void*)vtable, &fn, sizeof(fn))) return false;
        return IsValidPointer((void*)fn);
    }

    static bool TrySetCameraRotation(void* cam, Vector2 rot)
    {
        if (!PointerLooksAlive(cam)) return false;
        if (std::isnan(rot.x) || std::isnan(rot.y)) return false;
        if (std::isinf(rot.x) || std::isinf(rot.y)) return false;

        constexpr uintptr_t ROT_OFFSET = 0x78;
        uintptr_t dst = reinterpret_cast<uintptr_t>(cam) + ROT_OFFSET;
        if (!IsValidPointer((void*)dst)) return false;

        __try {
            *(Vector2*)dst = rot;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static Unity::Vector3 GetBodyCenter(void* avatar)
    {
        if (!PointerLooksAlive(avatar) || !Cache::get_Position) return {};
        __try {
            auto pos = reinterpret_cast<Unity::Vector3(__cdecl*)(void*, void*)>(Cache::get_Position)(avatar, nullptr);
            if (std::isnan(pos.x) || std::isnan(pos.y) || std::isnan(pos.z)) return {};
            if (std::isinf(pos.x) || std::isinf(pos.y) || std::isinf(pos.z)) return {};
            pos.y += 0.85f;
            return pos;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return {};
        }
    }

    // throttled - FindObjectsOfType during weapon switch is likely the crash source
    static void RefreshCamera()
    {
        if (++camRefreshCounter < 30) return;
        camRefreshCounter = 0;
        __try {
            auto* arr = Unity::Object::FindObjectsOfType<Unity::CComponent>("FirstPersonCamera");
            if (!arr || arr->m_uMaxLength == 0) { fpCamera = nullptr; return; }
            void* cam = arr->operator[](0);
            fpCamera = PointerLooksAlive(cam) ? cam : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            fpCamera = nullptr;
        }
    }

    static void RefreshTargets()
    {
        targets.clear();
        localAvatar = nullptr;
        localPos = {};

        Cache::Init();
        if (!Cache::initialized) return;

        __try {
            auto* avatars = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");
            if (!avatars || avatars->m_uMaxLength == 0) return;

            targets.reserve(avatars->m_uMaxLength);

            for (size_t i = 0; i < avatars->m_uMaxLength; ++i)
            {
                void* av = nullptr;
                __try { av = avatars->operator[](i); }
                __except (EXCEPTION_EXECUTE_HANDLER) { continue; }

                if (!PointerLooksAlive(av)) continue;

                bool isLocal = false;
                Unity::Vector3 pos{};

                __try {
                    isLocal = reinterpret_cast<bool(__cdecl*)(void*, void*)>(Cache::get_IsLocal)(av, nullptr);
                    pos = reinterpret_cast<Unity::Vector3(__cdecl*)(void*, void*)>(Cache::get_Position)(av, nullptr);
                    if (std::isnan(pos.x) || std::isinf(pos.x)) continue;
                }
                __except (EXCEPTION_EXECUTE_HANDLER) { continue; }

                if (isLocal) {
                    localAvatar = av;
                    localPos = pos;
                    continue;
                }

                Unity::Vector3 body = GetBodyCenter(av);
                if (body.x == 0.f && body.y == 0.f && body.z == 0.f) continue;

                TargetInfo t;
                t.avatar = av;
                t.worldPos = pos;
                t.bodyPos = body;
                t.valid = true;
                targets.push_back(t);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            targets.clear();
        }
    }

    static void* FindClosestTargetInFOV()
    {
        void* best = nullptr;
        float bestDist = FovCircle() + 1.0f;
        for (const auto& t : targets) {
            if (!t.valid) continue;
            if (t.distanceToCenter >= bestDist) continue;
            bestDist = t.distanceToCenter;
            best = t.avatar;
        }
        return best;
    }

    static Vector2 CalcAngle(const Unity::Vector3& from, const Unity::Vector3& to)
    {
        Vector2 ang{};
        Unity::Vector3 delta = to - from;
        float hyp = std::sqrt(delta.x * delta.x + delta.z * delta.z);
        if (hyp < 0.001f) return ang;
        ang.x = -std::atan2(delta.y, hyp) * (180.0f / 3.141592653589793f);
        ang.y = std::atan2(delta.x, delta.z) * (180.0f / 3.141592653589793f);
        return ang;
    }

    static void Update()
    {
        if (!Enable()) {
            LockedTarget() = nullptr;
            IsLocked() = false;
            return;
        }

        RefreshCamera();
        if (!PointerLooksAlive(fpCamera)) return;

        auto mainCam = ESP::GetMainCamera();
        if (!mainCam) return;

        RefreshTargets();

        if (!PointerLooksAlive(localAvatar)) {
            localAvatar = nullptr;
            localPos = {};
            LockedTarget() = nullptr;
            IsLocked() = false;
            return;
        }

        if (localPos.x == 0.f && localPos.z == 0.f) return;

        ImVec2 center = GetScreenCenter();
        float  screenH = ImGui::GetIO().DisplaySize.y;

        for (auto& t : targets)
        {
            if (!t.valid) continue;
            if (!PointerLooksAlive(t.avatar)) { t.valid = false; continue; }

            auto w2s = ESP::WorldToScreen(mainCam, t.bodyPos);
            if (w2s.z <= 0.001f) { t.distanceToCenter = 999999.f; continue; }

            ImVec2 sp{ w2s.x, screenH - w2s.y };
            t.screenPos = sp;
            t.distanceToCenter = Distance2D(sp, center);
        }

        bool keyDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        if (keyDown && !WasKeyPressed()) {
            IsLocked() = !IsLocked();
            if (!IsLocked()) LockedTarget() = nullptr;
        }
        WasKeyPressed() = keyDown;

        if (IsLocked() && LockedTarget() && !PointerLooksAlive(LockedTarget())) {
            LockedTarget() = nullptr;
            IsLocked() = false;
        }

        if (IsLocked() && LockedTarget())
        {
            bool  found = false;
            float d = 999999.f;
            for (const auto& t : targets) {
                if (t.avatar != LockedTarget()) continue;
                if (!t.valid || !PointerLooksAlive(t.avatar)) {
                    LockedTarget() = nullptr;
                    IsLocked() = false;
                    return;
                }
                found = true;
                d = t.distanceToCenter;
                break;
            }
            if (!found || d > FovCircle()) {
                LockedTarget() = nullptr;
                IsLocked() = false;
            }
        }

        if (IsLocked() && !LockedTarget())
            LockedTarget() = FindClosestTargetInFOV();

        if (!IsLocked() || !LockedTarget()) return;
        if (!PointerLooksAlive(fpCamera))   return;

        for (const auto& t : targets)
        {
            if (t.avatar != LockedTarget()) continue;
            if (!t.valid || !PointerLooksAlive(t.avatar)) break;

            Unity::Vector3 eye = localPos;
            eye.y += 1.65f;

            Vector2 angles = CalcAngle(eye, t.bodyPos);
            TrySetCameraRotation(fpCamera, angles);
            break;
        }
    }

    static void Render()
    {
        if (!Enable()) return;

        auto* dl = ImGui::GetBackgroundDrawList();
        ImVec2 c = GetScreenCenter();

        ImU32 circleCol = IM_COL32(
            (int)(AimbotColor()[0] * 255),
            (int)(AimbotColor()[1] * 255),
            (int)(AimbotColor()[2] * 255),
            IsLocked() ? 220 : 100
        );
        dl->AddCircle(c, FovCircle(), circleCol, 64, 1.5f);

        float cs = 12.f;
        ImU32 crossCol = IsLocked() ? IM_COL32(60, 255, 80, 240) : IM_COL32(200, 200, 220, 160);
        dl->AddLine({ c.x - cs, c.y }, { c.x + cs, c.y }, crossCol, 1.4f);
        dl->AddLine({ c.x, c.y - cs }, { c.x, c.y + cs }, crossCol, 1.4f);

        if (IsLocked() && LockedTarget())
            dl->AddText({ c.x + 20, c.y - 32 }, IM_COL32(80, 255, 100, 255), "locked");
    }

    static void RenderUI()
    {
        ImGui::Text("Aimbot");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
        ToggleButton("Aimbot (RMB)", &Enable());
        if (Enable())
        {
            ImGui::Indent();
            ImGui::SliderFloat("FOV", &FovCircle(), 30.f, 500.f, "%.0f px");
            ImGui::ColorEdit3("Circle", AimbotColor());
            ImGui::Unindent();
        }
    }
}