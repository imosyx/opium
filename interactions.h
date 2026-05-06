#include <algorithm>
#include "Offsets.h"

void ApplyInteractionToAll(float damage, float impulseX, float impulseY, float impulseZ,
    uint8_t killedByType, bool isLocal) {

    if (!pProximityDamageAndImpulse_Create || !pInteractionDataHandlerBase_HandleInteraction) {
        printf("[ERROR] Interaction functions not initialized\n");
        return;
    }

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");

        if (!avatarList || avatarList->m_uMaxLength == 0) {
            printf("[WARN] No Avatar objects found\n");
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        Vector3 impulse = { impulseX, impulseY, impulseZ };
        InteractionData interactionData = pProximityDamageAndImpulse_Create(
            damage,
            impulse,
            killedByType,
            nullptr
        );

        int successCount = 0;

        for (int i = 0; i < avatarList->m_uMaxLength; i++) {
            auto avatar = avatarList->operator[](i);
            if (!avatar) continue;

            try {
                uintptr_t avatarPtr = (uintptr_t)avatar;
                void* interactionHandler = *(void**)(avatarPtr + 0x48);

                if (!interactionHandler) continue;

                bool result = pInteractionDataHandlerBase_HandleInteraction(
                    interactionHandler,
                    interactionData,
                    isLocal,
                    nullptr
                );

                if (result) {
                    successCount++;
                }
            }
            catch (...) {
                continue;
            }
        }

    }
    catch (...) {
        printf("[ERROR] Exception in ApplyInteractionToAll\n");
    }

    IL2CPP::Thread::Detach(m_pThisThread);
}

static bool showFOVCircle = false;
static float pieMenuFOV = 120.0f; // radius in pixels

void* GetPlayerInFOV() {
    if (!hasValidPlayer) return nullptr;

    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    void* closest = nullptr;
    float closestDist = FLT_MAX;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

    try {
        void* cam = ESP::GetMainCamera();
        if (!cam) { IL2CPP::Thread::Detach(pThread); return nullptr; }

        auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");
        if (!avatarList || avatarList->m_uMaxLength == 0) {
            IL2CPP::Thread::Detach(pThread); return nullptr;
        }

        for (int i = 0; i < avatarList->m_uMaxLength; i++) {
            auto avatar = avatarList->operator[](i);
            if (!avatar) continue;

            bool isLocal = *(bool*)((uintptr_t)avatar + 0x28);
            if (isLocal) continue;

            auto avatarClass = avatar->m_Object.m_pClass;
            if (!avatarClass) continue;

            auto posMethod = IL2CPP::Class::Utils::GetMethodPointer(avatarClass, "get_Position");
            if (!posMethod) continue;

            typedef Unity::Vector3(__cdecl* GetPos_t)(void*, void*);
            Unity::Vector3 worldPos = ((GetPos_t)posMethod)(avatar, nullptr);

            // head position
            Unity::Vector3 headPos = { worldPos.x, worldPos.y + 1.8f, worldPos.z };
            Unity::Vector3 screen = ESP::WorldToScreen(cam, headPos);
            if (screen.z <= 0) continue;

            float sh = io.DisplaySize.y;
            ImVec2 screenPos(screen.x, sh - screen.y);

            float dx = screenPos.x - center.x;
            float dy = screenPos.y - center.y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist < pieMenuFOV && dist < closestDist) {
                closestDist = dist;
                closest = avatar;
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(pThread);
    return closest;
}

int FindPlayerIndexByInstance(void* instance) {
    for (size_t i = 0; i < Config::discoveredPlayers.size(); i++) {
        if (Config::discoveredPlayers[i].instance == instance) {
            return (int)i;
        }
    }
    return -1;
}

void ApplyInteractionToPlayer(void* avatarComponent, float damage, Vector3 impulse, bool continuous) {
    if (!avatarComponent) {
        printf("[ERROR] avatarComponent is null\n");
        return;
    }

    if (!pProximityDamageAndImpulse_Create || !pInteractionDataHandlerBase_HandleInteraction) {
        printf("[ERROR] Interaction functions not initialized\n");
        return;
    }

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        uintptr_t avatarPtr = (uintptr_t)avatarComponent;
        void* interactionHandler = *(void**)(avatarPtr + 0x48);

        if (!interactionHandler) {
            printf("[ERROR] InteractionDataHandler is null at offset 0x48\n");
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        InteractionData interactionData = pProximityDamageAndImpulse_Create(
            damage,
            impulse,
            8,
            nullptr
        );

        bool result = pInteractionDataHandlerBase_HandleInteraction(
            interactionHandler,
            interactionData,
            false,
            nullptr
        );

        if (!result) {
            printf("[WARN] HandleInteraction returned false\n");
        }
    }
    catch (...) {
        printf("[ERROR] Exception in ApplyInteractionToPlayer\n");
    }

    IL2CPP::Thread::Detach(m_pThisThread);
}

bool IsPlayerFlying(void* playerInstance) {
    for (const auto& target : Config::activeFlyTargets) {
        if (target.instance == playerInstance) return true;
    }
    return false;
}

void UpdateActiveFlyTargets() {
    if (Config::activeFlyTargets.empty()) return;
    DWORD currentTime = GetTickCount64();

    for (auto it = Config::activeFlyTargets.begin(); it != Config::activeFlyTargets.end();) {
        if (currentTime - it->lastApplied >= 100) {
            void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

            try {
                auto handlerList = Unity::Object::FindObjectsOfType<Unity::CComponent>("InteractionDataHandlerBase");

                if (handlerList && handlerList->m_uMaxLength > 0) {
                    for (int i = 0; i < handlerList->m_uMaxLength; i++) {
                        auto handler = handlerList->operator[](i);
                        if (!handler) continue;

                        auto handlerGameObject = handler->GetGameObject();
                        if (handlerGameObject && handlerGameObject == it->instance) {
                            InteractionData flyData = {};
                            flyData.damage = 0.0f;
                            flyData.impulse = { 0.0f, 165.1f, 0.0f };
                            flyData.interactionType = 13;
                            flyData.playerKilledByType = 8;

                            pInteractionDataHandlerBase_HandleInteraction(handler, flyData, false, nullptr);
                            break;
                        }
                    }
                }
            }
            catch (...) {}

            IL2CPP::Thread::Detach(m_pThisThread);
            it->lastApplied = currentTime;
        }
        ++it;
    }
}

inline ImVec2 operator+(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); }
inline ImVec2 operator-(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x - b.x, a.y - b.y); }
inline ImVec2 operator*(const ImVec2& a, float b) { return ImVec2(a.x * b, a.y * b); }
inline ImVec2 operator/(const ImVec2& a, float b) { return ImVec2(a.x / b, a.y / b); }

struct PieMenu {
    bool open = false;
    int hovered = -1;
    int selected = -1;
    void* targetPlayer = nullptr;
    std::string targetPlayerName = "";
    RECT savedClipRect = {};

    // Config
    std::vector<std::string> items = { "Kill", "Fling", "Float", "Void", "Pull", "", "", ""};
    float outerRadius = 165.0f * 0.85f; // 140.25
    float innerRadius = 75.0f * 0.85f;  // 63.75

    // Animation state
    float openAnim = 0.0f;
    float hoverAnims[8] = { 0 };
    float rippleAnims[8] = { 0 };
    float globalTime = 0.0f;

    // Colors (dark modern theme)
    ImU32 colBg = IM_COL32(10, 10, 14, 255);
    ImU32 colSliceBase = IM_COL32(25, 25, 30, 240);
    ImU32 colSliceHover = IM_COL32(45, 45, 55, 255);
    ImU32 colSliceActive = IM_COL32(60, 60, 75, 255);
    ImU32 colAccent = IM_COL32(120, 80, 220, 255);
    ImU32 colText = IM_COL32(220, 220, 230, 255);
    ImU32 colTextDim = IM_COL32(120, 120, 130, 255);

    void Open() {
        open = true;
        hovered = -1;
        openAnim = 0.0f;
        globalTime = 0.0f;
        for (int i = 0; i < 8; i++) {
            hoverAnims[i] = 0.0f;
            rippleAnims[i] = 0.0f;
        }
        GetClipCursor(&savedClipRect);
        targetPlayer = GetPlayerInFOV();

        if (targetPlayer) {
            try {
                auto avatarComp = (Unity::CComponent*)targetPlayer;
                auto gameObj = avatarComp->GetGameObject();
                bool isFlying = IsPlayerFlying(gameObj);

                // Same method as playerlist: read UIHandler at +0x88, name string at +0x38
                targetPlayerName = "Unknown";
                void* uiHandler = *(void**)((uintptr_t)avatarComp + 0x88);
                if (uiHandler) {
                    void* nameStrObj = *(void**)((uintptr_t)uiHandler + 0x38);
                    if (nameStrObj) {
                        std::wstring wname = AntiFilter::IL2CPPStringToWide(nameStrObj);
                        if (!wname.empty()) {
                            int sz = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);
                            if (sz > 0) {
                                std::string utf8(sz - 1, 0);
                                WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &utf8[0], sz, nullptr, nullptr);
                                targetPlayerName = utf8;
                            }
                        }
                    }
                }

                if (isFlying) targetPlayerName += "  [FLYING]";
            }
            catch (...) {
                targetPlayerName = "Unknown";
            }
        }

        ClipCursor(NULL);
        ShowCursor(TRUE);
    }

    void Close() {
        open = false;
        targetPlayer = nullptr;
        ClipCursor(&savedClipRect);
        ShowCursor(FALSE);
    }

    float EaseOutBack(float x) {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1;
        return 1 + c3 * powf(x - 1, 3) + c1 * powf(x - 1, 2);
    }

    float EaseOutCubic(float x) {
        return 1 - powf(1 - x, 3);
    }

    void Update() {
        ImGuiIO& io = ImGui::GetIO();
        globalTime += io.DeltaTime;



        if (!open && ImGui::IsMouseClicked(2)) {
            Open();
        }
        if (!open) return;

        // Animation
        if (openAnim < 1.0f) {
            openAnim += io.DeltaTime * 6.0f;
            if (openAnim > 1.0f) openAnim = 1.0f;
        }

        float ease = EaseOutBack(openAnim);
        float currentOuter = outerRadius * ease;
        float currentInner = innerRadius * ease;

        ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        const int numSlices = 8;
        const float sliceAngle = IM_PI * 2.0f / numSlices;

        // Mouse constraint to ring
        ImVec2 mouseDelta = ImVec2(io.MousePos.x - center.x, io.MousePos.y - center.y);
        float mouseDist = sqrtf(mouseDelta.x * mouseDelta.x + mouseDelta.y * mouseDelta.y);

        if (mouseDist > 0.001f) {
            if (mouseDist > currentOuter) {
                float scale = currentOuter / mouseDist;
                io.MousePos = ImVec2(center.x + mouseDelta.x * scale, center.y + mouseDelta.y * scale);
            }
            else if (mouseDist < currentInner && mouseDist > 0.001f) {
                float scale = currentInner / mouseDist;
                io.MousePos = ImVec2(center.x + mouseDelta.x * scale, center.y + mouseDelta.y * scale);
            }
        }

        // Calculate hovered slice
        float angle = atan2f(mouseDelta.y, mouseDelta.x);
        if (angle < 0) angle += 2.0f * IM_PI;
        int newHover = (int)(angle / sliceAngle) % numSlices;

        // Update hover animations
        for (int i = 0; i < numSlices; i++) {
            float target = (i == newHover && !items[i].empty()) ? 1.0f : 0.0f;
            hoverAnims[i] += (target - hoverAnims[i]) * io.DeltaTime * 12.0f;
            if (rippleAnims[i] > 0) rippleAnims[i] -= io.DeltaTime * 2.0f;
        }
        hovered = newHover;

        // Background vignette
        draw->AddCircleFilled(center, currentOuter + 20, IM_COL32(0, 0, 0, (int)(100 * openAnim)), 64);

        // Draw slices
        for (int i = 0; i < numSlices; i++) {
            bool isEmpty = items[i].empty();
            float t = hoverAnims[i];

            // Calculate slice geometry
            float startAngle = i * sliceAngle - sliceAngle * 0.5f;
            float endAngle = startAngle + sliceAngle;

            // Expand on hover
            float expand = t * 8.0f;
            float rOut = currentOuter + expand;
            float rIn = currentInner - expand * 0.5f;

            // Color interpolation
            ImU32 base = isEmpty ? colSliceBase : (t > 0.5f ? colSliceActive : colSliceBase);
            ImU32 hover = isEmpty ? colSliceHover : colSliceHover;

            ImU32 col = IM_COL32(
                (int)(25 + (45 - 25) * t + (isEmpty ? 0 : 35 * rippleAnims[i])),
                (int)(25 + (45 - 25) * t + (isEmpty ? 0 : 35 * rippleAnims[i])),
                (int)(30 + (55 - 30) * t + (isEmpty ? 0 : 45 * rippleAnims[i])),
                (int)(240 + (15 * t))
            );

            // Draw slice as polygon
            ImVector<ImVec2> points;
            const int segments = 32;
            for (int s = 0; s <= segments; s++) {
                float a = startAngle + (endAngle - startAngle) * (s / (float)segments);
                points.push_back(ImVec2(center.x + cosf(a) * rOut, center.y + sinf(a) * rOut));
            }
            for (int s = segments; s >= 0; s--) {
                float a = startAngle + (endAngle - startAngle) * (s / (float)segments);
                points.push_back(ImVec2(center.x + cosf(a) * rIn, center.y + sinf(a) * rIn));
            }

            draw->AddConvexPolyFilled(points.Data, points.Size, col);

            // Hover accent line
            if (t > 0.01f && !isEmpty) {
                ImU32 accent = IM_COL32(
                    120 + (int)(80 * t),
                    80 + (int)(100 * t),
                    220 + (int)(35 * t),
                    (int)(255 * t)
                );
                draw->PathArcTo(center, rOut, startAngle, endAngle, 32);
                draw->PathStroke(accent, false, 3.0f);
            }
        }

        // Center info
        float centerRad = (currentInner - 8) * (0.8f + 0.2f * sinf(globalTime * 2.0f) * (targetPlayer ? 1.0f : 0.0f));
        draw->AddCircleFilled(center, centerRad, IM_COL32(15, 15, 20, 250), 32);
        draw->AddCircle(center, centerRad, IM_COL32(60, 60, 70, 200), 32, 2.0f);

        char buf[128];
        if (targetPlayer) {
            snprintf(buf, sizeof(buf), "%s", targetPlayerName.c_str());
        }
        else {
            snprintf(buf, sizeof(buf), "No Target");
        }

        ImVec2 textSize = ImGui::CalcTextSize(buf);
        ImVec2 textPos(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);

        ImU32 centerCol = targetPlayer ?
            IM_COL32(180, 140, 255, (int)(255 * openAnim)) :
            IM_COL32(100, 100, 110, (int)(200 * openAnim));

        draw->AddText(NULL, 18.0f, ImVec2(textPos.x + 1, textPos.y + 1),
            IM_COL32(0, 0, 0, 100), buf);
        draw->AddText(NULL, 18.0f, textPos, centerCol, buf);

        // Labels
        for (int i = 0; i < numSlices; i++) {
            if (items[i].empty()) continue;

            float midAngle = i * sliceAngle;
            float labelRadius = (currentInner + currentOuter) * 0.5f;
            ImVec2 pos(center.x + cosf(midAngle) * labelRadius,
                center.y + sinf(midAngle) * labelRadius);

            std::string label = items[i];


            ImVec2 lblSize = ImGui::CalcTextSize(label.c_str());
            pos.x -= lblSize.x * 0.5f;
            pos.y -= lblSize.y * 0.5f;

            float textAlpha = 0.7f + 0.3f * hoverAnims[i];
            float scale = 1.0f + hoverAnims[i] * 0.15f;

            ImU32 textCol = hoverAnims[i] > 0.5f ?
                IM_COL32(200, 170, 255, (int)(255 * openAnim)) :
                IM_COL32(180, 180, 190, (int)(200 * openAnim * textAlpha));

            // Shadow
            draw->AddText(NULL, 20.0f * scale, ImVec2(pos.x + 1, pos.y + 1),
                IM_COL32(0, 0, 0, 120), label.c_str());
            draw->AddText(NULL, 20.0f * scale, pos, textCol, label.c_str());
        }

        // Input handling
        if (ImGui::IsMouseReleased(2)) {
            if (hovered >= 0 && !items[hovered].empty() && targetPlayer) {
                selected = hovered;
                rippleAnims[hovered] = 1.0f;
                ExecuteAction();
            }
            Close();
        }
    }

    void ExecuteAction() {
        if (!targetPlayer || selected < 0) return;

        std::string action = items[selected];

        if (action == "Kill") {
            ApplyInteractionToPlayer(targetPlayer, 100.0f, Vector3{ 0, 0, 0 }, false);
        }
        else if (action == "Fling") {
            float randomX = ((rand() % 2000) - 1000) / 100.0f;
            float randomZ = ((rand() % 2000) - 1000) / 100.0f;
            float horizontalMag = sqrt(randomX * randomX + randomZ * randomZ);
            if (horizontalMag > 0.0f) {
                randomX /= horizontalMag;
                randomZ /= horizontalMag;
            }
            float totalTargetMagnitude = 5000.0f * sqrt(3.0f);
            float fixedY = 5000.0f;
            float remainingMagnitude = sqrtf(totalTargetMagnitude * totalTargetMagnitude - fixedY * fixedY);
            Vector3 flingImpulse = {
                randomX * remainingMagnitude,
                fixedY,
                randomZ * remainingMagnitude
            };
            ApplyInteractionToPlayer(targetPlayer, 0.0f, flingImpulse, false);
        }
        else if (action == "Float") {
            auto avatarComp = (Unity::CComponent*)targetPlayer;
            auto gameObj = avatarComp->GetGameObject();

            if (!gameObj) return;

            bool isFlying = false;
            for (auto it = Config::activeFlyTargets.begin(); it != Config::activeFlyTargets.end(); ++it) {
                if (it->instance == gameObj) {
                    Config::activeFlyTargets.erase(it);
                    isFlying = true;
                    break;
                }
            }

            if (!isFlying) {
                Config::ActiveFlyTarget newTarget;
                newTarget.instance = gameObj;
                newTarget.lastApplied = GetTickCount64();
                Config::activeFlyTargets.push_back(newTarget);
            }
        }
        else if (action == "Void") {
            union {
                uint32_t i;
                float f;
            } converter;
            converter.i = 0x7F7FFF00;
            float voidImpulse = converter.f;
            Vector3 impulse = { 0.0f, voidImpulse, 0.0f };
            ApplyInteractionToPlayer(targetPlayer, 0.0f, impulse, false);
        }
        else if (action == "Pull") {
            auto avatarComp = (Unity::CComponent*)targetPlayer;
            Vector3 targetPos = { 0, 0, 0 };

            try {
                auto avatarClass = avatarComp->m_Object.m_pClass;
                if (avatarClass) {
                    auto posMethod = IL2CPP::Class::Utils::GetMethodPointer(avatarClass, "get_Position");
                    if (posMethod) {
                        typedef Unity::Vector3(__cdecl* GetPositionFunc)(void*, void*);
                        auto getPos = (GetPositionFunc)posMethod;
                        Unity::Vector3 unityPos = getPos(avatarComp, nullptr);
                        targetPos.x = unityPos.x;
                        targetPos.y = unityPos.y;
                        targetPos.z = unityPos.z;
                    }
                }
            }
            catch (...) {
                return;
            }

            Vector3 direction = {
                lastKnownPosition.x - targetPos.x,
                lastKnownPosition.y - targetPos.y,
                lastKnownPosition.z - targetPos.z
            };

            float distance = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);

            if (distance < 0.1f) return;

            direction.x /= distance;
            direction.y /= distance;
            direction.z /= distance;

            float pullForce = 200.0f + (distance * 800.0f);
            if (pullForce > 1000.0f) pullForce = 2000.0f;

            Vector3 pullImpulse = {
                direction.x * pullForce,
                direction.y * pullForce + 300.0f,
                direction.z * pullForce
            };

            ApplyInteractionToPlayer(targetPlayer, 0.0f, pullImpulse, false);
        }

        selected = -1;
    }

    std::string GetSelectedItem() {
        if (selected >= 0 && selected < (int)items.size())
            return items[selected];
        return "";
    }
};

DWORD WINAPI FlyAllPlayersThread(LPVOID param) {
    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");

        if (!avatarList || avatarList->m_uMaxLength == 0) {
            IL2CPP::Thread::Detach(m_pThisThread);
            return 0;
        }

        int successCount = 0;
        for (int i = 0; i < avatarList->m_uMaxLength; i++) {
            auto avatar = avatarList->operator[](i);
            if (!avatar) continue;

            auto gameObj = avatar->GetGameObject();
            if (!gameObj || gameObj == localPlayerInstance) continue;

            bool alreadyFlying = false;
            for (const auto& target : Config::activeFlyTargets) {
                if (target.instance == gameObj) {
                    alreadyFlying = true;
                    break;
                }
            }

            if (!alreadyFlying) {
                Config::ActiveFlyTarget newTarget;
                newTarget.instance = gameObj;
                newTarget.lastApplied = GetTickCount64();
                Config::activeFlyTargets.push_back(newTarget);
                successCount++;
            }

            Sleep(10);
        }
    }
    catch (...) {
        printf("[ERROR] Exception in FlyAllPlayersThread\n");
    }

    IL2CPP::Thread::Detach(m_pThisThread);
    return 0;
}

DWORD WINAPI HealAllPlayersThread(LPVOID param) {
    EquipWeapon(70); // HealRay
    Sleep(300);

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
        typedef InteractionData(*HealRayHitPackage_Create_t)();
        auto pHealCreate = (HealRayHitPackage_Create_t)(base + Offsets::HealRayHitPackage::Create);

        for (int run = 0; run < 5; run++) {
            auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");
            if (!avatarList || avatarList->m_uMaxLength == 0) break;

            for (int i = 0; i < avatarList->m_uMaxLength; i++) {
                auto avatar = avatarList->operator[](i);
                if (!avatar) continue;

                auto gameObj = avatar->GetGameObject();
                if (!gameObj || gameObj == localPlayerInstance) continue;

                uintptr_t avatarPtr = (uintptr_t)avatar;
                void* interactionHandler = *(void**)(avatarPtr + 0x48);
                if (!interactionHandler) continue;

                InteractionData healData = pHealCreate();

                pInteractionDataHandlerBase_HandleInteraction(
                    interactionHandler, healData, false, nullptr);

                Sleep(50);
            }
            Sleep(50);
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
    return 0;
}

DWORD WINAPI SlapKillAllThread(LPVOID param) {
    EquipWeapon(65); // SlapGun
    Sleep(300); // let equip settle

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
        typedef InteractionData(*SlapGunHitPackage_Create_t)(Vector3);
        auto pSlapCreate = (SlapGunHitPackage_Create_t)(base + Offsets::SlapGunHitPackage::Create);

        for (int run = 0; run < 3; run++) {
            auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");
            if (!avatarList || avatarList->m_uMaxLength == 0) break;

            for (int i = 0; i < avatarList->m_uMaxLength; i++) {
                auto avatar = avatarList->operator[](i);
                if (!avatar) continue;

                auto gameObj = avatar->GetGameObject();
                if (!gameObj || gameObj == localPlayerInstance) continue;

                uintptr_t avatarPtr = (uintptr_t)avatar;
                void* interactionHandler = *(void**)(avatarPtr + 0x48);
                if (!interactionHandler) continue;

                // Big upward slap impulse to kill
                Vector3 slapImpulse = { 0.0f, 500.0f, 0.0f };
                InteractionData slapData = pSlapCreate(slapImpulse);

                pInteractionDataHandlerBase_HandleInteraction(
                    interactionHandler, slapData, false, nullptr);

                Sleep(30);
            }
            Sleep(50);
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
    return 0;
}

DWORD WINAPI FlameAllThread(LPVOID param) {
    EquipWeapon(10); // Flamethrower
    Sleep(300);

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
        typedef InteractionData(*FlamethrowerHitPackage_Create_t)();
        auto pFlameCreate = (FlamethrowerHitPackage_Create_t)(base + Offsets::FlamethrowerHitPackage::Create);

        auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");
        if (avatarList && avatarList->m_uMaxLength > 0) {
            for (int i = 0; i < avatarList->m_uMaxLength; i++) {
                auto avatar = avatarList->operator[](i);
                if (!avatar) continue;

                auto gameObj = avatar->GetGameObject();
                if (!gameObj || gameObj == localPlayerInstance) continue;

                uintptr_t avatarPtr = (uintptr_t)avatar;
                void* interactionHandler = *(void**)(avatarPtr + 0x48);
                if (!interactionHandler) continue;

                InteractionData flameData = pFlameCreate();

                pInteractionDataHandlerBase_HandleInteraction(
                    interactionHandler, flameData, false, nullptr);

                Sleep(30);
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
    return 0;
}

void UpdateCustomInteractionLoop() {
    if (!Config::enableCustomInteractionLoop) return;

    DWORD currentTime = GetTickCount64();
    if (currentTime - Config::lastCustomInteraction < Config::customInteractionInterval) {
        return;
    }

    Config::lastCustomInteraction = currentTime;

    static float customDamage = 10.0f;
    static Vector3 customImpulse = { 0.0f, 100.0f, 0.0f };

    if (Config::targetSpecificPlayerForCustom) {
        if (Config::customInteractionTarget) {
            try {
                auto gameObj = (Unity::CGameObject*)Config::customInteractionTarget;
                if (gameObj && gameObj->m_CachedPtr != nullptr) {
                    ApplyInteractionToPlayer(Config::customInteractionTarget,
                        customDamage, customImpulse, true);
                }
            }
            catch (...) {}
        }
    }
    else {
        ApplyInteractionToAll(customDamage, customImpulse.x,
            customImpulse.y, customImpulse.z, 8, false);
    }
}

void HealTarget(void* targetInstance) {
    if (!targetInstance) return;
    EquipWeapon(70);
    uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    typedef InteractionData(*HealRayHitPackage_Create_t)();
    auto pHealCreate = (HealRayHitPackage_Create_t)(base + Offsets::HealRayHitPackage::Create);
    void* avatarComp = GetAvatarComponentFromGameObject(targetInstance);
    if (!avatarComp) return;
    void* interactionHandler = *(void**)((uintptr_t)avatarComp + 0x48);
    if (!interactionHandler) return;
    pInteractionDataHandlerBase_HandleInteraction(interactionHandler, pHealCreate(), false, nullptr);

}
void FlameTarget(void* targetInstance) {
    if (!targetInstance) return;
    EquipWeapon(10);
    uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    typedef InteractionData(*FlamethrowerHitPackage_Create_t)();
    auto pFlameCreate = (FlamethrowerHitPackage_Create_t)(base + Offsets::FlamethrowerHitPackage::Create);
    void* avatarComp = GetAvatarComponentFromGameObject(targetInstance);
    if (!avatarComp) return;
    void* interactionHandler = *(void**)((uintptr_t)avatarComp + 0x48);
    if (!interactionHandler) return;
    pInteractionDataHandlerBase_HandleInteraction(interactionHandler, pFlameCreate(), false, nullptr);

}


void SlapTarget(void* targetInstance) {
    if (!targetInstance) return;
    EquipWeapon(65);

    uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    typedef InteractionData(*SlapGunHitPackage_Create_t)(Vector3);
    auto pSlapCreate = (SlapGunHitPackage_Create_t)(base + Offsets::SlapGunHitPackage::Create);

    void* avatarComp = GetAvatarComponentFromGameObject(targetInstance);
    if (!avatarComp) return;

    void* interactionHandler = *(void**)((uintptr_t)avatarComp + 0x48);
    if (!interactionHandler) return;

    for (int i = 0; i < 5; i++) {
        pInteractionDataHandlerBase_HandleInteraction(interactionHandler, pSlapCreate({ 0.0f, 0.0f, 0.0f }), false, nullptr);
    } // each hit like 25 or 20 health
    
}