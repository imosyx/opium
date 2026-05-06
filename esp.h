#pragma once
#include <vector>
#include <algorithm>
#include <string>

namespace ESP {

    static bool esp = false;

    struct BoneLine {
        ImVec2 a;
        ImVec2 b;
        bool isLocal;
    };

    struct BoxInfo {
        ImVec2 topLeft;
        ImVec2 bottomRight;
        ImVec2 footPosition;
        bool isLocal;
    };

    struct Box3DInfo {
        ImVec2 points[8];
        bool isLocal;
    };

    struct NameInfo {
        ImVec2 screenPos;   // top-center of player (above head)
        std::string name;
        bool isLocal;
        float distance;     // world distance for scale/alpha
    };

    // cfg
    bool enableESP = false;
    bool enableSnaplines = false;
    bool enableBones = false;
    bool enable3DBoxes = false;
    bool enableNames = false;

    float espColor[3] = { 1.f, 1.f, 1.f };
    float snaplineColor[3] = { 1.f, 1.f, 1.f };
    float boneColor[3] = { 1.f, 1.f, 1.f };
    float box3DColor[3] = { 1.f, 0.f, 0.f };
    float nameColor[3] = { 1.f, 1.f, 1.f };

    std::vector<BoxInfo>   boxes;
    std::vector<BoneLine>  bones;
    std::vector<Box3DInfo> boxes3D;
    std::vector<NameInfo>  names;


    inline void* GetMainCamera() {
        static auto camClass = IL2CPP::Class::Find("UnityEngine.Camera");
        static auto getMain = IL2CPP::Class::Utils::GetMethodPointer(camClass, "get_main");
        typedef void* (__cdecl* fn)(void*);
        return getMain ? ((fn)getMain)(nullptr) : nullptr;
    }

    inline Unity::Vector3 WorldToScreen(void* cam, Unity::Vector3 pos) {
        static auto camClass = IL2CPP::Class::Find("UnityEngine.Camera");
        static auto w2s = IL2CPP::Class::Utils::GetMethodPointer(camClass, "WorldToScreenPoint");
        typedef Unity::Vector3(__cdecl* fn)(void*, Unity::Vector3, int);
        return w2s ? ((fn)w2s)(cam, pos, 2) : Unity::Vector3();
    }

    inline std::string IL2CppStringToStdString(void* strObj) {
        if (!strObj) return "";
        // IL2CPP String layout:  [object header 0x10] [int32 length 0x10] [wchar_t chars 0x14]
        int32_t len = *(int32_t*)((uintptr_t)strObj + 0x10);
        if (len <= 0 || len > 256) return "";
        wchar_t* chars = (wchar_t*)((uintptr_t)strObj + 0x14);
        std::wstring ws(chars, len);
        int sz = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (sz <= 0) return "";
        std::string out(sz - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &out[0], sz, nullptr, nullptr);
        return out;
    }

    inline void* GetBodyData(void* avatar) {
        void* mvAvatar = *(void**)((uintptr_t)avatar + 0x20);
        if (!mvAvatar) return nullptr;
        void* mvBody = *(void**)((uintptr_t)mvAvatar + 0x1E8);
        if (!mvBody) return nullptr;
        static auto mvBodyClass = IL2CPP::Class::Find("MVBody");
        static auto getBodyData = IL2CPP::Class::Utils::GetMethodPointer(mvBodyClass, "get_BodyData");
        typedef void* (__cdecl* fn)(void*);
        return getBodyData ? ((fn)getBodyData)(mvBody) : nullptr;
    }

    inline Unity::Vector3 GetBoneWorld(void* transform) {
        static auto tClass = IL2CPP::Class::Find("UnityEngine.Transform");
        static auto getPos = IL2CPP::Class::Utils::GetMethodPointer(tClass, "get_position");
        typedef Unity::Vector3(__cdecl* fn)(void*);
        return getPos ? ((fn)getPos)(transform) : Unity::Vector3();
    }

    inline bool GetBoundingBox(void* cam, Unity::Vector3 pos, float height, BoxInfo& out) {
        Unity::Vector3 pts[8];
        float hw = 0.5f;
        pts[0] = { pos.x - hw, pos.y,          pos.z - hw };
        pts[1] = { pos.x + hw, pos.y,          pos.z - hw };
        pts[2] = { pos.x - hw, pos.y,          pos.z + hw };
        pts[3] = { pos.x + hw, pos.y,          pos.z + hw };
        pts[4] = { pos.x - hw, pos.y + height, pos.z - hw };
        pts[5] = { pos.x + hw, pos.y + height, pos.z - hw };
        pts[6] = { pos.x - hw, pos.y + height, pos.z + hw };
        pts[7] = { pos.x + hw, pos.y + height, pos.z + hw };

        ImGuiIO& io = ImGui::GetIO();
        float sh = io.DisplaySize.y;

        ImVec2 minVec(FLT_MAX, FLT_MAX);
        ImVec2 maxVec(-FLT_MAX, -FLT_MAX);
        bool ok = false;

        for (int i = 0; i < 8; i++) {
            auto s = WorldToScreen(cam, pts[i]);
            if (s.z <= 0) continue;
            ok = true;
            float y = sh - s.y;
            minVec.x = (std::min)(minVec.x, s.x);
            minVec.y = (std::min)(minVec.y, y);
            maxVec.x = (std::max)(maxVec.x, s.x);
            maxVec.y = (std::max)(maxVec.y, y);
        }
        if (!ok) return false;

        out.topLeft = minVec;
        out.bottomRight = maxVec;

        auto foot = WorldToScreen(cam, pos);
        out.footPosition = ImVec2(foot.x, sh - foot.y);
        return true;
    }

    inline bool Get3DBox(void* cam, Unity::Vector3 pos, float height, Box3DInfo& out) {
        Unity::Vector3 pts[8];
        float hw = 0.5f;
        pts[0] = { pos.x - hw, pos.y,          pos.z - hw };
        pts[1] = { pos.x + hw, pos.y,          pos.z - hw };
        pts[2] = { pos.x + hw, pos.y,          pos.z + hw };
        pts[3] = { pos.x - hw, pos.y,          pos.z + hw };
        pts[4] = { pos.x - hw, pos.y + height, pos.z - hw };
        pts[5] = { pos.x + hw, pos.y + height, pos.z - hw };
        pts[6] = { pos.x + hw, pos.y + height, pos.z + hw };
        pts[7] = { pos.x - hw, pos.y + height, pos.z + hw };

        ImGuiIO& io = ImGui::GetIO();
        float sh = io.DisplaySize.y;

        bool allValid = true;
        for (int i = 0; i < 8; i++) {
            auto s = WorldToScreen(cam, pts[i]);
            if (s.z <= 0) { allValid = false; break; }
            out.points[i] = ImVec2(s.x, sh - s.y);
        }
        return allValid;
    }

    inline void Update() {
        boxes.clear();
        bones.clear();
        boxes3D.clear();
        names.clear();

        void* thread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
        auto cam = GetMainCamera();
        if (!cam) { IL2CPP::Thread::Detach(thread); return; }

        auto avatars = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");
        auto aClass = IL2CPP::Class::Find("Avatar");

        auto getPos = IL2CPP::Class::Utils::GetMethodPointer(aClass, "get_Position");
        auto isLocal = IL2CPP::Class::Utils::GetMethodPointer(aClass, "get_IsLocal");

        static auto bodyDataClass = IL2CPP::Class::Find("BodyData");
        static auto getPartBone = IL2CPP::Class::Utils::GetMethodPointer(bodyDataClass, "GetPartBone", 1);

        typedef Unity::Vector3(__cdecl* fnPos)(void*, void*);
        typedef bool(__cdecl* fnLocal)(void*, void*);
        typedef void* (__cdecl* GetPartBone_t)(void*, int);

        ImGuiIO& io = ImGui::GetIO();
        float sh = io.DisplaySize.y;

        for (uint32_t i = 0; i < avatars->m_uMaxLength; i++) {
            auto av = avatars->operator[](i);
            if (!av) continue;

            bool local = ((fnLocal)isLocal)(av, nullptr);
            auto pos = ((fnPos)getPos)(av, nullptr);

            // -- 2D box (always compute when boxes OR snaplines OR names need it) --
            bool needBox = enableESP || enableSnaplines || enableNames;
            BoxInfo b; b.isLocal = local;
            bool boxOk = needBox ? GetBoundingBox(cam, pos, 2.f, b) : false;

            if (enableESP && boxOk)
                boxes.push_back(b);

            // -- 3D box --
            if (enable3DBoxes) {
                Box3DInfo b3d; b3d.isLocal = local;
                if (Get3DBox(cam, pos, 2.f, b3d)) boxes3D.push_back(b3d);
            }

            // -- Names --
            if (enableNames && boxOk) {
                // Get name from uiHandler (same offsets as playerlist)
                void* uiHandler = *(void**)((uintptr_t)av + 0x88);
                std::string pName = "Unknown";
                if (uiHandler) {
                    void* nameStrObj = *(void**)((uintptr_t)uiHandler + 0x38);
                    std::string n = IL2CppStringToStdString(nameStrObj);
                    if (!n.empty()) pName = n;
                }

                // World distance (rough, using foot position on screen as proxy)
                // We use the head screen pos = top of bounding box center
                ImVec2 headPos = ImVec2(
                    (b.topLeft.x + b.bottomRight.x) * 0.5f,
                    b.topLeft.y - 4.f   // slightly above the box
                );

                // Approximate distance from camera using box height: bigger box = closer
                float boxH = b.bottomRight.y - b.topLeft.y;
                float dist = (boxH > 0.f) ? (200.f / boxH) : 999.f;

                names.push_back({ headPos, pName, local, dist });
            }

            // -- Snaplines need foot position even without box rendering --
            // We store it inside BoxInfo, so add to boxes if only snaplines are on
            // and box wasn't already pushed
            if (!enableESP && enableSnaplines && boxOk && !local)
                boxes.push_back(b);  // used for foot positions only

            // -- Bones (skip local) --
            if (!enableBones || local) continue;

            void* bodyData = GetBodyData(av);
            if (!bodyData) continue;

            auto drawBone = [&](int a, int bIdx) {
                void* ta = ((GetPartBone_t)getPartBone)(bodyData, a);
                void* tb = ((GetPartBone_t)getPartBone)(bodyData, bIdx);
                if (!ta || !tb) return;

                auto wa = WorldToScreen(cam, GetBoneWorld(ta));
                auto wb = WorldToScreen(cam, GetBoneWorld(tb));
                if (wa.z <= 0 || wb.z <= 0) return;

                bones.push_back({
                    ImVec2(wa.x, sh - wa.y),
                    ImVec2(wb.x, sh - wb.y),
                    local
                    });
                };

            drawBone(1, 0);   // Torso → Head
            drawBone(1, 3);   // Torso → Left Arm
            drawBone(1, 2);   // Torso → Right Arm
            drawBone(1, 4);   // Torso → Right Upper Leg
            drawBone(4, 5);   // Right leg chain
            drawBone(1, 6);   // Torso → Left Upper Leg
            drawBone(6, 7);   // Left leg chain

            /*
            public enum PartIndex { Head, Torso, RArm, LArm, RUpLeg, RLowLeg, LUpLeg, LLowLeg, Holster, Size }
            */
        }

        IL2CPP::Thread::Detach(thread);
    }
    inline void DrawOutlinedText(ImDrawList* dl, ImFont* font, float fontSize,
        ImVec2 pos, ImU32 colFill, ImU32 colOutline,
        const char* text, bool centered = true)
    {
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text);
        ImVec2 drawPos = centered
            ? ImVec2(pos.x - textSize.x * 0.5f, pos.y - textSize.y * 0.5f)
            : pos;

        // bg pill
        const float padX = 3.f, padY = 1.5f;
        dl->AddRectFilled(
            ImVec2(drawPos.x - padX, drawPos.y - padY),
            ImVec2(drawPos.x + textSize.x + padX, drawPos.y + textSize.y + padY),
            IM_COL32(0, 0, 0, 160), 3.f
        );

        // drop shadow then fill
        const float o = 1.f;
        dl->AddText(font, fontSize, ImVec2(drawPos.x + o, drawPos.y + o), colOutline, text);
        dl->AddText(font, fontSize, drawPos, colFill, text);
    }

    inline void Render() {
        auto* dl = ImGui::GetBackgroundDrawList();
        ImGuiIO& io = ImGui::GetIO();

        ImU32 boxCol = IM_COL32(espColor[0] * 255, espColor[1] * 255, espColor[2] * 255, 255);
        ImU32 lineCol = IM_COL32(snaplineColor[0] * 255, snaplineColor[1] * 255, snaplineColor[2] * 255, 255);
        ImU32 boneCol = IM_COL32(boneColor[0] * 255, boneColor[1] * 255, boneColor[2] * 255, 255);
        ImU32 box3DCol = IM_COL32(box3DColor[0] * 255, box3DColor[1] * 255, box3DColor[2] * 255, 255);

        ImVec2 origin(io.DisplaySize.x * 0.5f, io.DisplaySize.y);

        // -- 2D Boxes --
        if (enableESP) {
            for (auto& b : boxes) {
                if (b.isLocal) continue;
                dl->AddRect(b.topLeft, b.bottomRight, boxCol, 0, 0, 2.f);
            }
        }

        // -- Snaplines  (independent of box toggle, uses stored foot positions) --
        if (enableSnaplines) {
            for (auto& b : boxes) {
                if (b.isLocal) continue;
                dl->AddLine(origin, b.footPosition, lineCol, 1.5f);
            }
        }

        // -- 3D Boxes --
        if (enable3DBoxes) {
            for (auto& b3d : boxes3D) {
                if (b3d.isLocal) continue;
                // bottom sq
                dl->AddLine(b3d.points[0], b3d.points[1], box3DCol, 1.5f);
                dl->AddLine(b3d.points[1], b3d.points[2], box3DCol, 1.5f);
                dl->AddLine(b3d.points[2], b3d.points[3], box3DCol, 1.5f);
                dl->AddLine(b3d.points[3], b3d.points[0], box3DCol, 1.5f);
                // top sq
                dl->AddLine(b3d.points[4], b3d.points[5], box3DCol, 1.5f);
                dl->AddLine(b3d.points[5], b3d.points[6], box3DCol, 1.5f);
                dl->AddLine(b3d.points[6], b3d.points[7], box3DCol, 1.5f);
                dl->AddLine(b3d.points[7], b3d.points[4], box3DCol, 1.5f);
                // verticals
                dl->AddLine(b3d.points[0], b3d.points[4], box3DCol, 1.5f);
                dl->AddLine(b3d.points[1], b3d.points[5], box3DCol, 1.5f);
                dl->AddLine(b3d.points[2], b3d.points[6], box3DCol, 1.5f);
                dl->AddLine(b3d.points[3], b3d.points[7], box3DCol, 1.5f);
            }
        }

        // -- Bones --
        if (enableBones) {
            for (auto& l : bones) {
                if (l.isLocal) continue;
                dl->AddLine(l.a, l.b, boneCol, 1.5f);
            }
        }

        // -- Names --
        if (enableNames) {
            ImFont* font = ImGui::GetFont();
            ImU32 nameCol = IM_COL32(nameColor[0] * 255, nameColor[1] * 255, nameColor[2] * 255, 255);
            ImU32 outlineCol = IM_COL32(0, 0, 0, 220);

            for (auto& n : names) {
                if (n.isLocal) continue;
                
                // Scale font size by distance: closer = bigger, capped
                float fontSize = ImClamp(16.f - n.distance * 0.03f, 11.f, 18.f);

                // Fade alpha at long range
                float alpha = ImClamp(1.f - (n.distance - 20.f) / 80.f, 0.35f, 1.f);
                ImU32 fillCol = IM_COL32(
                    nameColor[0] * 255,
                    nameColor[1] * 255,
                    nameColor[2] * 255,
                    (int)(alpha * 255)
                );
                ImU32 shadowCol = IM_COL32(0, 0, 0, (int)(alpha * 200));

                DrawOutlinedText(dl, font, fontSize, n.screenPos, fillCol, shadowCol,
                    n.name.c_str(), /*centered=*/true);
            }
        }
    }

    inline void RenderUI() {
        ToggleButton("##wallhacks", &esp); ImGui::SameLine(); ImGui::Text("wallhacks");

        if (esp) {
            Highlight();

            ImGui::Checkbox("##2D Boxes", &enableESP);       ImGui::SameLine(); ImGui::Text("2D Boxes");
            if (enableESP)

                ImGui::ColorEdit3("2D Box Color", espColor, ImGuiColorEditFlags_NoInputs);

            ImGui::Checkbox("##3D Boxes", &enable3DBoxes);   ImGui::SameLine(); ImGui::Text("3D Boxes");
            if (enable3DBoxes)
                ImGui::ColorEdit3("3D Box Color", box3DColor, ImGuiColorEditFlags_NoInputs);

            ImGui::Checkbox("##Snaplines", &enableSnaplines); ImGui::SameLine(); ImGui::Text("Snaplines");
            if (enableSnaplines)
                ImGui::ColorEdit3("Snapline Color", snaplineColor, ImGuiColorEditFlags_NoInputs);

            ImGui::Checkbox("##Bones", &enableBones);     ImGui::SameLine(); ImGui::Text("Bones");
            if (enableBones)
                ImGui::ColorEdit3("Bone Color", boneColor, ImGuiColorEditFlags_NoInputs);

            ImGui::Checkbox("##Names", &enableNames);     ImGui::SameLine(); ImGui::Text("Names");
            if (enableNames)
                ImGui::ColorEdit3("Name Color", nameColor, ImGuiColorEditFlags_NoInputs);

            EndHighlight();
        }
    }
}