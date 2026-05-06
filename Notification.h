#pragma once
#include <string>

enum class NotificationType {
    Info,
    Success,
    Warning,
    Error
};

typedef void(__cdecl* PostGameMsgString_t)(int32_t gameMsgType, void* message, void* method);
extern PostGameMsgString_t pPostGameMsgString;

inline const char* GetTypeHex(NotificationType type) {
    switch (type) {
    case NotificationType::Success: return "5DBD7A";
    case NotificationType::Warning: return "D4A847";
    case NotificationType::Error:   return "C45252";
    default:                        return "7EB3E0";
    }
}

// Both use pPostGameMsgString with thread attach
inline void PushNotification(const std::string& content, NotificationType type = NotificationType::Info, float duration = 0.f) {
    if (!pPostGameMsgString) return;

    void* thread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    const char* col = GetTypeHex(type);
    char msg[2048]{};
    snprintf(msg, sizeof(msg), "<size=30><color=#%s><b>%s</b></color></size>", col, content.c_str());
    void* strObj = IL2CPP::String::New(msg);
    pPostGameMsgString(3, strObj, nullptr);

    IL2CPP::Thread::Detach(thread);
}

inline void PushNotification(const std::string& header, const std::string& content, NotificationType type = NotificationType::Info, float duration = 0.f) {
    if (!pPostGameMsgString) return;

    void* thread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

    const char* col = GetTypeHex(type);
    char msg[2048]{};
    snprintf(msg, sizeof(msg), "<size=30><color=#%s><b>%s</b></color>  <color=#999999>%s</color></size>", col, header.c_str(), content.c_str());
    void* strObj = IL2CPP::String::New(msg);
    pPostGameMsgString(3, strObj, nullptr);

    IL2CPP::Thread::Detach(thread);
}

inline void RenderNotificationStack() {}