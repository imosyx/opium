// ============ PLAYERLIST.H ============
#define NOMINMAX
#include <algorithm>
static std::unordered_map<int, CollapseState> g_cs;

static int g_ppSelectedActorNr = -1;
static int g_ppSidebarTab = 0;

static std::unordered_map<int, std::array<char, 256>> g_ppMimicBuf;
static std::unordered_map<int, bool> g_ppMimicSays;
static std::unordered_map<int, float> g_ppAvatarScale;

inline int GetOrAddTarget(const CachedPlayer& p) {
    for (auto& t : Config::targetedPlayers)
        if (t.instance == p.gameObject) return t.id;

    TargetedPlayer nt;
    nt.instance = p.gameObject;
    nt.name = p.name;
    nt.id = Config::nextTargetId++;
    nt.isFlying = false;
    nt.enableRadiusKill = false;
    nt.radiusKillDistance = 10.f;
    nt.radiusKillInterval = 500;
    nt.lastRadiusKill = 0;
    Config::targetedPlayers.push_back(nt);
    return nt.id;
}

inline bool SidebarButton(const char* label, int tabIndex, int currentTab, const ImVec4& activeColor, float width) {
    bool isActive = (tabIndex == currentTab);
    ImVec4 btnColor = isActive ? activeColor : ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
    ImVec4 hoverColor = isActive ? activeColor : ImVec4(0.30f, 0.30f, 0.30f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

    bool clicked = ImGui::Button(label, ImVec2(width, 24));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

static void* g_pendingAvatarCopyTarget = nullptr;
static bool g_pendingAvatarCopy = false;

static void DoAvatarCubesCopy() {
    static const char* limbNames[] = { "Head","Torso","RArm","LArm","RUpLeg","RLowLeg","LUpLeg","LLowLeg" };
    uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    static auto pGetBodyPart = (void* (__cdecl*)(void*, void*))(base + 0x4732B0);

    struct Bounds { float cx, cy, cz, ex, ey, ez; };
    static auto pGetBounds = (Bounds(__cdecl*)(void*))(base + 0x48BAA0);
    static auto pGetCubeAt = (void* (__cdecl*)(void*, Builder::Vec3i, void*))(base + 0x48A2E0);

    void* ac = GetAvatarComponentFromGameObject(g_pendingAvatarCopyTarget);
    if (!ac) { PushNotification("ERR: no avatar comp", NotificationType::Error, 3.0f); return; }

    void* mvAvatar = *(void**)((uintptr_t)ac + 0x20);
    if (!mvAvatar) { PushNotification("ERR: no mvAvatar", NotificationType::Error, 3.0f); }

    void* mvBody = *(void**)((uintptr_t)mvAvatar + 0x1E8);
    if (!mvBody) { PushNotification("ERR: no mvBody", NotificationType::Error, 3.0f); }

    std::ostringstream json;
    json << "{\n";
    bool firstLimb = true;

    for (const char* limbName : limbNames) {
        void* partStr = IL2CPP::String::New(limbName);
        if (!partStr) continue;

        void* cubeModelInst = pGetBodyPart(mvBody, partStr);
        if (!cubeModelInst) continue;

        Bounds b{};
        b = pGetBounds(cubeModelInst);

        int x0 = (int)floorf(b.cx - b.ex - 1), x1 = (int)ceilf(b.cx + b.ex + 1);
        int y0 = (int)floorf(b.cy - b.ey - 1), y1 = (int)ceilf(b.cy + b.ey + 1);
        int z0 = (int)floorf(b.cz - b.ez - 1), z1 = (int)ceilf(b.cz + b.ez + 1);

        auto clamp16 = [](int v) { return v < -64 ? -64 : v > 64 ? 64 : v; };
        x0 = clamp16(x0); x1 = clamp16(x1);
        y0 = clamp16(y0); y1 = clamp16(y1);
        z0 = clamp16(z0); z1 = clamp16(z1);

        std::vector<Builder::SavedVoxel> voxels;

        for (int x = x0; x <= x1; x++)
            for (int y = y0; y <= y1; y++)
                for (int z = z0; z <= z1; z++) {
                    Builder::Vec3i pos((int16_t)x, (int16_t)y, (int16_t)z);
                    void* cube = pGetCubeAt(cubeModelInst, pos, nullptr);
                    if (!cube) continue;

                    Builder::SavedVoxel sv;
                    sv.pos = pos;
                    if (!Builder::ReadCubeData(cube, sv.corners, sv.mat)) continue;
                    voxels.push_back(sv);
                }

        std::sort(voxels.begin(), voxels.end(), [](const Builder::SavedVoxel& a, const Builder::SavedVoxel& b) {
            if (a.pos.y != b.pos.y) return a.pos.y < b.pos.y;
            if (a.pos.x != b.pos.x) return a.pos.x < b.pos.x;
            return a.pos.z < b.pos.z;
            });

        if (!firstLimb) json << ",\n";
        firstLimb = false;

        json << "  \"" << limbName << "\": {\n";
        json << "    \"cubeCount\": " << voxels.size() << ",\n";
        json << "    \"voxels\": [\n";
        for (size_t i = 0; i < voxels.size(); i++) {
            auto& v = voxels[i];
            json << "      {\"x\":" << v.pos.x << ",\"y\":" << v.pos.y << ",\"z\":" << v.pos.z
                << ",\"mat\":" << (int)v.mat << ",\"corners\":[";
            for (int c = 0; c < 8; c++) { json << (int)v.corners[c]; if (c < 7) json << ","; }
            json << "]}";
            if (i + 1 < voxels.size()) json << ",";
            json << "\n";
        }
        json << "    ]\n  }";
    }

    json << "\n}";
    std::string result = json.str();
    ImGui::SetClipboardText(result.c_str());

    char buf[64];
    snprintf(buf, sizeof(buf), "%zu chars copied", result.size());
    PushNotification(std::string("AVATAR CUBES: ") + buf, NotificationType::Info, 3.0f);
}

static void DoLimbCopy() {
    static const char* pNames[] = { "Head","Torso","RArm","LArm","RUpLeg","RLowLeg","LUpLeg","LLowLeg","Holster" };
    uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");

    static auto pGetBodyPart = (void* (__cdecl*)(void*, void*))(base + 0x4732B0);

    void* ac = GetAvatarComponentFromGameObject(g_pendingLimbTarget);
    if (!ac) {
        PushNotification("LIMB ERR: no avatar comp", NotificationType::Error, 3.0f);
        return; }

    void* mvAvatar = *(void**)((uintptr_t)ac + 0x20);
    if (!mvAvatar) { PushNotification("LIMB ERR: mvavatar", NotificationType::Error, 3.0f); }

    void* mvBody = *(void**)((uintptr_t)mvAvatar + 0x1E8);
    if (!mvBody) { PushNotification("LIMB ERR: mvbody", NotificationType::Error, 3.0f); return; }

    void* partStr = IL2CPP::String::New(pNames[g_pendingLimbIndex]);
    if (!partStr) { PushNotification("LIMB ERR: partstr", NotificationType::Error, 3.0f); return; }

    void* cubeModelInst = pGetBodyPart(mvBody, partStr);
    if (!cubeModelInst) { PushNotification("LIMB ERR: part not found", NotificationType::Error, 3.0f); return; }

    // MVWorldObject.id
    int cubeModelID = *(int32_t*)((uintptr_t)cubeModelInst + 0x10);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", cubeModelID);
    ImGui::SetClipboardText(buf);

    PushNotification(std::string("COPIED: ") + buf, NotificationType::Success, 3.0f);
}

void UpdatePrivateChat() {
    if (!Config::enablePrivateChat || !Config::privateChatTarget ||
        !localPlayerInstance || !hasValidPlayer) return;

    DWORD now = GetTickCount64();
    if (now - Config::lastPrivateChatTP < 50) return; // 20hz
    Config::lastPrivateChatTP = now;

    Vector3 targetPos = GetPlayerPosition(Config::privateChatTarget);
    if (targetPos.x == 0 && targetPos.y == 0 && targetPos.z == 0) return;

    const float PRIV_RADIUS = 290.0f; // inside target's 304m hear range
    const float SAFE_DIST = 305.0f; // must be >= this from all OTHER players

    static float angle = 0.0f;
    angle += 15.0f;
    if (angle >= 360.0f) angle -= 360.0f;

    float bestX = targetPos.x, bestY = targetPos.y + PRIV_RADIUS, bestZ = targetPos.z;
    bool found = false;

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    try {
        auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");

        // Try horizontal ring first (24 angles)
        for (int a = 0; a < 24 && !found; a++) {
            float testAngle = (angle + a * 15.0f) * 0.01745329251f;
            float cx = targetPos.x + cosf(testAngle) * PRIV_RADIUS;
            float cy = targetPos.y; // same height as target
            float cz = targetPos.z + sinf(testAngle) * PRIV_RADIUS;

            bool tooClose = false;

            if (avatarList && avatarList->m_uMaxLength > 0) {
                for (int i = 0; i < (int)avatarList->m_uMaxLength; i++) {
                    auto avatar = avatarList->operator[](i);
                    if (!avatar) continue;

                    bool isLocal = *(bool*)((uintptr_t)avatar + 0x28);
                    if (isLocal) continue;

                    auto gameObj = avatar->GetGameObject();
                    if (gameObj == Config::privateChatTarget) continue; // target is fine

                    Vector3 otherPos = { 0,0,0 };
                    try {
                        auto avatarClass = avatar->m_Object.m_pClass;
                        if (!avatarClass) continue;
                        auto posMethod = IL2CPP::Class::Utils::GetMethodPointer(avatarClass, "get_Position");
                        if (!posMethod) continue;
                        typedef Unity::Vector3(__cdecl* GetPositionFunc)(void*, void*);
                        Unity::Vector3 up = ((GetPositionFunc)posMethod)(avatar, nullptr);
                        otherPos = { up.x, up.y, up.z };
                    }
                    catch (...) { continue; }

                    float dx = cx - otherPos.x;
                    float dy = cy - otherPos.y;
                    float dz = cz - otherPos.z;
                    float dist = sqrtf(dx * dx + dy * dy + dz * dz);

                    if (dist < SAFE_DIST) {
                        tooClose = true;
                        break;
                    }
                }
            }

            if (!tooClose) {
                bestX = cx; bestY = cy; bestZ = cz;
                found = true;
            }
        }

        // If horizontal ring failed, try going vertically above target
        // (PRIV_RADIUS up = 290m above target, still within their 304m sphere)
        if (!found && avatarList) {
            float cx = targetPos.x;
            float cy = targetPos.y + PRIV_RADIUS; // straight up
            float cz = targetPos.z;

            bool tooClose = false;
            for (int i = 0; i < (int)avatarList->m_uMaxLength; i++) {
                auto avatar = avatarList->operator[](i);
                if (!avatar) continue;

                bool isLocal = *(bool*)((uintptr_t)avatar + 0x28);
                if (isLocal) continue;

                auto gameObj = avatar->GetGameObject();
                if (gameObj == Config::privateChatTarget) continue;

                Vector3 otherPos = { 0,0,0 };
                try {
                    auto avatarClass = avatar->m_Object.m_pClass;
                    if (!avatarClass) continue;
                    auto posMethod = IL2CPP::Class::Utils::GetMethodPointer(avatarClass, "get_Position");
                    if (!posMethod) continue;
                    typedef Unity::Vector3(__cdecl* GetPositionFunc)(void*, void*);
                    Unity::Vector3 up = ((GetPositionFunc)posMethod)(avatar, nullptr);
                    otherPos = { up.x, up.y, up.z };
                }
                catch (...) { continue; }

                float dx = cx - otherPos.x;
                float dy = cy - otherPos.y;
                float dz = cz - otherPos.z;
                float dist = sqrtf(dx * dx + dy * dy + dz * dz);

                if (dist < SAFE_DIST) { tooClose = true; break; }
            }

            if (!tooClose) {
                bestX = cx; bestY = cy; bestZ = cz;
                found = true;
            }
        }
        // If still not found: bestX/Y/Z already set to vertical fallback above,
        // UpdatePrivateChat will keep retrying every 50ms naturally
    }
    catch (...) {}
    IL2CPP::Thread::Detach(m_pThisThread);

    // Only teleport if we found a confirmed safe spot
    if (found && pSetTransform) {
        Vector3 tpPos = { bestX, bestY, bestZ };
        Quaternion rot = EulerToQuaternion(0, 0, 0);
        pSetTransform(localPlayerInstance, tpPos, rot, nullptr);
    }
}

inline void DrawPlayersTab() {
    ImGui::Spacing();
    float w = ImGui::GetContentRegionAvail().x;



    if (g_cachedPlayers.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No players found");
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.122f, 0.122f, 0.122f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::BeginChild("##playerlistbg", ImVec2(0, 0), true);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    union { uint32_t i; float f; } vc; vc.i = 0x7F7FFF00;

    for (int i = 0; i < (int)g_cachedPlayers.size(); i++) {
        auto& p = g_cachedPlayers[i];
        ImGui::PushID(p.actorNr);
        auto& s = g_cs[p.actorNr];


        struct { int level; bool isTourist; uint8_t buildTarget; std::string region; }
        mvInfo = { -1, false, 0, "?" };

        if (g_MVNetworkGameInstance && p.actorNr > 0) {
            void* pc = *(void**)((uintptr_t)g_MVNetworkGameInstance + 0x1B8);
            if (pc) {
                auto* dict = (IL2Cpp::Dict::Dictionary<int32_t, void*>*) * (void**)((uintptr_t)pc + 0x10);
                if (dict && dict->_entries) {
                    for (uint32_t di = 0; di < dict->_entries->max_length; di++) {
                        auto& de = dict->_entries->m_Items[di];
                        if (de.hashCode < 0 || !de.value || de.key != p.actorNr) continue;
                        auto cls = IL2CPP::Class::Find("MVPlayer");
                        if (cls) {
                            auto GetBool = [&](const char* n)->bool { void* fn = IL2CPP::Class::Utils::GetMethodPointer(cls, n); return fn ? reinterpret_cast<bool(__cdecl*)(void*)>(fn)(de.value) : false; };
                            auto GetByte = [&](const char* n)->uint8_t { void* fn = IL2CPP::Class::Utils::GetMethodPointer(cls, n); return fn ? reinterpret_cast<uint8_t(__cdecl*)(void*)>(fn)(de.value) : 0; };
                            auto GetStr = [&](const char* n)->void* { void* fn = IL2CPP::Class::Utils::GetMethodPointer(cls, n); return fn ? reinterpret_cast<void* (__cdecl*)(void*)>(fn)(de.value) : nullptr; };
                            mvInfo.level = *(int32_t*)((uintptr_t)de.value + 0x20);
                            mvInfo.isTourist = GetBool("get_IsTourist");
                            mvInfo.buildTarget = GetByte("get_BuildTarget");
                            void* rs = GetStr("get_RegionCode");
                            if (rs) {
                                std::wstring ww = AntiFilter::IL2CPPStringToWide(rs);
                                if (!ww.empty()) {
                                    int sz = WideCharToMultiByte(CP_UTF8, 0, ww.c_str(), -1, nullptr, 0, nullptr, nullptr);
                                    if (sz > 0) { mvInfo.region.resize(sz - 1); WideCharToMultiByte(CP_UTF8, 0, ww.c_str(), -1, &mvInfo.region[0], sz, nullptr, nullptr); }
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }

        bool open = PlayerCollapsingHeader(p, i, true);

        if (mvInfo.level >= 0) {
            const char* btStr[] = { "?","Standalone","WebGL","Android","iOS" };
            const char* platform = (mvInfo.buildTarget >= 1 && mvInfo.buildTarget <= 4) ? btStr[mvInfo.buildTarget] : "?";
            char lvlBuf[16];  snprintf(lvlBuf, sizeof(lvlBuf), "Lv.%d", mvInfo.level);
            char platBuf[24]; snprintf(platBuf, sizeof(platBuf), "[%s]", platform);
            const char* regStr = mvInfo.region.c_str();
            float totalW = ImGui::CalcTextSize(lvlBuf).x + 6.f + ImGui::CalcTextSize(platBuf).x + 6.f + ImGui::CalcTextSize(regStr).x;
            ImVec2 rowMin = ImGui::GetItemRectMin();
            ImVec2 rowMax = ImGui::GetItemRectMax();
            float ty = rowMin.y + (rowMax.y - rowMin.y) * .5f - ImGui::GetTextLineHeight() * .5f;
            float cx = rowMax.x - totalW - 8.f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(ImVec2(cx, ty), IM_COL32(255, 210, 80, 255), lvlBuf);  cx += ImGui::CalcTextSize(lvlBuf).x + 6.f;
            dl->AddText(ImVec2(cx, ty), IM_COL32(140, 200, 140, 255), platBuf); cx += ImGui::CalcTextSize(platBuf).x + 6.f;
            dl->AddText(ImVec2(cx, ty), IM_COL32(165, 165, 165, 255), regStr);
        }

        if (open) {
            ImGui::Indent(15.0f);
            float btnW = (w - 45.0f) / 3.0f;
            float btnW4 = (w - 45.0f) / 4.0f;

            // ---- ROW 1: Kill / Fling / Void / Heal ----
            if (ImGui::Button("Kill##pl", ImVec2(btnW4, 0)))
                ApplyInteractionToPlayer(p.avatarInstance, 100.f, Vector3{ 0,0,0 }, false);
            ImGui::SameLine();
            if (ImGui::Button("Fling##pl", ImVec2(btnW4, 0))) {
                float rx = ((rand() % 2000) - 1000) / 100.f, rz = ((rand() % 2000) - 1000) / 100.f;
                float hm = sqrtf(rx * rx + rz * rz); if (hm > 0.f) { rx /= hm; rz /= hm; }
                ApplyInteractionToPlayer(p.avatarInstance, 0.f, Vector3{ rx * 4000.f,5000.f,rz * 4000.f }, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Void##pl", ImVec2(btnW4, 0)))
                ApplyInteractionToPlayer(p.avatarInstance, 0.f, Vector3{ vc.f,vc.f,vc.f }, false);

            ImGui::SameLine();
            if (ImGui::Button("Heal##pl", ImVec2(btnW4, 0)))
                CreateThread(nullptr, 0, [](LPVOID lp)->DWORD {HealTarget(lp); return 0; }, p.gameObject, 0, nullptr);

            // ---- ROW 2: Flame / Slap / Pull / TP ----
            if (ImGui::Button("Flame##pl", ImVec2(btnW4, 0)))
                CreateThread(nullptr, 0, [](LPVOID lp)->DWORD {FlameTarget(lp); return 0; }, p.gameObject, 0, nullptr);
            ImGui::SameLine();
            if (ImGui::Button("Slap##pl", ImVec2(btnW4, 0)))
                CreateThread(nullptr, 0, [](LPVOID lp)->DWORD {SlapTarget(lp); return 0; }, p.gameObject, 0, nullptr);
            ImGui::SameLine();
            if (ImGui::Button("Pull##pl", ImVec2(btnW4, 0))) {
                Vector3 tp = GetPlayerPosition(p.gameObject);
                Vector3 dir = { lastKnownPosition.x - tp.x, lastKnownPosition.y - tp.y, lastKnownPosition.z - tp.z };
                float dist = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                if (dist > .1f) {
                    dir.x /= dist; dir.y /= dist; dir.z /= dist;
                    float f = min(150.f + dist * 250.f, 800.f);
                    ApplyInteractionToPlayer(p.avatarInstance, 0.f, Vector3{ dir.x * f,dir.y * f + 300.f,dir.z * f }, false);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("TP to##pl", ImVec2(btnW4, 0))) {
                Config::customInteractionTarget = p.gameObject;
                Config::customInteractionTargetName = p.name;
                TeleportToCustomTarget();
            }

            // ---- ROW 3: Kick / Ban ----
            if (ImGui::Button("Kick##pl", ImVec2(btnW, 0))) {
                void* ac = GetAvatarComponentFromGameObject(p.gameObject);
                if (ac) ApplyInteractionToPlayer(ac, 0.f, Vector3{ 0,-199,0 }, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Ban##pl", ImVec2(btnW, 0))) {
                void* ac = GetAvatarComponentFromGameObject(p.gameObject);
                if (ac) ApplyInteractionToPlayer(ac, 0.f, Vector3{ 0,-198,0 }, false);
            }

            if (ImGui::Button("crashtest##pl", ImVec2(btnW4, 0)))
                ApplyInteractionToPlayer(p.avatarInstance, 0.f, Vector3{ 0.f, 0.f, -3.0e38f }, false);


            ImGui::Spacing();

            // ---- LOOPS ----
            int tid = GetOrAddTarget(p);
            auto& loop = Config::loopingInteractions[tid];
            if (!loop.targetInstance) {
                loop.targetInstance = p.gameObject; loop.enabled = false;
                loop.damage = 0.f; loop.impulse = { 0,0,0 }; loop.intervalMs = 500; loop.lastApplied = 0;
            }

            bool isLoopKill = loop.enabled && loop.damage >= 100.f;
            bool isLoopVoid = loop.enabled && loop.damage < 1.f && loop.impulse.y > 1e10f;

            ImGui::PushStyleColor(ImGuiCol_Button, isLoopKill ? ImVec4(0.70f, 0.15f, 0.15f, 1.f) : ImVec4(0.30f, 0.10f, 0.10f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.20f, 0.20f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.82f, 0.25f, 0.25f, 1.f));
            if (ImGui::Button(isLoopKill ? "[ON] Loop Kill##lk" : "Loop Kill##lk", ImVec2(btnW, 0))) {
                if (isLoopKill) loop.enabled = false;
                else { loop.enabled = true; loop.damage = 100.f; loop.impulse = { 0,0,0 }; loop.intervalMs = 500; }
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, isLoopVoid ? ImVec4(0.25f, 0.10f, 0.55f, 1.f) : ImVec4(0.12f, 0.06f, 0.28f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.33f, 0.14f, 0.65f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.18f, 0.72f, 1.f));
            if (ImGui::Button(isLoopVoid ? "[ON] Loop Void##lv" : "Loop Void##lv", ImVec2(btnW, 0))) {
                if (isLoopVoid) loop.enabled = false;
                else { loop.enabled = true; loop.damage = 0.f; loop.impulse = { 0.f,vc.f,0.f }; loop.intervalMs = 500; }
            }
            ImGui::PopStyleColor(3);

            if (loop.enabled) {
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderInt("Interval ms##qi", &loop.intervalMs, 50, 2000);
            }

            // ---- CUSTOM LOOP ----
            ImGui::Spacing();
            static std::unordered_map<int, bool> showCL;
            ToggleButton(("##showcl" + std::to_string(p.actorNr)).c_str(), &showCL[p.actorNr]);
            ImGui::SameLine(); ImGui::Text("Custom Loop");

            if (showCL[p.actorNr]) {
                ImGui::Indent(10.0f);
                ToggleButton(("##looping" + std::to_string(p.actorNr)).c_str(), &loop.enabled);
                ImGui::SameLine();
                ImGui::TextColored(loop.enabled ? ImVec4(0, 1, 0, 1) : ImVec4(0.7f, 0.7f, 0.7f, 1), loop.enabled ? "ACTIVE" : "Off");
                if (loop.enabled) {
                    float pbW = (w - 45.f) / 4.f;
                    if (ImGui::Button("Kill##clp", ImVec2(pbW, 0))) { loop.damage = 100; loop.impulse = { 0,0,0 }; }       ImGui::SameLine();
                    if (ImGui::Button("Fling##clp", ImVec2(pbW, 0))) { loop.damage = 0;   loop.impulse = { 2500,5000,2500 }; } ImGui::SameLine();
                    if (ImGui::Button("Float##clp", ImVec2(pbW, 0))) { loop.damage = 0;   loop.impulse = { 0,165.1f,0 }; }   ImGui::SameLine();
                    if (ImGui::Button("Void##clp", ImVec2(pbW, 0))) { loop.damage = 0;   loop.impulse = { 0,vc.f,0 }; }
                    ImGui::InputFloat(("##dmg" + std::to_string(p.actorNr)).c_str(), &loop.damage, 1.f, 10.f, "%.1f");
                    ImGui::InputFloat(("##ix" + std::to_string(p.actorNr)).c_str(), &loop.impulse.x, 100.f, 1000.f, "%.1f");
                    ImGui::InputFloat(("##iy" + std::to_string(p.actorNr)).c_str(), &loop.impulse.y, 100.f, 1000.f, "%.1f");
                    ImGui::InputFloat(("##iz" + std::to_string(p.actorNr)).c_str(), &loop.impulse.z, 100.f, 1000.f, "%.1f");
                    ImGui::InputInt(("##int" + std::to_string(p.actorNr)).c_str(), &loop.intervalMs, 10, 100);
                    loop.intervalMs = std::clamp(loop.intervalMs, 50, 10000);
                }
                ImGui::Unindent(10.0f);
            }

            // ---- BLACKLIST ----
            if (loop.enabled) {
                ImGui::Spacing();
                bool blisted = IsBlacklisted(p.name);
                if (!blisted) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.12f, 0.12f, 0.80f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.18f, 0.18f, 0.90f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.22f, 0.22f, 1.00f));
                    char bll[64]; snprintf(bll, sizeof(bll), "Blacklist + Auto-%s##bla", isLoopKill ? "Kill" : "Void");
                    if (ImGui::Button(bll, ImVec2(-1, 0))) {
                        AddToBlacklist(p.name, isLoopKill ? BlacklistEntry::LoopType::Kill : BlacklistEntry::LoopType::Void);
                        PushNotification("BLACKLISTED: " + p.name + " will auto-loop on rejoin.", NotificationType::Error, 3.0f);
                    }
                    ImGui::PopStyleColor(3);
                }
                else {
                    ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), ICON_FA_SKULL " %s is BLACKLISTED", p.name.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove##unbl")) RemoveFromBlacklist(p.name);
                }
            }

            // ---- AVATAR SCALE ----
            ImGui::Spacing();
            static std::unordered_map<int, float> scaleMap;
            float& scl = scaleMap[p.actorNr];
            if (scl <= 0.f) scl = 1.f;
            float sliderW = (w - 45.f) - 55.f - ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetNextItemWidth(sliderW);
            ImGui::SliderFloat(("##ppscl" + std::to_string(p.actorNr)).c_str(), &scl, 0.f, 20.f, "%.2fx");
            ImGui::SameLine();
            if (ImGui::Button(("Set##sc" + std::to_string(p.actorNr)).c_str(), ImVec2(28, 0))) {
                void* ac = GetAvatarComponentFromGameObject(p.gameObject);
                if (ac) SetTargetSize(ac, scl);
            }
            ImGui::SameLine();
            if (ImGui::Button(("R##sc" + std::to_string(p.actorNr)).c_str(), ImVec2(20, 0))) {
                scl = 1.f;
                void* ac = GetAvatarComponentFromGameObject(p.gameObject);
                if (ac) SetTargetSize(ac, 1.f);
            }

            // ---- FOLLOW ----
            ImGui::Spacing();
            ToggleButton("##follow", &Config::followCustomTarget);
            ImGui::SameLine(); ImGui::Text("Follow Target");
            if (Config::followCustomTarget) {
                Config::customInteractionTarget = p.gameObject;
                Config::customInteractionTargetName = p.name;
            }

            // ---- PRIVATE CHAT ----
            ImGui::Spacing();
            bool isPrivTarget = (Config::privateChatTarget == p.gameObject);
            static Vector3 g_preChatPos = { 0,0,0 };

            bool pcWasOn = Config::enablePrivateChat;
            ToggleButton(("##pc" + std::to_string(p.actorNr)).c_str(), &Config::enablePrivateChat);

            // just got toggled
            if (pcWasOn != Config::enablePrivateChat) {
                if (Config::enablePrivateChat) {
                    // turning ON — save current position
                    g_preChatPos = lastKnownPosition;
                    Config::privateChatTarget = p.gameObject;
                    Config::privateChatTargetName = p.name;
                }
                else {
                    // turning OFF — TP back
                    if (pSetTransform && localPlayerInstance && hasValidPlayer &&
                        !(g_preChatPos.x == 0 && g_preChatPos.y == 0 && g_preChatPos.z == 0)) {
                        Quaternion rot = EulerToQuaternion(0, 0, 0);
                        pSetTransform(localPlayerInstance, g_preChatPos, rot, nullptr);
                    }
                    Config::privateChatTarget = nullptr;
                    Config::privateChatTargetName = "";
                }
            }

            if (Config::enablePrivateChat && !isPrivTarget) {
                Config::privateChatTarget = p.gameObject;
                Config::privateChatTargetName = p.name;
            }
            ImGui::SameLine();
            ImGui::Text("Private Chat");
            if (isPrivTarget && Config::enablePrivateChat) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "-> %s", p.name.c_str());
                ImGui::Spacing();

                bool anySnooping = false;
                for (auto& cp : g_cachedPlayers) {
                    if (cp.isLocal || cp.gameObject == p.gameObject) continue;
                    Vector3 op = GetPlayerPosition(cp.gameObject);
                    float dx = lastKnownPosition.x - op.x;
                    float dy = lastKnownPosition.y - op.y;
                    float dz = lastKnownPosition.z - op.z;
                    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
                    if (dist < 305.0f) {
                        if (!anySnooping) {
                            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), ICON_FA_TRIANGLE_EXCLAMATION " can hear you:");
                            anySnooping = true;
                        }
                        ImGui::TextColored(ImVec4(1.f, 0.6f, 0.6f, 1.f),
                            "  " ICON_FA_USER " %s (%.0fm)", cp.name.c_str(), dist);
                    }
                }
                if (!anySnooping)
                    ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), ICON_FA_EYE " safe");
            }

            ImGui::Spacing();
            if (ImGui::Button(("Chat Test##ct" + std::to_string(p.actorNr)).c_str(), ImVec2(-1, 0))) {
                CallComponentMethod("SendMessageControl", "SwapChat", 9);
                void* capturedGameObj = p.gameObject;
                g_preChatPos = lastKnownPosition;
                Config::privateChatTarget = capturedGameObj;
                Config::privateChatTargetName = p.name;
                Config::enablePrivateChat = true;

                CreateThread(nullptr, 0, [](LPVOID lp) -> DWORD {
                    Sleep(600); // let UpdatePrivateChat land us somewhere first

                    const int MAX_WAIT_MS = 10000;
                    const int POLL_INTERVAL = 200;
                    int elapsed = 0;
                    int snoopCount = 0;

                    while (elapsed < MAX_WAIT_MS) {
                        snoopCount = 0;
                        {
                            std::lock_guard<std::mutex> lock(g_playerCacheMutex);

                            // Use lastKnownPosition AFTER we've already been teleported
                            // by UpdatePrivateChat — this is now the spoofed position
                            Vector3 myPos = lastKnownPosition;

                            for (auto& cp : g_cachedPlayers) {
                                if (cp.isLocal || cp.gameObject == Config::privateChatTarget) continue;
                                Vector3 op = GetPlayerPosition(cp.gameObject);
                                float dx = myPos.x - op.x;
                                float dy = myPos.y - op.y;
                                float dz = myPos.z - op.z;
                                float dist = sqrtf(dx * dx + dy * dy + dz * dz);
                                if (dist < 305.0f) snoopCount++;
                            }
                        }

                        if (snoopCount == 0) break;

                        Sleep(POLL_INTERVAL);
                        elapsed += POLL_INTERVAL;
                    }

                    // Fully safe OR timed out with <=3 nearby — send anyway
                    if (snoopCount <= 3) {
                        char temp[512]{};
                        strcpy_s(temp, "</color>  </color><quad=size=-2147483648 width=2147483648");
                        wchar_t wmsg[512]{};
                        MultiByteToWideChar(CP_UTF8, 0, temp, -1, wmsg, 512);
                        CallComponentMethod("SendMessageControl", "SendChatMessage", wmsg);
                        Sleep(300);
                    }
                    // >3 after timeout: abort, fall through to TP back

                    Config::enablePrivateChat = false; // triggers TP back via toggle handler
                    return 0;
                    }, capturedGameObj, 0, nullptr);
            }

            // ---- MIMIC ----
            ImGui::SeparatorText("mimic player chat");
            static std::unordered_map<int, std::array<char, 256>> mimicInputs;
            static std::unordered_map<int, bool> mimicSays;
            if (mimicInputs.find(p.actorNr) == mimicInputs.end()) mimicInputs[p.actorNr] = {};
            ToggleButton(("##says" + std::to_string(p.actorNr)).c_str(), &mimicSays[p.actorNr]);
            ImGui::SameLine(); ImGui::Text("[SAYS] chat");
            ImGui::SetNextItemWidth(-(btnW + ImGui::GetStyle().ItemSpacing.x));
            ImGui::InputText(("##mimic" + std::to_string(p.actorNr)).c_str(), mimicInputs[p.actorNr].data(), 256);
            ImGui::SameLine();
            if (ImGui::Button(("Send##mimic" + std::to_string(p.actorNr)).c_str(), ImVec2(btnW, 0))) {
                std::string fullMsg = mimicSays[p.actorNr]
                    ? "<color=#6FA5FF>[" + p.name + "]</color> <color=#E98400>(says):</color> " + mimicInputs[p.actorNr].data()
                    : "<color=#6FA5FF>[" + p.name + "]:</color> " + mimicInputs[p.actorNr].data();
                postGameMsg(fullMsg.c_str(), 3);
            }

            // ---- LIMB IDs ----
            ImGui::Spacing();
            ImGui::SeparatorText("Limb IDs");
            static const char* partNames[] = { "Head","Torso","RArm","LArm","RUpLeg","RLowLeg","LUpLeg","LLowLeg","Holster" };
            float limbBtnW = (w - 45.f) / 3.f;
            for (int pi = 0; pi < 9; pi++) {
                if (pi > 0 && pi % 3 != 0) ImGui::SameLine();
                char lblBuf[64]; snprintf(lblBuf, sizeof(lblBuf), "%s##limb%d_%d", partNames[pi], p.actorNr, pi);
                if (ImGui::Button(lblBuf, ImVec2(limbBtnW, 0))) {
                    g_pendingLimbTarget = p.gameObject; g_pendingLimbIndex = pi; g_pendingLimbCopy = true;
                }
            }

            if (ImGui::Button("copy avatar to json##avatarcopy", ImVec2(-1, 0))) {
                g_pendingAvatarCopyTarget = p.gameObject;
                g_pendingAvatarCopy = true;
            }

            // ---- INFO ----
            if (mvInfo.level >= 0) {
                ImGui::Spacing();
                ImGui::Text("-----------INFO---------");
                const char* btStr2[] = { "?","Standalone","WebGL","Android","iOS" };
                const char* platform2 = (mvInfo.buildTarget >= 1 && mvInfo.buildTarget <= 4) ? btStr2[mvInfo.buildTarget] : "?";

                if (g_MVNetworkGameInstance && p.actorNr > 0) {
                    void* pc2 = *(void**)((uintptr_t)g_MVNetworkGameInstance + 0x1B8);
                    if (pc2) {
                        auto* dict2 = (IL2Cpp::Dict::Dictionary<int32_t, void*>*) * (void**)((uintptr_t)pc2 + 0x10);
                        if (dict2 && dict2->_entries) {
                            for (uint32_t di = 0; di < dict2->_entries->max_length; di++) {
                                auto& de2 = dict2->_entries->m_Items[di];
                                if (de2.hashCode < 0 || !de2.value || de2.key != p.actorNr) continue;
                                auto cls2 = IL2CPP::Class::Find("MVPlayer"); if (!cls2) break;
                                auto GetInt2 = [&](const char* n)->int { void* fn = IL2CPP::Class::Utils::GetMethodPointer(cls2, n); return fn ? reinterpret_cast<int(__cdecl*)(void*)>(fn)(de2.value) : -1; };
                                auto GetBool2 = [&](const char* n)->bool { void* fn = IL2CPP::Class::Utils::GetMethodPointer(cls2, n); return fn ? reinterpret_cast<bool(__cdecl*)(void*)>(fn)(de2.value) : false; };
                                int profileID = GetInt2("get_ProfileID"), woId = GetInt2("get_WoId"), actorNr2 = GetInt2("get_ActorNr");
                                bool isSub = GetBool2("get_IsSubscriber");
                                ImGui::Text("Lv.%d  |  ID: %d  |  Actor: %d  |  WoId: %d", mvInfo.level, profileID, actorNr2, woId);
                                ImGui::Text("Region: %s  |  Platform: %s", mvInfo.region.c_str(), platform2);
                                ImGui::Text("%s%s", mvInfo.isTourist ? "[Tourist] " : "", isSub ? "[Subscriber]" : "");
                                break;
                            }
                        }
                    }
                }
            }
            s.fullH = ImGui::GetCursorPosY();
            if (s.fullH < 1.f) s.fullH = 200.f;
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::Spacing();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

