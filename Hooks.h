#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include <vector>
#include <string>
#include "Libs/Il2CPPResolver/IL2CPP_Resolver.hpp"
#include <queue>
#include <cmath>
#include <thread>
#include <algorithm>
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <iostream>
//#include "Hooks.h"
#include <mutex>
#include "IL2CppDictionary.h"
#include "Notification.h"
#include <map>
static std::map<int, bool> states;

static bool isUnloading = false;
static HMODULE g_hModule = nullptr; // Store the DLL module handle

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct IntVector {
    short x;
    short y;
    short z;

    IntVector(short _x = 0, short _y = 0, short _z = 0) : x(_x), y(_y), z(_z) {}
};



struct Quaternion { float x, y, z, w; };

// Forward declaration
void ApplyInteractionToPlayer(void* playerInstance, float damage, Vector3 impulse, bool continuous);


struct InteractionData {
    float damage;
    Vector3 impulse;
    uint8_t interactionType;
    uint8_t playerKilledByType;
    uint8_t padding[6];
};

struct PendingNotif {
    std::string title;
    std::string body;
};
static std::mutex g_notifMutex;
static std::vector<PendingNotif> g_pendingNotifs;

struct CubeDeleteTask {
    IntVector position;
    void* cubeModelInstance;
};
bool reconnecting = false;

static std::queue<CubeDeleteTask> cubeDeleteQueue;
static bool processingDeleteQueue = false;
static DWORD lastCubeDelete = 0;
static const DWORD CUBE_DELETE_INTERVAL = 62; // ~16/sec

enum class CornerState { Idle, WaitingB };
static CornerState g_cornerState = CornerState::Idle;
static IntVector   g_cornerA;
static void* g_cornerCubeModel = nullptr;
static bool        g_cornerPickerMode = false;

static void* g_templateCube = nullptr;
Quaternion MultiplyQuaternions(const Quaternion& a, const Quaternion& b);

// timing stuff for motor spinbot
static DWORD lastMotorSpinTime = 0;

// another quaternion struct for motor - yeah i know it's duplicate but whatever
struct MotorQuaternion { float x, y, z, w; };

// hook function pointers - basically stealing the game's functions
typedef void(*SetTransform_t)(void* __this, Vector3 position, Quaternion rotation, void* method);
SetTransform_t oSetTransform = nullptr;  // original function
SetTransform_t pSetTransform = nullptr;  // pointer to function

// hook for player position
typedef Vector3(__cdecl* GetPosition_t)(void* _this, void* method);
GetPosition_t oGetPosition = nullptr;
Vector3 g_CurrentPosition = { 0, 0, 0 };

// weapon equip yada
typedef bool(__cdecl* AvatarEquipable_Equip_t)(void* __this, int32_t type, int32_t equipType, void* itemData, int32_t variantID, void* method);
AvatarEquipable_Equip_t pAvatarEquipable_Equip = nullptr;

typedef bool(__cdecl* AvatarEquipable_Equip_Hook_t)(void* __this, int32_t type, int32_t equipType, void* itemData, int32_t variantID, void* method);
AvatarEquipable_Equip_Hook_t oAvatarEquipable_Equip = nullptr;

// bullet speed modifier
typedef float(__cdecl* MVPickupOwner__GetAbsolutProjectileSpeed_t)(void* __this, float projectileSpeed, void* method);
MVPickupOwner__GetAbsolutProjectileSpeed_t pMVPickupOwner__GetAbsolutProjectileSpeed = nullptr;

// motor rotation hook - this is where the magic happens
typedef void(__cdecl* AvatarMotor_FixedUpdate_t)(void* __this, void* motorApi, const void* method);
AvatarMotor_FixedUpdate_t oAvatarMotor_FixedUpdate = nullptr;

// motorapi methods - these control the actual movement/rotation
typedef Vector3(__cdecl* IMotorAPI_get_Direction_t)(void* __this, void* method);
typedef void(__cdecl* IMotorAPI_set_Direction_t)(void* __this, Vector3 direction, void* method);
typedef Quaternion(__cdecl* IMotorAPI_get_Rotation_t)(void* __this, void* method);
typedef void(__cdecl* IMotorAPI_set_Rotation_t)(void* __this, Quaternion rotation, void* method);
// motorapi function pointers
IMotorAPI_get_Direction_t pIMotorAPI_get_Direction = nullptr;
IMotorAPI_set_Direction_t pIMotorAPI_set_Direction = nullptr;
IMotorAPI_get_Rotation_t pIMotorAPI_get_Rotation = nullptr;
IMotorAPI_set_Rotation_t pIMotorAPI_set_Rotation = nullptr;

// ProximityDamageAndImpulse static method
typedef InteractionData(__cdecl* ProximityDamageAndImpulse_Create_t)(float damage, Vector3 impulse, uint8_t playerKilledByType, void* method);
ProximityDamageAndImpulse_Create_t pProximityDamageAndImpulse_Create = nullptr;

// Function pointer for InteractionDataHandlerBase$$HandleInteraction
typedef bool(__cdecl* InteractionDataHandlerBase_HandleInteraction_t)(void* __this, InteractionData interaction, bool interactionIsLocal, void* method);
InteractionDataHandlerBase_HandleInteraction_t pInteractionDataHandlerBase_HandleInteraction = nullptr;

typedef void(__cdecl* MVCubeModelBase_RemoveCube_t)(void* __this, IntVector pos, void* method);
MVCubeModelBase_RemoveCube_t pMVCubeModelBase_RemoveCube = nullptr;


// MVAvatarLocal Update hook
typedef void* (__cdecl* MVAvatarLocal_Update_t)(void* __this, void* interactionMap, void* method); //local player instance capture
MVAvatarLocal_Update_t oMVAvatarLocal_Update = nullptr;

typedef void(__cdecl* MVCubeModelBase_AddCube_t)(void* __this, IntVector pos, void* cube, void* method);
MVCubeModelBase_AddCube_t pMVCubeModelBase_AddCube = nullptr;
MVCubeModelBase_AddCube_t oMVCubeModelBase_AddCube = nullptr;  // ADD THIS - original function

typedef void* (__cdecl* CubeBase_ctor_Material_t)(void* __this, uint8_t material, void* method);
CubeBase_ctor_Material_t pCubeBase_ctor_Material = nullptr;

typedef void* (__cdecl* CubeBase_ctor_Full_t)(void* __this, void* byteCorners, void* faceMaterials, void* method);
CubeBase_ctor_Full_t pCubeBase_ctor_Full = nullptr;


typedef void(__cdecl* MVCubeModelBase_AddCubeNetworkUpdate_t)(void* __this, IntVector pos, void* cube, void* method);
MVCubeModelBase_AddCubeNetworkUpdate_t pMVCubeModelBase_AddCubeNetworkUpdate = nullptr;

typedef void(__cdecl* MVCubeModelBase_HandleDelta_t)(void* __this, void* method);
MVCubeModelBase_HandleDelta_t pMVCubeModelBase_HandleDelta = nullptr;

typedef void(__cdecl* TakeDamage_t)(void* __this, float amount, void* damageDealer, int damageType, void* method);
TakeDamage_t oTakeDamage = nullptr;

// Rapid fire hook
typedef void(__cdecl* PickupItemWithDelay_UpdateControllerUpdate_t)(void* __this, void* method);
PickupItemWithDelay_UpdateControllerUpdate_t oPickupItemWithDelay_UpdateControllerUpdate = nullptr;

// Function pointers for fly system
typedef void(__cdecl* ProtectedTransform_set_rotation_t)(void* __this, Quaternion value, void* method);
ProtectedTransform_set_rotation_t oProtectedTransform_set_rotation = nullptr;

typedef Vector3(__cdecl* MVRigidBody_ApplyGravity_t)(void* __this, Vector3 velocity, Vector3 velocityPrevFrame, void* interactableLocal, void* method);
MVRigidBody_ApplyGravity_t oMVRigidBody_ApplyGravity = nullptr;

void ApplyInteractionToAll(float damage, float impulseX, float impulseY, float impulseZ,
    uint8_t killedByType = 8, bool isLocal = false);

typedef void(__cdecl* ShowChatBubble_t)(void* text, void* senderName, int32_t anchorId, void* chatBubbleAnchor, void* method);
ShowChatBubble_t oShowChatBubble = nullptr;


// Function pointer for DoKilledNotification
typedef void(__cdecl* AvatarInteractable_DoKilledNotification_t)(
    void* __this,
    void* damageDealer,
    int playerKilledByType,
    void* method
    );
AvatarInteractable_DoKilledNotification_t pAvatarInteractable_DoKilledNotification = nullptr;

typedef void(__cdecl* AvatarMotor_FixedUpdateFunction_t)(void* __this, void* motorApi, void* method);
AvatarMotor_FixedUpdateFunction_t oAvatarMotor_FixedUpdateFunction = nullptr;

typedef void(__cdecl* CubeModelingStateMachine_StartEdit_t)(void* __this, void* targetCubeModel, void* constraint, void* method);
CubeModelingStateMachine_StartEdit_t oCubeModelingStateMachine_StartEdit = nullptr;


typedef void(__cdecl* MVInteractable_AddModifier_t)(void* __this, int type, int id, void* additionalModifiers, void* method);
MVInteractable_AddModifier_t pMVInteractable_AddModifier = nullptr;

// Clear modifiers
typedef void(__cdecl* MVInteractable_ClearModifiers_t)(void* __this, void* method);
MVInteractable_ClearModifiers_t pMVInteractable_ClearModifiers = nullptr;

typedef void(__cdecl* MVAvatarLocal_SetAnimation_t)(void* __this, void* animationState, void* method);
MVAvatarLocal_SetAnimation_t oMVAvatarLocal_SetAnimation = nullptr;

typedef void(__cdecl* ChatControllerBase__ReceiveMessage_t)(void* __this, int32_t msgType, void* message, void* method);
ChatControllerBase__ReceiveMessage_t oChatControllerBase__ReceiveMessage = nullptr;

typedef bool(__cdecl* MVCubeModelBase_ContainsCube_t)(void* __this, IntVector pos, void* method);
MVCubeModelBase_ContainsCube_t pMVCubeModelBase_ContainsCube = nullptr;

typedef void(__cdecl* SendChatMessage_t)(void* __this, void* chatMsg, void* method);
SendChatMessage_t oSendChatMessage = nullptr;


static void* oCornersToByteArray = nullptr;

typedef void* (*CornersToByteArray_t)(void* corners, void* method);

// Captured instance
static void* g_OperationRequestsInstance = nullptr;
typedef void(__cdecl* CloneTempWorldObject_t)(void* __this, void* root, Vector3 position, Quaternion rotation, void* method);


static Unity::il2cppClass* g_dictObjObjClass = nullptr;

typedef void(__cdecl* PostNotificationOperation_t)(
    void* __this,
    int32_t type,
    void* notificationData,   // Dictionary<object, object>*
    void* method
    );
PostNotificationOperation_t oPostNotificationOperation = nullptr;


namespace NotificationSystem {
    // PlayerKilledByType enum from game decompilation
    enum class NotifType : uint8_t {
        None = 0,
        CenterGun = 1,
        BazookaGun = 2,
        RailGun = 3,
        Suicide = 4,
        Impact = 5,
        Environmental = 6,
        MeleeWeapon = 7,
        Explosive = 8,
        Fire = 9,
        FallOffWorld = 10,
        Mutant = 11,
        Shotgun = 12,
        FlameThrower = 13,
        Crushed = 14,
        Ghost = 15,
        AdvancedGhost = 16,
        SixShooter = 17,
        DoubleSixShooter = 18,
        ThrowingStar = 19,
        MultiThrowingStar = 20,
        SlapGun = 21,
        RayHealGun = 22,
        CustomGun = 23
    };

    inline bool enableNotifSpam = false;
    inline int selectedNotifType = 1; // CenterGun
    inline int spamDelay = 10; // milliseconds between spam notifications
    inline int notifsPerTick = 15; // How many notifications to send at once
    inline ULONGLONG lastNotifSpam = 0;
    inline int currentSpamIndex = 0;
    inline bool cycleNotifTypes = true; // Cycle through all kill types when spamming

    // All kill types
    inline int workingTypes[24] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
}

// global vars that keep track of stuff
bool init = false;
Present oPresent;
HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;
ImFont* modernFont = nullptr;
// player tracking vars
void* localPlayerInstance = nullptr;
Vector3 lastKnownPosition = { 0, 0, 0 };
bool hasValidPlayer = false;
bool show_menu = true;

// ui pages enum
enum class UIPage {
    TELEPORT,
    ESP,
    WEAPONS,
    AIMBOT,
    EXPLOITS,
    VISUALS,
    NETWORK,
    CUBES,
    MISC,
    CHAT,
    THEMES,
    SETTINGS,
    PHOTON,
    OTHER
};
// InteractionPackageType enum (you may need to adjust values based on the game)
enum class InteractionPackageType : uint8_t {
    None = 0,
    ProximityDamage = 1,
    // Add other types as needed
};


struct PlayerInfo {
    void* instance;
    Vector3 position;
    std::string name;
    bool isValid;
    DWORD lastUpdate;
};
void* networkGameInstance = nullptr;

bool showNotifCfg = false;
// toggle states for ui
bool weaponSpawnEnabled = true;
bool teleportEnabled;
bool espEnabled;
void* gameControllerInstance = nullptr;
bool hasGameController = false;

struct TargetedPlayer {
    void* instance;
    std::string name;
    int id;
    bool isFlying;

    bool enableRadiusKill;
    float radiusKillDistance;
    int radiusKillInterval;
    ULONGLONG lastRadiusKill;
};



// Fly system state
static bool enableFly = false;
static float flySpeed = 8.0f;
static Quaternion currentCameraRotation = { 0, 0, 0, 1 };
static bool isChatActive = false;
float cameraDistance = 5.0f;
float walkSpeed = 8.0f;
static bool enableNoclip = false;
static bool walkspeedEnabled = false;
static bool camdistance = false;
BYTE originalbyte = 0x55;

static bool antiban = false;
static int avatarRadius_X = 1;
static int avatarRadius_Y = 1;
static int avatarRadius_Z = 1;
static bool avatarRadius = false;


namespace Config {
    inline int spoofedXP = 0;

    // Private chat system
    inline bool enablePrivateChat = false;
    inline void* privateChatTarget = nullptr;
    inline std::string privateChatTargetName = "";
    inline DWORD lastPrivateChatTP = 0;
    inline std::vector<std::string> privateChatNearbyPlayers; // players who can hear you

    inline bool enableSnowEffect = false;

    struct ChatBubbleConfig {
        // Per-feature toggles
        bool enableRadius = false;
        bool enableBgColor = false;
        bool enableBorderColor = false;
        bool enableNameColor = false;
        bool enableHideName = false;
        bool enableWrapWidth = false;
        bool enableMinSize = false;
        bool enableMuteSound = false;
        bool enableTextColor = false;



        float radius = 20.0f;
        float borderThickness = 0.0f;
        ImVec4 bgColor = ImVec4(1.f, 1.f, 1.f, 1.f);
        ImVec4 borderColor = ImVec4(0.f, 0.f, 0.f, 1.f);
        ImVec4 nameContainerColor = ImVec4(0.8f, 0.95f, 0.85f, 1.f);
        ImVec4 textColor = ImVec4(0.f, 0.f, 0.f, 1.f);
        float wrapWidth = 200.0f;
        float minW = 60.0f;
        float minH = 40.0f;
    } bubbleCfg;

    inline bool enableChatBubbleCustomizer = false;

    inline bool enableLevelSpoof = false;
    inline int spoofedLevel = 1;


    inline bool enableCustomGunPatch = false;


    // in Config namespace
    inline std::unordered_map<std::string, std::string> customGunValues; // key -> string representation
    inline void* capturedItemData = nullptr;
    inline int capturedWeaponType = -1;

    bool cornerPickerMode = false;
    // Add to Config namespace/struct:
    char postGameMsgText[512]{};
    int postGameMsgType = 3; // default AdminMsg
    bool anti_filter = false;

    // Force field - pushes players away from you
    inline bool enableForceField = false;
    inline float forceFieldRadius = 15.0f;
    inline float forceFieldStrength = 700.0f;
    inline DWORD lastForceFieldUpdate = 0;
    inline int forceFieldUpdateInterval = 220; // ms between updates

    // Radius kill - damages players near you
    inline bool enableRadiusKill = false;
    inline float radiusKillDistance = 10.0f;
    inline float radiusKillDamage = 5.0f;
    inline DWORD lastRadiusKill = 0;
    inline int radiusKillInterval = 500; // ms between damage ticks

    bool enableSpam = false;
    bool enableRainbow = false;
    int spamDelay = 10; // milliseconds
    DWORD lastSpamTime = 0;
    float rainbowHue = 0.0f;

    struct LastInteraction {
        void* targetPlayer = nullptr;
        std::string actionName = "";
        float damage = 0.0f;
        Vector3 impulse = { 0, 0, 0 };
        bool isValid = false;
    } lastInteraction;


    inline bool enableShieldModifier = false;
    inline float shieldValue = 0.0f;

    // Click to teleport
    inline bool enableClickToLook = false;
    inline float teleportDistance = 15.0f; // Max raycast distance

    inline bool antiChatCrash = false;
    inline bool enableFOVChanger = false;
    inline float customFOV = 60.0f;
    inline bool enableClipPlaneChanger = false;
    inline float nearClipPlane = 0.3f;
    inline float farClipPlane = 1000.0f;
    inline bool enableOrthographic = false;
    inline float orthographicSize = 5.0f;
    inline bool enableAspectRatio = false;
    inline float customAspect = 1.777f; // 16:9
    inline bool enableClearFlags = false;
    inline int clearFlagsMode = 1; // 1=Skybox, 2=SolidColor, 3=Depth, 4=Nothing
    inline bool enableBackgroundColor = false;
    inline ImVec4 bgColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    inline bool enableCullingMask = false;
    inline int cullingMask = -1; // -1 = everything

    inline bool enableCustomAnimation = false;
    inline int selectedAnimationIndex = 0;
    inline const char* availableAnimations[] = {
        "Idle",
        "Walk",
        "Jump",
        "Swim",
        "Dead",
        "TPose"
    };

    inline bool showModifiersWindow = false;

    struct ModifierInfo {
        int type;
        std::string name;
        std::string description;
    };

    inline std::vector<ModifierInfo> availableModifiers = {
        {1, "Fire", "Set player on fire"},
        {2, "Mutant", "Mutant transformation"},
        {3, "Sticky", "Sticky movement"},
        {4, "Poison", "Poison effect"},
        {5, "WallJump", "Wall jump ability"},
        {6, "InstantDeath", "Instant death"},
        {7, "NoFriction", "No friction"},
        {8, "FlamerBurn", "Flamethrower burn"},
        {9, "Underwater", "Underwater mode"},
        {10, "Frozen", "Freeze player"},
        {11, "NinjaRun", "Super speed"},
        {12, "Shrunken", "Shrink player"},
        {13, "WindFriction", "Wind resistance"},
        {14, "DisableVehiclePickup", "No vehicles"},
        {15, "Enlarged", "Enlarge player"},
        {16, "Shielded", "Shield protection"},
        {17, "SpawnProtection", "Spawn protection"},
        {18, "RayHeal", "Healing effect"},
        {19, "TimeAttackFlagDebriefSlow", "Slowdown"},
        {20, "Lethal", "Lethal touch"},
        {21, "HealingMat", "Healing material"},
        {22, "SlowMat", "Slow material"},
        {23, "SpeedMat", "Speed material"},
        {24, "CrumbleMat", "Crumble material"},
        {25, "RayHealEnemy", "Enemy heal"}
    };

    inline int selectedAvatarState = 1; // Default to Playing

    inline bool enableDestroyHitbox = false;

    inline bool enableNoConstraints = false;

    inline bool enableCameraSyncRotation = false;
    inline float cameraSyncSmoothness = 0.0f;

    struct LoopingInteraction {
        void* targetInstance;
        bool enabled;
        float damage;
        Vector3 impulse;
        int intervalMs;
        ULONGLONG lastApplied;
    };

    inline std::map<int, LoopingInteraction> loopingInteractions;

    // Anti-notifications
    inline bool enableAntiNotifications = false;

    inline bool followCustomTarget = false;

    inline std::vector<void*> multiTargets;
    inline std::vector<std::string> multiTargetNames;
    std::vector<TargetedPlayer> targetedPlayers;
    int nextTargetId = 1;

    inline bool enableIgnoreDamage = false;
    inline BYTE ignoreDamageOriginalByte = 0x55; // Save original byte
    inline BYTE InfModifierAddrOriginalByte = 0x55;
    inline bool InfModifiers = false;


    inline bool enableCustomInteractionLoop = false;
    inline int customInteractionInterval = 100; // milliseconds
    inline DWORD lastCustomInteraction = 0;

    inline bool modifyChatText = false;


    inline char niggerText[512] = "NIGGER";
    inline char screenBlocker[512] = "<size=2> <style=h1><scale=11900><rotate=70>||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||| </size>";

    inline bool enableSlapStrength = false;
    inline float slapStrengthValue = 5000.0f;
    inline bool enableSizeModifier = false;
    inline float avatarSize = 1.0f;
    inline bool enableImmortality = false;
    inline bool enableRapidFire = false;


    // Add to Config namespace
    inline void* customInteractionTarget = nullptr;
    inline std::string customInteractionTargetName = "";
    inline bool targetSpecificPlayerForCustom = true;

    void* hoveredPlayerInstance = nullptr;
    std::string hoveredPlayerName = "";
    bool isHoveringPlayer = false;

    // Camera settings
    bool enableCameraChanger = false;

    // Enhanced cube breaking
    bool enableEnhancedBreaking = false;
    int breakRadius = 1;
    bool progressiveBreaking = false;  // NEW: Toggle for progressive vs instant

    bool enableContinuousInteraction = false;
    float continuousInteractionInterval = 1.0f; // seconds
    DWORD lastContinuousInteraction = 0;

    // Void impulse
    Vector3 voidImpulse = { 0.0f, 0.0f, 0.0f };


    struct ActiveEffectTarget {
        void* instance;
        void* cachedHandler;
        int effectType;
        DWORD lastApplied;
    };

    std::vector<ActiveEffectTarget> activeEffectTargets;
    bool interactionTargetAll = true;
    std::string targetPlayerName = "";
    void* selectedTargetInstance = nullptr;  // Use instance pointer instead of index
    int selectedTargetIndex = -1;  // Keep for display purposes only
    // Replace old proximity damage settings with interaction data settings
    bool enableInteractionData = false;
    float interactionDamage = 10.0f;
    Vector3 interactionImpulse = { 0.0f, 100.0f, 0.0f };
    uint8_t interactionPlayerKilledByType = 8;  // Explosive
    uint8_t interactionType = 13;  // ProximityDamage type
    bool enableContinuousFlyAll = false;
    DWORD lastFlyAllInteraction = 0;
    bool interactionIsLocal = true;


    // Chat bubble anchor exploit
    bool enableChatBubbleExploit = false;
    void* chatBubbleTargetInstance = nullptr;
    std::string chatBubbleTargetName = "";
    // chat settings
    bool enableCustomChat = false;
    char customChatMessage[512] = "";
    // melee impulse (super kick/punch)
    bool enableMeleeImpulse = false;

    // motor rotation - this controls the player's body rotation
    bool enableMotorRotation = false;
    float motorRotationX = 0.0f;
    float motorRotationY = 0.0f;
    float motorRotationZ = 0.0f;
    float motorRotationW = 1.0f;

    // advanced motor options
    bool preserveMovement = true;     // keep movement when rotating
    bool normalizeQuaternion = true;  // auto fix quaternion math

    float motorSpinAngle = 0.0f;

    // motor spinbot - automatic spinning on each axis
    bool enableMotorSpinbot = false;
    bool motorSpinX = false;
    bool motorSpinY = false;
    bool motorSpinZ = false;
    float motorSpinSpeedX = 1.0f;
    float motorSpinSpeedY = 1.0f;
    float motorSpinSpeedZ = 1.0f;
    bool reverseMotorSpinX = false;
    bool reverseMotorSpinY = false;
    bool reverseMotorSpinZ = false;
    float motorSpinAngleX = 0.0f;
    float motorSpinAngleY = 0.0f;
    float motorSpinAngleZ = 0.0f;

    // weapon 
    bool enableWeapons = true;
    int selectedWeaponID = 5;
    DWORD lastWeaponSwitch = 0;
    bool useWeaponHotkeys = false;
    int hotkeyWeaponID = 1;
    bool infiniteAmmo = false;

    // bullet speed hack
    bool enableBulletSpeedMod = false;
    float bulletSpeedMultiplier = 1.0f;

    // weapon list
    struct WeaponInfo {
        int id;
        std::string name;
    };

    std::vector<WeaponInfo> availableWeapons = {
        {1, "Center Gun"}, {2, "Impulse Gun"}, {4, "Bazooka"}, {5, "Hand"},
        {6, "Rail Gun"}, {8, "Melee Weapon"}, {9, "Shotgun"}, {10, "Flamethrower"},
        {11, "Cube Gun"}, {12, "Six Shooter"}, {13, "Double Six Shooter"}, {15, "Custom Gun"},
        {45, "Throwing Star"}, {46, "Multi Throwing Star"}, {59, "Costume"}, {60, "Mouse Gun"},
        {62, "Growth Gun"}, {65, "Slap Gun"}, {70, "Heal Ray"}
    };

    // regular rotation control
    bool enableRotationMod = false;
    float rotationX = 0.0f;
    float rotationY = 0.0f;
    float rotationZ = 0.0f;

    // spinbot 
    bool enableSpinbot = false;
    bool spinX = false;
    bool spinY = false;
    bool spinZ = false;
    float spinSpeedX = 90.0f;
    float spinSpeedY = 90.0f;
    float spinSpeedZ = 90.0f;
    bool reverseSpinX = false;
    bool reverseSpinY = false;
    bool reverseSpinZ = false;

    // ui colors
    ImVec4 accentColor = ImVec4(0.711f, 0.420f, 0.917f, 1.000f);
    ImVec4 backgroundColor = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    ImVec4 sidebarColor = ImVec4(0.12f, 0.12f, 0.15f, 1.0f);
    ImVec4 activeColor = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);

    bool showWatermark = true;

    // teleport system
    bool enableTeleport = false;
    std::vector<PlayerInfo> discoveredPlayers;
    int selectedTeleportTarget = -1;
    float teleportCooldown = 0.0f;
    DWORD lastTeleportTime = 0;

    void* followTargetInstance = nullptr;
    std::string followTargetName = "";

    // click teleport system
    bool enableClickTeleport = true;
    ImVec2 clickPosition = { 0, 0 };
    float clickTeleportDistance = 12.5f;
    bool useRaycast = true; // raycast vs fixed distance

    float orbitRadius = 5.0f;
    float orbitSpeed = 90.0f; // degrees per sec
    float orbitHeight = 0.0f;
    static float orbitAngle = 0.0f;

    // follow system vars
    bool enableFollow = false;
    int followTargetIndex = -1;
    float followUpdateRate = 30.0f;
    DWORD lastFollowUpdate = 0;

    struct ActiveFlyTarget {
        void* instance;
        DWORD lastApplied;
    };
    std::vector<ActiveFlyTarget> activeFlyTargets;

    // ui state
    UIPage currentPage = UIPage::TELEPORT;
    int selectedPreset = -1;

}

// Spinbot timing
static DWORD lastSpinTime = 0;

std::string GenerateShortID(void* instance) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(instance);
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%04X", (unsigned int)(addr & 0xFFFF));
    return std::string(buffer);
}

void QuaternionToEuler(const Quaternion& q, float& pitch, float& yaw, float& roll) {
    // Pitch (X-axis rotation)
    float sinp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    pitch = atan2f(sinp, cosp) * 57.2957795f; // Convert to degrees

    // Yaw (Y-axis rotation)
    float siny = 2.0f * (q.w * q.y - q.z * q.x);
    float cosy = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    yaw = atan2f(siny, cosy) * 57.2957795f;

    // Roll (Z-axis rotation)
    float sinr = 2.0f * (q.w * q.z + q.x * q.y);
    float cosr = 1.0f - 2.0f * (q.z * q.z + q.x * q.x);
    roll = atan2f(sinr, cosr) * 57.2957795f;
}
Quaternion LerpQuaternion(const Quaternion& a, const Quaternion& b, float t) {
    Quaternion result;

    // Dot product to determine if we should take the short path
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

    // If dot < 0, negate b to take the shorter path
    Quaternion bAdjusted = b;
    if (dot < 0.0f) {
        bAdjusted.x = -b.x;
        bAdjusted.y = -b.y;
        bAdjusted.z = -b.z;
        bAdjusted.w = -b.w;
        dot = -dot;
    }

    // Linear interpolation
    result.x = a.x + t * (bAdjusted.x - a.x);
    result.y = a.y + t * (bAdjusted.y - a.y);
    result.z = a.z + t * (bAdjusted.z - a.z);
    result.w = a.w + t * (bAdjusted.w - a.w);

    // Normalize the result
    float mag = sqrtf(result.x * result.x + result.y * result.y +
        result.z * result.z + result.w * result.w);
    if (mag > 0.0f) {
        result.x /= mag;
        result.y /= mag;
        result.z /= mag;
        result.w /= mag;
    }

    return result;
}
Quaternion EulerToQuaternion(float x, float y, float z)
{
    x *= 0.01745329251f;
    y *= 0.01745329251f;
    z *= 0.01745329251f;

    float cx = cosf(x * 0.5f);
    float sx = sinf(x * 0.5f);
    float cy = cosf(y * 0.5f);
    float sy = sinf(y * 0.5f);
    float cz = cosf(z * 0.5f);
    float sz = sinf(z * 0.5f);

    // Z * X * Y (Unity order)
    Quaternion q;
    q.w = cz * cx * cy + sz * sx * sy;
    q.x = cz * sx * cy + sz * cx * sy;
    q.y = cz * cx * sy - sz * sx * cy;
    q.z = sz * cx * cy - cz * sx * sy;

    return q;
}
void QuaternionToDirections(const Quaternion& q, Vector3& forward, Vector3& right, Vector3& up) {
    // Forward vector (camera's looking direction)
    forward.x = 2.0f * (q.x * q.z + q.w * q.y);
    forward.y = 2.0f * (q.y * q.z - q.w * q.x);
    forward.z = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);

    // Right vector
    right.x = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    right.y = 2.0f * (q.x * q.y + q.w * q.z);
    right.z = 2.0f * (q.x * q.z - q.w * q.y);

    // Up vector
    up.x = 2.0f * (q.x * q.y - q.w * q.z);
    up.y = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
    up.z = 2.0f * (q.y * q.z + q.w * q.x);
}

void* __cdecl hkMVAvatarLocal_Update(void* __this, void* interactionMap, void* method) {

    if (__this) {
        localPlayerInstance = __this;
        hasValidPlayer = true;
    }



    //return original func
    if (oMVAvatarLocal_Update) {
        return oMVAvatarLocal_Update(__this, interactionMap, method);
    }
    return nullptr;
}


//weapon-bullet speed
float __cdecl hkGetAbsolutProjectileSpeed(void* __this, float projectileSpeed, void* method) {
    float originalSpeed = pMVPickupOwner__GetAbsolutProjectileSpeed(__this, projectileSpeed, method);

    if (Config::enableBulletSpeedMod) {
        return originalSpeed * Config::bulletSpeedMultiplier;
    }

    return originalSpeed;
}

Vector3 __cdecl hkGetPosition(void* _this, void* method) {
    Vector3 pos = oGetPosition(_this, method);
    g_CurrentPosition = pos;
    if (_this == localPlayerInstance) {
        lastKnownPosition = pos;
        hasValidPlayer = true;
    }
    return pos;
}


void EquipWeapon(int weaponID) {
    if (!Config::enableWeapons || !hasValidPlayer) {
        return;
    }

    if (!pAvatarEquipable_Equip) {
        printf("[ERROR] Weapon equip function not initialized\n");
        return;
    }

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto equipableList = Unity::Object::FindObjectsOfType<Unity::CComponent>("AvatarEquipable");

        if (!equipableList || equipableList->m_uMaxLength == 0) {
            printf("[WARN] No AvatarEquipable objects found\n");
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        for (int i = 0; i < equipableList->m_uMaxLength; i++) {
            auto equipable = equipableList->operator[](i);
            if (!equipable) continue;

            bool result = pAvatarEquipable_Equip(equipable, weaponID, 0, nullptr, 0, nullptr);
            if (result) {
                //printf("[INFO] Successfully equipped weapon ID: %d\n", weaponID);
                break;
            }
        }
    }
    catch (...) {
        printf("[ERROR] Exception in weapon equip\n");
    }

    IL2CPP::Thread::Detach(m_pThisThread);
}

void __cdecl hkSetTransform(void* __this, Vector3 position, Quaternion rotation, void* method) {
    localPlayerInstance = __this;
    hasValidPlayer = true;
    if (Config::enableRotationMod) {
        Quaternion customRot = EulerToQuaternion(Config::rotationX, Config::rotationY, Config::rotationZ);
        rotation = customRot;
    }
    return oSetTransform(__this, position, rotation, method);
}

void UpdateWeaponSystem() {
    if (!Config::enableWeapons) return;

    DWORD currentTime = GetTickCount64();

    // Hotkeys
    if (Config::useWeaponHotkeys) {
        // Number keys 1-9 
        for (int i = 1; i <= 9; i++) {
            if (GetAsyncKeyState('0' + i) & 1) {
                if (i <= Config::availableWeapons.size()) {
                    EquipWeapon(Config::availableWeapons[i - 1].id);
                }
            }
        }
    }
}

void __cdecl hkMVCubeModelBase_RemoveCube(void* __this, IntVector pos, void* method) {
    if (pMVCubeModelBase_RemoveCube) {
        pMVCubeModelBase_RemoveCube(__this, pos, method);
        // Sync the initial player-triggered break
        if (pMVCubeModelBase_HandleDelta)
            pMVCubeModelBase_HandleDelta(__this, nullptr);
    }

    if (!Config::enableEnhancedBreaking || !__this) return;

    if (Config::cornerPickerMode) {
        if (g_cornerState == CornerState::Idle) {
            g_cornerA = pos;
            g_cornerCubeModel = __this;
            g_cornerState = CornerState::WaitingB;
        }
        else {
            IntVector cornerB = pos;
            g_cornerState = CornerState::Idle;

            int minX = min(g_cornerA.x, cornerB.x), maxX = max(g_cornerA.x, cornerB.x);
            int minY = min(g_cornerA.y, cornerB.y), maxY = max(g_cornerA.y, cornerB.y);
            int minZ = min(g_cornerA.z, cornerB.z), maxZ = max(g_cornerA.z, cornerB.z);

            for (int x = minX; x <= maxX; x++)
                for (int y = minY; y <= maxY; y++)
                    for (int z = minZ; z <= maxZ; z++) {
                        if (x == g_cornerA.x && y == g_cornerA.y && z == g_cornerA.z) continue;
                        if (x == cornerB.x && y == cornerB.y && z == cornerB.z)   continue;
                        CubeDeleteTask task;
                        task.position = IntVector((short)x, (short)y, (short)z);
                        task.cubeModelInstance = g_cornerCubeModel;
                        cubeDeleteQueue.push(task);
                    }

            processingDeleteQueue = true;
        }
        return;
    }

    // Radius mode
    if (Config::breakRadius > 0) {
        for (int x = -Config::breakRadius; x <= Config::breakRadius; x++)
            for (int y = -Config::breakRadius; y <= Config::breakRadius; y++)
                for (int z = -Config::breakRadius; z <= Config::breakRadius; z++) {
                    if (x == 0 && y == 0 && z == 0) continue;
                    IntVector targetPos(pos.x + x, pos.y + y, pos.z + z);
                    try {
                        pMVCubeModelBase_RemoveCube(__this, targetPos, nullptr);
                        // Sync each radius delete
                        if (pMVCubeModelBase_HandleDelta)
                            pMVCubeModelBase_HandleDelta(__this, nullptr);
                    }
                    catch (...) {}
                }
    }
}

void UpdateCubeDeleteQueue() {
    if (!processingDeleteQueue || cubeDeleteQueue.empty()) {
        processingDeleteQueue = false;
        return;
    }

    DWORD now = GetTickCount64();
    if (now - lastCubeDelete < CUBE_DELETE_INTERVAL) return;
    lastCubeDelete = now;

    while (!cubeDeleteQueue.empty()) {
        auto task = cubeDeleteQueue.front();
        cubeDeleteQueue.pop();

        if (!pMVCubeModelBase_RemoveCube || !task.cubeModelInstance) continue;

        bool exists = false;
        try {
            if (pMVCubeModelBase_ContainsCube)
                exists = pMVCubeModelBase_ContainsCube(task.cubeModelInstance, task.position, nullptr);
        }
        catch (...) {}

        if (!exists) continue;

        try {
            pMVCubeModelBase_RemoveCube(task.cubeModelInstance, task.position, nullptr);

            // *** Sync deletion to server, same as AutoBuilder does after AddCube ***
            if (pMVCubeModelBase_HandleDelta)
                pMVCubeModelBase_HandleDelta(task.cubeModelInstance, nullptr);
        }
        catch (...) {}

        break; // one real cube per tick
    }

    if (cubeDeleteQueue.empty())
        processingDeleteQueue = false;
}

void SetLocalPlayerSize(float newSize) {
    if (!localPlayerInstance || !hasValidPlayer) {
        printf("[ERROR] No valid local player instance\n");
        return;
    }

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        uintptr_t mvAvatarPtr = (uintptr_t)localPlayerInstance;

        // Size field at offset 0x190 from mvavatar
        // This is a pointer to MVRuntimeDataVariableClampedFloat
        uintptr_t sizeFieldPtr = mvAvatarPtr + 0x190;
        uintptr_t sizeObjectPtr = *(uintptr_t*)sizeFieldPtr;

        if (!sizeObjectPtr) {
            printf("[ERROR] Size object pointer is null\n");
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        //size clamps [min, max] 
        // +0x48: min (float)
        // +0x4C: max (float)
        float* minPtr = (float*)(sizeObjectPtr + 0x48);
        float* maxPtr = (float*)(sizeObjectPtr + 0x4C);

        // FLT_MAX is 3.402823466e+38
        *minPtr = -FLT_MIN;
        *maxPtr = FLT_MAX; 

        Unity::CComponent* sizeObj = (Unity::CComponent*)sizeObjectPtr;
        sizeObj->CallMethodSafe<void*>("set_Value", newSize);

        //printf("[SUCCESS] Set player size to %.2f (NO LIMIT)\n", newSize);
    }
    catch (...) {
        printf("[EXCEPTION] Error in SetLocalPlayerSize\n");
    }

    IL2CPP::Thread::Detach(m_pThisThread);
}

// SEH-only helpers - no C++ objects allowed inside __try blocks
static bool SafeReadInt32(void* ptr, int32_t& out) {
    __try { out = *(int32_t*)ptr; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeReadByte(void* ptr, uint8_t& out) {
    __try { out = *(uint8_t*)ptr; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeReadWChars(void* ptr, int32_t len, wchar_t* buf, int32_t bufLen) {
    __try {
        wchar_t* src = (wchar_t*)ptr;
        int32_t copy = min(len, bufLen - 1);
        for (int32_t i = 0; i < copy; i++) buf[i] = src[i];
        buf[copy] = 0;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static std::map<int, std::string> ReadMsgDict(void* message) {
    std::map<int, std::string> out;
    if (!message) return out;

    auto dict = (IL2Cpp::Dict::Dictionary<void*, void*>*)message;

    int32_t maxLen = 0;
    if (!SafeReadInt32((void*)((uintptr_t)&dict->_entries), maxLen)) return out;
    if (!dict->_entries) return out;

    uint32_t entryCount = dict->_entries->max_length;

    for (uint32_t i = 0; i < entryCount; i++) {
        auto& e = dict->_entries->m_Items[i];
        if (e.hashCode < 0 || !e.key || !e.value) continue;

        // Read key
        int key = -1;
        int32_t keyLen = 0;
        if (SafeReadInt32((void*)((uintptr_t)e.key + 0x10), keyLen) && keyLen >= 1 && keyLen <= 3) {
            wchar_t kbuf[8] = {};
            if (SafeReadWChars((void*)((uintptr_t)e.key + 0x14), keyLen, kbuf, 8)) {
                int parsed = 0; bool valid = true;
                for (int32_t c = 0; c < keyLen; c++) {
                    if (kbuf[c] >= L'0' && kbuf[c] <= L'9')
                        parsed = parsed * 10 + (kbuf[c] - L'0');
                    else { valid = false; break; }
                }
                if (valid) key = parsed;
            }
        }
        if (key < 0) {
            uint8_t bval = 0;
            if (SafeReadByte((void*)((uintptr_t)e.key + 0x10), bval) && bval < 20)
                key = (int)bval;
        }

        // Read value string
        std::string val;
        int32_t vlen = 0;
        if (SafeReadInt32((void*)((uintptr_t)e.value + 0x10), vlen) && vlen > 0 && vlen < 512) {
            wchar_t vbuf[512] = {};
            if (SafeReadWChars((void*)((uintptr_t)e.value + 0x14), vlen, vbuf, 512)) {
                std::wstring w(vbuf, vlen);
                int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (sz > 1 && sz < 1024) {
                    val.resize(sz - 1);
                    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &val[0], sz, nullptr, nullptr);
                }
            }
        }

        if (key >= 0) out[key] = val;
    }

    return out;
}
void postGameMsgString(const char* message, int msgType); // forward decl

// global
std::atomic<bool> g_notify{ false };

void __cdecl hkChatControllerBase__ReceiveMessage(
    void* __this, int32_t msgType, void* message, void* method)
{
    try {
        if (message) {
            auto vals = ReadMsgDict(message);

            for (auto& [k, v] : vals) {
                std::string lower = v;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

                if (lower.find("quad") != std::string::npos) {
                    g_notify = true;   // trigger notif on main thread
                    return;            // BLOCK message
                }
            }

            for (auto& [k, v] : vals) {
                const std::string tag = "<size=-100>opm![";
                size_t pos = v.find(tag);
                if (pos != std::string::npos) {
                    size_t nameStart = pos + tag.size();
                    size_t nameEnd = v.find("]:", nameStart);
                    if (nameEnd != std::string::npos) {
                        std::string senderName = v.substr(nameStart, nameEnd - nameStart);
                        std::string opmContent = v.substr(nameEnd + 2);
                        size_t end = opmContent.find("</size>");
                        if (end != std::string::npos) opmContent = opmContent.substr(0, end);
                        if (!opmContent.empty() && opmContent[0] == ' ')
                            opmContent = opmContent.substr(1);

                        {
                            std::lock_guard<std::mutex> lock(g_notifMutex);
                            g_pendingNotifs.push_back({
                                "<color=#B06060><b>[OPM] <color=#db4064>(" + senderName +
                                ")</color> </b></color><color=#CCCCCC>: " + opmContent + "</color>"
                                });
                        }
                    }
                    break;
                }
            }
        }
    }
    catch (...) {}

    if (oChatControllerBase__ReceiveMessage)
        oChatControllerBase__ReceiveMessage(__this, msgType, message, method);
}

// Camera rotation capture hook
void __cdecl hkProtectedTransform_set_rotation(void* __this, Quaternion value, void* method) {
    currentCameraRotation = value;
    if (oProtectedTransform_set_rotation) {
        oProtectedTransform_set_rotation(__this, value, method);
    }
}

typedef void(__cdecl* ChatFocusChanged_t)(void* __this, bool enterChatMode, void* method);
ChatFocusChanged_t oChatFocusChanged = nullptr;

static void* g_chatControllerInstance = nullptr;

void __cdecl hkChatFocusChanged(void* __this, bool enterChatMode, void* method) {
    isChatActive = enterChatMode;
    g_chatControllerInstance = __this; // cache it here
    if (oChatFocusChanged)
        oChatFocusChanged(__this, enterChatMode, method);
}

Vector3 __cdecl hkMVRigidBody_ApplyGravity(void* __this, Vector3 velocity, Vector3 velocityPrevFrame, void* interactableLocal, void* method) {

    if (enableFly && hasValidPlayer) {
        // read lobby state from cached instance - zero overhead
        bool inLobby = false;
        if (g_chatControllerInstance)
            inLobby = *(bool*)((uintptr_t)g_chatControllerInstance + 0xFC);

        Vector3 flyVelocity = { 0, 0, 0 };

        if (!isChatActive && !inLobby) {
            Vector3 forward, right, up;
            QuaternionToDirections(currentCameraRotation, forward, right, up);

            bool w = GetAsyncKeyState('W') & 0x8000;
            bool a = GetAsyncKeyState('A') & 0x8000;
            bool s = GetAsyncKeyState('S') & 0x8000;
            bool d = GetAsyncKeyState('D') & 0x8000;
            bool space = GetAsyncKeyState(VK_SPACE) & 0x8000;
            bool shift = GetAsyncKeyState(VK_SHIFT) & 0x8000;

            if (w) { flyVelocity.x += forward.x * flySpeed; flyVelocity.y += forward.y * flySpeed; flyVelocity.z += forward.z * flySpeed; }
            if (s) { flyVelocity.x -= forward.x * flySpeed; flyVelocity.y -= forward.y * flySpeed; flyVelocity.z -= forward.z * flySpeed; }
            if (d) { flyVelocity.x += right.x * flySpeed;   flyVelocity.y += right.y * flySpeed;   flyVelocity.z += right.z * flySpeed; }
            if (a) { flyVelocity.x -= right.x * flySpeed;   flyVelocity.y -= right.y * flySpeed;   flyVelocity.z -= right.z * flySpeed; }
            if (space) flyVelocity.y += flySpeed;
            if (shift) flyVelocity.y -= flySpeed;
        }

        return flyVelocity;
    }

    if (oMVRigidBody_ApplyGravity)
        return oMVRigidBody_ApplyGravity(__this, velocity, velocityPrevFrame, interactableLocal, method);

    return velocity;
}

void __cdecl hkShowChatBubble(void* text, void* senderName, int32_t anchorId, void* chatBubbleAnchor, void* method) {
    if (Config::modifyChatText) {
        return;
    }


    if (oShowChatBubble) {
        oShowChatBubble(text, senderName, anchorId, chatBubbleAnchor, method);
    }
}

void RetPatch(uintptr_t offset, bool enable, BYTE& originalByte) {
    uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    if (!base) return;
    uintptr_t addr = base + offset;
    DWORD oldProtect;
    if (VirtualProtect((LPVOID)addr, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        if (enable) {
            originalByte = *(BYTE*)addr;
            *(BYTE*)addr = 0xC3;
        }
        else {
            *(BYTE*)addr = originalByte;
        }
        VirtualProtect((LPVOID)addr, 1, oldProtect, &oldProtect);
    }
}

void* GetAvatarComponentFromGameObject(void* gameObject) {
    if (!gameObject) return nullptr;

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    void* result = nullptr;

    try {
        auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");
        if (avatarList && avatarList->m_uMaxLength > 0) {
            for (int i = 0; i < avatarList->m_uMaxLength; i++) {
                auto avatar = avatarList->operator[](i);
                if (!avatar) continue;

                auto avatarGameObj = avatar->GetGameObject();
                if (avatarGameObj == gameObject) {
                    result = avatar;
                    break;
                }
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
    return result;
}

Vector3 GetPlayerPosition(void* gameObject) {
    Vector3 pos = { 0, 0, 0 };

    if (!gameObject) return pos;

    void* avatarComponent = GetAvatarComponentFromGameObject(gameObject);
    if (!avatarComponent) return pos;

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto avatarComp = (Unity::CComponent*)avatarComponent;
        auto avatarClass = avatarComp->m_Object.m_pClass;

        if (avatarClass) {
            auto posMethod = IL2CPP::Class::Utils::GetMethodPointer(avatarClass, "get_Position");
            if (posMethod) {
                typedef Unity::Vector3(__cdecl* GetPositionFunc)(void*, void*);
                auto getPos = (GetPositionFunc)posMethod;
                Unity::Vector3 unityPos = getPos(avatarComp, nullptr);
                pos.x = unityPos.x;
                pos.y = unityPos.y;
                pos.z = unityPos.z;
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
    return pos;
}

void TeleportToCustomTarget() {
    if (!Config::customInteractionTarget || !localPlayerInstance || !hasValidPlayer) {
        printf("[ERROR] Cannot teleport - missing target or local player\n");
        return;
    }

    // Get target position directly
    Vector3 targetPos = GetPlayerPosition(Config::customInteractionTarget);

    if (targetPos.x == 0 && targetPos.y == 0 && targetPos.z == 0) {
        printf("[WARN] Invalid target position\n");
        return;
    }

    // Get current rotation (preserve it)
    Quaternion currentRot = EulerToQuaternion(0, 0, 0);

    // Call SetTransform directly - localPlayerInstance is already from MVAvatarLocal_Update
    if (pSetTransform) {
        pSetTransform(localPlayerInstance, targetPos, currentRot, nullptr);
        //printf("[INFO] Teleported to %s at [%.1f, %.1f, %.1f]\n",
            //Config::customInteractionTargetName.c_str(),
            //targetPos.x, targetPos.y, targetPos.z);
    }
}

void UpdateFollowCustomTarget() {
    if (!Config::followCustomTarget || !Config::customInteractionTarget ||
        !localPlayerInstance || !hasValidPlayer) {
        return;
    }

    // Get target position directly
    Vector3 targetPos = GetPlayerPosition(Config::customInteractionTarget);

    if (targetPos.x == 0 && targetPos.y == 0 && targetPos.z == 0) {
        return;
    }

    // Get current rotation
    Quaternion currentRot = EulerToQuaternion(0, 0, 0);

    // Set position to match target exactly - localPlayerInstance is from MVAvatarLocal_Update
    if (pSetTransform) {
        pSetTransform(localPlayerInstance, targetPos, currentRot, nullptr);
    }
}
// Send a single notification
void SendCustomNotification(int notificationType = 0) {
    if (!localPlayerInstance || !hasValidPlayer) {
        printf("[WARN] No valid local player for notification\n");
        return;
    }

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        // Find AvatarInteractable component
        auto avatarInteractables = Unity::Object::FindObjectsOfType<Unity::CComponent>("AvatarInteractable");

        if (avatarInteractables && avatarInteractables->m_uMaxLength > 0) {
            auto avatarInteractable = avatarInteractables->operator[](0);

            if (avatarInteractable) {
                auto interactableClass = avatarInteractable->m_Object.m_pClass;

                // Get DoKilledNotification method
                auto notifMethod = IL2CPP::Class::Utils::GetMethodPointer(
                    interactableClass,
                    "DoKilledNotification"
                );

                if (notifMethod) {
                    typedef void(__cdecl* DoKilledNotif_t)(void*, void*, int, void*);
                    auto doNotif = (DoKilledNotif_t)notifMethod;

                    // Call with null damageDealer and our custom type
                    doNotif(avatarInteractable, nullptr, notificationType, nullptr);

                    // DIS WILL LAG U NIGAS
                    //printf("[INFO] Sent notification type %d\n", notificationType);
                }
            }
        }
    }
    catch (...) {
        //printf("[ERROR] Exception in SendCustomNotification\n");
    }

    IL2CPP::Thread::Detach(m_pThisThread);
}

// Update function to handle notification spam
void UpdateNotificationSpam() {
    if (!NotificationSystem::enableNotifSpam) {
        return;
    }

    ULONGLONG now = GetTickCount64();

    if (now - NotificationSystem::lastNotifSpam >= NotificationSystem::spamDelay) {
        NotificationSystem::lastNotifSpam = now;

        // Send multiple notifications at once for maximum chaos
        for (int i = 0; i < NotificationSystem::notifsPerTick; i++) {
            int typeToSend = NotificationSystem::selectedNotifType;

            // Cycle through the 3 working types if enabled
            if (NotificationSystem::cycleNotifTypes) {
                typeToSend = NotificationSystem::workingTypes[NotificationSystem::currentSpamIndex];
                NotificationSystem::currentSpamIndex = (NotificationSystem::currentSpamIndex + 1) % 3;
            }

            SendCustomNotification(typeToSend);
        }
    }
}


void UpdateLoopingInteractions() {
    ULONGLONG now = GetTickCount64();

    for (auto& pair : Config::loopingInteractions) {
        auto& loop = pair.second;

        if (!loop.enabled || !loop.targetInstance) continue;
        if (now - loop.lastApplied < (ULONGLONG)loop.intervalMs) continue;

        loop.lastApplied = now;

        void* avatarComp = GetAvatarComponentFromGameObject(loop.targetInstance);
        if (avatarComp) {
            ApplyInteractionToPlayer(avatarComp, loop.damage, loop.impulse, true);
        }
    }
}

void __cdecl hkAvatarMotor_FixedUpdateFunction(void* __this, void* motorApi, void* method) {
    if (motorApi) {
        uintptr_t motorApiPtr = (uintptr_t)motorApi;
        Quaternion* rotationPtr = (Quaternion*)(motorApiPtr + 0x1C);

        // Camera sync rotation [smth new i added]
        if (Config::enableCameraSyncRotation) {
            if (Config::cameraSyncSmoothness > 0.0f) {
                // Smooth transition to camera rotation
                *rotationPtr = LerpQuaternion(*rotationPtr, currentCameraRotation,
                    0.1f / Config::cameraSyncSmoothness);
            }
            else {
                // Instant snap to camera rotation
                *rotationPtr = currentCameraRotation;
            }
        }
        // Manual rotation (existing code)
        else if (Config::enableMotorRotation) {
            Quaternion customRot = EulerToQuaternion(
                Config::motorRotationX,
                Config::motorRotationY,
                Config::motorRotationZ
            );
            *rotationPtr = customRot;
        }
    }

    // Call original
    if (oAvatarMotor_FixedUpdateFunction) {
        oAvatarMotor_FixedUpdateFunction(__this, motorApi, method);
    }
}


void __cdecl hkCubeModelingStateMachine_StartEdit(void* __this, void* targetCubeModel, void* constraint, void* method) {
    if (Config::enableNoConstraints) {
        constraint = nullptr;
    }
    //dbg [pass null]

    if (oCubeModelingStateMachine_StartEdit) {
        oCubeModelingStateMachine_StartEdit(__this, targetCubeModel, constraint, method);
    }
}

void ApplyModifier(int modifierType) {
    if (!localPlayerInstance || !hasValidPlayer) {
        printf("[ERROR] No valid local player for modifier\n");
        return;
    }

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        // Find MVInteractable component
        auto interactables = Unity::Object::FindObjectsOfType<Unity::CComponent>("MVInteractable");

        if (!interactables || interactables->m_uMaxLength == 0) {
            printf("[WARN] No MVInteractable objects found\n");
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        for (int i = 0; i < interactables->m_uMaxLength; i++) {
            auto interactable = interactables->operator[](i);
            if (!interactable) continue;

            // Call AddModifier with type, id=-1, no additional modifiers
            if (pMVInteractable_AddModifier) {
                pMVInteractable_AddModifier(interactable, modifierType, -1, nullptr, nullptr);
                //printf("[INFO] Applied modifier type %d\n", modifierType);
                break;
            }
        }
    }
    catch (...) {
        printf("[ERROR] Exception in ApplyModifier\n");
    }

    IL2CPP::Thread::Detach(m_pThisThread);
}

void __cdecl hkMVAvatarLocal_SetAnimation(void* __this, void* animationState, void* method) {

    if (Config::enableCustomAnimation && __this == localPlayerInstance) {
        const char* selectedAnim = Config::availableAnimations[Config::selectedAnimationIndex];
        animationState = IL2CPP::String::New(selectedAnim);
    }

    if (oMVAvatarLocal_SetAnimation) {
        oMVAvatarLocal_SetAnimation(__this, animationState, method);
    }
}

void ClickToLookTeleport() {
    if (!Config::enableClickToLook || !localPlayerInstance || !hasValidPlayer) {
        return;
    }

    // Check for left click
    static bool lastClickState = false;
    bool currentClickState = GetAsyncKeyState(VK_RBUTTON) & 0x8000;

    if (currentClickState && !lastClickState) {
        // Just use camera rotation we already have from the camera sync hook
        // Convert camera rotation to forward direction
        Vector3 forward, right, up;
        QuaternionToDirections(currentCameraRotation, forward, right, up);

        // Teleport in the direction you're looking
        Vector3 teleportPos;
        teleportPos.x = lastKnownPosition.x + (forward.x * Config::teleportDistance);
        teleportPos.y = lastKnownPosition.y + (forward.y * Config::teleportDistance);
        teleportPos.z = lastKnownPosition.z + (forward.z * Config::teleportDistance);

        // Teleport using SetTransform
        if (pSetTransform) {
            Quaternion currentRot = EulerToQuaternion(0, 0, 0);
            pSetTransform(localPlayerInstance, teleportPos, currentRot, nullptr);
        }
    }

    lastClickState = currentClickState;
}

void SetLocalPlayerShield(float newShield) {
    if (!localPlayerInstance || !hasValidPlayer) {
        printf("[ERROR] No valid local player instance\n");
        return;
    }

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        uintptr_t mvAvatarPtr = (uintptr_t)localPlayerInstance;

        // Shield field at offset 0x160 from MVAvatar (from your dump)
        // This is a pointer to MVRuntimeDataVariableClampedFloat
        uintptr_t shieldFieldPtr = mvAvatarPtr + 0x160;
        uintptr_t shieldObjectPtr = *(uintptr_t*)shieldFieldPtr;

        if (!shieldObjectPtr) {
            printf("[ERROR] Shield object pointer is null\n");
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        // Shield clamps from MVRuntimeDataVariableClampedFloat
        // +0x48: min (float)
        // +0x4C: max (float)
        float* minPtr = (float*)(shieldObjectPtr + 0x48);
        float* maxPtr = (float*)(shieldObjectPtr + 0x4C);

        // Set limits to 0-100 range
        *minPtr = 0.0f;
        *maxPtr = 100.0f;

        // Set the value using its setter
        Unity::CComponent* shieldObj = (Unity::CComponent*)shieldObjectPtr;
        shieldObj->CallMethodSafe<void*>("set_Value", newShield);

    }
    catch (...) {
        printf("[EXCEPTION] Error in SetLocalPlayerShield\n");
    }

    IL2CPP::Thread::Detach(m_pThisThread);
}

//hsv to rgb and make rainbow effect
std::string HSVtoHex(float h, float s, float v) {
    float c = v * s;
    float x = c * (1 - abs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r, g, b;
    if (h >= 0 && h < 60) {
        r = c; g = x; b = 0;
    }
    else if (h >= 60 && h < 120) {
        r = x; g = c; b = 0;
    }
    else if (h >= 120 && h < 180) {
        r = 0; g = c; b = x;
    }
    else if (h >= 180 && h < 240) {
        r = 0; g = x; b = c;
    }
    else if (h >= 240 && h < 300) {
        r = x; g = 0; b = c;
    }
    else {
        r = c; g = 0; b = x;
    }

    int ri = (int)((r + m) * 255);
    int gi = (int)((g + m) * 255);
    int bi = (int)((b + m) * 255);

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(2) << ri
        << std::setw(2) << gi << std::setw(2) << bi;
    return ss.str();
}

void UpdateForceField() {
    if (!Config::enableForceField || !hasValidPlayer) return;

    DWORD currentTime = GetTickCount64();
    if (currentTime - Config::lastForceFieldUpdate < Config::forceFieldUpdateInterval) {
        return;
    }
    Config::lastForceFieldUpdate = currentTime;

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    if (!m_pThisThread) return;

    try {
        auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");
        if (!avatarList || avatarList->m_uMaxLength == 0) {
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        for (int i = 0; i < avatarList->m_uMaxLength; i++) {
            auto avatar = avatarList->operator[](i);
            if (!avatar) continue;

            auto gameObj = avatar->GetGameObject();
            if (!gameObj) continue;

            // Skip local player via IsLocal property (offset 0x28) instead of instance compare
            bool isLocal = *(bool*)((uintptr_t)avatar + 0x28);
            if (isLocal) continue;

            // Get player position
            Vector3 targetPos = { 0, 0, 0 };
            try {
                auto avatarClass = avatar->m_Object.m_pClass;
                if (!avatarClass) continue;

                auto posMethod = IL2CPP::Class::Utils::GetMethodPointer(avatarClass, "get_Position");
                if (!posMethod) continue;

                typedef Unity::Vector3(__cdecl* GetPositionFunc)(void*, void*);
                auto getPos = (GetPositionFunc)posMethod;
                Unity::Vector3 unityPos = getPos(avatar, nullptr);

                targetPos.x = unityPos.x;
                targetPos.y = unityPos.y;
                targetPos.z = unityPos.z;
            }
            catch (...) {
                continue;
            }

            float dx = targetPos.x - lastKnownPosition.x;
            float dy = targetPos.y - lastKnownPosition.y;
            float dz = targetPos.z - lastKnownPosition.z;

            float horizontalDist = sqrtf(dx * dx + dz * dz);
            float distance3D = sqrtf(dx * dx + dy * dy + dz * dz);

            if (distance3D < Config::forceFieldRadius && horizontalDist > 0.1f) {

                Vector3 horizontalDir = {
                    dx / horizontalDist,
                    0.0f,
                    dz / horizontalDist
                };

                float normalizedDist = distance3D / Config::forceFieldRadius;
                float forceMult = 1.0f - (normalizedDist * normalizedDist);

                float horizontalForce = Config::forceFieldStrength * forceMult;

                float upwardForce = horizontalForce * 0.4f;
                if (upwardForce < 50.0f)  upwardForce = 50.0f;
                if (upwardForce > 150.0f) upwardForce = 150.0f;

                Vector3 pushImpulse = {
                    horizontalDir.x * horizontalForce,
                    upwardForce,
                    horizontalDir.z * horizontalForce
                };

                try {
                    uintptr_t avatarPtr = (uintptr_t)avatar;
                    void* interactionHandler = *(void**)(avatarPtr + 0x48);

                    if (interactionHandler && pProximityDamageAndImpulse_Create &&
                        pInteractionDataHandlerBase_HandleInteraction) {

                        InteractionData pushData = pProximityDamageAndImpulse_Create(
                            0.0f,
                            pushImpulse,
                            8,
                            nullptr
                        );

                        pInteractionDataHandlerBase_HandleInteraction(
                            interactionHandler,
                            pushData,
                            false,
                            nullptr
                        );
                    }
                }
                catch (...) {}
            }
        }
    }
    catch (...) {
        printf("[ERROR] Exception in UpdateForceField\n");
    }

    IL2CPP::Thread::Detach(m_pThisThread);
}

void UpdateRadiusKill() {
    if (!Config::enableRadiusKill || !hasValidPlayer) return;

    DWORD currentTime = GetTickCount64();
    if (currentTime - Config::lastRadiusKill < Config::radiusKillInterval) {
        return;
    }
    Config::lastRadiusKill = currentTime;

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    if (!m_pThisThread) return;

    try {
        auto avatarList = Unity::Object::FindObjectsOfType<Unity::CComponent>("Avatar");
        if (!avatarList || avatarList->m_uMaxLength == 0) {
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        int killCount = 0;
        for (int i = 0; i < avatarList->m_uMaxLength; i++) {
            auto avatar = avatarList->operator[](i);
            if (!avatar) continue;

            auto gameObj = avatar->GetGameObject();
            if (!gameObj) continue;

            // Skip local player via IsLocal property (offset 0x28) instead of instance compare
            bool isLocal = *(bool*)((uintptr_t)avatar + 0x28);
            if (isLocal) continue;

            Vector3 targetPos = { 0, 0, 0 };
            try {
                auto avatarClass = avatar->m_Object.m_pClass;
                if (!avatarClass) continue;

                auto posMethod = IL2CPP::Class::Utils::GetMethodPointer(avatarClass, "get_Position");
                if (!posMethod) continue;

                typedef Unity::Vector3(__cdecl* GetPositionFunc)(void*, void*);
                auto getPos = (GetPositionFunc)posMethod;
                Unity::Vector3 unityPos = getPos(avatar, nullptr);

                targetPos.x = unityPos.x;
                targetPos.y = unityPos.y;
                targetPos.z = unityPos.z;
            }
            catch (...) {
                continue;
            }

            float dx = targetPos.x - lastKnownPosition.x;
            float dy = targetPos.y - lastKnownPosition.y;
            float dz = targetPos.z - lastKnownPosition.z;
            float distance = sqrtf(dx * dx + dy * dy + dz * dz);

            if (distance < Config::radiusKillDistance) {
                try {
                    uintptr_t avatarPtr = (uintptr_t)avatar;
                    void* interactionHandler = *(void**)(avatarPtr + 0x48);

                    if (interactionHandler && pProximityDamageAndImpulse_Create &&
                        pInteractionDataHandlerBase_HandleInteraction) {

                        InteractionData damageData = pProximityDamageAndImpulse_Create(
                            Config::radiusKillDamage,
                            Vector3{ 0, 0, 0 },
                            8,
                            nullptr
                        );

                        pInteractionDataHandlerBase_HandleInteraction(
                            interactionHandler,
                            damageData,
                            false,
                            nullptr
                        );

                        killCount++;
                    }
                }
                catch (...) {}
            }
        }
    }
    catch (...) {
        printf("[ERROR] Exception in UpdateRadiusKill\n");
    }

    IL2CPP::Thread::Detach(m_pThisThread);
}

template<typename Func>
bool ExecuteOnComponent(const char* componentName, Func callback, bool breakOnFirst = true) {
    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    bool success = false;

    try {
        auto components = Unity::Object::FindObjectsOfType<Unity::CComponent>(componentName);

        if (!components || components->m_uMaxLength == 0) {
            printf("[WARN] No %s objects found\n", componentName);
            IL2CPP::Thread::Detach(m_pThisThread);
            return false;
        }

        for (int i = 0; i < components->m_uMaxLength; i++) {
            auto component = components->operator[](i);
            if (!component) continue;

            // Execute the callback - it returns true if successful
            if (callback(component)) {
                success = true;
                if (breakOnFirst) break;
            }
        }
    }
    catch (...) {
        printf("[ERROR] Exception in ExecuteOnComponent<%s>\n", componentName);
    }

    IL2CPP::Thread::Detach(m_pThisThread);
    return success;
}

// Simpler version for void functions that don't need return values
template<typename Func>
void ForEachComponent(const char* componentName, Func callback) {
    if (!localPlayerInstance || !hasValidPlayer) {
        printf("[ERROR] No valid local player\n");
        return;
    }

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto components = Unity::Object::FindObjectsOfType<Unity::CComponent>(componentName);

        if (!components || components->m_uMaxLength == 0) {
            printf("[WARN] No %s objects found\n", componentName);
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        for (int i = 0; i < components->m_uMaxLength; i++) {
            auto component = components->operator[](i);
            if (component) {
                callback(component);
            }
        }
    }
    catch (...) {
        printf("[ERROR] Exception in ForEachComponent<%s>\n", componentName);
    }

    IL2CPP::Thread::Detach(m_pThisThread);
}

inline bool CallComponentMethod(const char* componentName, const char* methodName) {
    return ExecuteOnComponent(componentName, [methodName](void* component) {
        try {
            auto comp = (Unity::CComponent*)component;
            auto compClass = comp->m_Object.m_pClass;

            auto method = IL2CPP::Class::Utils::GetMethodPointer(compClass, methodName);
            if (!method) return false;

            typedef void(__cdecl* MethodFunc_t)(void*, void*);
            ((MethodFunc_t)method)(component, nullptr);
            return true;
        }
        catch (...) {
            return false;
        }
        });
}

// Call a method on a component WITH parameters (void return)
template<typename... Args>
bool CallComponentMethod(const char* componentName, const char* methodName, Args... args) {
    return ExecuteOnComponent(componentName, [methodName, args...](void* component) {
        try {
            auto comp = (Unity::CComponent*)component;
            auto compClass = comp->m_Object.m_pClass;

            auto method = IL2CPP::Class::Utils::GetMethodPointer(compClass, methodName);
            if (!method) return false;

            typedef void(__cdecl* MethodFunc_t)(void*, Args..., void*);
            ((MethodFunc_t)method)(component, args..., nullptr);
            return true;
        }
        catch (...) {
            return false;
        }
        });
}

// Call a method with return value
template<typename ReturnType, typename... Args>
bool CallComponentMethodWithReturn(const char* componentName, const char* methodName, ReturnType& outResult, Args... args) {
    return ExecuteOnComponent(componentName, [methodName, &outResult, args...](void* component) {
        try {
            auto comp = (Unity::CComponent*)component;
            auto compClass = comp->m_Object.m_pClass;

            auto method = IL2CPP::Class::Utils::GetMethodPointer(compClass, methodName);
            if (!method) return false;

            typedef ReturnType(__cdecl* MethodFunc_t)(void*, Args..., void*);
            outResult = ((MethodFunc_t)method)(component, args..., nullptr);
            return true;
        }
        catch (...) {
            return false;
        }
        });
}


// Call method on ALL components WITH parameters
template<typename... Args>
void CallComponentMethodOnAll(const char* componentName, const char* methodName, Args... args) {
    ForEachComponent(componentName, [methodName, args...](void* component) {
        try {
            auto comp = (Unity::CComponent*)component;
            auto compClass = comp->m_Object.m_pClass;

            auto method = IL2CPP::Class::Utils::GetMethodPointer(compClass, methodName);
            if (method) {
                typedef void(__cdecl* MethodFunc_t)(void*, Args..., void*);
                ((MethodFunc_t)method)(component, args..., nullptr);
            }
        }
        catch (...) {}
        });
}

void SetCameraFOV(float fov) {
    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        auto cameras = Unity::Object::FindObjectsOfType<Unity::CComponent>("UnityEngine.Camera");

        if (cameras && cameras->m_uMaxLength > 0) {
            for (int i = 0; i < cameras->m_uMaxLength; i++) {
                auto cam = cameras->operator[](i);
                if (cam) {
                    cam->CallMethodSafe<void*>("set_fieldOfView", fov);
                }
            }
        }
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
}


namespace AntiFilter {

    inline bool enabled = false;
    const std::unordered_map<wchar_t, wchar_t> charSpoof = {
        { L'a', L'а' },
        { L'b', L'b' },
        { L'c', L'с' },
        { L'd', L'd' },
        { L'e', L'е' },
        { L'f', L'f' },
        { L'g', L'g' },
        { L'h', L'h' },
        { L'i', L'і' },
        { L'j', L'ј' },
        { L'k', L'k' },
        { L'l', L'l' },
        { L'm', L'm' },
        { L'n', L'n' },
        { L'o', L'о' },
        { L'p', L'р' },
        { L'q', L'q' },
        { L'r', L'r' },
        { L's', L'ѕ' },
        { L't', L't' },
        { L'u', L'u' },
        { L'v', L'v' },
        { L'w', L'w' },
        { L'x', L'х' },
        { L'y', L'у' },
        { L'z', L'z' }
    };

    const std::vector<std::wstring> filteredWords = {
        L"fuck", L"fucker", L"fucking", L"fucked", L"fucks", L"fuk", L"fuc",
        L"shit", L"shits", L"shitty",
        L"bitch", L"bitches",
        L"nigga",
        L"nigger",
        L"negro",
        L"ass", L"asses", L"asshole",
        L"cheat", L"cheater", L"cheating",
        L"hack", L"hacker", L"hacking",
        L"poof",
        L"dick", L"dicks",
        L"penis",
        L"sex",
        L"pussy",
        L"vagina",
        L"tits", L"tit",
        L"boobs", L"boob",
        L"cunt", L"cunts",
        L"cock", L"cocks",
        L"whore", L"whores",
        L"slut", L"sluts",
        L"faggot", L"fag", L"fags",
        L"porn",
        L"rape", L"raping",
        L"threesome",
        L"engine",
        L"tampermonkey",
        L"bastard",
        L"hooker",
        L"suck",
        L"pedophile",
        L"anus"
    };

    std::wstring SpoofWord(const std::wstring& word) {
        std::wstring result = word;
        for (size_t i = 0; i < result.size(); i++) {
            auto it = charSpoof.find(result[i]);
            if (it != charSpoof.end()) {
                result[i] = it->second;
                //doesnt break
            }
        }
        return result;
    }

    // Case-insensitive find of a word in text, returns start pos or npos
    size_t FindWordCI(const std::wstring& text, const std::wstring& word, size_t startPos = 0) {
        if (word.empty()) return std::wstring::npos;

        for (size_t i = startPos; i + word.size() <= text.size(); i++) {
            bool match = true;
            for (size_t j = 0; j < word.size(); j++) {
                if (towlower(text[i + j]) != towlower(word[j])) {
                    match = false;
                    break;
                }
            }
            if (match) return i;
        }
        return std::wstring::npos;
    }

    // Process message - finds filtered words and spoofs them
    std::wstring ProcessMessage(const std::wstring& input) {
        std::wstring result = input;

        for (const auto& word : filteredWords) {
            size_t pos = 0;
            while ((pos = FindWordCI(result, word, pos)) != std::wstring::npos) {
                // Extract the actual casing from the original text
                std::wstring original = result.substr(pos, word.size());
                std::wstring spoofed = SpoofWord(original);
                result.replace(pos, word.size(), spoofed);
                pos += spoofed.size();
            }
        }

        return result;
    }

    // Convert IL2CPP System.String to std::wstring
    // IL2CPP strings: [object header][length int32][chars...]
    std::wstring IL2CPPStringToWide(void* strObj) {
        if (!strObj) return L"";
        // length at offset 0x10, chars at 0x14 (standard il2cpp layout)
        int32_t length = *(int32_t*)((uintptr_t)strObj + 0x10);
        if (length <= 0 || length > 4096) return L"";
        wchar_t* chars = (wchar_t*)((uintptr_t)strObj + 0x14);
        return std::wstring(chars, length);
    }

    void* WideToIL2CPPString(const std::wstring& str) {
        // Convert wstring to UTF-8 string for IL2CPP::String::New
        int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return nullptr;
        std::string utf8(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &utf8[0], size, nullptr, nullptr);
        return IL2CPP::String::New(utf8);
    }
}

// The actual hook
void __cdecl hkSendChatMessage(void* __this, void* chatMsg, void* method) {
    if (AntiFilter::enabled && chatMsg) {
        try {
            std::wstring original = AntiFilter::IL2CPPStringToWide(chatMsg);
            if (!original.empty()) {
                std::wstring processed = AntiFilter::ProcessMessage(original);
                if (processed != original) {
                    void* newMsg = AntiFilter::WideToIL2CPPString(processed);
                    if (newMsg) chatMsg = newMsg;
                }
            }
        }
        catch (...) {}
    }

    if (oSendChatMessage) {
        oSendChatMessage(__this, chatMsg, method);
    }
}

void SetTargetSize(void* avatarComponent, float newSize) {
    if (!avatarComponent) return;

    void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    try {
        // Avatar.mvAvatar is at offset 0x20
        uintptr_t avatarPtr = (uintptr_t)avatarComponent;
        uintptr_t mvAvatarPtr = *(uintptr_t*)(avatarPtr + 0x20);

        if (!mvAvatarPtr) {
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        // MVAvatar.Size (MVRuntimeDataVariableClampedFloat) is at offset 0x190
        uintptr_t sizeFieldPtr = mvAvatarPtr + 0x190;
        uintptr_t sizeObjectPtr = *(uintptr_t*)sizeFieldPtr;

        if (!sizeObjectPtr) {
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        float* minPtr = (float*)(sizeObjectPtr + 0x48);
        float* maxPtr = (float*)(sizeObjectPtr + 0x4C);
        *minPtr = -2.0f;
        *maxPtr = FLT_MAX;

        Unity::CComponent* sizeObj = (Unity::CComponent*)sizeObjectPtr;
        sizeObj->CallMethodSafe<void*>("set_Value", newSize);
    }
    catch (...) {}

    IL2CPP::Thread::Detach(m_pThisThread);
}

// replace old fake lag statics with:
enum class NetPosMode { Off, Ghost, Freeze, Strafe };
static NetPosMode g_netPosMode = NetPosMode::Off;
static Vector3 g_ghostPos = { 99999.f, 0.f, 99999.f };
static Vector3 g_freezePos = { 0,0,0 };
static bool    g_freezeCaptured = false;
static float   g_strafeOffset = 80.f;
static float   g_strafeHz = 8.f;
static bool    g_strafeSide = false;
static DWORD   g_strafeTimer = 0;
static Vector3 g_lastNetPosition = { 0,0,0 };
static void* g_lastNetRotation = nullptr;
static uint8_t g_lastNetPackageType = 0;
static int32_t g_lastNetId = 0;
static DWORD g_lastNetRepeatTime = 0;

// keep these alongside the NetPosMode vars
static bool enableFakeLag = false;
static int  fakeLagInterval = 10;
static int  g_netRepeatCounter = 0;


typedef void(__cdecl* UpdateWorldObject_t)(void* __this, int32_t id, Vector3 position, void* rotation, uint8_t packageType, void* method);
UpdateWorldObject_t oUpdateWorldObject = nullptr;
void __cdecl hkUpdateWorldObject(void* __this, int32_t id, Vector3 position, void* rotation, uint8_t packageType, void* method) {
    if (__this)
        g_OperationRequestsInstance = __this;

    Vector3 sendPos = position;

    switch (g_netPosMode) {
    case NetPosMode::Ghost:
        sendPos = g_ghostPos;
        break;

    case NetPosMode::Freeze:
        if (!g_freezeCaptured) {
            g_freezePos = position;
            g_freezeCaptured = true;
        }
        sendPos = g_freezePos;
        break;

    case NetPosMode::Strafe: {
        DWORD now = GetTickCount64();
        DWORD interval = (DWORD)(1000.f / max(g_strafeHz, 0.1f));

        if (now - g_strafeTimer >= interval) {
            g_strafeSide = !g_strafeSide;
            g_strafeTimer = now;
        }

        sendPos.x += g_strafeSide ? g_strafeOffset : -g_strafeOffset;
        break;
    }

    default:
        break;
    }

    g_lastNetPosition = position;
    g_lastNetRotation = rotation;
    g_lastNetPackageType = packageType;
    g_lastNetId = id;

    if (enableFakeLag) {
        DWORD now = GetTickCount64();
        if (now - g_lastNetRepeatTime < (DWORD)fakeLagInterval)
            return;

        g_lastNetRepeatTime = now;
    }

    if (oUpdateWorldObject)
        oUpdateWorldObject(__this, id, sendPos, rotation, packageType, method);
}

typedef void(__cdecl* PostGameMsg_t)(void* __this, int32_t gameMsgType, void* gameMsgData, void* method);
PostGameMsg_t oPostGameMsg = nullptr;

static void* s_dictAddMethod = nullptr;


void __cdecl hkPostNotificationOperation(void* __this, int32_t type, void* notificationData, void* method)
{
    if (notificationData) {
        HMODULE hModule = GetModuleHandleA("GameAssembly.dll");
        auto il2cpp_object_get_class = (void* (*)(void*))           GetProcAddress(hModule, "il2cpp_object_get_class");
        auto il2cpp_class_get_method_from_name = (void* (*)(void*, const char*, int)) GetProcAddress(hModule, "il2cpp_class_get_method_from_name");

        if (!g_dictObjObjClass && il2cpp_object_get_class)
            g_dictObjObjClass = (Unity::il2cppClass*)il2cpp_object_get_class(notificationData);

        if (!s_dictAddMethod && g_dictObjObjClass && il2cpp_class_get_method_from_name) {
            void* methodInfo = il2cpp_class_get_method_from_name(g_dictObjObjClass, "Add", 2);
            if (methodInfo) {
                s_dictAddMethod = *(void**)methodInfo;
                //printf("[PostNotif] captured Add: %p\n", s_dictAddMethod);
            }
        }

    }
    if (oPostNotificationOperation)
        oPostNotificationOperation(__this, type, notificationData, method);
}

void postGameMsg(const char* message, int msgType)
{
    if (!g_OperationRequestsInstance) return;

    void* dictClass = IL2Cpp::Dict::GetDictClass();
    if (!dictClass) { printf("[postGameMsg] no dict class\n"); return; }

    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    uint8_t b = 5;
    void* boxedKey = IL2Cpp::Box("System", "Byte", &b);
    void* strVal = IL2CPP::String::New(message);
    auto dict = IL2Cpp::Dict::Dictionary<void*, void*>::CreateWithEntry(boxedKey, strVal);
    if (!dict) { printf("[postGameMsg] dict create failed\n"); IL2CPP::Thread::Detach(pThread); return; }
    if (!oPostGameMsg)
        oPostGameMsg = (PostGameMsg_t)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + 9822384);
    oPostGameMsg(g_OperationRequestsInstance, msgType, dict, nullptr);
    IL2CPP::Thread::Detach(pThread);
}

void SendChatMessage(const char* message) { //spam
    char temp[512]{};
    strcpy_s(temp, "</color>  </color>");

    if (Config::enableRainbow) {
        std::string hexColor = HSVtoHex(Config::rainbowHue, 1.0f, 1.0f);
        std::string coloredMsg = "<color=#" + hexColor + ">" + std::string(message) + "</color>";
        strcat_s(temp, coloredMsg.c_str());

        // Update rainbow hue for next message
        Config::rainbowHue += 10.0f;
        if (Config::rainbowHue >= 360.0f) Config::rainbowHue = 0.0f;
    }
    else {
        strcat_s(temp, message);
    }

    wchar_t wmsg[512]{};
    MultiByteToWideChar(CP_UTF8, 0, temp, -1, wmsg, 512);
    CallComponentMethod("SendMessageControl", "SendChatMessage", wmsg);
}


typedef void(__cdecl* PickupItemCustomGun_SetConfiguration_t)(void* __this, void* itemData, void* method);
PickupItemCustomGun_SetConfiguration_t oPickupItemCustomGun_SetConfiguration = nullptr;
//anti sunbullet
void __cdecl hkPickupItemCustomGun_SetConfiguration(void* __this, void* itemData, void* method) {
    if (Config::enableCustomGunPatch && itemData) {
        auto dict = (IL2Cpp::Dict::Dictionary<void*, void*>*)itemData;
        if (dict->_entries) {
            for (uint32_t i = 0; i < dict->_entries->max_length; i++) {
                auto& e = dict->_entries->m_Items[i];
                if (e.hashCode < 0 || !e.key) continue;

                int32_t keyLen = *(int32_t*)((uintptr_t)e.key + 0x10);
                if (keyLen <= 0 || keyLen > 64) continue;
                wchar_t* kchars = (wchar_t*)((uintptr_t)e.key + 0x14);
                std::wstring key(kchars, keyLen);

                auto PatchFloat = [&](float val) {
                    if (!e.value) return;
                    *(float*)((uintptr_t)e.value + 0x10) = val;
                    };

                if (key == L"ProjectileSize")   PatchFloat(0.3f);
                else if (key == L"MuzzleFlareSize")  PatchFloat(0.3f);
                else if (key == L"AttackCooldown")   PatchFloat(0.100f);
                else if (key == L"ProjectilesPerShot") PatchFloat(1.0f);
                else if (key == L"ShotsPerBurst") PatchFloat(1.0f);
            }
        }
    }

    if (oPickupItemCustomGun_SetConfiguration)
        oPickupItemCustomGun_SetConfiguration(__this, itemData, method);
}

bool mutanttype = false;

void __cdecl hkTakeDamage(void* __this, float amount, void* damageDealer, int damageType, void* method) {
    if (Config::enableImmortality) {
        amount = 0;

    }
    oTakeDamage(__this, amount, damageDealer, damageType, method);
}


// class scanner
static std::vector<std::pair<std::string, int>> g_scannedClasses;
static bool g_scanned = false;
static char g_classFilterBuf[128] = {};

inline void ScanAllClassesWithInstances() {
    g_scannedClasses.clear();
    g_scanned = false;

    HMODULE hModule = GetModuleHandleA("GameAssembly.dll");
    if (!hModule) return;

    auto fn_get_assemblies = (void** (*)(void*, size_t*))  GetProcAddress(hModule, "il2cpp_domain_get_assemblies");
    auto fn_get_image = (void* (*)(void*))           GetProcAddress(hModule, "il2cpp_assembly_get_image");
    auto fn_class_count = (size_t(*)(void*))           GetProcAddress(hModule, "il2cpp_image_get_class_count");
    auto fn_get_class = (void* (*)(void*, size_t))   GetProcAddress(hModule, "il2cpp_image_get_class");
    auto fn_class_name = (const char* (*)(void*))      GetProcAddress(hModule, "il2cpp_class_get_name");

    if (!fn_get_assemblies || !fn_get_image || !fn_class_count || !fn_get_class || !fn_class_name)
        return;

    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    size_t asmCount = 0;
    void** assemblies = fn_get_assemblies(IL2CPP::Domain::Get(), &asmCount);

    if (assemblies) {
        for (size_t a = 0; a < asmCount; a++) {
            void* image = fn_get_image(assemblies[a]);
            if (!image) continue;

            size_t classCount = fn_class_count(image);
            for (size_t c = 0; c < classCount; c++) {
                void* klass = fn_get_class(image, c);
                if (!klass) continue;
                const char* name = fn_class_name(klass);
                if (!name || !name[0]) continue;

                try {
                    auto objs = Unity::Object::FindObjectsOfType<Unity::CComponent>(name);
                    if (objs && objs->m_uMaxLength > 0)
                        g_scannedClasses.push_back({ name, (int)objs->m_uMaxLength });
                }
                catch (...) {}
            }
        }
    }

    std::sort(g_scannedClasses.begin(), g_scannedClasses.end(),
        [](auto& a, auto& b) { return a.second > b.second; });

    g_scanned = true;
    IL2CPP::Thread::Detach(pThread);
}


// --- MVNetworkGame capture ---
static void* g_MVNetworkGameInstance = nullptr;

typedef void(__cdecl* MVNetworkGame_OnUpdateWorldObjectEvent_t)(void* __this, void* photonEvent, void* method);
MVNetworkGame_OnUpdateWorldObjectEvent_t oMVNetworkGame_OnUpdateWorldObjectEvent = nullptr;

void __cdecl hkMVNetworkGame_OnUpdateWorldObjectEvent(void* __this, void* photonEvent, void* method) {
    if (__this) g_MVNetworkGameInstance = __this;



    if (oMVNetworkGame_OnUpdateWorldObjectEvent)
        oMVNetworkGame_OnUpdateWorldObjectEvent(__this, photonEvent, method);
}

typedef void(__cdecl* SetTimeScale_t)(float, void*);
static SetTimeScale_t pSetTimeScale = nullptr;

// --- TIMESCALE ---
static bool enableTimeScale = false;
static float timeScaleValue = 1.0f;

static void ResetChatBubble(void* bubble) {
    if (!bubble) return;
    uintptr_t b = (uintptr_t)bubble;

    // Reset background
    void* bg = *(void**)(b + 0x38);
    if (bg) {
        uintptr_t bgb = (uintptr_t)bg;
        *(float*)(bgb + 0x120) = 50.0f;   // default radius
        *(float*)(bgb + 0x124) = 0.0f;    // default border
        *(float*)(bgb + 0x128) = 0.f; *(float*)(bgb + 0x12C) = 0.f;
        *(float*)(bgb + 0x130) = 0.f; *(float*)(bgb + 0x134) = 1.f;
        auto bgComp = (Unity::CComponent*)bg;
        Unity::Vector4 white = { 1.f, 1.f, 1.f, 1.f };
        bgComp->CallMethodSafe<void*>("set_color", white);
    }

    // Reset name container
    void* nc = *(void**)(b + 0x20);
    if (nc) {
        auto ncComp = (Unity::CComponent*)nc;
        ncComp->CallMethodSafe<void*>("set_enabled", true);
        uintptr_t nb = (uintptr_t)nc;
        *(float*)(nb + 0x120) = 20.0f;
        *(float*)(nb + 0x124) = 0.0f;
        Unity::Vector4 defaultGreen = { 0.8f, 0.95f, 0.85f, 1.f };
        ncComp->CallMethodSafe<void*>("set_color", defaultGreen);
    }

    // (TextMeshProUGUI* at 0x28) - restore visibility
    void* nameTmp = *(void**)(b + 0x28);
    if (nameTmp) {
        auto nameComp = (Unity::CComponent*)nameTmp;
        nameComp->CallMethodSafe<void*>("set_enabled", true);
        // Reset text color to black
        Unity::Vector4 black = { 0.f, 0.f, 0.f, 1.f };
        nameComp->CallMethodSafe<void*>("set_color", black);
    }

    // Reset message text (TextMeshProUGUI* at 0x30) color
    void* msgTmp = *(void**)(b + 0x30);
    if (msgTmp) {
        auto msgComp = (Unity::CComponent*)msgTmp;
        Unity::Vector4 black = { 0.f, 0.f, 0.f, 1.f };
        msgComp->CallMethodSafe<void*>("set_color", black);
    }

    // Reset wrap/size
    *(float*)(b + 0x5C) = 200.0f;
    *(float*)(b + 0x54) = 60.0f;
    *(float*)(b + 0x58) = 40.0f;

   
    void* audio = *(void**)(b + 0x80);
    if (audio) {
        auto audioComp = (Unity::CComponent*)audio;
        audioComp->CallMethodSafe<void*>("set_volume", 1.0f);
    }
}

static void ApplyChatBubbleConfig(void* bubble, const Config::ChatBubbleConfig& cfg) {
    if (!bubble) return;
    uintptr_t b = (uintptr_t)bubble;

    // --- Background ---
    void* bg = *(void**)(b + 0x38);
    if (bg) {
        uintptr_t bgb = (uintptr_t)bg;
        auto bgComp = (Unity::CComponent*)bg;

        if (cfg.enableRadius) {
            *(float*)(bgb + 0x120) = cfg.radius;
            *(float*)(bgb + 0x124) = cfg.borderThickness;
        }
        if (cfg.enableBgColor) {
            Unity::Vector4 col = { cfg.bgColor.x, cfg.bgColor.y, cfg.bgColor.z, cfg.bgColor.w };
            bgComp->CallMethodSafe<void*>("set_color", col);
        }
        if (cfg.enableBorderColor) {
            *(float*)(bgb + 0x128) = cfg.borderColor.x;
            *(float*)(bgb + 0x12C) = cfg.borderColor.y;
            *(float*)(bgb + 0x130) = cfg.borderColor.z;
            *(float*)(bgb + 0x134) = cfg.borderColor.w;
        }
    }

    //(RoundedRectangle at 0x20) ---
    void* nc = *(void**)(b + 0x20);
    if (nc) {
        uintptr_t nb = (uintptr_t)nc;
        auto ncComp = (Unity::CComponent*)nc;

        if (cfg.enableRadius) {
            *(float*)(nb + 0x120) = cfg.radius;
            *(float*)(nb + 0x124) = cfg.borderThickness;
        }
        if (cfg.enableNameColor && !cfg.enableHideName) {
            Unity::Vector4 ncol = { cfg.nameContainerColor.x, cfg.nameContainerColor.y,
                                   cfg.nameContainerColor.z, cfg.nameContainerColor.w };
            ncComp->CallMethodSafe<void*>("set_color", ncol);
        }
        // hide/show the whole container
        if (cfg.enableHideName) {
            ncComp->CallMethodSafe<void*>("set_enabled", false);
        }
        else {
            ncComp->CallMethodSafe<void*>("set_enabled", true);
        }
    }

    void* nameTmp = *(void**)(b + 0x28);
    if (nameTmp) {
        auto nameComp = (Unity::CComponent*)nameTmp;
        if (cfg.enableHideName) {
            nameComp->CallMethodSafe<void*>("set_enabled", false);
        }
        else {
            nameComp->CallMethodSafe<void*>("set_enabled", true);
            if (cfg.enableTextColor) {
                Unity::Vector4 tc = { cfg.textColor.x, cfg.textColor.y, cfg.textColor.z, cfg.textColor.w };
                nameComp->CallMethodSafe<void*>("set_color", tc);
            }
        }
    }

    // (TextMeshProUGUI at 0x30) ---
    void* msgTmp = *(void**)(b + 0x30);
    if (msgTmp) {
        auto msgComp = (Unity::CComponent*)msgTmp;
        if (cfg.enableTextColor) {
            Unity::Vector4 tc = { cfg.textColor.x, cfg.textColor.y, cfg.textColor.z, cfg.textColor.w };
            msgComp->CallMethodSafe<void*>("set_color", tc);
        }
    }

    if (cfg.enableWrapWidth)
        *(float*)(b + 0x5C) = cfg.wrapWidth;

    if (cfg.enableMinSize) {
        *(float*)(b + 0x54) = cfg.minW;
        *(float*)(b + 0x58) = cfg.minH;
    }

    void* audio = *(void**)(b + 0x80);
    if (audio) {
        auto audioComp = (Unity::CComponent*)audio;
        audioComp->CallMethodSafe<void*>("set_volume", cfg.enableMuteSound ? 0.0f : 1.0f);
    }
}

// Track if we need to reset on toggle-off
static bool g_bubbleWasEnabled = false;

void UpdateChatBubbles() {
    bool anyEnabled = Config::enableChatBubbleCustomizer && (
        Config::bubbleCfg.enableRadius ||
        Config::bubbleCfg.enableBgColor ||
        Config::bubbleCfg.enableBorderColor ||
        Config::bubbleCfg.enableNameColor ||
        Config::bubbleCfg.enableHideName ||
        Config::bubbleCfg.enableWrapWidth ||
        Config::bubbleCfg.enableMinSize ||
        Config::bubbleCfg.enableMuteSound ||
        Config::bubbleCfg.enableTextColor
        );

    // Reset all bubbles when master toggle turns off
    if (g_bubbleWasEnabled && !Config::enableChatBubbleCustomizer) {
        void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
        try {
            auto bubbles = Unity::Object::FindObjectsOfType<Unity::CComponent>("ChatBubble");
            if (bubbles && bubbles->m_uMaxLength > 0)
                for (int i = 0; i < (int)bubbles->m_uMaxLength; i++)
                    if (auto b = bubbles->operator[](i)) ResetChatBubble(b);
        }
        catch (...) {}
        IL2CPP::Thread::Detach(pThread);
    }
    g_bubbleWasEnabled = Config::enableChatBubbleCustomizer;

    if (!anyEnabled) return;

    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    try {
        auto bubbles = Unity::Object::FindObjectsOfType<Unity::CComponent>("ChatBubble");
        if (bubbles && bubbles->m_uMaxLength > 0)
            for (int i = 0; i < (int)bubbles->m_uMaxLength; i++)
                if (auto b = bubbles->operator[](i))
                    ApplyChatBubbleConfig(b, Config::bubbleCfg);
    }
    catch (...) {}
    IL2CPP::Thread::Detach(pThread);
}

struct BubblePreset {
    const char* name;
    Config::ChatBubbleConfig cfg;
};

// helper to build a "dark" preset cleanly
static auto MakeDarkPreset = []() {
    Config::ChatBubbleConfig c{};
    c.enableRadius = true;  c.radius = 24.f; c.borderThickness = 0.0f;
    c.enableBgColor = true;  c.bgColor = { 0.10f, 0.10f, 0.12f, 0.93f };
    c.enableBorderColor = true;  c.borderColor = { 0.35f, 0.35f, 0.45f, 1.f };
    c.enableNameColor = true;  c.nameContainerColor = { 0.15f, 0.15f, 0.20f, 1.f };
    c.enableTextColor = true;  c.textColor = { 0.92f, 0.92f, 0.92f, 1.f };
    c.enableWrapWidth = true;  c.wrapWidth = 700.f;
    c.enableMinSize = true;  c.minW = 50.f; c.minH = 28.f;
    c.enableMuteSound = true;
    c.enableHideName = true;
    return c;
    };
static auto MakeMinimalPreset = []() {
    Config::ChatBubbleConfig c{};
    c.enableRadius = true;  c.radius = 3.f; c.borderThickness = 0.f;
    c.enableBgColor = true;  c.bgColor = { 0.95f, 0.95f, 0.95f, 0.82f };
    c.enableNameColor = true;  c.nameContainerColor = { 0.85f, 0.85f, 0.85f, 0.82f };
    c.enableTextColor = true;  c.textColor = { 0.05f, 0.05f, 0.05f, 1.f };
    c.enableWrapWidth = true;  c.wrapWidth = 160.f;
    c.enableMinSize = true;  c.minW = 40.f; c.minH = 22.f;
    c.enableMuteSound = true;
    return c;
    };
static auto MakeSharpPreset = []() {
    Config::ChatBubbleConfig c{};
    c.enableRadius = true;  c.radius = 0.f; c.borderThickness = 1.5f;
    c.enableBgColor = true;  c.bgColor = { 0.08f, 0.08f, 0.10f, 0.95f };
    c.enableBorderColor = true;  c.borderColor = { 0.55f, 0.28f, 0.90f, 1.f };
    c.enableNameColor = true;  c.nameContainerColor = { 0.12f, 0.08f, 0.18f, 1.f };
    c.enableTextColor = true;  c.textColor = { 0.88f, 0.88f, 0.88f, 1.f };
    c.enableWrapWidth = true;  c.wrapWidth = 170.f;
    c.enableMinSize = true;  c.minW = 45.f; c.minH = 24.f;
    c.enableMuteSound = true;
    return c;
    };


typedef bool(__cdecl* InteractionDataHandler_HandleInteraction_t)(void* __this, void* interactor, InteractionData interaction, bool interactionIsLocal, void* method);
InteractionDataHandler_HandleInteraction_t oInteractionDataHandler_HandleInteraction = nullptr;

inline bool enableSafeInteractionPatch = false;

bool __cdecl hkInteractionDataHandler_HandleInteraction(void* __this, void* interactor, InteractionData interaction, bool interactionIsLocal, void* method)
{
    if (enableSafeInteractionPatch)
    {
        // Force interactionType to 13 (ProximityDamageAndImpulse) and playerKilledByType to 8
        if (pProximityDamageAndImpulse_Create)
        {
            interaction = pProximityDamageAndImpulse_Create(
                interaction.damage,
                interaction.impulse,
                8,      // playerKilledByType = Explosive
                nullptr
            );
        }
    }

    if (oInteractionDataHandler_HandleInteraction)
        return oInteractionDataHandler_HandleInteraction(__this, interactor, interaction, interactionIsLocal, method);

    return false;
}

// Add these to your hooks/function pointers section

typedef void* (__cdecl* ThemeRepository_get_Instance_t)(void* method);
ThemeRepository_get_Instance_t pThemeRepository_get_Instance = nullptr;

typedef void* (__cdecl* ThemeRepository_CreateTemporaryThemeVisualization_t)(void* __this, void* identifier, void* method);
ThemeRepository_CreateTemporaryThemeVisualization_t pThemeRepository_CreateTemporaryThemeVisualization = nullptr;

typedef void(__cdecl* ThemeRepository_DestroyTemporary_t)(void* __this, void* theme, void* method);
ThemeRepository_DestroyTemporary_t pThemeRepository_DestroyTemporary = nullptr;

typedef void* (__cdecl* ThemeRepository_get_CurrentThemeVisualization_t)(void* __this, void* method);
ThemeRepository_get_CurrentThemeVisualization_t pThemeRepository_get_CurrentThemeVisualization = nullptr;

typedef void* (__cdecl* ThemeRepository_GetThemePrefab_t)(void* __this, void* identifier, void* method);
ThemeRepository_GetThemePrefab_t pThemeRepository_GetThemePrefab = nullptr;

typedef bool(__cdecl* ThemeRepository_get_ThemesEnabled_t)(void* __this, void* method);
ThemeRepository_get_ThemesEnabled_t pThemeRepository_get_ThemesEnabled = nullptr;

typedef void(__cdecl* ThemeRepository_set_ThemesEnabled_t)(void* __this, bool value, void* method);
ThemeRepository_set_ThemesEnabled_t pThemeRepository_set_ThemesEnabled = nullptr;

// theme state
static void* g_currentThemePreview = nullptr;
static int g_selectedThemeIndex = 9; // Normal = index 9
static bool g_themesEnabled = true;

static const char* themeIdentifiers[] = {
    "Animals", "BoxHalloween", "BoxPumpkin", "BoxSkull",
    "Candy", "Cartoon", "Christmas", "Cloudy",
    "Heart", "Normal", "Pumpkin", "Puzzle",
    "RoundBoxSkull", "RoundCircleSkull", "RoundSkull",
    "RoundSquare", "RoundSquareSkull", "Scary",
    "Square", "SquareSkull", "Triangles"
};
static const int THEME_COUNT = 21;

void* GetThemeRepositoryInstance() {
    if (!pThemeRepository_get_Instance) return nullptr;
    try { return pThemeRepository_get_Instance(nullptr); }
    catch (...) { return nullptr; }
}

void DestroyThemePreview() {
    if (!g_currentThemePreview) return;
    void* repo = GetThemeRepositoryInstance();
    if (!repo) { g_currentThemePreview = nullptr; return; }
    try {
        pThemeRepository_DestroyTemporary(repo, g_currentThemePreview, nullptr);
    }
    catch (...) {}
    g_currentThemePreview = nullptr;
}

void ApplyThemeByName(const char* identifier) {
    void* repo = GetThemeRepositoryInstance();
    if (!repo) return;

    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    try {
        // destroy old preview safely
        if (g_currentThemePreview) {
            try { pThemeRepository_DestroyTemporary(repo, g_currentThemePreview, nullptr); }
            catch (...) {}
            g_currentThemePreview = nullptr;
        }

        void* idStr = IL2CPP::String::New(identifier);
        if (!idStr) { IL2CPP::Thread::Detach(pThread); return; }

        void* newTheme = pThemeRepository_CreateTemporaryThemeVisualization(repo, idStr, nullptr);
        if (newTheme) g_currentThemePreview = newTheme;
    }
    catch (...) {}
    IL2CPP::Thread::Detach(pThread);
}

void SetThemesEnabled(bool enabled) {
    void* repo = GetThemeRepositoryInstance();
    if (!repo || !pThemeRepository_set_ThemesEnabled) return;
    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    try {
        pThemeRepository_set_ThemesEnabled(repo, enabled, nullptr);
        if (!enabled) DestroyThemePreview();
    }
    catch (...) {}
    IL2CPP::Thread::Detach(pThread);
}

typedef void(__cdecl* PostGameMsgString_t)(int32_t gameMsgType, void* message, void* method);
extern PostGameMsgString_t pPostGameMsgString;

void postGameMsgString(const char* message, int msgType);

PostGameMsgString_t pPostGameMsgString = nullptr;

void postGameMsgString(const char* message, int msgType) {
    if (!pPostGameMsgString) return;
    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    void* strObj = IL2CPP::String::New(message);
    pPostGameMsgString(msgType, strObj, nullptr);
    IL2CPP::Thread::Detach(pThread);
}
 //PhotonPeer instance capture
static void* g_photonPeerInstance = nullptr;

typedef int(__cdecl* PhotonPeer_get_ServerTime_t)(void* __this, void* method);
PhotonPeer_get_ServerTime_t oPhotonPeer_get_ServerTime = nullptr;

typedef bool(__cdecl* PhotonPeer_Connect_t)(void* __this, void* serverAddress, void* applicationName, void* method);
PhotonPeer_Connect_t pPhotonPeer_Connect = nullptr;

int __cdecl hkPhotonPeer_get_ServerTime(void* __this, void* method)
{
    if (__this)


        g_photonPeerInstance = __this;

    return oPhotonPeer_get_ServerTime(__this, method);
}

static bool g_manualConnectPending = false;
static std::string g_manualConnectTarget = "";

bool __cdecl hkPhotonPeer_Connect(void* __this, void* serverAddress, void* applicationName, void* method)
{
    if (__this) g_photonPeerInstance = __this;

    if (!g_manualConnectPending)
        return pPhotonPeer_Connect(__this, serverAddress, applicationName, method);

    g_manualConnectPending = false;
    *(void**)((uintptr_t)__this + 0xB0) = nullptr;
    return pPhotonPeer_Connect(__this, serverAddress, applicationName, method);
}

typedef void(__cdecl* PhotonPeer_Disconnect_t)(void* __this, void* method);
PhotonPeer_Disconnect_t pPhotonPeer_Disconnect = nullptr;


typedef void(__cdecl* MVNetworkGame_OnStatusChanged_t)(void* __this, int statusCode, void* method);
MVNetworkGame_OnStatusChanged_t oMVNetworkGame_OnStatusChanged = nullptr;

static bool g_blockStatusChanged = false;

void __cdecl hkMVNetworkGame_OnStatusChanged(void* __this, int statusCode, void* method)
{

    if (g_blockStatusChanged) {
        if (statusCode == 1025 || statusCode == 1030) {
            return;
        }
        if (statusCode == 1024) {
            g_blockStatusChanged = false;
            reconnecting = false;
        }
    }



    if (oMVNetworkGame_OnStatusChanged)
        oMVNetworkGame_OnStatusChanged(__this, statusCode, method);
}

typedef void* (__cdecl* GetGSD_t)(void*);
GetGSD_t pMVGameControllerBase_get_GameSessionData = nullptr;

typedef void(__cdecl* PurchaseProduct_t)(void* __this, int32_t productTypeID, void* productData, void* method);
PurchaseProduct_t oPurchaseProduct = nullptr;
void __cdecl hkPurchaseProduct(void* __this, int32_t productTypeID, void* productData, void* method) {
    if (productData) {
        auto dict = (IL2Cpp::Dict::Dictionary<void*, void*>*)productData;
        if (dict->_entries) {
            for (uint32_t i = 0; i < dict->_entries->max_length; i++) {
                auto& e = dict->_entries->m_Items[i];
                if (e.hashCode < 0 || !e.key) continue;

                int32_t newKey = 200; // whatever you want
                __try {
                    *(int32_t*)((uintptr_t)e.key + 0x10) = newKey;
                    e.hashCode = newKey;
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
    }

    IL2Cpp::LogDictionary(productData, "after patch");

    if (oPurchaseProduct)
        oPurchaseProduct(__this, productTypeID, productData, method);
}

typedef void(__cdecl* MVGroup_PlayModeInitialize_t)(void* __this, void* method);
MVGroup_PlayModeInitialize_t oMVGroup_PlayModeInitialize = nullptr;

void __cdecl hkMVGroup_PlayModeInitialize(void* __this, void* method)
{
    if (reconnecting) return;
    if (oMVGroup_PlayModeInitialize)
        oMVGroup_PlayModeInitialize(__this, method);
}

/*

    {
      "Address": 9773488,
      "Name": "MVNetworkGame.EventHandling$$HandleEvent",
      "Signature": "void MVNetworkGame_EventHandling__HandleEvent (MVNetworkGame_EventHandling_o* __this, uint8_t eventCode, ExitGames_Client_Photon_EventData_o* photonEvent, const MethodInfo* method);",
      "TypeSignature": "viiii"
    },

*/

bool resetTerrain = false;
typedef void(__cdecl* HandleEvent_t)(void* __this, int32_t eventcode, void* photonevent, void* method);
HandleEvent_t oHandleEvent = nullptr;

void __cdecl hkhandleEvent(void* __this, int32_t eventcode, void* photonevent, void* method) {
    if (resetTerrain && eventcode == 82)
        eventcode = 52;

    oHandleEvent(__this, eventcode, photonevent, method);
}

// --- MVPlayer capture ---
static void* g_MVPlayerInstance = nullptr;

typedef int(__cdecl* MVPlayer_get_ActorNr_t)(void* __this, void* method);
MVPlayer_get_ActorNr_t oMVPlayer_get_ActorNr = nullptr;

typedef void(__cdecl* MVPlayer_set_Level_t)(void* __this, int level, void* method);
MVPlayer_set_Level_t pMVPlayer_set_Level = nullptr;

int __cdecl hkMVPlayer_get_ActorNr(void* __this, void* method)
{
    if (__this)
        g_MVPlayerInstance = __this;

    return oMVPlayer_get_ActorNr(__this, method);
}

void SetPlayerLevel(int level)
{
    if (!g_MVPlayerInstance || !pMVPlayer_set_Level) return;
    void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
    pMVPlayer_set_Level(g_MVPlayerInstance, level, nullptr);
    IL2CPP::Thread::Detach(pThread);
}