// sidebar shit with icons - copied this from some imgui tutorial lol
#include <set>        // add this
#include "Icons.h" // fontawesome stuff

static float buttonColorTransition[11] = { 0.0f }; // track transition state for each button
inline ImFont* g_BoldFont = nullptr;

struct CachedPlayer {
    void* avatarInstance;
    void* gameObject;
    std::string name;
    int actorNr;
    bool isLocal;
};
extern std::vector<CachedPlayer> g_cachedPlayers; 

static bool g_sidebarCollapsed = true;
static float g_sidebarAnimWidth = 125.0f;
static float g_indicatorY = 0.0f;       // current animated Y position
static float g_indicatorTargetY = 0.0f; // target Y position
static bool  g_indicatorInit = false;   // skip animation on first frame

bool ThemedSidebarButton(const char* icon, const char* label, bool active, int buttonIndex, const ImVec2& size = ImVec2(-1, 24)) {
    auto& style = ImGui::GetStyle();

    float targetTransition = active ? 1.0f : 0.0f;
    float speed = ImGui::GetIO().DeltaTime * 8.0f;
    buttonColorTransition[buttonIndex] += (targetTransition - buttonColorTransition[buttonIndex]) * speed;
    float t = buttonColorTransition[buttonIndex];

    ImVec4 buttonColor = currentSidebarColor;
    ImVec4 hoverColor = ImVec4(buttonColor.x + 0.05f, buttonColor.y + 0.05f, buttonColor.z + 0.05f, 1.0f);
    ImVec4 activeClickColor = ImVec4(buttonColor.x + 0.03f, buttonColor.y + 0.03f, buttonColor.z + 0.03f, 1.0f);
    ImVec4 finalButtonColor = ImVec4(
        buttonColor.x + (hoverColor.x - buttonColor.x) * t,
        buttonColor.y + (hoverColor.y - buttonColor.y) * t,
        buttonColor.z + (hoverColor.z - buttonColor.z) * t,
        1.0f
    );

    ImGui::PushStyleColor(ImGuiCol_Button, finalButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeClickColor);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

    bool clicked = ImGui::InvisibleButton(label, size);
    ImVec2 btnMin = ImGui::GetItemRectMin();
    ImVec2 btnMax = ImGui::GetItemRectMax();

    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    ImVec4 bgColor = held ? activeClickColor : (hovered ? hoverColor : finalButtonColor);
    ImGui::GetWindowDrawList()->AddRectFilled(btnMin, btnMax, ImGui::ColorConvertFloat4ToU32(bgColor), 4.0f);

    ImVec4 iconColor = ImVec4(
        currentAccentColor.x * t + 0.35f * (1.0f - t),
        currentAccentColor.y * t + 0.35f * (1.0f - t),
        currentAccentColor.z * t + 0.35f * (1.0f - t),
        1.0f
    );
    ImVec4 labelColor = ImVec4(0.35f + 0.65f * t, 0.35f + 0.65f * t, 0.35f + 0.65f * t, 1.0f);

    // collapse anim: how much text is visible (0=collapsed, 1=expanded)
    float expandT = (g_sidebarAnimWidth - 54.0f) / (125.0f - 54.0f);
    expandT = ImClamp(expandT, 0.0f, 1.0f);

    float btnCenterY = btnMin.y + (btnMax.y - btnMin.y) * 0.5f;
    float fontSize = ImGui::GetFontSize();

    if (expandT < 0.05f) {
        ImVec2 iconSize = ImGui::CalcTextSize(icon);
        // scale the measured size by 1.2 to match the render scale
        float scaledW = iconSize.x * 1.2f;
        float scaledH = iconSize.y * 1.2f;
        ImGui::GetWindowDrawList()->AddText(
            ImGui::GetFont(), fontSize * 1.2f,
            ImVec2(btnMin.x + (btnMax.x - btnMin.x) * 0.5f - scaledW * 0.5f,
                btnCenterY - scaledH * 0.5f),
            ImGui::ColorConvertFloat4ToU32(iconColor),
            icon
        );
    }
    else {
        // expanded: icon + label
        float padding = (btnMax.x - btnMin.x) * 0.1f;
        float textY = btnCenterY - fontSize * 0.5f;

        ImGui::GetWindowDrawList()->AddText(ImVec2(btnMin.x + padding, textY),
            ImGui::ColorConvertFloat4ToU32(iconColor), icon);
        float iconWidth = ImGui::CalcTextSize(icon).x;

        // fade text out as collapsing
        ImVec4 fadedLabel = ImVec4(labelColor.x, labelColor.y, labelColor.z, labelColor.w * expandT);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(btnMin.x + padding + iconWidth + 8.0f, textY),
            ImGui::ColorConvertFloat4ToU32(fadedLabel),
            label
        );
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    return clicked;
}

void DrawSidebar() {
    float targetWidth = g_sidebarCollapsed ? 54.0f : 125.0f;
    g_sidebarAnimWidth += (targetWidth - g_sidebarAnimWidth) * ImGui::GetIO().DeltaTime * 12.0f;
    const float sidebarWidth = g_sidebarAnimWidth;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, currentSidebarColor);
    ImGui::PushStyleColor(ImGuiCol_Border, currentSidebarColor);
    ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, 0), true, ImGuiWindowFlags_NoScrollbar);

    float expandT = ImClamp((g_sidebarAnimWidth - 54.0f) / (125.0f - 54.0f), 0.0f, 1.0f);

    // --- Logo area (unchanged) ---
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("LogoRect",
        ImVec2(0, ImGui::GetFontSize() + ImGui::GetStyle().WindowPadding.y * 2),
        true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImVec4 mutedAccent = ImVec4(
        currentAccentColor.x * 0.60f + 0.20f,
        currentAccentColor.y * 0.60f + 0.20f,
        currentAccentColor.z * 0.60f + 0.20f,
        1.0f);

    ImVec2 logoMin = ImGui::GetWindowPos();
    ImVec2 logoMax = ImVec2(logoMin.x + ImGui::GetWindowWidth(), logoMin.y + ImGui::GetWindowHeight());
    if (ImGui::IsMouseHoveringRect(logoMin, logoMax) && ImGui::IsMouseClicked(0))
        g_sidebarCollapsed = !g_sidebarCollapsed;

    if (expandT > 0.05f) {
        float projectNameWidth = 0.0f;
        if (g_BoldFont) { ImGui::PushFont(g_BoldFont); projectNameWidth = ImGui::CalcTextSize("OPIUM").x; ImGui::PopFont(); }
        else projectNameWidth = ImGui::CalcTextSize("OPIUM").x;
        ImVec2 versionSize = ImGui::CalcTextSize("Dev");
        float totalWidth = projectNameWidth + versionSize.x + ImGui::GetStyle().ItemSpacing.x;
        float cursorX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f - 5.0f;
        ImGui::SetCursorPosX(cursorX);
        ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y);
        ImGui::BeginGroup();
        ImVec4 fadedAccent = ImVec4(mutedAccent.x, mutedAccent.y, mutedAccent.z, expandT);
        if (g_BoldFont) ImGui::PushFont(g_BoldFont);
        ImGui::SetWindowFontScale(1.4f);
        ImGui::TextColored(fadedAccent, "OPIUM");
        ImGui::SetWindowFontScale(1.0f);
        if (g_BoldFont) ImGui::PopFont();
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, expandT), "Dev");
        ImGui::EndGroup();
    }
    else {
        ImGui::SetWindowFontScale(1.3f);
        ImVec2 iconSz = ImGui::CalcTextSize(ICON_FA_CIRCLE_DOT);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - iconSz.x) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y);
        ImGui::TextColored(mutedAccent, ICON_FA_SKULL);
        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::PopStyleColor();

    // ---- NAVIGATION BUTTONS + INDICATOR ----
    ImVec2 btnSize = ImVec2(-1, 26);

    // Collect which page is active and record button rects after each draw
    struct NavEntry { UIPage page; const char* icon; const char* label; int idx; };
    NavEntry navItems[] = {
        { UIPage::WEAPONS,  ICON_FA_GUN,                "WEAPONS",  2 },
        { UIPage::NETWORK,  ICON_FA_USERS,              "PLAYERS",  4 },
        { UIPage::EXPLOITS, ICON_FA_TRIANGLE_EXCLAMATION,"EXPLOITS", 3 },
        { UIPage::MISC,     ICON_FA_CUBE,               "MISC",     7 },
        { UIPage::VISUALS,  ICON_FA_EYE,                "VISUALS",  1 },
        { UIPage::CUBES,    ICON_FA_CUBES,              "CUBES",    9 },
        { UIPage::OTHER,    ICON_FA_HAMMER,             "OTHER",    8 },
        { UIPage::CHAT,     ICON_FA_COMMENTS,           "CHAT",     10 },
        { UIPage::THEMES,   ICON_FA_PALETTE,            "THEMES",   5 },
        { UIPage::SETTINGS, ICON_FA_GEAR,               "SETTINGS", 6 },
    };

    // We'll collect the center-Y of the active button after drawing all buttons
    float activeButtonCenterY = -1.0f;
    float activeButtonMinY = -1.0f;
    float activeButtonMaxY = -1.0f;

    ImVec2 sidebarScreenPos = ImGui::GetWindowPos();

    for (auto& nav : navItems) {
        bool active = (Config::currentPage == nav.page);
        if (ThemedSidebarButton(nav.icon, nav.label, active, nav.idx, btnSize)) {
            Config::currentPage = nav.page;
        }
        if (active) {  // ← remove the || condition entirely
            ImVec2 rMin = ImGui::GetItemRectMin();
            ImVec2 rMax = ImGui::GetItemRectMax();
            activeButtonCenterY = (rMin.y + rMax.y) * 0.5f;
        }
    }

    // Animate indicator toward target
    if (activeButtonCenterY >= 0.0f) {
        g_indicatorTargetY = activeButtonCenterY;
        if (!g_indicatorInit) {
            g_indicatorY = g_indicatorTargetY;
            g_indicatorInit = true;
        }
    }
    g_indicatorY += (g_indicatorTargetY - g_indicatorY) * ImGui::GetIO().DeltaTime * 14.0f;

    // Draw the indicator bar — left edge of sidebar, vertically centered on g_indicatorY
    {
        const float barW = 3.0f;
        const float barHalfH = 8.0f;   // half-height of the pill
        const float barRounding = barHalfH;
        const float inset = 2.0f;   // gap from the very left edge so it doesn't clip border

        ImVec2 barMin = ImVec2(sidebarScreenPos.x + inset, g_indicatorY - barHalfH);
        ImVec2 barMax = ImVec2(sidebarScreenPos.x + inset + barW, g_indicatorY + barHalfH);

        ImGui::GetWindowDrawList()->AddRectFilled(
            barMin, barMax,
            ImGui::ColorConvertFloat4ToU32(currentAccentColor),
            barRounding
        );
    }

    

    ImGui::EndChild();
    ImGui::PopStyleColor(1);
}
void setwndsize()
{
    ImGui::SetNextWindowSize(ImVec2(540, 380), ImGuiCond_Always);
}
void DrawModifiersWindow() {
    setwndsize();
    if (!Config::showModifiersWindow) return;

    if (ImGui::Begin("Avatar Modifiers", &Config::showModifiersWindow)) {

        // Header
        ImGui::PushStyleColor(ImGuiCol_ChildBg, currentSidebarColor);
        ImGui::BeginChild("ModifierHeader", ImVec2(0, 45), true, ImGuiWindowFlags_NoScrollbar);

        ImGui::SetCursorPosY(12);
        ImGui::SetCursorPosX(15);
        ImGui::PushStyleColor(ImGuiCol_Text, currentAccentColor);
        ImGui::Text("AVATAR MODIFIERS");
        ImGui::PopStyleColor();

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Content
        ImGui::BeginChild("ModifierContent", ImVec2(0, 0), false);

        const int columns = 3;
        const float edgePadding = 20.0f;

        ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
        ImVec2 avail = ImGui::GetContentRegionAvail();

        // apply padding
        avail.x -= edgePadding * 2;
        avail.y -= edgePadding * 2;

        float totalRows = ceilf((float)Config::availableModifiers.size() / columns);

        float buttonWidth = (avail.x - spacing.x * (columns - 1)) / columns;
        float buttonHeight = (avail.y - spacing.y * (totalRows - 1)) / totalRows;

        // clamps
        if (buttonWidth < 120.0f) buttonWidth = 120.0f;
        if (buttonHeight < 28.0f) buttonHeight = 28.0f;

        float gridWidth = buttonWidth * columns + spacing.x * (columns - 1);
        float gridHeight = buttonHeight * totalRows + spacing.y * (totalRows - 1);

        float startX = edgePadding + (avail.x - gridWidth) * 0.5f;
        float startY = edgePadding + (avail.y - gridHeight) * 0.5f;

        if (startX < edgePadding) startX = edgePadding;
        if (startY > 0) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + startY);

        ImGui::SetCursorPosX(startX);

        int count = 0;
        for (const auto& modifier : Config::availableModifiers) {

            if (count > 0 && count % columns == 0) {
                ImGui::SetCursorPosX(startX);
            }

            if (count % columns != 0) {
                ImGui::SameLine();
            }

            char label[128];
            snprintf(label, sizeof(label), "%s##mod%d",
                modifier.name.c_str(), modifier.type);

            if (ImGui::Button(label, ImVec2(buttonWidth, buttonHeight))) {
                ApplyModifier(modifier.type);
            }

            count++;
        }

        ImGui::EndChild();
    }
    ImGui::End();
}

struct SectionHighlight
{
    ImVec2 startPos;
    float baseX;
    float thickness;
    ImU32 color;
};

static SectionHighlight g_Section;

void Highlight(float thickness = 2.0f, ImU32 color = 0)
{
    ImVec2 cursor = ImGui::GetCursorScreenPos();   // position BEFORE indent
    g_Section.baseX = cursor.x;

    ImGui::Indent(15.0f);

    g_Section.startPos = ImGui::GetCursorScreenPos();
    g_Section.thickness = thickness;
    g_Section.color = (color == 0)
        ? ImGui::ColorConvertFloat4ToU32(currentAccentColor)
        : color;
}

void EndHighlight()
{
    ImVec2 endPos = ImGui::GetCursorScreenPos();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(
        ImVec2(g_Section.baseX, g_Section.startPos.y),
        ImVec2(g_Section.baseX + g_Section.thickness, endPos.y),
        g_Section.color
    );

    ImGui::Unindent(15.0f);
}

static const std::vector<std::string> g_watchlist = {
    "Crispy Kirby",
    "Tokeeto",
    "Aglorykon",
    "Boxin",
    "SillyBeatle"
};

bool IsWatchlisted(const std::string& name) {
    for (auto& w : g_watchlist)
        if (name == w) return true;
    return false;
}
// muted red — not vibrant
static const ImU32 WATCHLIST_COLOR = IM_COL32(210, 80, 80, 255);

void FlameTarget(void* targetInstance);
void SlapTarget(void* targetInstance);
void HealTarget(void* targetInstance);
//void PushNotification(const char* title, const char* msg, NotificationType type, float duration = 3.0f);
void TeleportToCustomTarget();
void ApplyInteractionToPlayer(void* avatarComponent, float damage, Vector3 impulse, bool continuous);
Vector3 GetPlayerPosition(void* gameObject);

inline float g_lastPlayerHeaderTagX = 0.0f;
inline float g_lastPlayerHeaderTagY = 0.0f;
struct CollapseState { float anim = 0.f; bool open = false; float fullH = 200.f; };
extern std::unordered_map<int, CollapseState> g_cs;

inline bool PlayerCollapsingHeader(const CachedPlayer& p, int uniqueId,
    bool styleColors = false,
    ImVec4 headerBg = ImVec4(0, 0, 0, 0))
{
    auto& s = g_cs[p.actorNr];

    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetFrameHeight();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(("###plr" + std::to_string(uniqueId)).c_str(), ImVec2(w, h));
    if (ImGui::IsItemClicked()) s.open = !s.open;

    bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (headerBg.w == 0.f)
        headerBg = ImVec4(ImMax(currentBackgroundColor.x - 0.03f, 0.f), ImMax(currentBackgroundColor.y - 0.03f, 0.f), ImMax(currentBackgroundColor.z - 0.03f, 0.f), 1.f);
    if (hov) { headerBg.x += 0.03f; headerBg.y += 0.03f; headerBg.z += 0.03f; }
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), ImGui::ColorConvertFloat4ToU32(headerBg), 4.f);

    float angle = s.anim * (IM_PI * 0.5f);
    float cs = cosf(angle), sn = sinf(angle);
    ImVec2 ac = ImVec2(pos.x + h * 0.5f, pos.y + h * 0.5f);
    float ar = h * 0.20f;
    auto R = [&](float px, float py) { return ImVec2(ac.x + cs * px - sn * py, ac.y + sn * px + cs * py); };
    dl->AddTriangleFilled(R(ar, 0), R(-ar, -ar * 0.7f), R(-ar, ar * 0.7f), ImGui::ColorConvertFloat4ToU32(currentAccentColor));

    {
        float ty = pos.y + (h - ImGui::GetTextLineHeight()) * 0.5f;
        float x = pos.x + h + 4.f;
        constexpr ImU32 purpleSoft = IM_COL32(170, 140, 210, 255);
        constexpr ImU32 greenSoft = IM_COL32(140, 200, 140, 255);
        dl->AddText(ImVec2(x, ty), purpleSoft, ICON_FA_USER);
        x += ImGui::CalcTextSize(ICON_FA_USER).x + 4.f;
        std::string actorStr = "[" + std::to_string(p.actorNr) + "]  ";
        dl->AddText(ImVec2(x, ty), purpleSoft, actorStr.c_str());
        x += ImGui::CalcTextSize(actorStr.c_str()).x;
        std::string nameStr = p.isLocal ? (p.name + "  (you)") : p.name;
        ImU32 nameCol = p.isLocal ? greenSoft : IsWatchlisted(p.name) ? WATCHLIST_COLOR : IM_COL32(255, 255, 255, 255);
        dl->AddText(ImVec2(x, ty), nameCol, nameStr.c_str());
        x += ImGui::CalcTextSize(nameStr.c_str()).x + 8.f;
        g_lastPlayerHeaderTagX = x;
        g_lastPlayerHeaderTagY = ty;
    }

    g_lastPlayerHeaderTagX = pos.x;
    g_lastPlayerHeaderTagY = pos.y;
    s.anim += ((s.open ? 1.f : 0.f) - s.anim) * ImClamp(ImGui::GetIO().DeltaTime * 12.f, 0.f, 1.f);

    if (!s.open && s.anim < 0.015f) {
        s.anim = 0.f;
        return false;
    }
    if (s.anim < 0.001f) return false;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
    ImGui::BeginChild(("##plrbody" + std::to_string(uniqueId)).c_str(),
        ImVec2(w, s.fullH * s.anim), false, (ImGuiWindowFlags)(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse));
    return true;
}

// Draws the standard 3-column action buttons for a player.
// redStyle: push the red button color scheme before calling.
inline void PlayerActionButtons(const CachedPlayer& p, float btnW, bool redStyle = false)
{
    if (redStyle) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.13f, 0.13f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.18f, 0.18f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.78f, 0.24f, 0.24f, 1.00f));
    }

    if (!redStyle) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.36f, 0.36f, 0.36f, 1.0f));
    }

    // Row 1
    if (ImGui::Button("Kill##plr", ImVec2(btnW, 0)))
        ApplyInteractionToPlayer(p.avatarInstance, 100.0f, Vector3{ 0,0,0 }, false);
    ImGui::SameLine();
    if (ImGui::Button("Fling##plr", ImVec2(btnW, 0))) {
        float rx = ((rand() % 2000) - 1000) / 100.0f;
        float rz = ((rand() % 2000) - 1000) / 100.0f;
        float hm = sqrtf(rx * rx + rz * rz);
        if (hm > 0.0f) { rx /= hm; rz /= hm; }
        float fixedY = 5000.0f;
        float rem = sqrtf(5000.0f * 5000.0f * 3.0f - fixedY * fixedY);
        ApplyInteractionToPlayer(p.avatarInstance, 0.0f, Vector3{ rx * rem, fixedY, rz * rem }, false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Void##plr", ImVec2(btnW, 0))) {
        union { uint32_t i; float f; } c; c.i = 0x7F7FFF00;
        ApplyInteractionToPlayer(p.avatarInstance, 0.0f, Vector3{ 0.0f, c.f, 0.0f }, false);
    }

    // Row 2
    if (ImGui::Button("Float##plr", ImVec2(btnW, 0)))
        ApplyInteractionToPlayer(p.avatarInstance, 0.0f, Vector3{ 0.0f, 165.1f, 0.0f }, false);
    ImGui::SameLine();
    if (ImGui::Button("Pull##plr", ImVec2(btnW, 0))) {
        Vector3 targetPos = GetPlayerPosition(p.gameObject);
        Vector3 dir = {
            lastKnownPosition.x - targetPos.x,
            lastKnownPosition.y - targetPos.y,
            lastKnownPosition.z - targetPos.z
        };
        float dist = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (dist > 0.1f) {
            dir.x /= dist; dir.y /= dist; dir.z /= dist;
            float force = min(150.0f + dist * 250.0f, 800.0f);
            ApplyInteractionToPlayer(p.avatarInstance, 0.0f,
                Vector3{ dir.x * force, dir.y * force + 300.0f, dir.z * force }, false);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("TP to##plr", ImVec2(btnW, 0))) {
        Config::customInteractionTarget = p.gameObject;
        Config::customInteractionTargetName = p.name;
        TeleportToCustomTarget();
    }

    // Row 3
    if (ImGui::Button("Flame##plr", ImVec2(btnW, 0)))
        CreateThread(nullptr, 0, [](LPVOID lp) -> DWORD { FlameTarget(lp); return 0; }, p.gameObject, 0, nullptr);
    ImGui::SameLine();
    if (ImGui::Button("Slap##plr", ImVec2(btnW, 0)))
        CreateThread(nullptr, 0, [](LPVOID lp) -> DWORD { SlapTarget(lp);  return 0; }, p.gameObject, 0, nullptr);
    ImGui::SameLine();
    if (ImGui::Button("Heal##plr", ImVec2(btnW, 0)))
        CreateThread(nullptr, 0, [](LPVOID lp) -> DWORD { HealTarget(lp);  return 0; }, p.gameObject, 0, nullptr);

    ImGui::PopStyleColor(3);
}

// Helper 1: Darken color inline
inline ImVec4 Darken(const ImVec4& c, float amt = 0.05f) {
    return ImVec4(ImMax(0.f, c.x - amt), ImMax(0.f, c.y - amt), ImMax(0.f, c.z - amt), c.w);
}

void Section(const char* name, float height, auto content) {
    float w = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Darken(currentBackgroundColor, 0.03f));
    ImGui::PushStyleColor(ImGuiCol_Border, Darken(currentBackgroundColor, 0.03f)); // match bg
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

    ImGui::BeginChild(name, ImVec2(w, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    content();
    ImGui::EndChild();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::Spacing();
}

bool slidder(const char* label, float* v, float v_min, float v_max, const char* format = "%.1f")
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float w = ImGui::CalcItemWidth();
    const ImVec2 pos = window->DC.CursorPos;
    const float height = g.FontSize + style.FramePadding.y * 2.0f;
    char val_buf_pre[64];
    ImFormatString(val_buf_pre, IM_ARRAYSIZE(val_buf_pre), format, *v);
    const float input_w = 45.0f;
    const float input_gap = 4.0f;

    const ImRect frame_bb(pos, ImVec2(pos.x + w - input_w - input_gap, pos.y + height));
    const ImRect total_bb(pos, ImVec2(pos.x + w, pos.y + height));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &frame_bb)) return false;

    const bool hovered = ImGui::ItemHoverable(frame_bb, id, ImGuiItemFlags_None);

    if (!ImGui::TempInputIsActive(id)) {
        const bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left, false);
        if (clicked || g.NavActivateId == id) {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
        }
    }

    bool value_changed = false;

    if (ImGui::TempInputIsActive(id)) {
        value_changed = ImGui::TempInputScalar(frame_bb, id, label, ImGuiDataType_Float, v, format);
    }
    else {
        if (ImGui::GetActiveID() == id) {
            if (g.IO.MouseDown[0]) {
                float grab_w_ = ImMax(style.GrabMinSize, 10.0f);
                float usable_w_ = frame_bb.GetWidth() - grab_w_;
                float t = (g.IO.MousePos.x - (frame_bb.Min.x + grab_w_ * 0.5f)) / usable_w_;
                t = ImClamp(t, 0.0f, 1.0f);
                float new_val = v_min + t * (v_max - v_min);
                if (*v != new_val) { *v = new_val; value_changed = true; }
            }
            else {
                ImGui::ClearActiveID();
            }
        }

        // pre-calc text so we know how wide the reserved zone is
        char val_buf[64];
        ImFormatString(val_buf, IM_ARRAYSIZE(val_buf), format, *v);


        float t = ImClamp((*v - v_min) / (v_max - v_min), 0.0f, 1.0f);
        float grab_w = ImMax(style.GrabMinSize, 10.0f);

        // usable width excludes both the grab AND the text zone on the right
        float usable_w = frame_bb.GetWidth() - grab_w;
        float grab_x = frame_bb.Min.x + t * usable_w;

        // Background track
        window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max,
            ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);

        // Fill — accent color, dimmed alpha so it reads as tint not solid block
        ImVec4 accent = currentAccentColor;
        accent.w = 0.4f;
        ImVec2 fill_max = ImVec2(grab_x + grab_w * 0.5f, frame_bb.Max.y);
        if (fill_max.x > frame_bb.Min.x) {
            window->DrawList->AddRectFilled(
                frame_bb.Min, fill_max,
                ImGui::ColorConvertFloat4ToU32(accent),
                style.FrameRounding);
        }

        // Grab
        ImRect grab_bb(
            ImVec2(grab_x, frame_bb.Min.y + style.GrabRounding),
            ImVec2(grab_x + grab_w, frame_bb.Max.y - style.GrabRounding)
        );
        ImVec4 grabCol = currentAccentColor;
        grabCol.w = ImGui::GetActiveID() == id ? 1.0f : 0.85f;
        window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max,
            ImGui::ColorConvertFloat4ToU32(grabCol),
            style.GrabRounding);


        // Input box sitting to the right of the track, outside it
        ImGui::SetCursorScreenPos(ImVec2(frame_bb.Max.x + input_gap, frame_bb.Min.y));
        ImGui::SetNextItemWidth(input_w);
        char input_buf[64];
        ImFormatString(input_buf, IM_ARRAYSIZE(input_buf), format, *v);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, style.FramePadding.y));
        if (ImGui::InputText(("##iv" + std::string(label)).c_str(), input_buf, sizeof(input_buf),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal)) {
            float parsed = (float)atof(input_buf);
            *v = parsed;
            value_changed = true;
        }
        ImGui::PopStyleVar();
    }


    return value_changed;
}