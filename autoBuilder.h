#pragma once
#include <Windows.h>
#include "MinHook.h"
#include <cstdint>
#include <iostream>
#include <vector>
#include <set>
#include <mutex>
#include <atomic>
#include <cmath>
#include <array>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "Offsets.h"
#include <chrono>

extern MVCubeModelBase_RemoveCube_t pMVCubeModelBase_RemoveCube;
namespace Builder {


    struct Vec3i {
        int16_t x, y, z;
        Vec3i(int16_t x = 0, int16_t y = 0, int16_t z = 0) : x(x), y(y), z(z) {}
        bool operator==(const Vec3i& o) const { return x == o.x && y == o.y && z == o.z; }
        bool operator<(const Vec3i& o)  const {
            if (x != o.x) return x < o.x;
            if (y != o.y) return y < o.y;
            return z < o.z;
        }
        Vec3i operator+(const Vec3i& o) const { return Vec3i(x + o.x, y + o.y, z + o.z); }
    };

    struct Il2Object { void* klass; void* monitor; };

    using Corners = std::array<uint8_t, 8>;

    struct Voxel {
        Vec3i   pos;
        uint8_t mat;
        Corners corners{ 20, 120, 124, 24, 4, 104, 100, 0 };
    };

    struct QueueEntry {
        Vec3i   pos;
        uint8_t mat;
        Corners corners{ 20, 120, 124, 24, 4, 104, 100, 0 };
    };

    struct Context {
        void* inst = nullptr;
        void* cubeClass = nullptr;
        void* byteArrClass = nullptr;
        Corners corners = {};
        bool    valid = false;
    };

    static constexpr uint8_t kIdentity[8] = { 20, 120, 124, 24,   4, 104, 100,  0 };
    static constexpr uint8_t kCuboid1[8] = { 0x2D, 0x5F, 0x63, 0x31, 0x1D, 0x4F, 0x4B, 0x19 };
    static constexpr uint8_t kCuboid3[8] = { 0x14, 0x78, 0x7A, 0x16, 0x02, 0x66, 0x64, 0x00 };
    static constexpr uint8_t kCuboid4[8] = { 0x16, 0x7A, 0x7C, 0x18, 0x04, 0x68, 0x66, 0x02 };

    static Corners MakeCorners(const uint8_t src[8]) {
        Corners c; for (int i = 0; i < 8; i++) c[i] = src[i]; return c;
    }

    static Vec3i  g_buildOrigin = {};

    using FnThreadAttach = void* (*)(void*);
    using FnThreadDetach = void  (*)(void*);
    using FnDomainGet = void* (*)();

    static FnThreadAttach threadAttach = nullptr;
    static FnThreadDetach threadDetach = nullptr;
    static FnDomainGet    domainGet = nullptr;

    using FnAddCube = void  (*)(void*, Vec3i, void*, void*);
    using FnCubeCtor = void* (*)(void*, void*, void*, void*);
    using FnObjNew = void* (*)(void*);
    using FnArrNew = void* (*)(void*, uintptr_t);
    using FnHandleDelta = void  (*)(void*, void*);
    using FnGetCubeAt = void* (*)(void*, Vec3i, void*);

    static FnAddCube     origAddCube = nullptr;
    static FnCubeCtor    cubeCtor = nullptr;
    static FnObjNew      objNew = nullptr;
    static FnArrNew      arrNew = nullptr;
    static FnHandleDelta handleDelta = nullptr;
    static FnGetCubeAt   getCubeAt = nullptr;

    static constexpr DWORD kChunkMs = 250;
    static constexpr size_t kChunkSize = 1;
    static int g_previewModelIdx = -1;
    using FnGetCurrentBody = void* (*)(void*, void*);
    using FnGetBodyPart = void* (*)(void*, void*, void*);
    using FnAddCubeDirect = void  (*)(void*, Vec3i, void*, void*);

    static FnGetCurrentBody fnCurrentBody = nullptr;
    static FnGetBodyPart    fnGetBodyPart = nullptr;
    static FnAddCubeDirect  fnAddCubeDirect = nullptr;

    using FnRemoveCube = void(*)(void*, Vec3i, void*);
    static FnRemoveCube fnRemoveCube = nullptr;


    struct State {
        int rotation = 0; // 0=0°, 1=90°, 2=180°, 3=270° around Y axis

        std::atomic<bool> enabled{ false };
        std::atomic<bool> placing{ false };
        std::atomic<bool> cancel{ false };
        std::atomic<bool> instant{ false };
        std::atomic<int>  total{ 0 };
        std::atomic<int>  placed{ 0 };
        std::atomic<DWORD> lastChunk{ 0 };

        std::mutex              mx;
        Context                 ctx;
        std::vector<QueueEntry> queue;
        size_t                  qIdx = 0;
        std::set<Vec3i>         seen;

        int shape = 0;
        int size = 5;
        std::mutex shapeMx;
    };
    static State s;

    struct SavedVoxel { Vec3i pos; uint8_t mat; Corners corners; };
    struct Model { std::string name; std::vector<SavedVoxel> voxels; };

    static std::vector<Model> g_models;

    enum class CaptureStep { Idle, WaitB };
    static CaptureStep  g_capStep = CaptureStep::Idle;
    static Vec3i        g_capCornerA;
    static void* g_capModel = nullptr;
    static char         g_capName[128] = "MyBuild";

    static bool g_captureMode = false;
    static bool g_pendingPlace = false;
    static int  g_pendingIdx = -1;

    static char g_loadPath[260] = {};
    static bool g_loadPathInited = false;

    static std::atomic<bool> g_avatarBuilding{ false };

    static bool Readable(const void* p, size_t sz = sizeof(void*)) {
        if (!p) return false;
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_EXECUTE)) return false;
        return (reinterpret_cast<uintptr_t>(p) + sz) <=
            (reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize);
    }

    static bool CtxOk(const Context& c) {
        return Readable(c.inst) && Readable(c.cubeClass) && Readable(c.byteArrClass) && c.valid;
    }

    static bool IsInstant(int shape) { return shape >= 13; }

    std::vector<Voxel> MakeHollowCube(int sz, uint8_t mat) {
        std::vector<Voxel> v; int h = sz / 2;
        for (int x = -h; x <= h; x++) for (int y = -h; y <= h; y++) for (int z = -h; z <= h; z++)
            if (x == -h || x == h || y == -h || y == h || z == -h || z == h)
                v.push_back({ Vec3i(x,y,z), mat });
        return v;
    }
    std::vector<Voxel> MakeSphere(int r, uint8_t mat) {
        std::vector<Voxel> v;
        float r2o = (float)(r * r), r2i = (float)((r - 1) * (r - 1));
        for (int x = -r; x <= r; x++) for (int y = -r; y <= r; y++) for (int z = -r; z <= r; z++) {
            float d = (float)(x * x + y * y + z * z);
            if (d <= r2o && d >= r2i) v.push_back({ Vec3i(x,y,z),mat });
        }
        return v;
    }
    std::vector<Voxel> MakePlane(int sz, uint8_t mat) {
        std::vector<Voxel> v; int h = sz / 2;
        for (int x = -h; x <= h; x++) for (int z = -h; z <= h; z++)
            v.push_back({ Vec3i(x,0,z),mat });
        return v;
    }
    std::vector<Voxel> MakeCylinder(int r, int h, uint8_t mat) {
        std::vector<Voxel> v; int ro2 = r * r, ri2 = (r - 1) * (r - 1), hh = h / 2;
        for (int y = -hh; y <= hh; y++) for (int x = -r; x <= r; x++) for (int z = -r; z <= r; z++) {
            int d2 = x * x + z * z;
            if ((d2 <= ro2 && d2 >= ri2) || (d2 <= ro2 && (y == -hh || y == hh)))
                v.push_back({ Vec3i(x,y,z),mat });
        }
        return v;
    }
    std::vector<Voxel> MakePyramid(int base, uint8_t mat) {
        std::vector<Voxel> v;
        for (int y = 0; y < base; y++) {
            int h = base - 1 - y;
            for (int x = -h; x <= h; x++) for (int z = -h; z <= h; z++)
                if (x == -h || x == h || z == -h || z == h || y == 0)
                    v.push_back({ Vec3i(x,y,z),mat });
        }
        return v;
    }
    std::vector<Voxel> MakeDiamond(int r, uint8_t mat) {
        std::vector<Voxel> v;
        for (int x = -r; x <= r; x++) for (int y = -r; y <= r; y++) for (int z = -r; z <= r; z++)
            if (abs(x) + abs(y) + abs(z) == r) v.push_back({ Vec3i(x,y,z),mat });
        return v;
    }
    std::vector<Voxel> MakeTorus(int R, int r, uint8_t mat) {
        std::vector<Voxel> v; float rf = (float)r, ri2 = (rf - 1) * (rf - 1), ro2 = rf * rf;
        for (int x = -(R + r); x <= (R + r); x++) for (int y = -r; y <= r; y++) for (int z = -(R + r); z <= (R + r); z++) {
            float dx = sqrtf((float)(x * x + z * z)) - (float)R;
            float d2 = dx * dx + (float)(y * y);
            if (d2 <= ro2 && d2 >= ri2) v.push_back({ Vec3i(x,y,z),mat });
        }
        return v;
    }
    std::vector<Voxel> MakeArch(int r, int depth, uint8_t mat) {
        std::vector<Voxel> v; int ro2 = r * r, ri2 = (r - 1) * (r - 1), hd = depth / 2;
        for (int z = -hd; z <= hd; z++) for (int x = -r; x <= r; x++) for (int y = 0; y <= r; y++) {
            int d2 = x * x + y * y;
            if ((d2 <= ro2 && d2 >= ri2) || ((z == -hd || z == hd) && d2 <= ro2))
                v.push_back({ Vec3i(x,y,z),mat });
        }
        return v;
    }
    std::vector<Voxel> MakeDome(int r, uint8_t mat) {
        std::vector<Voxel> v; float ro2 = (float)(r * r), ri2 = (float)((r - 1) * (r - 1));
        for (int x = -r; x <= r; x++) for (int y = 0; y <= r; y++) for (int z = -r; z <= r; z++) {
            float d = (float)(x * x + y * y + z * z);
            if (d <= ro2 && d >= ri2) v.push_back({ Vec3i(x,y,z),mat });
        }
        return v;
    }
    std::vector<Voxel> MakeStairs(int steps, int depth, uint8_t mat) {
        std::vector<Voxel> v; int hd = depth / 2;
        for (int s = 0; s < steps; s++) for (int z = -hd; z <= hd; z++)
            v.push_back({ Vec3i(s,s,z),mat });
        return v;
    }
    std::vector<Voxel> MakeCross(int sz, uint8_t mat) {
        std::vector<Voxel> v; int h = sz / 2;
        for (int i = -h; i <= h; i++) {
            v.push_back({ Vec3i(i,0,0),mat });
            v.push_back({ Vec3i(0,0,i),mat });
        }
        std::sort(v.begin(), v.end(), [](auto& a, auto& b) {return a.pos < b.pos; });
        v.erase(std::unique(v.begin(), v.end(), [](auto& a, auto& b) {return a.pos == b.pos; }), v.end());
        return v;
    }
    std::vector<Voxel> MakeRing(int r, uint8_t mat) {
        std::vector<Voxel> v; float ro2 = (float)(r * r), ri2 = (float)((r - 1) * (r - 1));
        for (int x = -r; x <= r; x++) for (int z = -r; z <= r; z++) {
            float d = (float)(x * x + z * z);
            if (d <= ro2 && d >= ri2) v.push_back({ Vec3i(x,0,z),mat });
        }
        return v;
    }
    std::vector<Voxel> MakeTower(int sz, int h, uint8_t mat) {
        std::vector<Voxel> v; int hh = sz / 2;
        for (int y = 0; y < h; y++) for (int x = -hh; x <= hh; x++) for (int z = -hh; z <= hh; z++)
            if (x == -hh || x == hh || z == -hh || z == hh) v.push_back({ Vec3i(x,y,z),mat });
        return v;
    }

    std::vector<Voxel> MakeTrapCube(uint8_t mat) {
        std::vector<Voxel> v; v.reserve(12);
        auto C1 = MakeCorners(kCuboid1), C3 = MakeCorners(kCuboid3);
        auto C4 = MakeCorners(kCuboid4), CI = MakeCorners(kIdentity);
        v.push_back({ Vec3i(0,0,-1),mat,C3 }); v.push_back({ Vec3i(1,0,-1),mat,C3 });
        v.push_back({ Vec3i(-1,0,0),mat,C1 }); v.push_back({ Vec3i(-1,0,1),mat,C1 });
        v.push_back({ Vec3i(2,0,0),mat,C1 });  v.push_back({ Vec3i(2,0,1),mat,C1 });
        v.push_back({ Vec3i(0,0,2),mat,C4 });  v.push_back({ Vec3i(1,0,2),mat,C4 });
        v.push_back({ Vec3i(0,2,0),mat,CI });  v.push_back({ Vec3i(1,2,0),mat,CI });
        v.push_back({ Vec3i(0,2,1),mat,CI });  v.push_back({ Vec3i(1,2,1),mat,CI });
        return v;
    }
    std::vector<Voxel> MakePillar(uint8_t mat) {
        std::vector<Voxel> v; v.reserve(15);
        for (int y = 0; y < 15; y++) v.push_back({ Vec3i(0,y,0),mat });
        return v;
    }
    std::vector<Voxel> MakeWallPanel(uint8_t mat) {
        std::vector<Voxel> v; v.reserve(15);
        for (int x = -2; x <= 2; x++) for (int y = 0; y < 3; y++) v.push_back({ Vec3i(x,y,0),mat });
        return v;
    }
    std::vector<Voxel> MakeCornerWall(uint8_t mat) {
        std::vector<Voxel> v; v.reserve(15);
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x <= 2; x++) v.push_back({ Vec3i(x,y,0),mat });
            for (int z = 1; z <= 2; z++) v.push_back({ Vec3i(0,y,z),mat });
        }
        return v;
    }
    std::vector<Voxel> MakeDoorframe(uint8_t mat) {
        std::vector<Voxel> v; v.reserve(13);
        for (int y = 0; y < 5; y++) v.push_back({ Vec3i(-1,y,0),mat });
        for (int y = 0; y < 5; y++) v.push_back({ Vec3i(1,y,0),mat });
        for (int x = -1; x <= 1; x++) v.push_back({ Vec3i(x,5,0),mat });
        return v;
    }
    std::vector<Voxel> MakeWindowFrame(uint8_t mat) {
        std::vector<Voxel> v; v.reserve(12);
        for (int x = -1; x <= 2; x++) for (int y = 0; y <= 3; y++)
            if (x == -1 || x == 2 || y == 0 || y == 3) v.push_back({ Vec3i(x,y,0),mat });
        return v;
    }

    static thread_local bool t_placing = false;

    static bool PlaceOne(const Context& ctx, Vec3i pos, uint8_t mat, const Corners& c) {
        {
            std::lock_guard<std::mutex> lk(s.mx);
            if (s.seen.count(pos)) return false;
        }

        void* cArr = arrNew(ctx.byteArrClass, 8);
        if (!cArr) return false;
        uint8_t* cd = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(cArr) + 0x20);
        for (int i = 0; i < 8; i++) cd[i] = c[i];

        void* mArr = arrNew(ctx.byteArrClass, 6);
        if (!mArr) return false;
        uint8_t* md = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(mArr) + 0x20);
        for (int i = 0; i < 6; i++) md[i] = mat;

        void* cube = objNew(ctx.cubeClass);
        if (!cube) return false;

        cubeCtor(cube, cArr, mArr, nullptr);
        t_placing = true;
        origAddCube(ctx.inst, pos, cube, nullptr);
        t_placing = false;

        { std::lock_guard<std::mutex> lk(s.mx); s.seen.insert(pos); }
        s.placed.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    static void BeginQueue(void* inst, void* cubeClass, void* byteArrClass,
        const Corners& c, Vec3i origin, const Model& m)
    {
        if (m.voxels.empty()) return;
        std::lock_guard<std::mutex> lk(s.mx);
        if (s.placing.load(std::memory_order_relaxed)) return;

        s.seen.clear();
        s.placed.store(0, std::memory_order_relaxed);
        s.cancel.store(false, std::memory_order_relaxed);
        s.ctx = { inst, cubeClass, byteArrClass, c, true };
        s.queue.clear(); s.qIdx = 0;
        s.queue.reserve(m.voxels.size());
        for (auto& v : m.voxels) s.queue.push_back({ origin + v.pos, v.mat, v.corners });
        s.instant.store(false, std::memory_order_release);
        s.total.store((int)s.queue.size(), std::memory_order_relaxed);
        s.lastChunk.store(GetTickCount(), std::memory_order_relaxed);

        s.placing.store(true, std::memory_order_release);
        printf("[builder] queued '%s' (%zu cubes)\n", m.name.c_str(), m.voxels.size());
    }

    void Tick() {
        if (!s.placing.load(std::memory_order_acquire)) return;

        if (s.cancel.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lk(s.mx);
            s.queue.clear(); s.qIdx = 0; s.seen.clear(); s.ctx = {};
            s.cancel.store(false, std::memory_order_release);
            s.placing.store(false, std::memory_order_release);
            printf("[builder] cancelled\n"); return;
        }

        if (!CtxOk(s.ctx)) {
            std::lock_guard<std::mutex> lk(s.mx);
            s.queue.clear(); s.qIdx = 0; s.ctx = {};
            s.placing.store(false, std::memory_order_release);
            printf("[builder] context gone\n"); return;
        }

        const bool fast = s.instant.load(std::memory_order_acquire);
        if (!fast) {
            DWORD now = GetTickCount();
            if (now - s.lastChunk.load(std::memory_order_relaxed) < kChunkMs) return;
        }

        void* thread = nullptr;
        if (domainGet && threadAttach) {
            void* domain = domainGet();
            if (domain) thread = threadAttach(domain);
        }

        Context ctx;
        { std::lock_guard<std::mutex> lk(s.mx); ctx = s.ctx; }

        const size_t limit = fast ? SIZE_MAX : kChunkSize;
        size_t did = 0; bool done = false;

        for (size_t i = 0; i < limit; i++) {
            QueueEntry qe;
            {
                std::lock_guard<std::mutex> lk(s.mx);
                if (s.qIdx >= s.queue.size()) { done = true; break; }
                qe = s.queue[s.qIdx++];
            }
            PlaceOne(ctx, qe.pos, qe.mat, qe.corners);
            did++;
        }

        if (!fast) s.lastChunk.store(GetTickCount(), std::memory_order_relaxed);

        if (did > 0 && handleDelta) {
            Context ctx2;
            { std::lock_guard<std::mutex> lk(s.mx); ctx2 = s.ctx; }
            if (ctx2.inst) handleDelta(ctx2.inst, nullptr);
        }

        if (thread && threadDetach) threadDetach(thread);

        if (done) {
            std::lock_guard<std::mutex> lk(s.mx);
            s.queue.clear(); s.qIdx = 0; s.ctx = {};
            s.placing.store(false, std::memory_order_release);
        }
    }

    static std::string JsonEsc(const std::string& s) {
        std::string o; o.reserve(s.size());
        for (char c : s) { if (c == '"') o += "\\\""; else if (c == '\\') o += "\\\\"; else o += c; }
        return o;
    }

    static std::string Serialize(const Model& m) {
        std::ostringstream o;
        o << "{\n  \"name\": \"" << JsonEsc(m.name) << "\",\n  \"voxels\": [\n";
        for (size_t i = 0; i < m.voxels.size(); i++) {
            auto& v = m.voxels[i];
            o << "    {\"x\":" << v.pos.x << ",\"y\":" << v.pos.y << ",\"z\":" << v.pos.z
                << ",\"mat\":" << (int)v.mat << ",\"corners\":[";
            for (int c = 0; c < 8; c++) { o << (int)v.corners[c]; if (c < 7) o << ","; }
            o << "]}"; if (i + 1 < m.voxels.size()) o << ","; o << "\n";
        }
        o << "  ]\n}"; return o.str();
    }

    static bool SaveToFile(const Model& m, const std::string& path) {
        std::ofstream f(path); if (!f.is_open()) return false;
        f << Serialize(m); return true;
    }

    static int ParseInt(const std::string& s, size_t& i) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) i++;
        bool neg = false; if (i < s.size() && s[i] == '-') { neg = true; i++; }
        int v = 0; while (i < s.size() && isdigit((unsigned char)s[i])) v = v * 10 + (s[i++] - '0');
        return neg ? -v : v;
    }

    // ---- Avatar Copy State ----
    struct AvatarLimbModel {
        std::string name;
        std::vector<SavedVoxel> voxels;
    };

    static std::vector<AvatarLimbModel> g_avatarLimbs;
    static char g_avatarJsonBuf[1024 * 256] = {};

    static bool ParseAvatarJson(const char* json) {
        g_avatarLimbs.clear();
        std::string s(json);
        static const char* limbKeys[] = {
            "Head","Torso","RArm","LArm","RUpLeg","RLowLeg","LUpLeg","LLowLeg"
        };
        for (const char* key : limbKeys) {
            std::string search = std::string("\"") + key + "\"";
            size_t kp = s.find(search);
            if (kp == std::string::npos) continue;
            size_t vp = s.find("\"voxels\"", kp);
            if (vp == std::string::npos) continue;
            size_t arrStart = s.find('[', vp);
            if (arrStart == std::string::npos) continue;
            int depth = 1;
            size_t arrEnd = arrStart + 1;
            while (arrEnd < s.size() && depth > 0) {
                if (s[arrEnd] == '[') depth++;
                else if (s[arrEnd] == ']') depth--;
                arrEnd++;
            }
            AvatarLimbModel limb;
            limb.name = key;
            size_t pos = arrStart + 1;
            while (pos < arrEnd) {
                size_t ob = s.find('{', pos);
                if (ob == std::string::npos || ob >= arrEnd) break;
                size_t cb = s.find('}', ob);
                if (cb == std::string::npos || cb >= arrEnd) break;
                std::string obj = s.substr(ob, cb - ob + 1);
                auto rf = [&](const char* k, int& val) {
                    size_t kpos = obj.find(k);
                    if (kpos == std::string::npos) return;
                    size_t col = obj.find(':', kpos);
                    if (col == std::string::npos) return;
                    col++;
                    val = ParseInt(obj, col);
                    };
                int x = 0, y = 0, z = 0, mat = 0;
                rf("\"x\"", x); rf("\"y\"", y); rf("\"z\"", z); rf("\"mat\"", mat);
                SavedVoxel sv{};
                sv.pos = Vec3i((int16_t)x, (int16_t)y, (int16_t)z);
                sv.mat = (uint8_t)mat;
                size_t ca = obj.find("\"corners\"");
                if (ca != std::string::npos) {
                    size_t a2 = obj.find('[', ca);
                    if (a2 != std::string::npos) {
                        a2++;
                        for (int c = 0; c < 8; c++) {
                            sv.corners[c] = (uint8_t)ParseInt(obj, a2);
                            size_t com = obj.find_first_of(",]", a2);
                            if (com == std::string::npos) break;
                            a2 = com + 1;
                        }
                    }
                }
                limb.voxels.push_back(sv);
                pos = cb + 1;
            }
            if (!limb.voxels.empty())
                g_avatarLimbs.push_back(limb);
        }
        return !g_avatarLimbs.empty();
    }

    static void* SafeGetCube(void* cm, Vec3i pos) {
        void* r = nullptr;
        __try { r = getCubeAt(cm, pos, nullptr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { r = nullptr; }
        return r;
    }

    static void* g_avatarBodyController = nullptr;

    static void* SafeCurrentBody(void* c) { void* r = nullptr; __try { r = fnCurrentBody(c, nullptr); } __except (EXCEPTION_EXECUTE_HANDLER) {} return r; }
    static void* SafeGetBodyPart(void* b, void* n) { void* r = nullptr; __try { r = fnGetBodyPart(b, n, nullptr); } __except (EXCEPTION_EXECUTE_HANDLER) {} return r; }
    static void SafeAddCubeDirect(void* inst, Vec3i pos, void* cube) { __try { fnAddCubeDirect(inst, pos, cube, nullptr); } __except (EXCEPTION_EXECUTE_HANDLER) {} }

    static void SafeRemoveCube(void* inst, Vec3i pos) {
        __try { pMVCubeModelBase_RemoveCube(inst, *reinterpret_cast<IntVector*>(&pos), nullptr); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void BuildAllLimbsOnBody() {
        if (!g_avatarLimbs.empty() && fnCurrentBody && fnGetBodyPart && fnAddCubeDirect
            && objNew && arrNew && cubeCtor) {
            g_avatarBuilding.store(true, std::memory_order_release);

            void* domain = domainGet ? domainGet() : nullptr;
            void* thread = (domain && threadAttach) ? threadAttach(domain) : nullptr;

            static auto strNew = (void* (*)(const char*))
                GetProcAddress(GetModuleHandleA("GameAssembly.dll"), "il2cpp_string_new");

            void* controller = g_avatarBodyController;
            if (!controller || !Readable(controller)) {
                printf("[builder] no body controller\n");
                g_avatarBuilding.store(false, std::memory_order_release);
                if (thread && threadDetach) threadDetach(thread);
                return;
            }

            void* mvBody = SafeCurrentBody(controller);
            if (!mvBody || !Readable(mvBody)) {
                printf("[builder] no mvbody\n");
                g_avatarBuilding.store(false, std::memory_order_release);
                if (thread && threadDetach) threadDetach(thread);
                return;
            }

            void* cubeClass = nullptr;
            void* byteArrClass = nullptr;
            {
                std::lock_guard<std::mutex> lk(s.mx);
                if (s.ctx.valid) {
                    cubeClass = s.ctx.cubeClass;
                    byteArrClass = s.ctx.byteArrClass;
                }
            }

            static const std::set<uint8_t> kNoBottomOverride = {
                1,3,4,6,9,11,13,14,15,16,17,18,19,21,24,29,31,32,33,34,
                37,38,40,41,42,43,47,48,49,50,51,52,54,55,56,57,58,60,
                61,62,63,64,65,66,67,68
            };

            for (auto& limb : g_avatarLimbs) {
                if (!strNew) break;
                void* partName = strNew(limb.name.c_str());
                if (!partName) continue;

                void* cubeModelInst = SafeGetBodyPart(mvBody, partName);

                if (!cubeModelInst || !Readable(cubeModelInst)) {
                    printf("[builder] GetBodyPart failed: %s\n", limb.name.c_str());
                    continue;
                }

                printf("[builder] building limb %s (%zu cubes)\n", limb.name.c_str(), limb.voxels.size());

                if (!cubeClass || !byteArrClass) {
                    for (auto& sv2 : limb.voxels) {
                        void* existingCube = SafeGetCube(cubeModelInst, sv2.pos);
                        if (!existingCube || !Readable(existingCube, 0x28)) continue;
                        cubeClass = reinterpret_cast<Il2Object*>(existingCube)->klass;
                        void* ma = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(existingCube) + 0x20);
                        if (ma && Readable(ma))
                            byteArrClass = reinterpret_cast<Il2Object*>(ma)->klass;
                        if (cubeClass && byteArrClass) break;
                    }
                }

                if (!cubeClass || !byteArrClass) {
                    constexpr int SCAN2 = 20;
                    for (int x = -SCAN2; x <= SCAN2 && (!cubeClass || !byteArrClass); x++)
                        for (int y = -SCAN2; y <= SCAN2 && (!cubeClass || !byteArrClass); y++)
                            for (int z = -SCAN2; z <= SCAN2 && (!cubeClass || !byteArrClass); z++) {
                                void* existingCube = SafeGetCube(cubeModelInst, Vec3i((int16_t)x, (int16_t)y, (int16_t)z));
                                if (!existingCube || !Readable(existingCube, 0x28)) continue;
                                cubeClass = reinterpret_cast<Il2Object*>(existingCube)->klass;
                                void* ma = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(existingCube) + 0x20);
                                if (ma && Readable(ma))
                                    byteArrClass = reinterpret_cast<Il2Object*>(ma)->klass;
                            }
                }

                Vec3i stragglerPos(0, 0, 0);
                bool hasStragglerPos = false;

                if (pMVCubeModelBase_RemoveCube) {
                    constexpr int SCAN = 20;
                    for (int x = -SCAN; x <= SCAN; x++)
                        for (int y = -SCAN; y <= SCAN; y++)
                            for (int z = -SCAN; z <= SCAN; z++) {
                                Vec3i p((int16_t)x, (int16_t)y, (int16_t)z);
                                if (SafeGetCube(cubeModelInst, p)) {
                                    if (!hasStragglerPos) { stragglerPos = p; hasStragglerPos = true; }
                                    else SafeRemoveCube(cubeModelInst, p);
                                }
                            }
                    if (handleDelta) handleDelta(cubeModelInst, nullptr);
                    Sleep(50);
                }

                if (!cubeClass || !byteArrClass) {
                    printf("[builder] couldn't get cube classes from limb %s\n", limb.name.c_str());
                    continue;
                }

                std::set<Vec3i> limbPosSet;
                for (auto& sv2 : limb.voxels) limbPosSet.insert(sv2.pos);

                static const Vec3i kFaceDir[6] = {
                    Vec3i(0,  1,  0),
                    Vec3i(0, -1,  0),
                    Vec3i(0,  0,  1),
                    Vec3i(0,  0, -1),
                    Vec3i(1,  0,  0),
                    Vec3i(-1,  0,  0),
                };

                for (auto& sv : limb.voxels) {
                    void* cArr = arrNew(byteArrClass, 8);
                    if (!cArr) continue;
                    uint8_t* cd = (uint8_t*)(reinterpret_cast<uintptr_t>(cArr) + 0x20);
                    for (int i = 0; i < 8; i++) cd[i] = sv.corners[i];

                    void* mArr = arrNew(byteArrClass, 6);
                    if (!mArr) continue;
                    uint8_t* md = (uint8_t*)(reinterpret_cast<uintptr_t>(mArr) + 0x20);

                    int overrideFace = 1;
                    if (kNoBottomOverride.find(sv.mat) == kNoBottomOverride.end()) {
                        if (limb.name == "Head") {
                            Vec3i above = sv.pos + kFaceDir[0];
                            if (limbPosSet.count(above)) {
                                for (auto& sv2 : limb.voxels) {
                                    if (sv2.pos == above) {
                                        if (sv2.corners == sv.corners)
                                            overrideFace = 0;
                                        break;
                                    }
                                }
                            }
                        }
                        else {
                            overrideFace = 1;
                        }
                    }

                    for (int i = 0; i < 6; i++) {
                        if (i == overrideFace && kNoBottomOverride.find(sv.mat) == kNoBottomOverride.end())
                            md[i] = 21;
                        else
                            md[i] = sv.mat;
                    }

                    void* cube = objNew(cubeClass);
                    if (!cube) continue;

                    cubeCtor(cube, cArr, mArr, nullptr);
                    SafeAddCubeDirect(cubeModelInst, sv.pos, cube);
                    s.placed.fetch_add(1, std::memory_order_relaxed);
                }

                if (handleDelta) handleDelta(cubeModelInst, nullptr);
                Sleep(50);
            }

            g_avatarBuilding.store(false, std::memory_order_release);
            if (thread && threadDetach) threadDetach(thread);
            printf("[builder] all limbs done\n");
        }
    }

    static bool LoadFromFile(const std::string& path, Model& out) {
        std::ifstream f(path); if (!f.is_open()) return false;
        std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        out.voxels.clear(); out.name = path;

        size_t np = s.find("\"name\"");
        if (np != std::string::npos) {
            size_t q1 = s.find('"', np + 7), q2 = (q1 != std::string::npos) ? s.find('"', q1 + 1) : std::string::npos;
            if (q2 != std::string::npos) out.name = s.substr(q1 + 1, q2 - q1 - 1);
        }
        size_t vp = s.find("\"voxels\""); if (vp == std::string::npos) return false;
        size_t arr = s.find('[', vp);      if (arr == std::string::npos) return false;
        size_t cur = arr + 1;

        while (cur < s.size()) {
            size_t ob = s.find('{', cur); if (ob == std::string::npos) break;
            size_t cb = s.find('}', ob);  if (cb == std::string::npos) break;
            std::string obj = s.substr(ob, cb - ob + 1);
            SavedVoxel sv{};
            auto rf = [&](const char* key, int& val) {
                size_t kp = obj.find(key); if (kp == std::string::npos) return;
                size_t col = obj.find(':', kp); if (col == std::string::npos) return;
                col++; val = ParseInt(obj, col);
                };
            int x = 0, y = 0, z = 0, mat = 0;
            rf("\"x\"", x); rf("\"y\"", y); rf("\"z\"", z); rf("\"mat\"", mat);
            sv.pos = Vec3i((int16_t)x, (int16_t)y, (int16_t)z); sv.mat = (uint8_t)mat;
            size_t ca = obj.find("\"corners\"");
            if (ca != std::string::npos) {
                size_t a2 = obj.find('[', ca);
                if (a2 != std::string::npos) {
                    a2++;
                    for (int c = 0; c < 8; c++) {
                        sv.corners[c] = (uint8_t)ParseInt(obj, a2);
                        size_t com = obj.find_first_of(",]", a2);
                        if (com == std::string::npos) break; a2 = com + 1;
                    }
                }
            }
            out.voxels.push_back(sv); cur = cb + 1;
        }
        return !out.voxels.empty();
    }

    static bool ReadCubeData(void* obj, Corners& outC, uint8_t& outMat) {
        if (!obj) return false;
        uintptr_t cb = reinterpret_cast<uintptr_t>(obj);
        void* bca = *reinterpret_cast<void**>(cb + 0x18);
        void* fma = *reinterpret_cast<void**>(cb + 0x20);
        if (!bca || !Readable(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(bca) + 0x20), 8)) return false;
        uint8_t* src = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(bca) + 0x20);
        for (int i = 0; i < 8; i++) outC[i] = src[i];
        outMat = 0;
        if (fma && Readable(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(fma) + 0x20)))
            outMat = *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(fma) + 0x20);
        return true;
    }

    static Model CaptureRegion(void* cm, Vec3i a, Vec3i b, const std::string& name) {
        Model m; m.name = name;
        if (!cm || !getCubeAt) return m;
        int x0 = min(a.x, b.x), x1 = max(a.x, b.x), y0 = min(a.y, b.y), y1 = max(a.y, b.y), z0 = min(a.z, b.z), z1 = max(a.z, b.z);
        for (int x = x0; x <= x1; x++) for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) {
            void* cb = SafeGetCube(cm, Vec3i((int16_t)x, (int16_t)y, (int16_t)z));
            if (!cb) continue;
            SavedVoxel sv; sv.pos = Vec3i((int16_t)(x - x0), (int16_t)(y - y0), (int16_t)(z - z0));
            if (!ReadCubeData(cb, sv.corners, sv.mat)) continue;
            m.voxels.push_back(sv);
        }
        return m;
    }

    static bool HandleCapture(void* inst, Vec3i pos, void* cubeModel) {
        if (g_capStep == CaptureStep::Idle) {
            g_capCornerA = pos; g_capModel = cubeModel;
            g_capStep = CaptureStep::WaitB;
            return true;
        }
        Model save = CaptureRegion(g_capModel, g_capCornerA, pos, std::string(g_capName));
        g_capStep = CaptureStep::Idle;
        if (save.voxels.empty()) { printf("[builder] nothing captured\n"); return true; }
        g_models.push_back(save);
        char dp[MAX_PATH] = {};
        GetModuleFileNameA(GetModuleHandleA("opium.dll"), dp, MAX_PATH);
        char* sl = strrchr(dp, '\\'); if (sl) *(sl + 1) = '\0';
        std::string dir = std::string(dp) + "builds\\"; CreateDirectoryA(dir.c_str(), nullptr);
        std::string fn = dir + std::string(g_capName) + ".json";
        if (SaveToFile(save, fn)) printf("[builder] saved to '%s'\n", fn.c_str());
        return true;
    }

    static void InitLoadPath() {
        if (g_loadPathInited) return;
        char dp[MAX_PATH] = {}; GetModuleFileNameA(GetModuleHandleA("opium.dll"), dp, MAX_PATH);
        char* sl = strrchr(dp, '\\'); if (sl) *(sl + 1) = '\0';
        std::string p = std::string(dp) + "builds\\MyBuild.json";
        strncpy_s(g_loadPath, p.c_str(), sizeof(g_loadPath) - 1);
        g_loadPathInited = true;
    }

    static std::string GetBuildsDir() {
        char dp[MAX_PATH] = {};
        GetModuleFileNameA(GetModuleHandleA("opium.dll"), dp, MAX_PATH);
        char* sl = strrchr(dp, '\\'); if (sl) *(sl + 1) = '\0';
        return std::string(dp) + "builds\\";
    }

    static void ScanBuildsFolder() {
        std::string dir = GetBuildsDir();
        CreateDirectoryA(dir.c_str(), nullptr);
        WIN32_FIND_DATAA fd{};
        HANDLE h = FindFirstFileA((dir + "*.json").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            std::string path = dir + fd.cFileName;
            std::string stem = fd.cFileName;
            stem = stem.substr(0, stem.rfind('.'));
            bool already = false;
            for (auto& m : g_models) if (m.name == stem) { already = true; break; }
            if (already) continue;
            Model tmp;
            if (LoadFromFile(path, tmp)) {
                tmp.name = stem;
                g_models.push_back(tmp);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }

    static Vec3i RotateY(Vec3i p, int rot) {
        switch (rot % 4) {
        case 1: return Vec3i(-p.z, p.y, p.x);
        case 2: return Vec3i(-p.x, p.y, -p.z);
        case 3: return Vec3i(p.z, p.y, -p.x);
        default: return p;
        }
    }

    void OnAddCube(void* inst, Vec3i pos, void* cube, void* method) {
        origAddCube(inst, pos, cube, method);

        if (t_placing) return;
        if (g_avatarBuilding.load(std::memory_order_acquire)) return;

        if (!s.enabled.load(std::memory_order_acquire)) return;

        if (!cube || !Readable(cube, 0x28)) return;

        if (g_captureMode) { HandleCapture(inst, pos, inst); return; }

        if (g_pendingPlace && g_pendingIdx >= 0 && g_pendingIdx < (int)g_models.size()) {
            g_pendingPlace = false;
            void* cubeClass = reinterpret_cast<Il2Object*>(cube)->klass;
            Corners caps = MakeCorners(kIdentity);
            void* bca = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(cube) + 0x18);
            if (bca && Readable(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(bca) + 0x20), 8)) {
                uint8_t* src = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(bca) + 0x20);
                for (int i = 0; i < 8; i++) caps[i] = src[i];
            }
            void* ma = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(cube) + 0x20);
            void* byteArrClass = nullptr;
            if (ma && Readable(ma)) byteArrClass = reinterpret_cast<Il2Object*>(ma)->klass;
            if (cubeClass && byteArrClass)
                BeginQueue(inst, cubeClass, byteArrClass, caps, pos, g_models[g_pendingIdx]);
            return;
        }

        uintptr_t cubeBase = reinterpret_cast<uintptr_t>(cube);
        void* cubeClass = reinterpret_cast<Il2Object*>(cube)->klass;
        if (!cubeClass) return;

        Corners caps{}; bool hasCaps = false;
        void* bca = *reinterpret_cast<void**>(cubeBase + 0x18);
        if (bca && Readable(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(bca) + 0x20), 8)) {
            uint8_t* src = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(bca) + 0x20);
            for (int i = 0; i < 8; i++) caps[i] = src[i]; hasCaps = true;
        }
        if (!hasCaps) { printf("[builder] couldn't read corners\n"); return; }

        uint8_t mat = 0;
        void* mats = *reinterpret_cast<void**>(cubeBase + 0x20);
        if (mats && Readable(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(mats) + 0x20)))
            mat = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(mats) + 0x20)[0];

        void* byteArrClass = nullptr;
        if (mats && Readable(mats))
            byteArrClass = reinterpret_cast<Il2Object*>(mats)->klass;
        if (!byteArrClass) return;

        int shape, sz;
        { std::lock_guard<std::mutex> lk(s.shapeMx); shape = s.shape; sz = s.size; }

        std::vector<Voxel> voxels;
        switch (shape) {
        case  0: voxels = MakeHollowCube(sz, mat);              break;
        case  1: voxels = MakeSphere(sz, mat);                  break;
        case  2: voxels = MakePlane(sz, mat);                   break;
        case  3: voxels = MakeCylinder(sz / 2, sz, mat);        break;
        case  4: voxels = MakePyramid(sz, mat);                 break;
        case  5: voxels = MakeDiamond(sz, mat);                 break;
        case  6: voxels = MakeTorus(sz, max(1, sz / 3), mat);   break;
        case  7: voxels = MakeArch(sz, sz, mat);                break;
        case  8: voxels = MakeDome(sz, mat);                    break;
        case  9: voxels = MakeStairs(sz, max(1, sz / 2), mat);  break;
        case 10: voxels = MakeCross(sz, mat);                   break;
        case 11: voxels = MakeRing(sz, mat);                    break;
        case 12: voxels = MakeTower(sz, sz * 2, mat);           break;
        case 13: voxels = MakeTrapCube(mat);                    break;
        case 14: voxels = MakePillar(mat);                      break;
        case 15: voxels = MakeWallPanel(mat);                   break;
        case 16: voxels = MakeCornerWall(mat);                  break;
        case 17: voxels = MakeDoorframe(mat);                   break;
        case 18: voxels = MakeWindowFrame(mat);                 break;
        default: break;
        }
        if (voxels.empty()) return;

        int lift = 0;
        switch (shape) {
        case 0: lift = sz / 2; break;
        case 1: lift = sz;     break;
        case 3: lift = sz / 2; break;
        case 5: lift = sz;     break;
        case 6: lift = max(1, sz / 3); break;
        }
        for (auto& v : voxels) v.pos.y += lift;

        int rot;
        { std::lock_guard<std::mutex> lk(s.shapeMx); rot = s.rotation; }
        for (auto& v : voxels) v.pos = RotateY(v.pos, rot);

        {
            std::lock_guard<std::mutex> lk(s.mx);
            if (s.placing.load(std::memory_order_relaxed)) return;
            s.seen.clear(); s.seen.insert(pos);
            s.placed.store(0, std::memory_order_relaxed);
            s.cancel.store(false, std::memory_order_relaxed);
            s.ctx = { inst, cubeClass, byteArrClass, caps, hasCaps };
            s.queue.clear(); s.qIdx = 0; s.queue.reserve(voxels.size());
            for (auto& v : voxels) s.queue.push_back({ pos + v.pos, v.mat, caps });
            s.instant.store(IsInstant(shape), std::memory_order_release);
            s.total.store((int)s.queue.size(), std::memory_order_relaxed);
            s.lastChunk.store(GetTickCount(), std::memory_order_relaxed);
            s.placing.store(true, std::memory_order_release);
        }
    }

    bool Initialize() {
        uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
        if (!base) { printf("[builder] no GameAssembly.dll\n"); return false; }
        HMODULE ga = GetModuleHandleA("GameAssembly.dll");

        cubeCtor = reinterpret_cast<FnCubeCtor>   (base + 0x17E2E70);
        getCubeAt = reinterpret_cast<FnGetCubeAt>  (base + Offsets::MVCubeModelBase::GetCubeBase);
        objNew = reinterpret_cast<FnObjNew>     (GetProcAddress(ga, "il2cpp_object_new"));
        arrNew = reinterpret_cast<FnArrNew>     (GetProcAddress(ga, "il2cpp_array_new"));
        threadAttach = reinterpret_cast<FnThreadAttach>(GetProcAddress(ga, "il2cpp_thread_attach"));
        threadDetach = reinterpret_cast<FnThreadDetach>(GetProcAddress(ga, "il2cpp_thread_detach"));
        domainGet = reinterpret_cast<FnDomainGet>   (GetProcAddress(ga, "il2cpp_domain_get"));

        if (!threadAttach || !threadDetach || !domainGet)
            printf("[builder] warning: thread functions not found\n");

        handleDelta = reinterpret_cast<FnHandleDelta>(base + Offsets::MVCubeModelBase::HandleDelta);

        if (!objNew || !arrNew) { printf("[builder] missing il2cpp exports\n"); return false; }
        if (!cubeCtor) { printf("[builder] missing cube ctor\n"); return false; }

        MH_STATUS st = MH_CreateHook(
            reinterpret_cast<void*>(base + Offsets::MVCubeModelBase::AddCube),
            reinterpret_cast<void*>(&OnAddCube),
            reinterpret_cast<void**>(&origAddCube));
        if (st != MH_OK) { printf("[builder] hook create failed: %d\n", st); return false; }

        st = MH_EnableHook(reinterpret_cast<void*>(base + Offsets::MVCubeModelBase::AddCube));
        if (st != MH_OK) { printf("[builder] hook enable failed: %d\n", st); return false; }

        printf("[builder] ready\n"); return true;
    }

    void Shutdown() {
        s.cancel.store(true, std::memory_order_release);
        DWORD dl = GetTickCount() + 1000;
        while (s.placing.load(std::memory_order_acquire)) {
            if (GetTickCount() > dl) { s.placing.store(false, std::memory_order_release); break; }
            Sleep(10);
        }
        MH_DisableHook(MH_ALL_HOOKS);
        printf("[builder] shutdown\n");
    }

    void RenderUI() {
        bool on = s.enabled.load(std::memory_order_relaxed);
        if (ToggleButton("##build", &on)) s.enabled.store(on, std::memory_order_release);
        ImGui::SameLine(); ImGui::Text("Autobuilder");
        if (!on) return;

        static bool scanned = false;
        if (!scanned) { ScanBuildsFolder(); scanned = true; }

        static int g_selectedBuild = -1;

        // progress bar always visible while placing
        if (s.placing.load(std::memory_order_acquire)) {
            size_t cur = 0, tot = 0;
            { std::lock_guard<std::mutex> lk(s.mx); cur = s.qIdx; tot = s.queue.size(); }
            if (tot > 0) {
                ImGui::ProgressBar((float)cur / (float)tot, ImVec2(-1, 0),
                    (std::to_string(cur) + "/" + std::to_string(tot)).c_str());
                if (ImGui::Button("Cancel", ImVec2(-1, 0)))
                    s.cancel.store(true, std::memory_order_release);
            }
        }
        else {
            int placed = s.placed.load(std::memory_order_relaxed);
            if (placed > 0) ImGui::TextDisabled("last: %d cubes", placed);
        }

        // ── Tabs ──────────────────────────────────────────────────────────
        if (ImGui::BeginTabBar("##builder_tabs")) {

            // ── [1] Shapes ────────────────────────────────────────────────
            if (ImGui::BeginTabItem("Shapes")) {
                const char* shapes[] = {
                    "Cube","Sphere","Plane","Cylinder","Pyramid","Diamond","Torus","Arch","Dome",
                    "Stairs","Cross","Ring","Tower","TrapCube","Pillar","Wall Panel","Corner Wall","Doorframe","Window Frame"
                };
                std::lock_guard<std::mutex> lk(s.shapeMx);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##shape", &s.shape, shapes, IM_ARRAYSIZE(shapes)))
                    s.instant.store(IsInstant(s.shape), std::memory_order_release);
                if (!IsInstant(s.shape)) {
                    ImGui::SetNextItemWidth(-1);
                    ImGui::SliderInt("##size", &s.size, 2, 20, "size: %d");
                }
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderInt("##rot", &s.rotation, 0, 3, "rot: %dx90");
                ImGui::EndTabItem();
            }

            // ── [2] Builds ────────────────────────────────────────────────
            if (ImGui::BeginTabItem("Builds")) {
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
                if (ImGui::SmallButton("Refresh")) ScanBuildsFolder();

                if (g_models.empty()) {
                    ImGui::TextDisabled("  none");
                }
                else {
                    for (int i = 0; i < (int)g_models.size(); i++) {
                        auto& m = g_models[i]; ImGui::PushID(i);
                        bool selected = (g_selectedBuild == i);
                        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                        if (ImGui::Button(m.name.c_str(), ImVec2(-1, 0))) g_selectedBuild = (selected ? -1 : i);
                        if (selected) ImGui::PopStyleColor();
                        ImGui::PopID();
                    }
                    if (g_selectedBuild >= 0 && g_selectedBuild < (int)g_models.size()) {
                        auto& m = g_models[g_selectedBuild];
                        ImGui::Separator();
                        ImGui::TextDisabled("%zu cubes", m.voxels.size());
                        float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.f;
                        if (ImGui::Button("Place", ImVec2(bw, 0))) { g_pendingPlace = true; g_pendingIdx = g_selectedBuild; }
                        ImGui::SameLine();
                        if (ImGui::Button("Save", ImVec2(bw, 0))) SaveToFile(m, GetBuildsDir() + m.name + ".json");
                        if (ImGui::Button("Delete", ImVec2(-1, 0))) {
                            g_models.erase(g_models.begin() + g_selectedBuild);
                            if (g_pendingIdx >= g_selectedBuild) g_pendingIdx--;
                            g_selectedBuild = -1;
                        }
                    }
                }
                ImGui::EndTabItem();
            }

            // ── [3] Capture ───────────────────────────────────────────────
            if (ImGui::BeginTabItem("Capture")) {
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##capname", g_capName, sizeof(g_capName));
                ToggleButton("##capmode", &g_captureMode); ImGui::SameLine();
                if (!g_captureMode) {
                    ImGui::TextDisabled("off");
                }
                else if (g_capStep == CaptureStep::Idle) {
                    ImGui::TextColored({ 0.4f,1,0.5f,1 }, "place A...");
                }
                else {
                    ImGui::TextColored({ 1,0.85f,0.2f,1 }, "A(%d,%d,%d) place B", g_capCornerA.x, g_capCornerA.y, g_capCornerA.z);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x")) { g_capStep = CaptureStep::Idle; g_captureMode = false; }
                }
                ImGui::Separator();
                ImGui::SetNextItemWidth(-56);
                ImGui::InputText("##loadpath", g_loadPath, sizeof(g_loadPath));
                ImGui::SameLine();
                if (ImGui::Button("Load", ImVec2(-1, 0))) {
                    Model tmp;
                    if (LoadFromFile(g_loadPath, tmp)) g_models.push_back(tmp);
                }
                ImGui::EndTabItem();
            }

            // ── [4] Avatar ────────────────────────────────────────────────
            if (ImGui::BeginTabItem("Avatar")) {
                ImGui::InputTextMultiline("##avatarjson", g_avatarJsonBuf, sizeof(g_avatarJsonBuf), ImVec2(-1, 48));
                if (ImGui::Button("Parse JSON", ImVec2(-1, 0))) {
                    if (ParseAvatarJson(g_avatarJsonBuf))
                        PushNotification("AVATAR", (std::to_string(g_avatarLimbs.size()) + " limbs loaded").c_str(), NotificationType::Info, 2.f);
                    else
                        PushNotification("AVATAR", "parse failed", NotificationType::Error, 2.f);
                }
                if (!g_avatarLimbs.empty()) {
                    ImGui::TextDisabled("%zu limbs ready", g_avatarLimbs.size());
                    if (ImGui::Button("Build All Limbs", ImVec2(-1, 0))) {
                        void* pThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());
                        try {
                            auto list = Unity::Object::FindObjectsOfType<Unity::CComponent>("AvatarEditModeBodyController");
                            if (list && list->m_uMaxLength > 0) g_avatarBodyController = list->operator[](0);
                        }
                        catch (...) {}
                        IL2CPP::Thread::Detach(pThread);
                        if (g_avatarBodyController)
                            CreateThread(nullptr, 0, [](LPVOID) -> DWORD { BuildAllLimbsOnBody(); return 0; }, nullptr, 0, nullptr);
                        else
                            PushNotification("AVATAR", "open avatar editor first", NotificationType::Error, 3.f);
                    }
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}