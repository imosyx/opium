#pragma once
#include "IL2CppDictionary.h"
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>
#include <mutex>

inline bool g_gunModsEnabled = false;
inline bool g_gunModsWindowOpen = false;

static std::mutex g_gunModsMutex;

// cached il2cpp exports — set once in InitializeHooks or on first use
static void* (*s_il2cpp_object_get_class)(void*) = nullptr;
static const char* (*s_il2cpp_class_get_name)(void*) = nullptr;
static void* (*s_il2cpp_class_get_method)(void*, const char*, int) = nullptr;



static void EnsureExports() {
    if (s_il2cpp_object_get_class) return;
    HMODULE h = GetModuleHandleA("GameAssembly.dll");
    s_il2cpp_object_get_class = (void* (*)(void*))            GetProcAddress(h, "il2cpp_object_get_class");
    s_il2cpp_class_get_name = (const char* (*)(void*))      GetProcAddress(h, "il2cpp_class_get_name");
    s_il2cpp_class_get_method = (void* (*)(void*, const char*, int)) GetProcAddress(h, "il2cpp_class_get_method_from_name");
}

struct WeaponDictEntry {
    std::string key;
    void* valuePtr = nullptr;
    enum class Type { String, Float, Bool, IntArray, FloatArray, Int, Unknown } type = Type::Unknown;
    float floatVal = 0.f;
    bool  boolVal = false;
    std::string strVal;
    std::vector<float> arrayVal;
};

inline std::vector<WeaponDictEntry> g_capturedWeaponDict;
inline int   g_capturedWeaponType = -1;
inline void* g_capturedItemData = nullptr;

static WeaponDictEntry MakeFloat(const char* k, float v) { WeaponDictEntry e; e.key = k; e.type = WeaponDictEntry::Type::Float;      e.floatVal = v;        return e; }
static WeaponDictEntry MakeInt(const char* k, int v) { WeaponDictEntry e; e.key = k; e.type = WeaponDictEntry::Type::Int;        e.floatVal = (float)v; return e; }
static WeaponDictEntry MakeBool(const char* k, bool v) { WeaponDictEntry e; e.key = k; e.type = WeaponDictEntry::Type::Bool;       e.boolVal = v;         return e; }
static WeaponDictEntry MakeStr(const char* k, const char* v) { WeaponDictEntry e; e.key = k; e.type = WeaponDictEntry::Type::String;     e.strVal = v;          return e; }
static WeaponDictEntry MakeColor(const char* k, float r, float g, float b, float a) {
    WeaponDictEntry e; e.key = k; e.type = WeaponDictEntry::Type::FloatArray; e.arrayVal = { r,g,b,a }; return e;
}
static WeaponDictEntry MakeVec3(const char* k, float x, float y, float z) {
    WeaponDictEntry e; e.key = k; e.type = WeaponDictEntry::Type::FloatArray; e.arrayVal = { x,y,z }; return e;
}

static constexpr int WEAPON_TYPE_CUSTOM_GUN = 15;
static constexpr int WEAPON_TYPE_COSTUME = 59;
static constexpr int WEAPON_TYPE_MELEE = 8;

static std::vector<WeaponDictEntry> MakeCustomGunDefaults() {
    return {
        MakeStr("Name",                      "Gun"),
        MakeFloat("MaxAmmo",                   999.f),
        MakeFloat("Damage",                    1.f),
        MakeFloat("ImpulseStrength",           0.f),
        MakeFloat("RecoilStrength",            0.f),
        MakeFloat("AttackAnimationTime",       0.05f),
        MakeFloat("AttackCooldown",            0.1f),
        MakeFloat("Radius",                    0.f),
        MakeInt("FireSoundEffect",           5),
        MakeFloat("FireSoundEffectVolume",     1.f),
        MakeInt("HitSoundEffect",            0),
        MakeFloat("HitSoundEffectVolume",      1.f),
        MakeFloat("MovementSpeedWhileHeld",    8.f),
        MakeInt("FireMode",                  1),
        MakeVec3("MuzzlePosition",            0.499296f, 2.868574f, 13.357887f),
        MakeVec3("MuzzleDirection",           0.f, 0.f, 1.f),
        MakeBool("MuzzleFlareOnFire",         true),
        MakeFloat("MuzzleFlareSize",           1.02f),
        MakeColor("MuzzleFlareColor",          1.f, 0.88f, 0.f, 0.1f),
        MakeBool("ThirdPersonCamera",         true),
        MakeBool("SmokeOnFire",               true),
        MakeFloat("ShotsPerBurst",             3.f),
        MakeFloat("BurstCooldownBetweenShots", 0.2f),
        MakeFloat("ProjectilesPerShot",        3.f),
        MakeBool("ChargeEnabled",             false),
        MakeFloat("ChargeTime",                1.f),
        MakeInt("ChargeSoundEffect",         1),
        MakeFloat("ChargeSoundEffectVolume",   1.f),
        MakeBool("ChargeParticlesEnabled",    true),
        MakeFloat("ChargeParticlesSize",       10.f),
        MakeColor("ChargeParticlesColor",      1.f, 1.f, 1.f, 0.75f),
        MakeBool("ChargeZoomEnabled",         true),
        MakeFloat("ChargeZoomAmmount",         30.f),
        MakeFloat("ChargeZoomTime",            1.f),
        MakeFloat("Range",                     100.f),
        MakeFloat("ProjectileSpeed",           100.f),
        MakeFloat("ProjectileSize",            0.17f),
        MakeColor("ProjectileColor",           1.f, 1.f, 0.65f, 1.f),
        MakeColor("ProjectileTrailColor",      1.f, 0.5f, 0.25f, 0.75f),
        MakeBool("SparksOnHit",               true),
        MakeFloat("SparksSize",                1.f),
        MakeColor("SparksColor",               1.f, 0.3f, 0.f, 1.f),
        MakeInt("CubeModelId",               0),
    };
}

static std::vector<WeaponDictEntry> MakeCostumeDefaults() {
    return {
        MakeStr("Name",              "Costume"),
        MakeFloat("Health",            999.f),
        MakeFloat("MovementSpeed",     8.f),
        MakeBool("PlayerInvisible",   false),
        MakeBool("PlayersCanCollide", false),
        MakeBool("TrailEnabled",      false),
        MakeFloat("TrailLength",       1.f),
        MakeColor("TrailColor",        0.f, 0.75f, 0.75f, 0.4f),
        MakeInt("CubeModelId",       0),
    };
}

static std::vector<WeaponDictEntry> MakeMeleeDefaults() {
    return {
        MakeStr("Name",                "Melee"),
        MakeFloat("Damage",              0.f),
        MakeFloat("ImpulseStrength",     0.f),
        MakeFloat("RecoilStrength",      0.f),
        MakeFloat("AttackAnimationTime", 0.5f),
        MakeFloat("AttackCooldown",      0.2f),
        MakeFloat("Range",               10.f),
        MakeFloat("Radius",              1.f),
        MakeInt("FireSoundEffect",     4),
        MakeInt("HitSoundEffect",      5),
        MakeColor("TrailColor",          0.f, 0.75f, 0.75f, 0.f),
        MakeColor("HitEffectColor",      1.f, 0.4f,  0.f,   0.f),
        MakeInt("CubeModelId",         0),
    };
}

static std::vector<WeaponDictEntry> GetHardcodedDict(int weaponType) {
    if (weaponType == WEAPON_TYPE_CUSTOM_GUN) return MakeCustomGunDefaults();
    if (weaponType == WEAPON_TYPE_COSTUME)    return MakeCostumeDefaults();
    if (weaponType == WEAPON_TYPE_MELEE)      return MakeMeleeDefaults();
    return {};
}

// called only from game thread (inside hook), no lock needed here
inline std::vector<WeaponDictEntry> ParseWeaponDict(void* itemData) {
    std::vector<WeaponDictEntry> result;
    if (!itemData) return result;

    EnsureExports();
    if (!s_il2cpp_object_get_class || !s_il2cpp_class_get_name) return result;

    auto dict = (IL2Cpp::Dict::Dictionary<void*, void*>*)itemData;
    uint32_t maxLen = dict->_entries ? dict->_entries->max_length : 0;

    for (uint32_t i = 0; i < maxLen; i++) {
        auto& e = dict->_entries->m_Items[i];
        if (e.hashCode < 0 || !e.key) continue;

        WeaponDictEntry entry;
        entry.valuePtr = e.value;

        auto keyWide = AntiFilter::IL2CPPStringToWide(e.key);
        int sz = WideCharToMultiByte(CP_UTF8, 0, keyWide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (sz > 0) { entry.key.resize(sz - 1); WideCharToMultiByte(CP_UTF8, 0, keyWide.c_str(), -1, &entry.key[0], sz, nullptr, nullptr); }
        if (entry.key.empty()) continue;

        if (!e.value) { result.push_back(entry); continue; }

        void* valClass = s_il2cpp_object_get_class(e.value);
        const char* tname = valClass ? s_il2cpp_class_get_name(valClass) : "?";

        if (strcmp(tname, "Single") == 0) { entry.type = WeaponDictEntry::Type::Float;  entry.floatVal = *(float*)((uintptr_t)e.value + 0x10); }
        else if (strcmp(tname, "Boolean") == 0) { entry.type = WeaponDictEntry::Type::Bool;   entry.boolVal = *(bool*)((uintptr_t)e.value + 0x10); }
        else if (strcmp(tname, "Int32") == 0) { entry.type = WeaponDictEntry::Type::Int;    entry.floatVal = (float)*(int32_t*)((uintptr_t)e.value + 0x10); }
        else if (strcmp(tname, "Single[]") == 0) {
            entry.type = WeaponDictEntry::Type::FloatArray;
            auto arr = (IL2Cpp::Dict::Array<float>*)e.value;
            for (uint32_t j = 0; j < arr->max_length; j++) entry.arrayVal.push_back(arr->m_Items[j]);
        }
        else if (strcmp(tname, "String") == 0) {
            entry.type = WeaponDictEntry::Type::String;
            auto w = AntiFilter::IL2CPPStringToWide(e.value);
            if (!w.empty()) {
                int s2 = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (s2 > 0) { entry.strVal.assign(s2 - 1, '\0'); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &entry.strVal[0], s2, nullptr, nullptr); }
            }
        }
        else { entry.type = WeaponDictEntry::Type::Unknown; entry.strVal = tname ? tname : "?"; }

        result.push_back(entry);
    }
    return result;
}

static void FlushFloatArray(WeaponDictEntry& entry) {
    if (!entry.valuePtr) return;
    auto arr = (IL2Cpp::Dict::Array<float>*)entry.valuePtr;
    for (size_t j = 0; j < entry.arrayVal.size() && j < arr->max_length; j++)
        arr->m_Items[j] = entry.arrayVal[j];
}

static void FlushEntry(WeaponDictEntry& entry) {
    if (!entry.valuePtr) return;
    switch (entry.type) {
    case WeaponDictEntry::Type::Float:      *(float*)((uintptr_t)entry.valuePtr + 0x10) = entry.floatVal;          break;
    case WeaponDictEntry::Type::Int:        *(int32_t*)((uintptr_t)entry.valuePtr + 0x10) = (int32_t)entry.floatVal; break;
    case WeaponDictEntry::Type::Bool:       *(bool*)((uintptr_t)entry.valuePtr + 0x10) = entry.boolVal;           break;
    case WeaponDictEntry::Type::FloatArray: FlushFloatArray(entry); break;
    default: break;
    }
}

// called from render thread — must have IL2CPP thread attached before calling
static void* BoxEntry(const WeaponDictEntry& e) {
    IL2Cpp::Dict::API::Init();
    switch (e.type) {
    case WeaponDictEntry::Type::Float: { float v = e.floatVal;          return IL2Cpp::Box("System", "Single", &v); }
    case WeaponDictEntry::Type::Int: { int32_t v = (int32_t)e.floatVal; return IL2Cpp::Box("System", "Int32", &v); }
    case WeaponDictEntry::Type::Bool: { bool v = e.boolVal;            return IL2Cpp::Box("System", "Boolean", &v); }
    case WeaponDictEntry::Type::String: return IL2CPP::String::New(e.strVal.c_str());
    case WeaponDictEntry::Type::FloatArray: {
        void* floatClass = IL2Cpp::Dict::API::FindClass("System", "Single");
        if (!floatClass) return nullptr;
        void* arr = IL2Cpp::Dict::API::fnArrayNew(floatClass, e.arrayVal.size());
        if (!arr) return nullptr;
        float* items = (float*)((uintptr_t)arr + 0x20);
        for (size_t i = 0; i < e.arrayVal.size(); i++) items[i] = e.arrayVal[i];
        return arr;
    }
    default: return nullptr;
    }
}

// called from render thread — attach IL2CPP thread before calling
inline void* BuildDictFromEntries(std::vector<WeaponDictEntry>& entries) {
    if (entries.empty()) return nullptr;
    IL2Cpp::Dict::API::Init();

    void* dictClass = IL2Cpp::Dict::GetDictClass();
    if (!dictClass) return nullptr;
    void* raw = IL2Cpp::Dict::API::fnObjectNew(dictClass);
    if (!raw) return nullptr;
    void* exc = nullptr;
    void* ctorInfo = IL2Cpp::Dict::API::fnClassGetMethod(dictClass, ".ctor", 0);
    if (!ctorInfo) return nullptr;
    IL2Cpp::Dict::API::fnRuntimeInvoke(ctorInfo, raw, nullptr, &exc);
    if (exc) return nullptr;

    void* addInfo = IL2Cpp::Dict::API::fnClassGetMethod(dictClass, "Add", 2);
    if (!addInfo) return nullptr;

    for (auto& e : entries) {
        if (e.key.empty()) continue;
        void* keyStr = IL2CPP::String::New(e.key.c_str());
        void* val = BoxEntry(e);
        if (!keyStr || !val) continue;
        void* args[2] = { keyStr, val };
        exc = nullptr;
        IL2Cpp::Dict::API::fnRuntimeInvoke(addInfo, raw, args, &exc);
    }
    return raw;
}

inline void EquipFromMenu(int weaponType) {
    if (!hasValidPlayer || !pAvatarEquipable_Equip) return;

    std::vector<WeaponDictEntry> dictCopy;
    {
        std::lock_guard<std::mutex> lk(g_gunModsMutex);
        dictCopy = g_capturedWeaponDict;
    }

    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    try {
        void* itemData = BuildDictFromEntries(dictCopy);

        auto list = Unity::Object::FindObjectsOfType<Unity::CComponent>("AvatarEquipable");
        if (list && list->m_uMaxLength > 0) {
            for (int i = 0; i < (int)list->m_uMaxLength; i++) {
                auto eq = list->operator[](i);
                if (!eq) continue;
                if (pAvatarEquipable_Equip(eq, weaponType, 0, itemData, 0, nullptr)) break;
            }
        }
    }
    catch (...) {}
    IL2CPP::Thread::Detach(pThread);
}

inline bool __cdecl hkAvatarEquipable_Equip(void* __this, int32_t type, int32_t equipType,
    void* itemData, int32_t variantID, void* method)
{


    if (!init)
        return oAvatarEquipable_Equip(__this, type, equipType, itemData, variantID, method);

    if (itemData && g_gunModsEnabled) {
        auto live = ParseWeaponDict(itemData);

        std::lock_guard<std::mutex> lk(g_gunModsMutex);

        bool reuseExisting = (g_capturedWeaponType == type && !g_capturedWeaponDict.empty());

        if (reuseExisting) {
            for (auto& entry : g_capturedWeaponDict)
                for (auto& l : live)
                    if (l.key == entry.key) { entry.valuePtr = l.valuePtr; FlushEntry(entry); break; }
        }
        else {
            auto hardcoded = GetHardcodedDict(type);
            if (!hardcoded.empty()) {
                for (auto& h : hardcoded)
                    for (auto& l : live)
                        if (l.key == h.key) { h.valuePtr = l.valuePtr; break; }
                g_capturedWeaponDict = std::move(hardcoded);
            }
            else {
                g_capturedWeaponDict = std::move(live);
            }
        }

        g_capturedWeaponType = type;
        g_capturedItemData = itemData;
    }

    return oAvatarEquipable_Equip(__this, type, equipType, itemData, variantID, method);
}

static bool IsColorKey(const std::string& key) {
    std::string k = key;
    std::transform(k.begin(), k.end(), k.begin(), ::tolower);
    return k.find("color") != std::string::npos || k.find("colour") != std::string::npos
        || k.find("tint") != std::string::npos;
}

// DrawEntry operates on a local copy — no live IL2CPP pointers, changes staged to flushQueue
struct FlushOp { size_t index; WeaponDictEntry updated; };

static void DrawEntry(WeaponDictEntry& entry, std::vector<FlushOp>* flushQueue, size_t idx) {
    ImGui::PushID(entry.key.c_str());

    switch (entry.type) {
    case WeaponDictEntry::Type::Float:
    case WeaponDictEntry::Type::Int: {
        std::string kl = entry.key; std::transform(kl.begin(), kl.end(), kl.begin(), ::tolower);
        bool isId = kl.find("id") != std::string::npos || kl.find("soundeffect") != std::string::npos
            || kl.find("firemode") != std::string::npos || entry.type == WeaponDictEntry::Type::Int;

        float avail = ImGui::GetContentRegionAvail().x;
        float labelW = avail * 0.45f;
        float inputW = avail - labelW - ImGui::GetStyle().ItemSpacing.x;

        ImGui::SetNextItemWidth(labelW); ImGui::TextUnformatted(entry.key.c_str());
        ImGui::SameLine(); ImGui::SetNextItemWidth(inputW);

        if (isId) {
            int iv = (int)entry.floatVal;
            if (ImGui::InputInt(("##" + entry.key).c_str(), &iv, 1, 100)) {
                entry.floatVal = (float)iv;
                if (flushQueue) flushQueue->push_back({ idx, entry });
            }
        }
        else {
            if (ImGui::DragFloat(("##" + entry.key).c_str(), &entry.floatVal, 0.05f))
                if (flushQueue) flushQueue->push_back({ idx, entry });
        }
        break;
    }
    case WeaponDictEntry::Type::Bool: {
        bool v = entry.boolVal;
        if (ImGui::Checkbox(("##" + entry.key).c_str(), &v)) {
            entry.boolVal = v;
            if (flushQueue) flushQueue->push_back({ idx, entry });
        }
        ImGui::SameLine(); ImGui::TextUnformatted(entry.key.c_str());
        break;
    }
    case WeaponDictEntry::Type::String: {
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.f), "%s", entry.key.c_str());
        char buf[256] = {};
        strncpy_s(buf, entry.strVal.c_str(), 255);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText(("##str_" + entry.key).c_str(), buf, sizeof(buf))) {
            entry.strVal = buf;
            if (flushQueue) flushQueue->push_back({ idx, entry });
        }
        break;
    }
    case WeaponDictEntry::Type::FloatArray: {
        if (IsColorKey(entry.key) && entry.arrayVal.size() >= 3) {
            while (entry.arrayVal.size() < 4) entry.arrayVal.push_back(1.f);
            ImGui::TextUnformatted(entry.key.c_str()); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
            if (ImGui::ColorEdit4(("##col_" + entry.key).c_str(), entry.arrayVal.data(),
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_Float))
                if (flushQueue) flushQueue->push_back({ idx, entry });
        }
        else {
            ImGui::TextUnformatted(entry.key.c_str()); ImGui::Indent(8.f);
            if (!entry.arrayVal.empty()) {
                float colW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * (float)(entry.arrayVal.size() - 1)) / (float)entry.arrayVal.size();
                for (size_t j = 0; j < entry.arrayVal.size(); j++) {
                    if (j > 0) ImGui::SameLine();
                    ImGui::SetNextItemWidth(colW);
                    if (ImGui::DragFloat(("##arr" + entry.key + std::to_string(j)).c_str(), &entry.arrayVal[j], 0.01f))
                        if (flushQueue) flushQueue->push_back({ idx, entry });
                }
            }
            ImGui::Unindent(8.f);
        }
        break;
    }
    default:
        ImGui::TextDisabled("%s: %s", entry.key.c_str(), entry.strVal.c_str());
        break;
    }

    ImGui::PopID();
}

inline void DrawGunModsWindow() {
    if (!g_gunModsWindowOpen) return;

    ImGui::SetNextWindowSize(ImVec2(490, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 280), ImVec2(700, 520));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, currentBackgroundColor);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(currentSidebarColor.x, currentSidebarColor.y, currentSidebarColor.z, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(currentSidebarColor.x, currentSidebarColor.y, currentSidebarColor.z, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(currentSidebarColor.x + .06f, currentSidebarColor.y + .06f, currentSidebarColor.z + .06f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(currentSidebarColor.x + .10f, currentSidebarColor.y + .10f, currentSidebarColor.z + .10f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(currentSidebarColor.x, currentSidebarColor.y, currentSidebarColor.z, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(currentSidebarColor.x + .06f, currentSidebarColor.y + .06f, currentSidebarColor.z + .06f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 6.f);

    bool windowOpen = ImGui::Begin("weapon mods", &g_gunModsWindowOpen);
    if (!windowOpen) { ImGui::PopStyleColor(7); ImGui::PopStyleVar(2); ImGui::End(); return; }

    // take a frame-local copy so we never hold the mutex during rendering
    std::vector<WeaponDictEntry> localDict;
    int localType;
    {
        std::lock_guard<std::mutex> lk(g_gunModsMutex);
        localDict = g_capturedWeaponDict;
        localType = g_capturedWeaponType;
    }

    static int s_selectedPreset = 0;
    const char* presetNames[] = { "Custom Gun", "Costume", "Melee" };
    if (ImGui::Combo("##preset", &s_selectedPreset, presetNames, IM_ARRAYSIZE(presetNames))) {
        int t = (s_selectedPreset == 0) ? WEAPON_TYPE_CUSTOM_GUN
            : (s_selectedPreset == 1) ? WEAPON_TYPE_COSTUME : WEAPON_TYPE_MELEE;
        std::lock_guard<std::mutex> lk(g_gunModsMutex);
        g_capturedWeaponDict = GetHardcodedDict(t);
        g_capturedWeaponType = t;
        g_capturedItemData = nullptr;
        localDict = g_capturedWeaponDict;
        localType = t;
    }
    ImGui::SameLine();
    bool hasLive = (g_capturedItemData != nullptr);
    ImGui::TextColored(hasLive ? ImVec4(0.4f, 1.f, 0.4f, 1.f) : ImVec4(0.9f, 0.75f, 0.2f, 1.f),
        hasLive ? "  live" : "  preview");

    ImGui::Spacing();

    static char filterBuf[64] = {};
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "filter properties...", filterBuf, sizeof(filterBuf));
    ImGui::Spacing();

    std::string filterLower = filterBuf;
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

    float listH = ImGui::GetContentRegionAvail().y - 68;
    if (listH < 80) listH = 80;

    ImVec4 listBg = ImVec4(currentSidebarColor.x, currentSidebarColor.y, currentSidebarColor.z, 1.f);
    ImVec4 listBorder = ImVec4(currentAccentColor.x * .5f, currentAccentColor.y * .5f, currentAccentColor.z * .5f, .6f);
    
        ImGui::PushStyleColor(ImGuiCol_ChildBg, listBg);
    ImGui::PushStyleColor(ImGuiCol_Border, listBorder);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(listBg.x + .05f, listBg.y + .05f, listBg.z + .05f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(listBg.x + .09f, listBg.y + .09f, listBg.z + .09f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(listBg.x + .05f, listBg.y + .05f, listBg.z + .05f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(listBg.x + .09f, listBg.y + .09f, listBg.z + .09f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    
    ImGui::BeginChild("WeaponDictEditor", ImVec2(0, listH), true);

    std::vector<FlushOp> flushQueue;

    for (size_t i = 0; i < localDict.size(); i++) {
        auto& entry = localDict[i];
        if (entry.key.empty()) continue;
        if (!filterLower.empty()) {
            std::string kl = entry.key; std::transform(kl.begin(), kl.end(), kl.begin(), ::tolower);
            if (kl.find(filterLower) == std::string::npos) continue;
        }
        DrawEntry(entry, &flushQueue, i);
        ImGui::Spacing();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);

    // flush changes back to the shared dict (valuePtr writes happen here, still on render thread,
    // but these are plain memory writes to GC-pinned boxed value fields — safe)
    if (!flushQueue.empty()) {
        std::lock_guard<std::mutex> lk(g_gunModsMutex);
        for (auto& op : flushQueue) {
            if (op.index < g_capturedWeaponDict.size()) {
                auto& dst = g_capturedWeaponDict[op.index];
                // copy value fields, keep valuePtr from live dict
                void* vp = dst.valuePtr;
                dst = op.updated;
                dst.valuePtr = vp;
                FlushEntry(dst);
            }
        }
    }

    ImGui::Spacing();

    float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.f;

    if (ImGui::Button("equip", ImVec2(bw, 24)))
        EquipFromMenu(localType);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.2f, 0.2f, 1.f));
    if (ImGui::Button("sun bullet", ImVec2(bw, 24))) {
        int equipType;
        {
            std::lock_guard<std::mutex> lk(g_gunModsMutex);
            equipType = localType; // already have localType from the snapshot above
            for (auto& e : g_capturedWeaponDict) {
                uint32_t fmax = 0x7F7FFFFF; float vmax = *(float*)&fmax;
                if (e.key == "ProjectileSize") { e.floatVal = vmax; FlushEntry(e); }
                else if (e.key == "MuzzleFlareSize") { e.floatVal = vmax; FlushEntry(e); }
                else if (e.key == "Radius") { e.floatVal = 0.01f; FlushEntry(e); }
                else if (e.key == "ProjectileSpeed") { e.floatVal = 0.01f; FlushEntry(e); }
                else if (e.key == "ProjectilesPerShot") { e.floatVal = vmax; FlushEntry(e); }
                else if (e.key == "ShotsPerBurst") { e.floatVal = vmax; FlushEntry(e); }
                else if (e.key == "AttackCooldown") { e.floatVal = 0.f;  FlushEntry(e); }
                else if (e.key == "FireSoundEffectVolume") { e.floatVal = 0.f; FlushEntry(e); }
                else if (e.key == "BurstCooldownBetweenShots") { e.floatVal = 0.f; FlushEntry(e); }
                else if (e.type == WeaponDictEntry::Type::FloatArray && IsColorKey(e.key)) {
                    while (e.arrayVal.size() < 4) e.arrayVal.push_back(1.f);
                    e.arrayVal = { 255.f, 255.f, 255.f, 0.f }; FlushFloatArray(e);
                }
            }
        } // lock released here
        EquipFromMenu(equipType); // called without holding the lock
    }
    ImGui::PopStyleColor(2);

    if (ImGui::Button("lag bullets", ImVec2(-1, 24))) {
        int equipType;
        {
            std::lock_guard<std::mutex> lk(g_gunModsMutex);
            equipType = localType;
            for (auto& e : g_capturedWeaponDict) {
                if (e.key == "ProjectileSize") { e.floatVal = 9999.f; FlushEntry(e); }
                else if (e.key == "ProjectileSpeed") { e.floatVal = 0.01f;  FlushEntry(e); }
                else if (e.key == "ProjectilesPerShot") { e.floatVal = 70.f;   FlushEntry(e); }
                else if (e.key == "FireSoundEffectVolume") { e.floatVal = 0.f;  FlushEntry(e); }
                else if (e.type == WeaponDictEntry::Type::FloatArray && IsColorKey(e.key)) {
                    while (e.arrayVal.size() < 4) e.arrayVal.push_back(1.f);
                    e.arrayVal = { 255.f, 255.f, 255.f, 0.f }; FlushFloatArray(e);
                }
            }
        } // lock released here
        EquipFromMenu(equipType); // called without holding the lock
    }

    if (ImGui::Button("draw bullets", ImVec2(-1, 24))) {
        int equipType;
        {
            std::lock_guard<std::mutex> lk(g_gunModsMutex);
            equipType = localType;
            for (auto& e : g_capturedWeaponDict) {
                if (e.key == "ProjectileSize") { e.floatVal = 0.170f; FlushEntry(e); }
                else if (e.key == "ProjectileSpeed") { e.floatVal = 0.001f;  FlushEntry(e); }
                else if (e.key == "ProjectilesPerShot") { e.floatVal = 1.f;   FlushEntry(e); }
                else if (e.key == "AttackCooldown") { e.floatVal = 0.f;   FlushEntry(e); }
                else if (e.key == "BurstCooldownBetweenShots") { e.floatVal = 0.f;   FlushEntry(e); }
                else if (e.key == "Damage") { e.floatVal = 0.f;   FlushEntry(e); }
                else if (e.key == "FireSoundEffectVolume") { e.floatVal = 0.f;  FlushEntry(e); }
                else if (e.key == "MuzzlePosition") { e.arrayVal = { -5.f, 7.5f, 13.f }; FlushFloatArray(e); }
                else if (e.key == "ThirdPersonCamera") { e.boolVal = false; FlushEntry(e); }  // add this
                else if (e.type == WeaponDictEntry::Type::FloatArray && IsColorKey(e.key)) {
                    while (e.arrayVal.size() < 4) e.arrayVal.push_back(1.f);
                    e.arrayVal = { 255.f, 255.f, 255.f, 1.f }; FlushFloatArray(e);
                }
            }
        } // lock released here
        EquipFromMenu(equipType); // called without holding the lock
    }

    ImGui::End();
    ImGui::PopStyleColor(7);
    ImGui::PopStyleVar(2);
}

inline void DrawGunModsToggle() {
    static bool s_wasEnabled = false;

    ImGui::Text("weapon modifiers");
    ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);

    if (ToggleButton("##gunmodstoggle", &g_gunModsEnabled) || (g_gunModsEnabled && !s_wasEnabled)) {
        if (g_gunModsEnabled) {
            std::lock_guard<std::mutex> lk(g_gunModsMutex);
            if (g_capturedWeaponDict.empty()) {
                g_capturedWeaponDict = GetHardcodedDict(WEAPON_TYPE_CUSTOM_GUN);
                if (g_capturedWeaponType < 0) g_capturedWeaponType = WEAPON_TYPE_CUSTOM_GUN;
            }
            g_gunModsWindowOpen = true;
        }
    }
    s_wasEnabled = g_gunModsEnabled;

    if (g_gunModsEnabled) {
        if (ImGui::Button(g_gunModsWindowOpen ? "hide##gmw" : "show##gmw"))
            g_gunModsWindowOpen = !g_gunModsWindowOpen;
    }
}