#include "framework/framework.h"
#include "Hooks.h"
#include "Utils.h"
//#include "aimBot.h"
//#include "TeleportTab.h"
#include "drawSidebar.h"
#include "Minhook.h"

#include "Fonts.h"
#include "Icons.h"
#include <vector>
//#include <string> 
#include <windows.h>
//#include "autoBuild.h"
#include "esp.h"
#include "interactions.h"
#include <psapi.h>
#include "aimbot.h"
#include "autoBuilder.h"
#include "CubeShaper.h"
#include "Notification.h"
#include "GunMods.h"
#include <set>
#include "Limbrotation.h"
#include <unordered_map>
#include "KeySystem.h"
#include "Dumper.h"
#include "Offsets.h"
#pragma comment(lib, "psapi.lib")

#define ICON_MIN_FA 0xe005 
#define ICON_MAX_FA 0xf8ff


static std::mutex g_playerCacheMutex;
static bool  g_pendingLimbCopy = false;
static void* g_pendingLimbTarget = nullptr;
static int   g_pendingLimbIndex = -1;
static std::vector<std::string> g_massMimicQueue;
static DWORD g_lastMassMimicTime = 0;


struct BlacklistEntry {
    std::string name;
    enum class LoopType { None, Kill, Void } loopType = LoopType::None;
};
static std::vector<BlacklistEntry> g_blacklist;

bool IsBlacklisted(const std::string& name) {
    for (auto& e : g_blacklist) if (e.name == name) return true;
    return false;
}
BlacklistEntry* GetBlacklistEntry(const std::string& name) {
    for (auto& e : g_blacklist) if (e.name == name) return &e;
    return nullptr;
}
void AddToBlacklist(const std::string& name, BlacklistEntry::LoopType loopType) {
    for (auto& e : g_blacklist) { if (e.name == name) { e.loopType = loopType; return; } }
    g_blacklist.push_back({ name, loopType });
}
void RemoveFromBlacklist(const std::string& name) {
    g_blacklist.erase(std::remove_if(g_blacklist.begin(), g_blacklist.end(),
        [&](const BlacklistEntry& e) { return e.name == name; }), g_blacklist.end());
}

static std::vector<std::pair<std::string, std::string>> g_pendingBlacklistNotifs;
static std::mutex g_pendingBlacklistMutex;

void CheckBlacklistOnJoin(const std::string& playerName, void* gameObject) {
    BlacklistEntry* entry = GetBlacklistEntry(playerName);
    if (!entry || !gameObject) return;

    {
        std::lock_guard<std::mutex> lock(g_pendingBlacklistMutex);
        g_pendingBlacklistNotifs.push_back({
            "BLACKLIST",
            playerName + " rejoined. auto-loop triggered."
            });
    }

    bool already = false;
    for (auto& t : Config::targetedPlayers)
        if (t.instance == gameObject) { already = true; break; }

    int loopId;
    if (!already) {
        TargetedPlayer nt;
        nt.instance = gameObject; nt.name = playerName;
        nt.id = Config::nextTargetId++; nt.isFlying = false;
        nt.enableRadiusKill = false; nt.radiusKillDistance = 10.0f;
        nt.radiusKillInterval = 500; nt.lastRadiusKill = 0;
        Config::targetedPlayers.push_back(nt);
        loopId = nt.id;
    }
    else {
        for (auto& t : Config::targetedPlayers)
            if (t.instance == gameObject) { loopId = t.id; break; }
    }

    union { uint32_t i; float f; } c; c.i = 0x7F7FFF00;
    auto& loop = Config::loopingInteractions[loopId];
    loop.targetInstance = gameObject;
    loop.enabled = true;
    loop.intervalMs = 500;
    loop.lastApplied = GetTickCount64() + 2000; // 2 second grace period before first tick
    if (entry->loopType == BlacklistEntry::LoopType::Kill) { loop.damage = 100.0f; loop.impulse = { 0,0,0 }; }
    else { loop.damage = 0.0f; loop.impulse = { 0.0f, c.f, 0.0f }; }
}

void helpText(const char* desc)
{
    ImGui::TextDisabled("[?]");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
void DrawWeaponsPage() {
    ImGui::BeginChild("WeaponsContent", ImVec2(0, 0), false);
    ImSmoothTabTransition();

    ImGui::Text("WEAPONSSSS");
    ImGui::Separator();
    ImGui::Spacing();

    // main toggle
    ImGui::Text("equip weapons"); ImGui::SameLine();
    if (ToggleButton("enable##weapons", &Config::enableWeapons)) {
        weaponSpawnEnabled = !weaponSpawnEnabled;
    }
    ImGui::SameLine();
    ImGui::Text("hotkeys (1-9): "); ImGui::SameLine();
    ToggleButton("##hotkeys", &Config::useWeaponHotkeys);



    if (weaponSpawnEnabled) {
        if (Config::useWeaponHotkeys) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ON");
            ImGui::BeginChild("Hotkeys", ImVec2(0, 85), true);
            ImGui::Text("press numbers to equip:");
            ImGui::Columns(3, "hotkeycols", false);
            for (size_t i = 0; i < (Config::availableWeapons.size() < 9 ? Config::availableWeapons.size() : 9); i++) {
                ImGui::Text("%d: %s", (int)i + 1, Config::availableWeapons[i].name.c_str());
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndChild();
        }

        struct WeaponEntry {
            const char* name;
            int id;
            const char* tooltip = nullptr;
        };

        struct WeaponGroup {
            const char* label;
            std::vector<WeaponEntry> weapons;
        };

        static const WeaponGroup groups[] = {
            { nullptr, {
                { "CenterGun",    1 },
                { "ImpulseGun",   2 },
                { "RailGun",      6 },
                { "Shotgun",      9 },
                { "Flamethrower", 10, "works any game" },
                { "CustomGun",    15 },
            }},
            { "faggot annoying", {
                { "Bazooka",    4  },
                { "CubeGun",    11 },
                { "GrowthGun",  62 },
                { "SixShooter", 12 },
                { "DoubleSix",  13 },
                { "MouseGun",   60 },
            }},
            { "melees and other", {
                { "Hand",         5  },
                { "MeleeWeapon",  8  },
                { "SlapGun",      65 },
                { "ThrowingStar", 45 },
                { "MultiStar",    46 },
                { "HealRay",      70, "works any game" },
                { "Costume",      59, "does absolutely fucking nothing" },
            }},
        };

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const int   perRow = 3;
        const float btnW = (availableWidth - spacing * (perRow - 1)) / perRow;

        if (ImGui::CollapsingHeader("spawn")) {
            for (auto& group : groups) {
                if (group.label) {
                    ImGui::Spacing();
                    SeparatorText(group.label);
                }

                for (size_t i = 0; i < group.weapons.size(); i++) {
                    auto& w = group.weapons[i];
                    if (i % perRow != 0) ImGui::SameLine();

                    if (ImGui::Button(w.name, ImVec2(btnW, 26))) {
                        if (w.id == WEAPON_TYPE_CUSTOM_GUN || w.id == WEAPON_TYPE_COSTUME)
                            EquipFromMenu(g_capturedWeaponType);
                        else
                            EquipWeapon(w.id);
                    }

                    if (w.tooltip && ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", w.tooltip);
                }
            }

        }

    }
    else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "weapons disabled");
    }
    ImGui::Spacing();

    // infinite amm

    ImGui::Spacing();
    float weapons1height = 90;
    if (Config::enableBulletSpeedMod)
        weapons1height = 110;

    Section("Weapons #1", weapons1height, [&] {

        ImGui::Text("inf ammo");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
        if (ToggleButton("##infammo", &Config::infiniteAmmo)) {
            UpdateGameValue<bool>("MVPickupOwner", "hasUnlimitedAmmoSetting", Config::infiniteAmmo);
        }

        // bullet speed

        ImGui::Spacing();
        ImGui::Text("projectile speed");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
        ToggleButton("##bulletspeed", &Config::enableBulletSpeedMod);


        if (Config::enableBulletSpeedMod) {

            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##speedslider", &Config::bulletSpeedMultiplier, 0.1f, 10.0f);

        }

        ImGui::Spacing();
        ImGui::Text("anti weapon ghostcrash");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
        ToggleButton("##safeinteract", &enableSafeInteractionPatch);


        
        helpText("you will be ghostcrashed for using a weapon \n that does not exist in the map. \n this feature prevents that. ");

        });


    Section("Weapons #3", 30, [&] {

        ImGui::Text("Slap Gun 5000x");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
        if (ToggleButton("##slapstrength", &Config::enableSlapStrength)) {
            if (Config::enableSlapStrength) {
                UpdateGameValue<float>("PickupItemSlapGun", "slapStrength", 999999);
            }
            else {
                UpdateGameValue<float>("PickupItemSlapGun", "slapStrength", 10);
            }
        }


        });


    ImGui::Spacing();

    Section("Aimbot", Aimbot::Enable() ? 75 : 35, [&] {
        Aimbot::RenderUI();
        });



    Section("Weapons #2", 60, [&] {

        ImGui::Text("Anti-sunbullet");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);

        if (ToggleButton("##customgunpatch", &Config::enableCustomGunPatch)) {}


        ImGui::Spacing();


        DrawGunModsToggle();
        });



    ImSmoothTabEnd();
    ImGui::EndChild();
}

void DrawExploitsPage() {
    ImGui::BeginChild("ExploitsContent", ImVec2(0, 0), false);
    ImSmoothTabTransition();

    // Use TabBar for better organization
    if (ImGui::BeginTabBar("ExploitsTabs", ImGuiTabBarFlags_None)) {



        if (ImGui::BeginTabItem("Player")) {
            ImSmoothTabTransition();

            float w = ImGui::GetContentRegionAvail().x;

            float rotHeight = 35; // base: just enable
            if (Config::enableMotorRotation) {
                rotHeight = 130; // sliders + spinbot header
                if (Config::enableMotorSpinbot) rotHeight = 200; // + spinbot axes
            }

            Section("Avatar Rotation", rotHeight, [&] {


                ImGui::Text("avatar rotation");
                ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
                ToggleButton("##en", &Config::enableMotorRotation);


                if (Config::enableMotorRotation) {
                    ImGui::Spacing();

                    // Stacked sliders, full width
                    ImGui::SetNextItemWidth(w - 50);
                    ImGui::SliderFloat("##rx", &Config::motorRotationX, -180, 180, "X: %.0f");
                    ImGui::SameLine();
                    if (ImGui::Button("R##rx", ImVec2(24, 0))) Config::motorRotationX = 0;

                    ImGui::SetNextItemWidth(w - 50);
                    ImGui::SliderFloat("##ry", &Config::motorRotationY, -180, 180, "Y: %.0f");
                    ImGui::SameLine();
                    if (ImGui::Button("R##ry", ImVec2(24, 0))) Config::motorRotationY = 0;

                    ImGui::SetNextItemWidth(w - 50);
                    ImGui::SliderFloat("##rz", &Config::motorRotationZ, -180, 180, "Z: %.0f");
                    ImGui::SameLine();
                    if (ImGui::Button("R##rz", ImVec2(24, 0))) Config::motorRotationZ = 0;

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Spinbot inside rotation section
                    ToggleButton("##spin", &Config::enableMotorSpinbot);
                    ImGui::SameLine(); ImGui::Text("Spinbot");

                    if (Config::enableMotorSpinbot) {
                        ImGui::Spacing();

                        auto Axis = [&](const char* n, bool* en, float* spd, bool* rev) {
                            ImGui::SetNextItemWidth(w - 140);
                            char lbl[32]; snprintf(lbl, 32, "%s##%s", *en ? "%.1f" : "Off", n);
                            ImGui::SliderFloat((std::string("##s") + n).c_str(), spd, 0.1f, 10, lbl);
                            ImGui::SameLine();
                            ToggleButton((std::string("##e") + n).c_str(), en, ImVec2(32, 16));
                            ImGui::SameLine();
                            ImGui::Text("%s", n);
                            if (*en) {
                                ImGui::SameLine();
                                ToggleButton((std::string("##r") + n).c_str(), rev, ImVec2(24, 12));
                                ImGui::SameLine(); ImGui::Text("Rev");
                            }
                            };

                        Axis("X", &Config::motorSpinX, &Config::motorSpinSpeedX, &Config::reverseMotorSpinX);
                        Axis("Y", &Config::motorSpinY, &Config::motorSpinSpeedY, &Config::reverseMotorSpinY);
                        Axis("Z", &Config::motorSpinZ, &Config::motorSpinSpeedZ, &Config::reverseMotorSpinZ);
                    }
                }
                });

            // --- AVATAR SIZE (dynamic height) ---
            float sizeHeight = Config::enableSizeModifier ? 80 : 35;
            Section("Avatar Size", sizeHeight, [&] {

                ImGui::Text("Avatar size");
                ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
                ToggleButton("##sz", &Config::enableSizeModifier);

                if (Config::enableSizeModifier) {
                    ImGui::Spacing();
                    if (slidder("##s", &Config::avatarSize, 0.1f, 30, "%.1fx"))
                        SetLocalPlayerSize(Config::avatarSize);

                    if (ImGui::Button("Reset", ImVec2(50, 0))) { Config::avatarSize = 1; SetLocalPlayerSize(1); }
                    ImGui::SameLine();
                    if (ImGui::Button("Hide", ImVec2(40, 0))) { Config::avatarSize = 0; SetLocalPlayerSize(0); }
                    ImGui::SameLine();

                    static bool crash = false;
                    if (ToggleButton("##crash", &crash, ImVec2(32, 16))) {
                        uint32_t h = 0xFFFFFFFF; float s = *(float*)&h;
                        Config::avatarSize = crash ? s : 1;
                        SetLocalPlayerSize(Config::avatarSize);
                    }
                    ImGui::SameLine(); ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1), "crash webgl");
                }
                });


            Section("Time Scale", enableTimeScale ? 60 : 30, [&] {
                ImGui::Text("Time Scale");
                ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
                
                if (ToggleButton("##timescale", &enableTimeScale)) {
                    // called on every toggle
                    if (!enableTimeScale && pSetTimeScale) {
                        pSetTimeScale(1.0f, nullptr); // restore on disable
                    }
                }
                

                if (enableTimeScale) {
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(w - 80);
                    slidder("##tsslider", &timeScaleValue, 0.0f, 10.0f, "%.2fx");
                    ImGui::SameLine();
                    if (ImGui::Button("R##ts", ImVec2(24, 0))) timeScaleValue = 1.0f;

                }
                });

            {
                bool isActive = g_netPosMode != NetPosMode::Off;
                float secH = 30.f;
                if (g_netPosMode == NetPosMode::Strafe)  secH = 80.f;
                else if (g_netPosMode == NetPosMode::Ghost) secH = 55.f;

                Section("Server Ghost", secH, [&] {
                    float fw = ImGui::GetContentRegionAvail().x;
                    float bw = (fw - ImGui::GetStyle().ItemSpacing.x * 4.f) / 5.f;

                    auto ModeBtn = [&](const char* lbl, NetPosMode m) {
                        bool active = g_netPosMode == m;
                        if (active) ImGui::PushStyleColor(ImGuiCol_Button, currentSidebarColor);
                        if (ImGui::Button(lbl, ImVec2(bw, 22))) {
                            if (g_netPosMode == m) g_netPosMode = NetPosMode::Off;
                            else { g_netPosMode = m; g_freezeCaptured = false; }
                        }
                        if (active) ImGui::PopStyleColor();
                        };

                    ModeBtn("Normal", NetPosMode::Off);    ImGui::SameLine();
                    ModeBtn("Invis", NetPosMode::Ghost);  ImGui::SameLine();
                    ModeBtn("Freeze", NetPosMode::Freeze); ImGui::SameLine();
                    ModeBtn("Strafe", NetPosMode::Strafe); ImGui::SameLine();
                    bool fl = enableFakeLag;
                    if (fl) ImGui::PushStyleColor(ImGuiCol_Button, Config::accentColor);
                    if (ImGui::Button("FakeLag", ImVec2(bw, 22))) enableFakeLag = !enableFakeLag;
                    if (fl) ImGui::PopStyleColor();

                    if (g_netPosMode == NetPosMode::Ghost) {
                        ImGui::Spacing();
                        ImGui::SetNextItemWidth(fw);
                        ImGui::SliderFloat("##ghosty", &g_ghostPos.y, -500.f, 500.f, "Y offset: %.0f");
                    }
                    if (g_netPosMode == NetPosMode::Strafe) {
                        ImGui::Spacing();
                        ImGui::SetNextItemWidth(fw - 50.f);
                        ImGui::SliderFloat("##strofs", &g_strafeOffset, 5.f, 500.f, "Offset: %.0f");
                        ImGui::SetNextItemWidth(fw - 50.f);
                        ImGui::SliderFloat("##strhz", &g_strafeHz, 1.f, 30.f, "Hz: %.0f");
                    }
                    if (g_netPosMode == NetPosMode::Freeze && g_freezeCaptured)
                        ImGui::TextColored(ImVec4(.5f, .8f, .5f, 1.f), "frozen @ %.0f %.0f %.0f",
                            g_freezePos.x, g_freezePos.y, g_freezePos.z);
                    });
            }

            // Two columns for combat/safety
            float colW = (w - 10) * 0.5f;

            ImGui::BeginChild("Left", ImVec2(colW, 0), false);
            {

                // --- SAFETY ---
                Section("Safety", 75, [&] {
                    if (ToggleButton("##imp", &Config::enableIgnoreDamage))
                        RetPatch(Offsets::MVRigidBody::AddImpulse_0x3BEEF0, Config::enableIgnoreDamage, originalbyte);
                    ImGui::SameLine(); ImGui::Text("Anti-Impulse");

                    ToggleButton("##im", &Config::enableImmortality);
                    ImGui::SameLine(); ImGui::Text("Immortal");
                    
                    if (ToggleButton("##ab", &antiban)) {
                        RetPatch(Offsets::HackingToolDetector::set_QuitRequest, antiban, originalbyte);
                        RetPatch(Offsets::HackingToolDetector::get_InstallTracesDetected, antiban, originalbyte);
                        RetPatch(Offsets::BrowserCommGotoRequests::GotoSignout, antiban, originalbyte);
                        RetPatch(Offsets::BrowserCommGotoRequests::GotoDisconnected, antiban, originalbyte);
                        RetPatch(Offsets::OperationRequests::Ban, antiban, originalbyte);
                        RetPatch(Offsets::CheatHandling::ExecuteBan, antiban, originalbyte);
                        
                    }
                    ImGui::SameLine(); ImGui::Text("Anti-Ban");
                    });

                // --- FORCE FIELD + RADIUS KILL combined ---
                float combatHeight = 60;
                if (Config::enableForceField) combatHeight += Config::enableRadiusKill ? 160 : 80;
                else if (Config::enableRadiusKill) combatHeight += 80;

                Section("Combat", combatHeight, [&] {
                    // Force Field
                    ToggleButton("##ff", &Config::enableForceField);
                    ImGui::SameLine(); ImGui::Text("Force Field");

                    if (Config::enableForceField) {
                        ImGui::Spacing();
                        if (ImGui::Button("Gravity", ImVec2(70, 0))) {
                            Config::forceFieldRadius = 20;
                            Config::forceFieldStrength = -700;
                            Config::forceFieldUpdateInterval = 220;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Push", ImVec2(70, 0))) {
                            Config::forceFieldRadius = 15;
                            Config::forceFieldStrength = 700;
                            Config::forceFieldUpdateInterval = 220;
                        }

                        ImGui::SetNextItemWidth(colW - 20);
                        ImGui::SliderFloat("Radius##ff", &Config::forceFieldRadius, 1, 50, "Radius: %.0f");
                        ImGui::SetNextItemWidth(colW - 20);
                        ImGui::SliderFloat("Power##ff", &Config::forceFieldStrength, 100, 2000, "Power: %.0f");
                    }

                    ImGui::Spacing();

                    // Radius Kill
                    ToggleButton("##rk", &Config::enableRadiusKill);
                    ImGui::SameLine(); ImGui::Text("Radius Kill");

                    if (Config::enableRadiusKill) {
                        ImGui::Spacing();
                        if (ImGui::Button("Touch", ImVec2(60, 0))) {
                            Config::radiusKillDistance = 2.5f;
                            Config::radiusKillDamage = 100;
                            Config::radiusKillInterval = 400;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Spam", ImVec2(60, 0))) {
                            Config::radiusKillDistance = 15.5f;
                            Config::radiusKillDamage = 1;
                            Config::radiusKillInterval = 100;
                        }

                        ImGui::SetNextItemWidth(colW - 20);
                        ImGui::SliderFloat("Dist##rk", &Config::radiusKillDistance, 1, 50, "Dist: %.0f");
                        ImGui::SetNextItemWidth(colW - 20);
                        ImGui::SliderFloat("Dmg##rk", &Config::radiusKillDamage, 10, 100, "Dmg: %.0f");
                    }
                    });
            }
            ImGui::EndChild();

            ImGui::SameLine(colW + 10);

            ImGui::BeginChild("Right", ImVec2(colW, 0), false);
            {
                // --- MISC ---
                static bool upsideDown_user = false;
                float miscHeight = 75;
                if (avatarRadius) miscHeight += 50;

                Section("Misc", miscHeight, [&] {
                    ToggleButton("##sl", &Config::enableCameraSyncRotation);
                    ImGui::SameLine(); ImGui::Text("Shiftlock+");

                    if (ToggleButton("##ud", &upsideDown_user)) {
                        SetLocalPlayerSize(upsideDown_user ? -1 : 1);
                        Config::motorRotationZ = upsideDown_user ? 180 : 0;
                        Config::enableMotorRotation = upsideDown_user;
                    }
                    ImGui::SameLine(); ImGui::Text("Flip");

                    ToggleButton("##hb", &avatarRadius);
                    ImGui::SameLine(); ImGui::Text("Hitbox");

                    if (avatarRadius) {
                        ImGui::Spacing();
                        ImGui::SetNextItemWidth(50);
                        ImGui::SliderInt("X", &avatarRadius_X, 1, 20);
                        ImGui::SetNextItemWidth(50);
                        ImGui::SliderInt("Y", &avatarRadius_Y, 1, 20);
                        ImGui::SetNextItemWidth(50);
                        ImGui::SliderInt("Z", &avatarRadius_Z, 1, 20);
                        if (ImGui::Button("Set", ImVec2(35, 0)))
                            if (ImGui::Button("Set", ImVec2(35, 0)))
                                UpdateGameValue<Vector3>("MvCharacterController", "elipsoidRadius", Vector3(avatarRadius_X, avatarRadius_Y, avatarRadius_Z));
                    }
                    });

            }
            ImGui::EndChild();

            ImSmoothTabEnd();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("globals")) {


            float bw = ImGui::GetContentRegionAvail().x;
            ImVec4 c = ImVec4(0.65f, 0.15f, 0.15f, 0.9f); // single red tone


            auto Btn = [&](const char* l, auto fn) {
                //ImGui::PushStyleColor(ImGuiCol_Button, c);
                //ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(c.x + 0.1f, c.y + 0.1f, c.z + 0.1f, c.w));
                if (ImGui::Button(l, ImVec2(bw, 20))) fn();
                //ImGui::PopStyleColor(2);
                };

            Btn("Kill All", [&] { ApplyInteractionToAll(100, 0, 0, 0, 8, false); });
            Btn("Slap All", [&] { CreateThread(nullptr, 0, SlapKillAllThread, nullptr, 0, nullptr); });
            Btn("Flame All", [&] { CreateThread(nullptr, 0, FlameAllThread, nullptr, 0, nullptr); });

            static bool fly = false;
            if (ImGui::Button(fly ? "Stop Fly All" : "Fly All", ImVec2(bw, 20))) {
                fly = !fly;
                if (fly) CreateThread(nullptr, 0, FlyAllPlayersThread, nullptr, 0, nullptr);
                else Config::activeFlyTargets.clear();
            }

            Btn("Heal All", [&] { CreateThread(nullptr, 0, HealAllPlayersThread, nullptr, 0, nullptr); });
            Btn("Fling All", [&] { ApplyInteractionToAll(0, 2500, 5000, 2500, 8, false); });
            Btn("Void All", [&] { union { uint32_t i; float f; }c; c.i = 0x7F7FFF00; ApplyInteractionToAll(0, 0, c.f, 0, 8, false); });

            ImGui::EndTabItem();
        }

        // ============ NOTIFICATIONS TAB ============
        if (ImGui::BeginTabItem("Notifications")) {
            ImGui::Spacing();

            // Notification Type Selection
            const char* notifTypes[] = {
                "None (0)", "CenterGun (1)", "BazookaGun (2)", "RailGun (3)",
                "Suicide (4)", "Impact (5)", "Environmental (6)", "MeleeWeapon (7)",
                "Explosive (8)", "Fire (9)", "FallOffWorld (10)", "Mutant (11)",
                "Shotgun (12)", "FlameThrower (13)", "Crushed (14)", "Ghost (15)",
                "AdvancedGhost (16)", "SixShooter (17)", "DoubleSixShooter (18)",
                "ThrowingStar (19)", "MultiThrowingStar (20)", "SlapGun (21)",
                "RayHealGun (22)", "CustomGun (23)"
            };

            int notifTypeMapping[] = {
                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
            };

            ImGui::Text("Notification Type:");
            ImGui::SetNextItemWidth(-1);
            static int selectedIndex = 0;
            if (ImGui::Combo("##notiftype", &selectedIndex, notifTypes, IM_ARRAYSIZE(notifTypes))) {
                NotificationSystem::selectedNotifType = notifTypeMapping[selectedIndex];
            }

            ImGui::Spacing();

            if (ImGui::Button("Send kill Notification", ImVec2(-1, 30))) {
                SendCustomNotification(NotificationSystem::selectedNotifType);
            }

            ImGui::Spacing();

            // Anti-Notifications
            ImGui::Text("Block All Notifications");
            ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
            if (ToggleButton("##antinotifs", &Config::enableAntiNotifications)) {//UpdOffset
                RetPatch(Offsets::NotificationArea::InstantiateNotification, Config::enableAntiNotifications, originalbyte); //NotificationArea$$InstantiateNotification
                RetPatch(Offsets::NotificationsManager::InstantiateNotification, Config::enableAntiNotifications, originalbyte); //NotificationsManager$$InstantiateNotification
            }


            ImGui::Spacing();

            ImGui::Text("Notification Spam");
            ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
            ToggleButton("##notifspam", &NotificationSystem::enableNotifSpam);

            if (NotificationSystem::enableNotifSpam) {
                ImGui::Spacing();
                ImGui::Indent(15.0f);

                ImGui::Text("Spam Delay", NotificationSystem::spamDelay);
                ImGui::SetNextItemWidth(-80);
                ImGui::SliderInt("##spamdelay", &NotificationSystem::spamDelay, 10, 1000);
                ImGui::SameLine();
                if (ImGui::Button("Fast##10ms", ImVec2(70, 0))) {
                    NotificationSystem::spamDelay = 10;
                }

                ImGui::Text("Notifications Per Tick", NotificationSystem::notifsPerTick);
                ImGui::SetNextItemWidth(-80);
                ImGui::SliderInt("##notifspertick", &NotificationSystem::notifsPerTick, 1, 50);
                ImGui::SameLine();
                if (ImGui::Button("Max##50", ImVec2(70, 0))) {
                    NotificationSystem::notifsPerTick = 50;
                }

                ImGui::Spacing();

                float notifsPerSecond = (1000.0f / NotificationSystem::spamDelay) * NotificationSystem::notifsPerTick;
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                    "Rate: %.0f notifications/second", notifsPerSecond);

                ImGui::Spacing();

                ToggleButton("##cycletypes", &NotificationSystem::cycleNotifTypes);
                ImGui::SameLine();
                ImGui::Text("Cycle Through Types");

                if (NotificationSystem::cycleNotifTypes) {
                    const char* typeNames[] = { "Kill", "Player Joined", "Subscriber" };
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Current: %s", typeNames[NotificationSystem::currentSpamIndex]);
                }

                ImGui::Unindent(15.0f);
            }

            ImGui::EndTabItem();
        }



        ImGui::EndTabBar();
    }

    ImSmoothTabEnd();
    ImGui::EndChild();
}

void DrawMiscPage() {
    ImGui::BeginChild("MISC", ImVec2(0, 0), false);
    ImSmoothTabTransition();

    if (ImGui::BeginTabBar("MiscTabs", ImGuiTabBarFlags_None)) {

        if (ImGui::BeginTabItem("Movement")) {
            ImSmoothTabTransition();

            float w = ImGui::GetContentRegionAvail().x;
            float toggleX = ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x;

            // --- WALKSPEED ---
            float walkHeight = walkspeedEnabled ? 60 : 35;
            Section("Walkspeed", walkHeight, [&] {
                ImGui::Text("Walkspeed");
                ImGui::SameLine(toggleX);
                ToggleButton("##walkspeedbool", &walkspeedEnabled);

                if (walkspeedEnabled) {
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(w - 50);
                    if (slidder("##walkspeed", &walkSpeed, 8.0f, 50.0f))
                        UpdateGameValue<float>("AvatarMotor", "walkSpeed", walkSpeed);
                    ImGui::SameLine();
                    if (ImGui::Button("R##ws", ImVec2(24, 0))) {
                        walkSpeed = 8.0f;
                        UpdateGameValue<float>("AvatarMotor", "walkSpeed", 8.0f);
                    }
                }
                });

            // --- CLICK TO TELEPORT ---
            float tpHeight = Config::enableClickToLook ? 100 : 35;
            Section("Click Teleport", tpHeight, [&] {
                ImGui::Text("Click to Teleport");
                ImGui::SameLine(toggleX);
                ToggleButton("##clicktolook", &Config::enableClickToLook);

                if (Config::enableClickToLook) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "right click to teleport in the direction you're looking");
                    ImGui::Spacing();

                    ImGui::SetNextItemWidth(w - 50);
                    if (slidder("##tpdist", &Config::teleportDistance, 5.0f, 50.0f, "Dist: %.0f")) {}
                    ImGui::SameLine();
                    if (ImGui::Button("R##tpdist", ImVec2(24, 0))) {
                        Config::teleportDistance = 15.0f;
                    }
                }
                });

            // --- CUSTOM ANIMATION ---
            float animHeight = Config::enableCustomAnimation ? 70 : 35;
            Section("Animation", animHeight, [&] {
                ImGui::Text("State Animation");
                ImGui::SameLine(toggleX);
                ToggleButton("##customAnim", &Config::enableCustomAnimation);

                if (Config::enableCustomAnimation) {
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(-10);
                    ImGui::Combo("##animSelect", &Config::selectedAnimationIndex,
                        Config::availableAnimations,
                        IM_ARRAYSIZE(Config::availableAnimations));
                }
                });

            // --- VELOCITY FLY ---
            float flyHeight = enableFly ? 130 : 35;
            Section("Velocity Fly", flyHeight, [&] {
                ImGui::Text("Velocity Fly");
                ImGui::SameLine(toggleX);
                ToggleButton("##enableFly", &enableFly);

                if (enableFly) {
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(w - 50);
                    slidder("##flySpeed", &flySpeed, 1.0f, 50.0f, "%.0f");

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Controls:");
                    ImGui::BulletText("WASD for movement");
                    ImGui::BulletText("Space - Ascend");
                    ImGui::BulletText("Shift - Descend");
                }
                });

            // --- NOCLIP ---
            Section("Noclip", 35, [&] {
                ImGui::Text("Noclip");
                ImGui::SameLine(toggleX);
                if (ToggleButton("##noclip", &enableNoclip)) {
                    if (enableNoclip) {
                        UpdateGameValue<Vector3>("MvCharacterController", "center", Vector3(99999, 99999, 99999));
                    }
                    else {
                        UpdateGameValue<Vector3>("MvCharacterController", "center", Vector3(0, 1, 0));
                    }
                }
                });

            // --- FACTOR FLY ---
            Section("Factor Fly", 35, [&] {
                static bool factorFly = false;
                ImGui::Text("Factor Fly");
                ImGui::SameLine(toggleX);
                if (ToggleButton("##niggafly", &factorFly)) {
                    if (factorFly) {
                        UpdateGameValue<float>("MvCharacterController", "offsetFactor", 100);
                    }
                    else {
                        UpdateGameValue<float>("MvCharacterController", "offsetFactor", 0);
                    }
                }
                });

            ImSmoothTabEnd();
            ImGui::EndTabItem();


        }


        if (ImGui::BeginTabItem("devtesting")) {
            ImSmoothTabTransition();

            static int s_assetID = 0;

            Section("Purchase Avatar Accessory", 60, [&] {
                float fw = ImGui::GetContentRegionAvail().x;

                ImGui::Text("Streaming Asset ID");
                ImGui::SetNextItemWidth(fw - 80);
                ImGui::InputInt("##assetid", &s_assetID, 1, 10);
                ImGui::SameLine();
                if (ImGui::Button("Buy", ImVec2(-1, 0))) {
                    if (g_OperationRequestsInstance) {
                        typedef void(__cdecl* PurchaseAvatarAccessory_t)(void* __this, int32_t streamingAssetID, void* method);
                        static PurchaseAvatarAccessory_t fnPurchase = nullptr;
                        if (!fnPurchase)
                            fnPurchase = (PurchaseAvatarAccessory_t)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + Offsets::OperationRequests::PurchaseAvatarAccessory);

                        void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
                        fnPurchase(g_OperationRequestsInstance, s_assetID, nullptr);
                        IL2CPP::Thread::Detach(pThread);

                        printf("[PurchaseAvatarAccessory] called assetID=%d\n", s_assetID);
                    }
                    else {
                        printf("[PurchaseAvatarAccessory] no OperationRequests instance yet\n");
                    }
                }


                });


            ImGui::Spacing();
            ImGui::Spacing();

            if (ImGui::Button("DUMP METHODS TO DLL DIR", ImVec2(-1, 25))) {
                Dumper::Run();
            }
            if (ImGui::Button("chatlog helloworld test", ImVec2(-1, 25))) {
                postGameMsgString("hello world", 3);
            }

            Section("level hack", Config::enableLevelSpoof ? 55 : 30, [&] {
                ImGui::Text("Level Spoof");
                ImGui::SameLine(ImGui::GetWindowWidth() - 40.f - ImGui::GetStyle().WindowPadding.x);
                ToggleButton("##lvlspoof", &Config::enableLevelSpoof);

                if (Config::enableLevelSpoof) {
                    ImGui::Spacing();
                    ImGui::SliderInt("##lvl", &Config::spoofedLevel, 1, 45, "Level %d");
                    ImGui::SameLine();
                    if (ImGui::Button("Set##lvl", ImVec2(-1, 0)))
                        SetPlayerLevel(Config::spoofedLevel);
                }
                });
            
            ToggleButton("nig", & resetTerrain);
            ImGui::SameLine();
            ImGui::Text("resetterrain");

            ImGui::Spacing();
            SeparatorText("photon nigga");
            ImGui::Spacing();

            if (!g_photonPeerInstance) ImGui::BeginDisabled();
            if (ImGui::Button("ReConnect", ImVec2(-1, 28)))
            {
                CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
                    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
                    try {
                        void* gsd = pMVGameControllerBase_get_GameSessionData(nullptr);

                        if (!gsd) { IL2CPP::Thread::Detach(pThread); return 0; }
                        void* ipStr = *(void**)((uintptr_t)gsd + 0x10);

                        if (!ipStr) { IL2CPP::Thread::Detach(pThread); return 0; }

                        reconnecting = true;
                        g_blockStatusChanged = true;
                        g_manualConnectPending = true;
                        *(void**)((uintptr_t)g_photonPeerInstance + 0xB0) = nullptr;
                        pPhotonPeer_Connect(g_photonPeerInstance, ipStr, IL2CPP::String::New("MVGameServer"), nullptr);
                    }
                    catch (...) { g_blockStatusChanged = false; }
                    IL2CPP::Thread::Detach(pThread);
                    return 0;
                    }, nullptr, 0, nullptr);
            }
            if (!g_photonPeerInstance) ImGui::EndDisabled();

            ImSmoothTabEnd();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImSmoothTabEnd();
    ImGui::EndChild();
}

void drawOtherPage() {
    ImGui::BeginChild("OTHER content", ImVec2(0, 0), false);
    ImSmoothTabTransition();
    ImGui::Text("USELESS / TESTING");
    ImGui::Separator();
    ImGui::Spacing();



    static char g_classNameBuf[256] = { 0 };
    static int  g_instanceCount = -1;

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##classname", g_classNameBuf, sizeof(g_classNameBuf)))
        g_instanceCount = -1;

    if (ImGui::Button("Find Instances", ImVec2(-1, 25)) && g_classNameBuf[0]) {
        void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
        try {
            std::string input(g_classNameBuf);
            std::string classPart = input;
            size_t dot = input.find_last_of('.');
            if (dot != std::string::npos) classPart = input.substr(dot + 1);

            auto objects = Unity::Object::FindObjectsOfType<Unity::CComponent>(classPart.c_str());
            g_instanceCount = objects ? (int)objects->m_uMaxLength : 0;
        }
        catch (...) { g_instanceCount = 0; }
        IL2CPP::Thread::Detach(pThread);
    }

    if (g_instanceCount >= 0)
        ImGui::Text("Instances: %d", g_instanceCount);

    if (ImGui::Button("Toggle Player Indicators", ImVec2(-1, 25))) {
        CallComponentMethod("ModeControllerBase", "TogglePlayerIndicators");
        //postGameMsg("<size=30><color=#7EB3E0><b>indicators</b></color>  <color=#999999>wont show at close distance</color></size>", 3);
    }

    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Scan Live Classes", ImVec2(-1, 25)))
        ScanAllClassesWithInstances();

    if (g_scanned) {
        ImGui::Text("%d classes found", (int)g_scannedClasses.size());
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##cf", g_classFilterBuf, sizeof(g_classFilterBuf));

        ImGui::BeginChild("##cl", ImVec2(-1, 250), true);
        for (auto& [name, count] : g_scannedClasses) {
            if (g_classFilterBuf[0]) {
                std::string n = name, f = g_classFilterBuf;
                std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                std::transform(f.begin(), f.end(), f.begin(), ::tolower);
                if (n.find(f) == std::string::npos) continue;
            }
            ImGui::Text("%-45s x%d", name.c_str(), count);
            ImGui::SameLine();
            if (ImGui::SmallButton(("cp##" + name).c_str())) {
                ImGui::SetClipboardText(name.c_str());
                strncpy_s(g_classNameBuf, name.c_str(), sizeof(g_classNameBuf) - 1);
            }
        }
        ImGui::EndChild();
    }


    ImGui::Spacing();
    SeparatorText("Player");
    ImGui::Spacing();




    if (ImGui::Button("avatarbadge45")) {
        CallComponentMethodOnAll("AvatarBadge", "UpdateBadge", 45);
    }

    // Shield controls grouped together
    ToggleButton("##shieldModifier", &Config::enableShieldModifier);
    ImGui::SameLine();
    ImGui::Text("Shield Modifier");

    if (Config::enableShieldModifier) {
        ImGui::Text("Shield Value", Config::shieldValue);
        if (ImGui::SliderFloat("##shieldValue", &Config::shieldValue, 0.0f, 100.0f)) {
            SetLocalPlayerShield(Config::shieldValue);
        }
    }


    ImGui::Spacing();
    ImGui::Spacing();

    // Modifier controls grouped together
    if (ImGui::Button("Modifiers Window", ImVec2(-1, 25))) {
        Config::showModifiersWindow = !Config::showModifiersWindow;
    }

    static bool infModifierActive = false;
    if (ImGui::Button("Clear All Modifiers", ImVec2(-1, 25))) {
        CallComponentMethod("MVInteractable", "ClearModifiers");
    }

   

    ImSmoothTabEnd();
    ImGui::EndChild();
}

std::vector<CachedPlayer> g_cachedPlayers;
static DWORD g_lastPlayerRefresh = 0;
static bool g_legacyPlayerlist = false;

#include "Playerlist.h"

std::set<int> g_previousActorNrs;

void RefreshPlayerCache() {
    DWORD nowRefresh = GetTickCount64();
    if (nowRefresh - g_lastPlayerRefresh <= 1000) return;
    g_lastPlayerRefresh = nowRefresh;

    std::vector<CachedPlayer> newPlayers;

    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    try {
        auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");
        if (avatarList && avatarList->m_uMaxLength > 0) {
            for (int i = 0; i < avatarList->m_uMaxLength; i++) {
                auto avatar = avatarList->operator[](i);
                if (!avatar) continue;

                bool isLocal = *(bool*)((uintptr_t)avatar + 0x28);
                void* uiHandler = *(void**)((uintptr_t)avatar + 0x88);

                std::string playerName = "Unknown";
                int actorNr = -1;

                if (uiHandler) {
                    actorNr = *(int32_t*)((uintptr_t)uiHandler + 0x30);
                    void* nameStrObj = *(void**)((uintptr_t)uiHandler + 0x38);
                    if (nameStrObj) {
                        std::wstring wname = AntiFilter::IL2CPPStringToWide(nameStrObj);
                        if (!wname.empty()) {
                            int sz = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);
                            if (sz > 0) {
                                std::string utf8(sz - 1, 0);
                                WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &utf8[0], sz, nullptr, nullptr);
                                playerName = utf8;
                            }
                        }
                    }
                }

                auto gameObj = avatar->GetGameObject();
                newPlayers.push_back({ (void*)avatar, gameObj, playerName, actorNr, isLocal });
            }
        }
    }
    catch (...) {}

    // detect joins/leaves and clean up stale targets
    static bool g_firstRefreshDone = false;
    std::set<int> newActorNrs;
    for (auto& p : newPlayers)
        if (p.actorNr > 0) newActorNrs.insert(p.actorNr);

    // find who left and remove their stale pointers from targeted players
    for (auto& prevNr : g_previousActorNrs) {
        if (!newActorNrs.count(prevNr)) {
            // player left, find their gameObject from old cache and clean up
            std::lock_guard<std::mutex> lock(g_playerCacheMutex);
            for (auto& old : g_cachedPlayers) {
                if (old.actorNr == prevNr) {
                    // remove from targeted players
                    Config::targetedPlayers.erase(
                        std::remove_if(Config::targetedPlayers.begin(), Config::targetedPlayers.end(),
                            [&](const TargetedPlayer& t) { return t.instance == old.gameObject; }),
                        Config::targetedPlayers.end());
                    // remove from looping interactions
                    for (auto it = Config::loopingInteractions.begin(); it != Config::loopingInteractions.end();) {
                        if (it->second.targetInstance == old.gameObject)
                            it = Config::loopingInteractions.erase(it);
                        else
                            ++it;
                    }
                    // clear follow target if it was this player
                    if (Config::customInteractionTarget == old.gameObject) {
                        Config::customInteractionTarget = nullptr;
                        Config::followCustomTarget = false;
                    }
                    break;
                }
            }
        }
    }

    if (!g_firstRefreshDone) {
        for (auto& p : newPlayers) {
            if (p.actorNr > 0 && IsWatchlisted(p.name)) {
                std::string msg = p.name + " IS IN THE SERVER.";
                PushNotification("MODERATOR ALERT", msg.c_str(), NotificationType::Error, 6.0f);
            }
        }
        g_firstRefreshDone = true;
    }
    else if (!g_previousActorNrs.empty()) {
        for (auto& p : newPlayers) {
            if (p.actorNr > 0 && !g_previousActorNrs.count(p.actorNr)) {
                if (IsWatchlisted(p.name)) {
                    std::string msg = p.name + " JOINED THE SERVER.";
                    PushNotification("MODERATOR ALERT", msg.c_str(), NotificationType::Error, 6.0f);
                }
                CheckBlacklistOnJoin(p.name, p.gameObject);
            }
        }
    }

    g_previousActorNrs = newActorNrs;

    {
        std::lock_guard<std::mutex> lock(g_playerCacheMutex);
        g_cachedPlayers = std::move(newPlayers);
    }

    IL2CPP::Thread::Detach(pThread);
}


void DrawNetworkPage() {
    std::vector<CachedPlayer> players;
    { std::lock_guard<std::mutex> lock(g_playerCacheMutex); players = g_cachedPlayers; }

    ImGui::BeginChild("NETWORK content", ImVec2(0, 0), false);
    ImSmoothTabTransition();

    if (ImGui::BeginTabBar("NetworkTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Players")) { DrawPlayersTab();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Custom")) { /* unchanged */    ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImSmoothTabEnd();
    ImGui::EndChild();
}


void DrawThemeSkyboxSection() {
    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    Unity::CComponent* skybox = nullptr;
    try {
        auto list = Unity::Object::FindObjectsOfType<Unity::CComponent>("ThemeSkybox");
        if (list && list->m_uMaxLength > 0) skybox = (Unity::CComponent*)list->operator[](0);
    }
    catch (...) {}

    if (!skybox) {
        IL2CPP::Thread::Detach(pThread);
        ImGui::TextDisabled("No ThemeSkybox found");
        return;
    }

    float w = ImGui::GetContentRegionAvail().x;
    float hw = (w - 8) * 0.5f;

    auto SetFloat = [&](const char* setter, float val) {
        skybox->CallMethodSafe<void*>(setter, val);
        };
    auto SetColor = [&](const char* setter, ImVec4 c) {
        Unity::Vector4 col{ c.x, c.y, c.z, c.w };
        skybox->CallMethodSafe<void*>(setter, col);
        };
    auto SetBool = [&](const char* setter, bool val) {
        skybox->CallMethodSafe<void*>(setter, val);
        };

    // --- Sky Colors ---
    static ImVec4 topColor = { 0.1f, 0.2f, 0.5f, 1.f };
    static ImVec4 bottomColor = { 0.6f, 0.7f, 0.9f, 1.f };

    ImGui::Text("Top"); ImGui::SameLine();
    if (ImGui::ColorEdit4("##tc", (float*)&topColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        SetColor("set_TopColor", topColor);
    ImGui::SameLine(hw + 8);
    ImGui::Text("Bot"); ImGui::SameLine();
    if (ImGui::ColorEdit4("##bc", (float*)&bottomColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        SetColor("set_BottomColor", bottomColor);

    ImGui::Spacing();

    // --- Sun ---
    if (ImGui::TreeNodeEx("Sun", ImGuiTreeNodeFlags_SpanFullWidth)) {
        static ImVec4 sunTint = { 1,1,0.8f,1 };
        static float  sunSize = 1.f, sunContrast = 0.5f, sunIntensity = 1.f;
        static float  sunFlare = 1.f, sunH = 0.3f, sunAxis = 0.f;

        ImGui::Text("Tint"); ImGui::SameLine();
        if (ImGui::ColorEdit4("##st", (float*)&sunTint, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
            SetColor("set_SunTint", sunTint);

        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##ssize", &sunSize, 0.1f, 3.f, "Size %.2f"))       SetFloat("set_SunSize", sunSize);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##scont", &sunContrast, 0.f, 1.f, "Contrast %.2f")) SetFloat("set_SunLightContrast", sunContrast);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##sint", &sunIntensity, 0.f, 8.f, "Intensity %.2f")) SetFloat("set_SunLightIntensity", sunIntensity);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##sflar", &sunFlare, 0.01f, 2.f, "Flare %.2f"))    SetFloat("set_SunFlareBrightness", sunFlare);
        ImGui::SetNextItemWidth(hw - 4);
        if (ImGui::SliderFloat("##sh", &sunH, -1.f, 1.f, "H %.2f")) { SetFloat("set_SunHeight", sunH); skybox->CallMethodSafe<void*>("RecalculateSunLight"); }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(hw - 4);
        if (ImGui::SliderFloat("##sa", &sunAxis, -180.f, 180.f, "Ax %.0f"))      SetFloat("set_SunAxisDegrees", sunAxis);

        ImGui::TreePop();
    }

    // --- Moon ---
    if (ImGui::TreeNodeEx("Moon", ImGuiTreeNodeFlags_SpanFullWidth)) {
        static ImVec4 moonTint = { 0.8f,0.9f,1.f,1.f };
        static float  moonSize = 1.f, moonContrast = 0.3f, moonIntensity = 0.5f;
        static float  moonFlare = 0.5f, moonH = -0.2f, moonAxis = 180.f;

        ImGui::Text("Tint"); ImGui::SameLine();
        if (ImGui::ColorEdit4("##mt", (float*)&moonTint, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
            SetColor("set_MoonTint", moonTint);

        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##msize", &moonSize, 0.1f, 3.f, "Size %.2f"))        SetFloat("set_MoonSize", moonSize);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##mcont", &moonContrast, 0.f, 1.f, "Contrast %.2f")) SetFloat("set_MoonLightContrast", moonContrast);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##mint", &moonIntensity, 0.f, 8.f, "Intensity %.2f")) SetFloat("set_MoonLightIntensity", moonIntensity);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##mflar", &moonFlare, 0.01f, 2.f, "Flare %.2f"))     SetFloat("set_MoonFlareBrightness", moonFlare);
        ImGui::SetNextItemWidth(hw - 4);
        if (ImGui::SliderFloat("##mh", &moonH, -1.f, 1.f, "H %.2f")) { SetFloat("set_MoonHeight", moonH); skybox->CallMethodSafe<void*>("RecalculateMoonLight"); }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(hw - 4);
        if (ImGui::SliderFloat("##ma", &moonAxis, -180.f, 180.f, "Ax %.0f"))       SetFloat("set_MoonAxisDegrees", moonAxis);

        ImGui::TreePop();
    }

    // --- Stars ---
    if (ImGui::TreeNodeEx("Stars", ImGuiTreeNodeFlags_SpanFullWidth)) {
        static ImVec4 starsTint = { 1,1,1,1 };
        static float  starsExt = 1.f, starsTwink = 1.f;

        ImGui::Text("Tint"); ImGui::SameLine();
        if (ImGui::ColorEdit4("##stt", (float*)&starsTint, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
            SetColor("set_StarsTint", starsTint);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##sext", &starsExt, 0.f, 10.f, "Extinction %.1f"))     SetFloat("set_StarsExtinction", starsExt);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##stwink", &starsTwink, 0.f, 25.f, "Twinkle %.1f"))       SetFloat("set_StarsTwinklingSpeed", starsTwink);

        ImGui::TreePop();
    }

    // --- Fog ---
    if (ImGui::TreeNodeEx("Fog", ImGuiTreeNodeFlags_SpanFullWidth)) {
        static bool  fogOn = false;
        static float fogDens = 0.01f, fogStart = 0.f, fogEnd = 500.f;

        if (ImGui::Checkbox("Enabled##fog", &fogOn))           SetBool("set_FogEnabled", fogOn);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##fdens", &fogDens, 0.f, 0.5f, "Density %.3f")) { SetFloat("set_FogDensity", fogDens); skybox->CallMethodSafe<void*>("RecalculateFogColor"); }
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##fstart", &fogStart, 0.f, 500.f, "Start %.0f"))      SetFloat("set_FogStartDistance", fogStart);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##fend", &fogEnd, 0.f, 2000.f, "End %.0f"))        SetFloat("set_FogEndDistance", fogEnd);

        ImGui::TreePop();
    }

    // --- Clouds & Exposure ---
    if (ImGui::TreeNodeEx("Clouds & Exposure", ImGuiTreeNodeFlags_SpanFullWidth)) {
        static float cH = 0.f, cOff = 0.5f, cSpd = 0.f, expo = 1.f;

        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##ch", &cH, -0.75f, 0.75f, "Height %.2f"))      SetFloat("set_CloudsHeight", cH);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##coff", &cOff, 0.f, 1.f, "Offset %.2f"))      SetFloat("set_CloudsOffset", cOff);
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##cspd", &cSpd, -50.f, 50.f, "Rot Spd %.1f"))     SetFloat("set_CloudsRotationSpeed", cSpd);
        ImGui::Spacing();
        ImGui::SetNextItemWidth(w - 20);
        if (ImGui::SliderFloat("##expo", &expo, 0.f, 10.f, "Exposure %.2f"))    SetFloat("set_Exposure", expo);

        ImGui::TreePop();
    }

    // --- Presets ---
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Presets");
    float pw = (w - ImGui::GetStyle().ItemSpacing.x * 3) / 4.f;

    auto ApplySkyPreset = [&](ImVec4 top, ImVec4 bot, float sunH_, float sunAx, float expo_) {
        SetColor("set_TopColor", top);    SetColor("set_BottomColor", bot);
        SetFloat("set_SunHeight", sunH_); SetFloat("set_SunAxisDegrees", sunAx);
        SetFloat("set_Exposure", expo_);
        skybox->CallMethodSafe<void*>("RecalculateSunLight");
        };

    if (ImGui::Button("Day", ImVec2(pw, 0))) ApplySkyPreset({ 0.2f,0.5f,1.f,1 }, { 0.6f,0.75f,0.9f,1 }, 0.5f, 0, 1.f);
    ImGui::SameLine();
    if (ImGui::Button("Sunset", ImVec2(pw, 0))) ApplySkyPreset({ 0.05f,0.05f,0.2f,1 }, { 0.9f,0.4f,0.1f,1 }, 0.05f, 20, 1.2f);
    ImGui::SameLine();
    if (ImGui::Button("Night", ImVec2(pw, 0))) ApplySkyPreset({ 0.01f,0.01f,0.05f,1 }, { 0.02f,0.02f,0.1f,1 }, -0.5f, 0, 0.3f);
    ImGui::SameLine();
    if (ImGui::Button("Void", ImVec2(pw, 0))) ApplySkyPreset({ 0,0,0,1 }, { 0,0,0,1 }, -1.f, 0, 0.f);

    IL2CPP::Thread::Detach(pThread);
}


void DrawVisualsPage() {
    ImGui::BeginChild("VisualsContent", ImVec2(0, 0), false);
    ImSmoothTabTransition();
    ImGui::Text("VISUALS");
    ImGui::Separator();
    ImGui::Spacing();

    float w = ImGui::GetContentRegionAvail().x;

    ESP::RenderUI();
    ImGui::Spacing();

    static float cameraheight;
    static bool camheight2;
    // --- CAMERA ---
    float camHeight = 90;
    if (camdistance)              camHeight += 30;
	if (camheight2)              camHeight += 30;
    if (Config::enableFOVChanger) camHeight += 30;

    Section("Camera", camHeight, [&] {
        ToggleButton("##camdistancee", &camdistance);
        ImGui::SameLine(); ImGui::Text("Camera Distance");
        if (camdistance) {
            ImGui::SetNextItemWidth(w - 50);
            if (slidder("##camera_distance", &cameraDistance, 1.0f, 50.0f))
                UpdateGameValue<float>("PlaymodeCamera", "distanceToAvatar", cameraDistance);
            ImGui::SameLine();
            if (ImGui::Button("R##cam", ImVec2(-1, 0))) {
                cameraDistance = 1.5;
                UpdateGameValue<float>("PlaymodeCamera", "distanceToAvatar", 1.5);
            }
        }

        ToggleButton("##camheight", &camheight2);
        ImGui::SameLine(); ImGui::Text("Camera Height");
        if (camheight2) {
            ImGui::SetNextItemWidth(w - 50);
            if (slidder("##camera_height", &cameraheight, 1.0f, 50.0f))
                UpdateGameValue<float>("PlaymodeCamera", "height", cameraheight);
            ImGui::SameLine();
            if (ImGui::Button("R##height", ImVec2(-1, 0))) {
                cameraheight = 5.0f;
                UpdateGameValue<float>("PlaymodeCamera", "height", 1.0f);
            }
        }

        ToggleButton("##fov", &Config::enableFOVChanger);
        ImGui::SameLine(); ImGui::Text("Custom FOV");
        if (Config::enableFOVChanger) {
            ImGui::SetNextItemWidth(w - 50);
            slidder("##fovslider", &Config::customFOV, 10.0f, 150.0f);


            ImGui::SameLine();
            if (ImGui::Button("R##fov", ImVec2(-1, 0))) {
                Config::customFOV = 60.0f;
                SetCameraFOV(60.0f);
            }
        }
        });

    ImGui::Spacing();

    float clipHeight = 35;
    if (Config::enableClipPlaneChanger) clipHeight += 35;

    Section("Render Distance", clipHeight, [&] {
        ToggleButton("##clipplanes", &Config::enableClipPlaneChanger);
        ImGui::SameLine(); ImGui::Text("Render Distance");
        if (Config::enableClipPlaneChanger) {
            ImGui::SetNextItemWidth(w - 50);
            if (slidder("##renderdist", &Config::farClipPlane, 5.0f, 1000.0f, "%.0fm")) {
                SetCameraClipPlanes(0.01f, Config::farClipPlane);
            }
            ImGui::SameLine();
            if (ImGui::Button("R##rd", ImVec2(-1, 0))) {
                Config::farClipPlane = 1000.0f;
                SetCameraClipPlanes(0.01f, 1000.0f);
            }
        }
        });

    ImGui::Spacing();

    // --- CULLING ---
    float cullHeight = 35;
    if (Config::enableCullingMask) cullHeight += 35;

    Section("Layer Culling", cullHeight, [&] {
        ToggleButton("##culling", &Config::enableCullingMask);
        ImGui::SameLine(); ImGui::Text("Layer Culling");
        if (Config::enableCullingMask) {
            ImGui::SetNextItemWidth(w - 100);
            if (ImGui::InputInt("##cullingmask", &Config::cullingMask))
                SetCullingMask(Config::cullingMask);
            ImGui::SameLine();
            if (ImGui::Button("All##cull", ImVec2(36, 0))) { Config::cullingMask = -1; SetCullingMask(-1); }
            ImGui::SameLine();
            if (ImGui::Button("None##cull", ImVec2(36, 0))) { Config::cullingMask = 0; SetCullingMask(0); }
        }
        });

    ImGui::Spacing();

    // --- FOV CIRCLE ---
    Section("FOV Circle", showFOVCircle ? 55 : 28, [&] {
        ToggleButton("##showfov", &showFOVCircle);
        ImGui::SameLine(); ImGui::Text("FOV Circle");
        if (showFOVCircle) {
            ImGui::SetNextItemWidth(w - 20);
            slidder("##piefov", &pieMenuFOV, 30.0f, 400.0f, "%.0fpx");
        }
        });

    ImGui::Spacing();

    // --- SKYBOX ---
    if (ImGui::CollapsingHeader("skybox / lighting")) {
        Section("Skybox", 200, [&] {
            DrawThemeSkyboxSection();
            });
    }

    ImGui::Spacing();

    // --- THEME PREVIEWS ---
    if (ImGui::CollapsingHeader("skybox theme previews")) {
        Section("skybox theme previews", 90, [&] {
            float fw = ImGui::GetContentRegionAvail().x;

            ToggleButton("##themesenabled", &g_themesEnabled);
            ImGui::SameLine();
            ImGui::Text("Themes Enabled");
            if (!g_themesEnabled) { SetThemesEnabled(false); return; }

            ImGui::Spacing();

            ImGui::SetNextItemWidth(fw - 90);
            if (ImGui::BeginCombo("##themepick",
                g_selectedThemeIndex >= 0 ? themeIdentifiers[g_selectedThemeIndex] : "Select..."))
            {
                for (int i = 0; i < THEME_COUNT; i++) {
                    bool sel = (g_selectedThemeIndex == i);
                    if (ImGui::Selectable(themeIdentifiers[i], sel)) {
                        g_selectedThemeIndex = i;
                        ApplyThemeByName(themeIdentifiers[i]);
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Button("Normal##rst", ImVec2(-1, 0))) {
                g_selectedThemeIndex = 9;
                ApplyThemeByName("Normal");
            }

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.12f, 0.12f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.18f, 0.18f, 1.f));
            if (ImGui::Button("Destroy Preview##thm", ImVec2(fw, 0))) {
                void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
                DestroyThemePreview();
                IL2CPP::Thread::Detach(pThread);
                g_selectedThemeIndex = -1;
            }
            ImGui::PopStyleColor(2);
            });
    }

    ImSmoothTabEnd();
    ImGui::EndChild();
}

void drawCubesPage() {
    ImGui::BeginChild("grape cubes", ImVec2(0, 0), false);
    ImSmoothTabTransition();
    ImGui::Text("drop the album carti");
    ImGui::Separator();
    ImGui::Spacing();

    // ---- CUBE DELETE ----
    ToggleButton("##enhancedBreaking", &Config::enableEnhancedBreaking); ImGui::SameLine();
    ImGui::Text("Cube Delete");

    if (Config::enableEnhancedBreaking) {
        ImGui::Spacing();

        // -- CORNER PICKER --
        ImGui::Checkbox("corner Picker", &Config::cornerPickerMode); //ImGui::SameLine();

        if (Config::cornerPickerMode) {
            ImGui::Spacing();
            ImGui::Indent(12.f);

            if (g_cornerState == CornerState::Idle) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f),
                    "Break corner A to start...");
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
                    "A: (%d, %d, %d)", g_cornerA.x, g_cornerA.y, g_cornerA.z);
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                    "Now break corner B...");
                if (ImGui::SmallButton("Cancel##cp")) {
                    g_cornerState = CornerState::Idle;
                    while (!cubeDeleteQueue.empty()) cubeDeleteQueue.pop();
                    processingDeleteQueue = false;
                }
            }

            if (!cubeDeleteQueue.empty() || processingDeleteQueue) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.1f, 1.0f),
                    "Deleting: %d cubes remaining", (int)cubeDeleteQueue.size());
                if (ImGui::SmallButton("Stop##cpstop")) {
                    while (!cubeDeleteQueue.empty()) cubeDeleteQueue.pop();
                    processingDeleteQueue = false;
                }
            }

            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f),
                "Fills box at 16 cubes/sec");
            ImGui::Unindent(12.f);
        }
        else {
            // -- RADIUS MODE --
            ImGui::Text("Break Radius:");
            ImGui::SetNextItemWidth(200);
            ImGui::SliderInt("##breakRadius", &Config::breakRadius, 1, 12);
            ImGui::SameLine();
            ImGui::Text("(%dx%dx%d)",
                Config::breakRadius * 2 + 1,
                Config::breakRadius * 2 + 1,
                Config::breakRadius * 2 + 1);
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "All cubes deleted instantly");
        }
    }

    ImGui::Spacing();

    ToggleButton("##noconstraints", &Config::enableNoConstraints);
    ImGui::SameLine();
    ImGui::Text("No Build Limit");

    ImGui::Spacing();

    Builder::RenderUI();

    ImGui::Spacing();

    CubeShaper::RenderUI();  // ADD THIS

    ImSmoothTabEnd();
    ImGui::EndChild();
}


void drawChatPage() {
    ImGui::BeginChild("chatttts", ImVec2(0, 0), false);
    ImSmoothTabTransition();
    ImGui::Text("opium...");
    ImGui::Separator();
    ImGui::Spacing();
    float w = ImGui::GetContentRegionAvail().x;
    float colW = (w - 10) * 0.5f;

    float bubbleH = Config::enableChatBubbleCustomizer ? 200.f : 32.f;
    Section("Chat Bubble", bubbleH, [&] {
        float fw = ImGui::GetContentRegionAvail().x;

        ImGui::Text("Chat bubble customizer");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
        ToggleButton("##cbc", &Config::enableChatBubbleCustomizer);

        if (!Config::enableChatBubbleCustomizer) return;

        ImGui::Spacing();

        // --- Presets ---
        float pw = (fw - ImGui::GetStyle().ItemSpacing.x * 4) / 5.f;

        auto ApplyPreset = [&](Config::ChatBubbleConfig&& p) {
            Config::bubbleCfg = std::move(p);
            };

        if (ImGui::Button("Default", ImVec2(pw, 20))) {
            Config::bubbleCfg = Config::ChatBubbleConfig{}; // all toggles off = full reset
            void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
            try {
                auto bubbles = Unity::Object::FindObjectsOfType<Unity::CComponent>("ChatBubble");
                if (bubbles) for (int i = 0; i < (int)bubbles->m_uMaxLength; i++)
                    if (auto bb = bubbles->operator[](i)) ResetChatBubble(bb);
            }
            catch (...) {}
            IL2CPP::Thread::Detach(pThread);
        }
        ImGui::SameLine();
        if (ImGui::Button("Dark", ImVec2(pw, 20))) ApplyPreset(MakeDarkPreset());
        ImGui::SameLine();
        if (ImGui::Button("Minimal", ImVec2(pw, 20))) ApplyPreset(MakeMinimalPreset());
        ImGui::SameLine();
        if (ImGui::Button("Sharp", ImVec2(pw, 20))) ApplyPreset(MakeSharpPreset());
        ImGui::SameLine();
        // Reset button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.15f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.2f, 0.2f, 1.f));
        if (ImGui::Button("Reset", ImVec2(pw, 20))) {
            Config::bubbleCfg = Config::ChatBubbleConfig{};
            void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
            try {
                auto bubbles = Unity::Object::FindObjectsOfType<Unity::CComponent>("ChatBubble");
                if (bubbles) for (int i = 0; i < (int)bubbles->m_uMaxLength; i++)
                    if (auto bb = bubbles->operator[](i)) ResetChatBubble(bb);
            }
            catch (...) {}
            IL2CPP::Thread::Detach(pThread);
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();


        float cw = (fw - 10) * 0.5f;

        // Left column
        ImGui::BeginChild("##bbl", ImVec2(cw, 0), false);
        {
            // Radius + border thickness
            ToggleButton("##benr", &Config::bubbleCfg.enableRadius);
            ImGui::SameLine(); ImGui::Text("Radius / Border");
            if (Config::bubbleCfg.enableRadius) {
                ImGui::SetNextItemWidth(cw - 20);
                ImGui::SliderFloat("##bbr", &Config::bubbleCfg.radius, 0.f, 60.f, "Radius %.0f");
                ImGui::SetNextItemWidth(cw - 20);
                ImGui::SliderFloat("##bbt", &Config::bubbleCfg.borderThickness, 0.f, 6.f, "Border %.1f");
            }

            ImGui::Spacing();

            // Background color
            ToggleButton("##benbg", &Config::bubbleCfg.enableBgColor);
            ImGui::SameLine(); ImGui::Text("Background");
            if (Config::bubbleCfg.enableBgColor) {
                ImGui::SetNextItemWidth(cw - 20);
                ImGui::ColorEdit4("##bbgc", (float*)&Config::bubbleCfg.bgColor,
                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            }

            ImGui::Spacing();

            // Border color
            ToggleButton("##benbc", &Config::bubbleCfg.enableBorderColor);
            ImGui::SameLine(); ImGui::Text("Border Color");
            if (Config::bubbleCfg.enableBorderColor) {
                ImGui::SetNextItemWidth(cw - 20);
                ImGui::ColorEdit4("##bbbc", (float*)&Config::bubbleCfg.borderColor,
                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            }

            ImGui::Spacing();

            // Wrap width
            ToggleButton("##benww", &Config::bubbleCfg.enableWrapWidth);
            ImGui::SameLine(); ImGui::Text("Wrap Width");
            if (Config::bubbleCfg.enableWrapWidth) {
                ImGui::SetNextItemWidth(cw - 20);
                ImGui::SliderFloat("##bbww", &Config::bubbleCfg.wrapWidth, 100.f, 700.f, "%.0f");
            }
        }
        ImGui::EndChild();

        ImGui::SameLine(cw + 10);

        // Right column
        ImGui::BeginChild("##bbr2", ImVec2(cw, 0), false);
        {
            // Name tag color
            ToggleButton("##bennc", &Config::bubbleCfg.enableNameColor);
            ImGui::SameLine(); ImGui::Text("Name Tag Color");
            if (Config::bubbleCfg.enableNameColor) {
                ImGui::SetNextItemWidth(cw - 20);
                ImGui::ColorEdit4("##bbnc2", (float*)&Config::bubbleCfg.nameContainerColor,
                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            }

            ImGui::Spacing();

            // Text color (message + name text)
            ToggleButton("##bentc", &Config::bubbleCfg.enableTextColor);
            ImGui::SameLine(); ImGui::Text("Text Color");
            if (Config::bubbleCfg.enableTextColor) {
                ImGui::SetNextItemWidth(cw - 20);
                ImGui::ColorEdit4("##bbtc", (float*)&Config::bubbleCfg.textColor,
                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            }

            ImGui::Spacing();

            // Hide name entirely
            ToggleButton("##benhn", &Config::bubbleCfg.enableHideName);
            ImGui::SameLine(); ImGui::Text("Hide Name");

            ImGui::Spacing();

            // Min size
            ToggleButton("##benms", &Config::bubbleCfg.enableMinSize);
            ImGui::SameLine(); ImGui::Text("Min Size");
            if (Config::bubbleCfg.enableMinSize) {
                ImGui::SetNextItemWidth((cw - 24) * 0.5f);
                ImGui::SliderFloat("##bbmw", &Config::bubbleCfg.minW, 10.f, 150.f, "W:%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth((cw - 24) * 0.5f);
                ImGui::SliderFloat("##bbmh", &Config::bubbleCfg.minH, 5.f, 80.f, "H:%.0f");
            }

            ImGui::Spacing();

            // Mute sound
            ToggleButton("##bensnd", &Config::bubbleCfg.enableMuteSound);
            ImGui::SameLine(); ImGui::Text("Mute Pop Sound");

        }
        ImGui::EndChild();
        });




    Section("Options", 40, [&] {
        ToggleButton("##Modify Chat Text", &Config::modifyChatText);
        ImGui::SameLine(); ImGui::Text("No Chat Bubble");
        ImGui::SameLine(colW + 10);
        ToggleButton("##anti_filter", &AntiFilter::enabled);
        ImGui::SameLine(); ImGui::Text("Anti Chat Filter");
        });

    float spamH = 55;
    if (Config::enableSpam) spamH += 28;

    Section("Chat Spam", spamH, [&] {
        ImGui::SetNextItemWidth(w - 20);
        ImGui::InputText("##chathaxtext", Config::niggerText, sizeof(Config::niggerText));

        ImGui::Spacing();

        ImGui::Checkbox("Spam", &Config::enableSpam);
        ImGui::SameLine(colW + 10);
        ImGui::Checkbox("Rainbow Mode", &Config::enableRainbow);

        if (Config::enableSpam) {
            ImGui::SetNextItemWidth(w - 80);
            ImGui::SliderInt("Delay##spam", &Config::spamDelay, 10, 200);
            ImGui::SameLine();
            if (ImGui::Button("Fast##10ms", ImVec2(-1, 0))) Config::spamDelay = 10;
        }

        ImGui::Spacing();
        if (ImGui::Button("Send Test##chattest", ImVec2((w - 20) * 0.5f, 22))) {
            char temp[512]{};
            strcpy_s(temp, "</color>  </color>");
            strcat_s(temp, Config::niggerText);
            wchar_t wmsg[512]{};
            MultiByteToWideChar(CP_UTF8, 0, temp, -1, wmsg, 512);
            CallComponentMethod("SendMessageControl", "SendChatMessage", wmsg);
        }
        });


    static ImVec4 adminColor = ImVec4(1, 1, 1, 1);
    static int    adminSize = 30;
    static bool   useQuad = false;
    static int    quadSize = 1000;
    static char   adminMsg[512] = {};

    float gmH = 145; // color/size row + quad row + input + presets + send
    if (useQuad) gmH += 22;

    Section("admin Message", gmH, [&] {

        ImGui::Text("Color:"); ImGui::SameLine();
        ImGui::ColorEdit3("##admincolor", (float*)&adminColor,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        ImGui::SameLine(colW + 10);
        ImGui::Text("Size:"); ImGui::SameLine();
        ImGui::SetNextItemWidth(colW - 45);
        ImGui::SliderInt("##adminsize", &adminSize, 30, 200);

        ImGui::Spacing();

        // Row: Quad toggle | quad size input if on
        ToggleButton("##usequad", &useQuad);
        ImGui::SameLine(); ImGui::Text("Quad Prefix");
        if (useQuad) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("##quadsize", &quadSize, 100, 500);
        }

        ImGui::Spacing();

        // Message input
        ImGui::SetNextItemWidth(w - 20);
        ImGui::InputText("##adminmsg", adminMsg, sizeof(adminMsg));

        ImGui::Spacing();

        // Presets
        float pw = (w - 20 - ImGui::GetStyle().ItemSpacing.x * 3) / 4.0f;
        if (ImGui::Button("Shutdown##p", ImVec2(pw, 0))) strncpy_s(adminMsg, "Server shutting down in 60 seconds.", sizeof(adminMsg));
        ImGui::SameLine();
        if (ImGui::Button("Restart##p", ImVec2(pw, 0))) strncpy_s(adminMsg, "Server restarting. Please rejoin.", sizeof(adminMsg));
        ImGui::SameLine();
        if (ImGui::Button("Warn##p", ImVec2(pw, 0))) strncpy_s(adminMsg, "Warning: follow the rules or you will be removed.", sizeof(adminMsg));
        ImGui::SameLine();
        if (ImGui::Button("Maint##p", ImVec2(pw, 0))) strncpy_s(adminMsg, "Server entering maintenance mode.", sizeof(adminMsg));

        ImGui::Spacing();

        if (ImGui::Button("Send Admin Message", ImVec2(w - 20, 22))) {
            char hexColor[8];
            snprintf(hexColor, sizeof(hexColor), "%02X%02X%02X",
                (int)(adminColor.x * 255), (int)(adminColor.y * 255), (int)(adminColor.z * 255));
            char finalMsg[700]{};
            if (useQuad)
                snprintf(finalMsg, sizeof(finalMsg),
                    "<quad=size=%d color=cyan><size=%d><color=#%s>%s</color></size>",
                    quadSize, adminSize, hexColor, adminMsg);
            else
                snprintf(finalMsg, sizeof(finalMsg),
                    "<size=%d><color=#%s>%s</color></size>",
                    adminSize, hexColor, adminMsg);
            postGameMsg(finalMsg, 3);
        }
        });

    Section("OPM Chat", 70, [&] {
        static char opmMsg[512]{};
        ImGui::SetNextItemWidth(w - 90);
        ImGui::InputText("##opmmsg", opmMsg, sizeof(opmMsg));
        ImGui::SameLine();
        if (ImGui::Button("Send##opm", ImVec2(-1, 0)) && opmMsg[0]) {
            // get local name from player cache
            std::string localName = "unknown";
            for (auto& p : g_cachedPlayers)
                if (p.isLocal && !p.name.empty()) { localName = p.name; break; }

            char finalMsg[700]{};
            snprintf(finalMsg, sizeof(finalMsg),
                "<size=-100>opm![%s]: %s</size>",
                localName.c_str(), opmMsg);
            postGameMsg(finalMsg, 3);
            opmMsg[0] = '\0';
        }
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "send to other opm users");
        });

    // --- DANGER ---
    Section("Danger", 40, [&] {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 0.8f));
        if (ImGui::Button("Chat Crash", ImVec2((w - 20) * 0.5f, 22))) {
            char temp[512]{};
            strcpy_s(temp, "</color>  </color><quad=size=-2147483648 width=2147483648");
            wchar_t wmsg[512]{};
            MultiByteToWideChar(CP_UTF8, 0, temp, -1, wmsg, 512);
            CallComponentMethod("SendMessageControl", "SendChatMessage", wmsg);
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button("Mass Impersonate", ImVec2((w - 20) * 0.5f, 22)))
            ImGui::OpenPopup("MassImpersonatePopup");

        if (ImGui::BeginPopup("MassImpersonatePopup")) {
            static char massMsg[512]{};
            ImGui::SetNextItemWidth(300);
            ImGui::InputText("##massmsg", massMsg, sizeof(massMsg));
            ImGui::SameLine();
            if (ImGui::Button("Send As All")) {
                std::lock_guard<std::mutex> lock(g_playerCacheMutex);
                for (auto& p : g_cachedPlayers) {
                    if (p.isLocal) continue;
                    char fm[700]{};
                    snprintf(fm, sizeof(fm), "<color=#6FA5FF>[%s]:</color> %s", p.name.c_str(), massMsg);
                    g_massMimicQueue.push_back(fm);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        });


    ImSmoothTabEnd();
    ImGui::EndChild();
}

void DrawMainContent() { //tab
    switch (Config::currentPage) {
    case UIPage::WEAPONS:
        DrawWeaponsPage();
        break;
    case UIPage::EXPLOITS:
        DrawExploitsPage();
        break;
    case UIPage::NETWORK:
        DrawNetworkPage();
        break;
    case UIPage::MISC:
        DrawMiscPage();
        break;
    case UIPage::THEMES:
        DrawThemesPage();
        break;
    case UIPage::SETTINGS:
        DrawSettingsPage();
        break;
    case UIPage::VISUALS:
        DrawVisualsPage();
        break;
    case UIPage::OTHER:
        drawOtherPage();
        break;
    case UIPage::CUBES:
        drawCubesPage();
        break;

    case UIPage::CHAT:
        drawChatPage();
        break;


    }
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    if (uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONUP ||
        (uMsg == WM_MOUSEMOVE && GetAsyncKeyState(VK_MBUTTON) & 0x8000)) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
            return true;
    }

    if (show_menu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    ImGuiIO& io = ImGui::GetIO();

    // Block keyboard input when ImGui wants it
    if (show_menu && io.WantCaptureKeyboard) {
        switch (uMsg) {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_UNICHAR:
            return 0;
        }
    }

    // Block mouse input when ImGui wants it
    if (show_menu && io.WantCaptureMouse) {
        switch (uMsg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_MOUSEMOVE:
            return 0;
        }
    }

    return CallWindowProcA(oWndProc, hWnd, uMsg, wParam, lParam);
}

void SetupImGuiFonts() {
    ImGuiIO& io = ImGui::GetIO();

    // FontAwesome configs (your existing code)
    ImFontConfig regular_config;
    regular_config.MergeMode = true;
    regular_config.PixelSnapH = true;
    regular_config.GlyphMinAdvanceX = 16.0f;

    ImFontConfig solid_config;
    solid_config.MergeMode = true;
    solid_config.PixelSnapH = true;
    solid_config.GlyphMinAdvanceX = 16.0f;

    static const ImWchar extended_ranges[] = { 0xe000, 0xf8ff, 0 };

    io.Fonts->AddFontFromMemoryTTF(
        (void*)igot400regulargirlsand400regularguysonymdick,
        68064, 16.0f, &regular_config, extended_ranges);

    io.Fonts->AddFontFromMemoryTTF(
        (void*)fasolid900,
        426112, 16.0f, &solid_config, extended_ranges);

    ImFontConfig unicode_config;
    unicode_config.MergeMode = true;
    unicode_config.PixelSnapH = true;
    unicode_config.GlyphOffset = ImVec2(0.0f, 1.0f); // align with Poppins baseline

    static ImVector<ImWchar> unicode_ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());    // Latin
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());   // Russian
    builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());   // Japanese
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull()); // Chinese
    builder.AddRanges(io.Fonts->GetGlyphRangesKorean());     // Korean
    builder.AddRanges(io.Fonts->GetGlyphRangesThai());
    builder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
    builder.BuildRanges(&unicode_ranges);

    io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\msyh.ttc",
        15.0f, &unicode_config, unicode_ranges.Data);


    // io.Fonts->Build();
}

void InitImGui() {

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();


    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

    // Configure font with proper alignment
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;
    fontConfig.PixelSnapH = true;

    // Load the embedded Poppins font with config
    io.Fonts->AddFontFromMemoryTTF(PoppinsFont2, sizeof(PoppinsFont2), 15.2f, &fontConfig);

    fontConfig.GlyphOffset = ImVec2(0.0f, -1.0f);

    SetupModernStyle();
    ApplyTheme(themePresets[7]);  // Apply Dark Purple theme on startup
    SetupImGuiFonts();

    ImFontConfig boldConfig;
    boldConfig.FontDataOwnedByAtlas = false;
    boldConfig.OversampleH = 2;
    boldConfig.OversampleV = 2;
    boldConfig.PixelSnapH = true;
    g_BoldFont = io.Fonts->AddFontFromMemoryTTF(PoppinsFont2, sizeof(PoppinsFont2), 15.2f, &boldConfig);

    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(pDevice, pContext);

}


static bool g_unloading = false;
typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
ResizeBuffers_t oResizeBuffers = nullptr;


HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    if (mainRenderTargetView) {
        pContext->OMSetRenderTargets(0, nullptr, nullptr);
        mainRenderTargetView->Release();
        mainRenderTargetView = nullptr;
    }
    HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    ID3D11Texture2D* pBackBuffer = nullptr;
    pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (pBackBuffer) {
        pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
        pBackBuffer->Release();
    }
    return hr;
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (g_unloading) return oPresent(pSwapChain, SyncInterval, Flags);


    if (!init) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice))) {
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;
            ID3D11Texture2D* pBackBuffer;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
            pBackBuffer->Release();
            oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
            InitImGui();
            init = true;
            printf("[INFO] rendering hook intialized. [DX.11]\n");
        }
        else {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }



    // Begin ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (KeySystem::DrawKeyScreen()) {
        ImGui::Render();
        pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        return oPresent(pSwapChain, SyncInterval, Flags);
    }


    DrawWatermark(); // forgot this fucking fat bitch
    DrawGunModsWindow();

    if (g_notify) {
        g_notify = false;
        postGameMsgString("<color=green><b> POTENTIAL CHATCRASH BLOCKED. </b></color>", 3);
    }

    {
        std::lock_guard<std::mutex> lock(g_pendingBlacklistMutex);
        for (auto& [title, msg] : g_pendingBlacklistNotifs)
            PushNotification(title, msg.c_str(), NotificationType::Error, 4.0f);
        g_pendingBlacklistNotifs.clear();
    }

    {
        std::lock_guard<std::mutex> lock(g_notifMutex);
        for (auto& n : g_pendingNotifs)
            PushNotification(n.title, n.body.c_str(), NotificationType::Info, 5.0f);
        g_pendingNotifs.clear();
    }

    if (Config::enablePrivateChat)
        UpdatePrivateChat();


    static PieMenu pieMenu;
    pieMenu.Update();

    if (!pieMenu.GetSelectedItem().empty()) {

    }
    UpdateActiveFlyTargets();
    UpdateNotificationSpam();
    UpdateLoopingInteractions();  
    Builder::Tick();
    DrawModifiersWindow();

    RefreshPlayerCache();

    UpdateForceField();
    UpdateRadiusKill();

    if (!g_massMimicQueue.empty()) {
        DWORD now = GetTickCount64();
        if (now - g_lastMassMimicTime >= 500) {
            g_lastMassMimicTime = now;
            postGameMsg(g_massMimicQueue.front().c_str(), 3);
            g_massMimicQueue.erase(g_massMimicQueue.begin());
        }
    }

    if (Config::enableChatBubbleCustomizer)
        UpdateChatBubbles();

    Aimbot::Update();
    Aimbot::Render();


    if (g_pendingAvatarCopy) {
        g_pendingAvatarCopy = false;
        DoAvatarCubesCopy();
    }

    if (g_pendingLimbCopy && g_pendingLimbTarget && g_pendingLimbIndex >= 0) {
        g_pendingLimbCopy = false;
        DoLimbCopy();
        g_pendingLimbTarget = nullptr;
        g_pendingLimbIndex = -1;
    }

    if (ESP::esp) {
        ESP::Update();
        ESP::Render();
    }

    if (Config::enableFOVChanger) {//snaps fix
        SetCameraFOV(Config::customFOV);
    }

    if (Config::enableSpam) {
        DWORD currentTime = GetTickCount();
        if (currentTime - Config::lastSpamTime >= Config::spamDelay) {
            SendChatMessage(Config::niggerText);
            Config::lastSpamTime = currentTime;
        }
    }

    //UpdateActiveEffectTargets();
    if (Config::enableClickToLook) {
        ClickToLookTeleport();
    }

    if (Config::enableShieldModifier) {
        SetLocalPlayerShield(Config::shieldValue);
    }
    if (Config::enableFOVChanger) {
        SetCameraFOV(Config::customFOV);
    }


    if (enableTimeScale && pSetTimeScale) {
        static DWORD lastTs = 0;
        DWORD now = GetTickCount64();
        if (now - lastTs >= 500) { lastTs = now; pSetTimeScale(timeScaleValue, nullptr); }
    }



    // Add this with your other update calls in hkPresent
    if (Config::enableMotorRotation) {
        static DWORD lastRotationUpdate = 0;
        DWORD currentTime = GetTickCount64();

        if (currentTime - lastRotationUpdate >= 16) { // ~60fps
            lastRotationUpdate = currentTime;

            // Apply spinbot if enabled
            if (Config::enableMotorSpinbot) {
                float deltaTime = 0.016f; // 16ms

                if (Config::motorSpinX) {
                    float speed = Config::motorSpinSpeedX * 90.0f * deltaTime;
                    Config::motorRotationX += Config::reverseMotorSpinX ? -speed : speed;
                    if (Config::motorRotationX > 180.0f) Config::motorRotationX -= 360.0f;
                    if (Config::motorRotationX < -180.0f) Config::motorRotationX += 360.0f;
                }

                if (Config::motorSpinY) {
                    float speed = Config::motorSpinSpeedY * 90.0f * deltaTime;
                    Config::motorRotationY += Config::reverseMotorSpinY ? -speed : speed;
                    if (Config::motorRotationY > 180.0f) Config::motorRotationY -= 360.0f;
                    if (Config::motorRotationY < -180.0f) Config::motorRotationY += 360.0f;
                }

                if (Config::motorSpinZ) {
                    float speed = Config::motorSpinSpeedZ * 90.0f * deltaTime;
                    Config::motorRotationZ += Config::reverseMotorSpinZ ? -speed : speed;
                    if (Config::motorRotationZ > 180.0f) Config::motorRotationZ -= 360.0f;
                    if (Config::motorRotationZ < -180.0f) Config::motorRotationZ += 360.0f;
                }
            }

            // SetAvatarRotation(Config::motorRotationX, Config::motorRotationY, Config::motorRotationZ);
        }
    }

    if (Config::followCustomTarget) {
        UpdateFollowCustomTarget();
    }

    if (Config::enableCustomInteractionLoop) {
        UpdateCustomInteractionLoop();
    }

    if (Config::enableEnhancedBreaking)
        UpdateCubeDeleteQueue();

    if (Config::enableWeapons) {
        UpdateWeaponSystem();
    }

    if (show_menu) {
        DrawModernInterface();
        //DrawAutoBuilderWindow();
        DrawSnowEffect();   // <-- add this line
    }

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetForegroundDrawList(); // or GetBackgroundDrawList()

    if (showFOVCircle) {
        ImVec2 screenCenter(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        draw->AddCircle(screenCenter, pieMenuFOV, IM_COL32(120, 80, 220, 80), 64, 1.5f);
        draw->AddCircle(screenCenter, pieMenuFOV, IM_COL32(120, 80, 220, 30), 64, 4.0f);
    }

    // toggle menu
    if (GetAsyncKeyState(VK_DELETE) & 1) {
        show_menu = !show_menu;
        // printf("[INFO] Interface toggled: %s\n", show_menu ? "visible" : "hidden");
    }

    if (GetAsyncKeyState(VK_END) & 1) {
        if (g_unloading) return oPresent(pSwapChain, SyncInterval, Flags);
        g_unloading = true;
        show_menu = false;

        CreateThread(nullptr, 0, [](LPVOID hMod) -> DWORD {
            Sleep(100);

            Builder::Shutdown();

            if (oWndProc) {
                SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);
                oWndProc = nullptr;
            }

            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();
            Sleep(100);

            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            //ImGui::DestroyContext();

            if (mainRenderTargetView) { mainRenderTargetView->Release(); mainRenderTargetView = nullptr; }
            if (pContext) { pContext->Release(); pContext = nullptr; }
            if (pDevice) { pDevice->Release(); pDevice = nullptr; }

            kiero::shutdown();
            FreeConsole();
            FreeLibraryAndExitThread((HMODULE)hMod, 0);
            return 0;
            }, g_hModule, 0, nullptr);
    }

    // Render
    ImGui::Render();
    pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return oPresent(pSwapChain, SyncInterval, Flags);
}

// paste this somewhere temporary and call it once at startup
void CheckIL2CppExports()
{
    uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    printf("il2cpp_object_new: %p\n", GetProcAddress((HMODULE)base, "il2cpp_object_new"));
    printf("il2cpp_value_box:  %p\n", GetProcAddress((HMODULE)base, "il2cpp_value_box"));
}

bool InitializeHooks()
{
    if (!IL2CPP::Initialize(true)) { printf("[!] IL2CPP init failed\n"); return false; }
    if (MH_Initialize() != MH_OK) { printf("[!] MinHook init failed\n"); return false; }

    const uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    if (!base) { printf("[!] GameAssembly.dll not found\n"); return false; }

    auto Resolve = [&](const char* cls, const char* method, uintptr_t fallback) -> uintptr_t {
        try {
            auto k = IL2CPP::Class::Find(cls);
            if (k) { auto m = IL2CPP::Class::Utils::GetMethodPointer(k, method); if (m) return (uintptr_t)m; }
        }
        catch (...) {}
        return base + fallback;
        };

    // Hook(address, &hkFn, (void**)&oFn, "Class$$Method")
    auto Hook = [&](uintptr_t addr, void* hk, void** orig, const char* name) {
        if (!SetupHook(addr, hk, orig, name)) printf("[!] %s\n", name);
        };

    pMVCubeModelBase_RemoveCube = (MVCubeModelBase_RemoveCube_t)(base + Offsets::MVCubeModelBase::AddCube);  // MVCubeModelBase$$RemoveCube
    pInteractionDataHandlerBase_HandleInteraction = (InteractionDataHandlerBase_HandleInteraction_t)(base + Offsets::InteractionDataHandlerBase::HandleInteraction);  // InteractionDataHandlerBase$$HandleInteraction
    pMVPickupOwner__GetAbsolutProjectileSpeed = (MVPickupOwner__GetAbsolutProjectileSpeed_t)(base + Offsets::MVPickupOwner::GetAbsolutProjectileSpeed);  // MVPickupOwner$$GetAbsolutProjectileSpeed
    pMVCubeModelBase_AddCube = (MVCubeModelBase_AddCube_t)(base + Offsets::MVCubeModelBase::AddCube);  // MVCubeModelBase$$AddCube
    pMVCubeModelBase_HandleDelta = (MVCubeModelBase_HandleDelta_t)(base + Offsets::MVCubeModelBase::HandleDelta);  // MVCubeModelBase$$HandleDelta
    pMVInteractable_ClearModifiers = (MVInteractable_ClearModifiers_t)(base + Offsets::MVInteractable::ClearModifiers);  // MVInteractable$$ClearModifiers
    pMVCubeModelBase_ContainsCube = (MVCubeModelBase_ContainsCube_t)(base + Offsets::MVCubeModelBase::ContainsCube);  // MVCubeModelBase$$ContainsCube
    pPostGameMsgString = (PostGameMsgString_t)(base + Offsets::MVGameControllerBase::PostGameMsg_0x4EDB80);
    pMVCubeModelBase_AddCubeNetworkUpdate = (MVCubeModelBase_AddCubeNetworkUpdate_t)Resolve("MVCubeModelBase", "AddCubeNetworkUpdate", Offsets::MVCubeModelBase::AddCubeNetworkUpdate);
    pProximityDamageAndImpulse_Create = (ProximityDamageAndImpulse_Create_t)Resolve("ProximityDamageAndImpulse", "Create", Offsets::ProximityDamageAndImpulse::Create);
    pMVInteractable_AddModifier = (MVInteractable_AddModifier_t)Resolve("MVInteractable", "AddModifier", Offsets::MVInteractable::AddModifier);
    pAvatarEquipable_Equip = (AvatarEquipable_Equip_t)Resolve("AvatarEquipable", "Equip", Offsets::AvatarEquipable::Equip);
    pSetTransform = (SetTransform_t)Resolve("MVAvatarLocal", "SetTransform", Offsets::MVAvatarLocal::SetTransform);

    pThemeRepository_get_Instance = (ThemeRepository_get_Instance_t)(base + Offsets::ThemeRepository::get_Instance);  // ThemeRepository$$get_Instance
    pThemeRepository_CreateTemporaryThemeVisualization = (ThemeRepository_CreateTemporaryThemeVisualization_t)(base + Offsets::ThemeRepository::CreateTemporaryThemeVisualization);  // ThemeRepository$$CreateTemporaryThemeVisualization
    pThemeRepository_DestroyTemporary = (ThemeRepository_DestroyTemporary_t)(base + Offsets::ThemeRepository::DestroyTemporary);  // ThemeRepository$$DestroyTemporary
    pThemeRepository_get_CurrentThemeVisualization = (ThemeRepository_get_CurrentThemeVisualization_t)(base + Offsets::ThemeRepository::get_CurrentThemeVisualization);  // ThemeRepository$$get_CurrentThemeVisualization
    pThemeRepository_GetThemePrefab = (ThemeRepository_GetThemePrefab_t)(base + Offsets::ThemeRepository::GetThemePrefab);  // ThemeRepository$$GetThemePrefab
    pThemeRepository_get_ThemesEnabled = (ThemeRepository_get_ThemesEnabled_t)(base + Offsets::ThemeRepository::get_ThemesEnabled);  // ThemeRepository$$get_ThemesEnabled
    pThemeRepository_set_ThemesEnabled = (ThemeRepository_set_ThemesEnabled_t)(base + Offsets::ThemeRepository::set_ThemesEnabled);  // ThemeRepository$$set_ThemesEnabled



    Builder::fnCurrentBody = (Builder::FnGetCurrentBody)(base + 5636272); //AvatarEditModeBodyController$$get_CurrentBody
    Builder::fnGetBodyPart = (Builder::FnGetBodyPart)(base + 4666032); //MVBody$$GetBodyPart
    Builder::fnAddCubeDirect = (Builder::FnAddCubeDirect)(base + 4761536); //MVCubeModelBase$$AddCube

    auto fnIcall = (void* (*)(const char*))GetProcAddress((HMODULE)base, "il2cpp_resolve_icall");
    if (fnIcall) pSetTimeScale = (SetTimeScale_t)fnIcall("UnityEngine.Time::set_timeScale");
    if (!pSetTimeScale) printf("[!] timescale\n");

    Hook(base + Offsets::MVAvatarLocal::Update, &hkMVAvatarLocal_Update, (void**)&oMVAvatarLocal_Update, "MVAvatarLocal$$Update");
    Hook(base + Offsets::MVCubeModelBase::RemoveCube, &hkMVCubeModelBase_RemoveCube, (void**)&pMVCubeModelBase_RemoveCube, "MVCubeModelBase$$RemoveCube");
    Hook(base + Offsets::MVPickupOwner::GetAbsolutProjectileSpeed, &hkGetAbsolutProjectileSpeed, (void**)&pMVPickupOwner__GetAbsolutProjectileSpeed, "MVPickupOwner$$GetAbsolutProjectileSpeed");
    Hook(base + Offsets::MVWorldObjectClient::get_Position, &hkGetPosition, (void**)&oGetPosition, "MVWorldObjectClient$$get_Position");
    Hook(base + Offsets::AvatarInteractable::TakeDamage, &hkTakeDamage, (void**)&oTakeDamage, "AvatarInteractable$$TakeDamage");
    Hook(base + Offsets::ChatControllerBase::ReceiveMessage, &hkChatControllerBase__ReceiveMessage, (void**)&oChatControllerBase__ReceiveMessage, "ChatControllerBase$$ReceiveMessage");
    Hook(base + Offsets::ProtectedTransform::set_rotation, &hkProtectedTransform_set_rotation, (void**)&oProtectedTransform_set_rotation, "ProtectedTransform$$set_rotation");
    Hook(base + Offsets::MVRigidBody::ApplyGravity, &hkMVRigidBody_ApplyGravity, (void**)&oMVRigidBody_ApplyGravity, "MVRigidBody$$ApplyGravity");
    Hook(base + Offsets::ChatBubbleManager::ShowChatBubble, &hkShowChatBubble, (void**)&oShowChatBubble, "ChatBubbleManager$$ShowChatBubble");
    Hook(base + Offsets::CubeModelingStateMachine::StartEdit, &hkCubeModelingStateMachine_StartEdit, (void**)&oCubeModelingStateMachine_StartEdit, "CubeModelingStateMachine$$StartEdit");
    Hook(base + Offsets::MVAvatarLocal::SetAnimation, &hkMVAvatarLocal_SetAnimation, (void**)&oMVAvatarLocal_SetAnimation, "MVAvatarLocal$$SetAnimation");
    Hook(base + Offsets::SendMessageControl::SendChatMessage, &hkSendChatMessage, (void**)&oSendChatMessage, "SendMessageControl$$SendChatMessage");
    Hook(base + Offsets::OperationRequests::UpdateWorldObject   , &hkUpdateWorldObject, (void**)&oUpdateWorldObject, "OperationRequests$$UpdateWorldObject");
    Hook(base + Offsets::OperationRequests::PostNotificationOperation, &hkPostNotificationOperation, (void**)&oPostNotificationOperation, "PostNotificationOperation");
    Hook(base + Offsets::PickupItemCustomGun::SetConfiguration, &hkPickupItemCustomGun_SetConfiguration, (void**)&oPickupItemCustomGun_SetConfiguration, "PickupItemCustomGun$$SetConfiguration");
    Hook(base + Offsets::MVNetworkGame::OnUpdateWorldObjectEvent, &hkMVNetworkGame_OnUpdateWorldObjectEvent, (void**)&oMVNetworkGame_OnUpdateWorldObjectEvent, "MVNetworkGame$$OnUpdateWorldObjectEvent");
    Hook(base + Offsets::InteractionDataHandler::HandleInteraction, &hkInteractionDataHandler_HandleInteraction, (void**)&oInteractionDataHandler_HandleInteraction, "InteractionDataHandler$$HandleInteraction");
    Hook(base + Offsets::ChatControllerUGUI::ChatFocusChanged, &hkChatFocusChanged, (void**)&oChatFocusChanged, "ChatControllerUGUI$$ChatFocusChanged");
    Hook(Resolve("AvatarMotor", "FixedUpdateFunction", 0), &hkAvatarMotor_FixedUpdateFunction, (void**)&oAvatarMotor_FixedUpdateFunction, "AvatarMotor$$FixedUpdateFunction");
    Hook((uintptr_t)pAvatarEquipable_Equip, &hkAvatarEquipable_Equip, (void**)&oAvatarEquipable_Equip, "AvatarEquipable$$Equip");
    Hook((uintptr_t)pSetTransform, &hkSetTransform, (void**)&oSetTransform, "MVAvatarLocal$$SetTransform");

    //photon
    Hook(base + 0x18B9C50, &hkPhotonPeer_get_ServerTime, (void**)&oPhotonPeer_get_ServerTime, "PhotonPeer$$get_ServerTimeInMilliSeconds");
    Hook(base + 0x18BA3E0, &hkPhotonPeer_Connect, (void**)&pPhotonPeer_Connect, "PhotonPeer$$Connect");
    Hook(base + 0x950D90, &hkMVNetworkGame_OnStatusChanged, (void**)&oMVNetworkGame_OnStatusChanged, "MVNetworkGame$$OnStatusChanged");

	pMVGameControllerBase_get_GameSessionData = (GetGSD_t)(base + Offsets::MVGameControllerBase::get_GameSessionData);

    Hook(base + Offsets::EventHandling::HandleEvent, &hkhandleEvent, (void**)&oHandleEvent, "handlevent");

    pMVPlayer_set_Level = (MVPlayer_set_Level_t)(base + Offsets::MVPlayer::set_Level);

    Hook(base + Offsets::MVPlayer::get_ActorNr, &hkMVPlayer_get_ActorNr,(void**)&oMVPlayer_get_ActorNr,"MVPlayer$$get_ActorNr");


    //update offsets in here 
    // |      | | |
    // v      v v v
    Builder::Initialize();
    CubeShaper::Initialize();


    IL2Cpp::Dict::GetDictClass();

    printf("[+] hooks loaded\n");
    return true;
}


void MainInitialization() {
    CreateConsole();
    printf("        OPIUM.nigger v4\n");


    if (!InitializeHooks()) {
        printf("[!] hook setup failed\n");
        Sleep(2000);
    }

    if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success) {
        kiero::bind(8, (void**)&oPresent, hkPresent);
        kiero::bind(13, (void**)&oResizeBuffers, hkResizeBuffers);
        printf("[+] rendering hooked\n");
    }
    else {
        printf("[!] kiero failed\n");
        Sleep(3000);
        return;
    }
}

DWORD WINAPI MainThread(LPVOID lpReserved) {
    // Sleep(2000);
    MainInitialization();
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    // do NOT handle DLL_PROCESS_DETACH — our unload thread handles everything
    return TRUE;
}
