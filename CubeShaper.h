#pragma once
#include <Windows.h>
#include "MinHook.h"

namespace CubeShaper {

    inline bool enableShapeHook = false;
    inline bool allowIllegalShapes = false;
    inline byte isLegalOrigByte = 0;
    inline byte customByteCorners[8] = { 20, 120, 124, 24, 4, 104, 100, 0 };

    struct CornerEdit { int x, z; };
    inline CornerEdit corners[8] = {
        {1,4},{7,8},{7,12},{1,8},
        {0,4},{6,8},{6,12},{0,0}
    };

    //   norm decoded (enc = (x<<4)|z):
    //   top: c0(1,4)=20  c1(7,8)=120  c2(7,12)=124  c3(1,8)=24
    //   bot: c4(0,4)=4   c5(6,8)=104  c6(6,4)=100   c7(0,0)=0
    //
    //   face winding from CubeBase.GetFace:
    //   Top:    c0,c1,c2,c3
    //   Bottom: c4,c5,c6,c7
    //   Front:  c7,c6,c1,c0
    //   Back:   c5,c4,c3,c2
    //   Left:   c4,c7,c0,c3
    //   Right:  c6,c5,c2,c1
    //
    /*
    
         Butterfly quad = two adjacent corners on a face swapped → face self-intersects
        → UV interpolation goes haywire → full glitch texture on that face.
        All "GX:" presets only use Normal's own coordinate values, just reassigned,
        so geometry stays within safe bounds while crossing specific face windings.
    
    */

    struct ShapePreset { const char* name; byte data[8]; };
    inline ShapePreset presets[] = {
        { "Normal",     { 0x14, 0x78, 0x7C, 0x18, 0x04, 0x68, 0x64, 0x00 } },
        { "SmallCube",  { 0x29, 0x5B, 0x5D, 0x2B, 0x21, 0x53, 0x51, 0x1F } },
        { "Cuboid1",    { 0x2D, 0x5F, 0x63, 0x31, 0x1D, 0x4F, 0x4B, 0x19 } },
        { "Cuboid2",    { 0x15, 0x79, 0x7B, 0x17, 0x03, 0x67, 0x65, 0x01 } },
        { "Cuboid3",    { 0x14, 0x78, 0x7A, 0x16, 0x02, 0x66, 0x64, 0x00 } },
        { "Cuboid4",    { 0x16, 0x7A, 0x7C, 0x18, 0x04, 0x68, 0x66, 0x02 } },
        { "Flat1",      { 0x0F, 0x73, 0x77, 0x13, 0x04, 0x68, 0x64, 0x00 } },
        { "Flat2",      { 0x0A, 0x6E, 0x72, 0x0E, 0x04, 0x68, 0x64, 0x00 } },
        { "Flat3",      { 0x05, 0x69, 0x6D, 0x09, 0x04, 0x68, 0x64, 0x00 } },
        { "TruncPyr1",  { 0x2E, 0x60, 0x62, 0x30, 0x04, 0x68, 0x64, 0x00 } },
        { "TruncPyr2",  { 0x29, 0x5B, 0x5D, 0x2B, 0x04, 0x68, 0x64, 0x00 } },
        { "TruncPyr3",  { 0x14, 0x78, 0x7C, 0x18, 0x1C, 0x4E, 0x4C, 0x1A } },
        { "Prism1",     { 0x14, 0x78, 0x7C, 0x18, 0x36, 0x36, 0x32, 0x32 } },
        { "Prism2",     { 0x46, 0x46, 0x4A, 0x4A, 0x04, 0x68, 0x64, 0x00 } },
        { "Prism3",     { 0x14, 0x78, 0x7C, 0x18, 0x02, 0x66, 0x66, 0x02 } },
        { "Tri1",       { 0x14, 0x78, 0x77, 0x13, 0x04, 0x68, 0x64, 0x00 } },
        { "Tri2",       { 0x14, 0x78, 0x68, 0x04, 0x04, 0x68, 0x64, 0x00 } },
        { "Tri3",       { 0x0F, 0x73, 0x68, 0x04, 0x04, 0x68, 0x64, 0x00 } },
        { "Tri4",       { 0x14, 0x14, 0x7C, 0x18, 0x04, 0x68, 0x00, 0x00 } },
        { "Tri5",       { 0x14, 0x78, 0x18, 0x18, 0x04, 0x04, 0x64, 0x00 } },
        { "Pyr1",       { 0x14, 0x14, 0x14, 0x14, 0x04, 0x68, 0x64, 0x00 } },
        { "Pyr2",       { 0x78, 0x78, 0x78, 0x78, 0x04, 0x68, 0x64, 0x00 } },
        { "Pyr3",       { 0x7C, 0x7C, 0x7C, 0x7C, 0x04, 0x68, 0x64, 0x00 } },
        { "Pyr4",       { 0x18, 0x18, 0x18, 0x18, 0x04, 0x68, 0x64, 0x00 } },
        { "Parallelogram1", { 0x14, 0x46, 0x7C, 0x4A, 0x36, 0x68, 0x32, 0x00 } },
        { "Parallelogram2", { 0x46, 0x78, 0x4A, 0x18, 0x04, 0x36, 0x64, 0x32 } },
        { "Rhombus1",   { 0x14, 0x60, 0x7C, 0x30, 0x1C, 0x68, 0x4C, 0x00 } },
        { "Rhombus2",   { 0x2E, 0x78, 0x62, 0x18, 0x04, 0x4E, 0x64, 0x1A } },
        { "GX:Twist",   { 124,  24,  20, 120,   4, 104, 100,   0 } },
        { "GX:TB",      { 124, 120,  20,  24, 100, 104,   4,   0 } },
        { "GX:AllBlack",{ 124,  24,  20, 120,   0, 100, 104,   4 } },
        { "GX:Invert",  {   4, 104, 100,   0,  20, 120, 124,  24 } },
        { "GX:Ring",    { 120, 124,  24,  20, 104, 100,   0,   4 } },
        { "GX:Chaos",   { 104, 120, 124,  24,   4,  20, 100,   0 } },
    };
    inline int selectedPreset = 0;


    inline byte EncodeCorner(int x, int z) {
        x = max(0, min(15, x));
        z = max(0, min(15, z));
        return (byte)((x << 4) | z);
    }
    inline void DecodeCorner(byte b, int& x, int& z) {
        x = (b >> 4) & 0xF;
        z = b & 0xF;
    }
    inline void SyncToBytes() {
        for (int i = 0; i < 8; i++)
            customByteCorners[i] = EncodeCorner(corners[i].x, corners[i].z);
    }
    inline void SyncFromBytes() {
        for (int i = 0; i < 8; i++)
            DecodeCorner(customByteCorners[i], corners[i].x, corners[i].z);
    }
    inline void ApplyPreset(int idx) {
        memcpy(customByteCorners, presets[idx].data, 8);
        SyncFromBytes();
        selectedPreset = idx;
    }

    // ===== HOOK =====
    // Signature: System_Byte_array* (UnityEngine_Vector3_array* corners, const MethodInfo* method)
    typedef void* (*CornersToByteArray_t)(void* corners, void* method);
    inline CornersToByteArray_t origCornersToByteArray = nullptr;

    inline void* hkCornersToByteArray(void* corners, void* method) {
        void* result = origCornersToByteArray(corners, method);

        if (!enableShapeHook || !result) return result;

        try {
            byte* data = reinterpret_cast<byte*>(reinterpret_cast<uintptr_t>(result) + 0x20);
            for (int i = 0; i < 8; i++)
                data[i] = customByteCorners[i];
        }
        catch (...) {}

        return result;
    }

    inline bool Initialize() {
        uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
        if (!base) return false;

        //MV.WorldObject.CubeDataPacker$$CornersToByteArray
        uintptr_t target = base + 0x17E44D0;
        MH_STATUS s = MH_CreateHook(
            reinterpret_cast<void*>(target),
            reinterpret_cast<void*>(&hkCornersToByteArray),
            reinterpret_cast<void**>(&origCornersToByteArray)
        );
        if (s != MH_OK) {
            printf("[CubeShaper] Hook failed: %d\n", s);
            return false;
        }
        MH_EnableHook(reinterpret_cast<void*>(target));
        printf("[CubeShaper] Initialized\n");
        return true;
    }

    inline void RenderUI() { //ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
        ImGui::Spacing();
        SeparatorText("Cube Shape Editor");
        ImGui::Spacing();

        ImGui::Text("Allow Illegal Shapes");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
        if (ToggleButton("##allowIllegal", &allowIllegalShapes)) {
            RetPatch(Offsets::Cube::IsLegal, allowIllegalShapes, isLegalOrigByte);
        }

        ImGui::Spacing();


        ImGui::Text("Override Cube Shape");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f - ImGui::GetStyle().WindowPadding.x);
        ToggleButton("##shapeHook", &enableShapeHook);
        if (!enableShapeHook) {
            ImGui::TextDisabled("enable to intercept placed cubes");
            return;
        }

        ImGui::Spacing();
        char byteStr[64];
        snprintf(byteStr, sizeof(byteStr), "{%d,%d,%d,%d,%d,%d,%d,%d}",
            customByteCorners[0], customByteCorners[1], customByteCorners[2], customByteCorners[3],
            customByteCorners[4], customByteCorners[5], customByteCorners[6], customByteCorners[7]);
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", byteStr);

        ImGui::Spacing();

        float w = ImGui::GetContentRegionAvail().x;
        const int COLS = 5;
        float bw = (w - ImGui::GetStyle().ItemSpacing.x * (COLS - 1)) / COLS;

        if (ImGui::CollapsingHeader("presets")) {
            int col = 0;
            int total = sizeof(presets) / sizeof(presets[0]);
            for (int i = 0; i < total; i++) {
                if (col > 0) ImGui::SameLine();
                bool sel = (selectedPreset == i);
                if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                if (ImGui::Button(presets[i].name, ImVec2(bw, 22))) ApplyPreset(i);
                if (sel) ImGui::PopStyleColor();
                col = (col + 1) % COLS;
            }
        }

        ImGui::Spacing();
        
        if (ImGui::CollapsingHeader("advanced")) {

            SeparatorText("Corners");
            float colW = (w - ImGui::GetStyle().ItemSpacing.x * 3) / 4.0f;
            bool changed = false;

            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Top (0-3):");
            ImGui::PushItemWidth(colW - 5);
            for (int i = 0; i < 4; i++) {
                if (i > 0) ImGui::SameLine();
                ImGui::PushID(i);
                ImGui::BeginGroup();
                ImGui::Text("%d", i);
                if (ImGui::SliderInt("##x", &corners[i].x, 0, 15, "X:%d")) changed = true;
                if (ImGui::SliderInt("##z", &corners[i].z, 0, 15, "Z:%d")) changed = true;
                ImGui::EndGroup();
                ImGui::PopID();
            }
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "Bottom (4-7):");
            for (int i = 4; i < 8; i++) {
                if (i > 4) ImGui::SameLine();
                ImGui::PushID(i);
                ImGui::BeginGroup();
                ImGui::Text("%d", i);
                if (ImGui::SliderInt("##x", &corners[i].x, 0, 15, "X:%d")) changed = true;
                if (ImGui::SliderInt("##z", &corners[i].z, 0, 15, "Z:%d")) changed = true;
                ImGui::EndGroup();
                ImGui::PopID();
            }
            ImGui::PopItemWidth();

            if (changed) { SyncToBytes(); selectedPreset = -1; }

        }

        ImGui::Spacing();
        if (ImGui::Button("Reset", ImVec2(-1, 24))) ApplyPreset(0);
    }
}