// level.cpp - room template loading + level stitching + tile rendering.
#include "level.hpp"
#include "tables.hpp"
#include "../core/logger.hpp"
#include "../core/paths.hpp"
#include "../render/renderer.hpp"
#include "../render/texture.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace game {

// ---------------- templates ----------------

struct RoomTemplate {
    std::wstring name, tags;
    int w = 0;                    // tiles wide
    std::vector<std::wstring> rows; // kRoomH rows of w chars
};

static constexpr int kRoomH = 22; // tiles tall

static bool loadTemplates(const std::wstring& dir, std::vector<RoomTemplate>& out) {
    // Enumerate *.txt via Win32 (MinGW <filesystem> ok too, but Win32 keeps it explicit).
    std::wstring pattern = core::joinPath(dir, L"*.txt");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        LOG_ERROR(L"level: no room templates in %s", dir.c_str());
        return false;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring path = core::joinPath(dir, fd.cFileName);
        std::ifstream f(path.c_str(), std::ios::binary);
        if (!f) { LOG_WARN(L"level: cannot open template %s", path.c_str()); continue; }
        std::stringstream ss; ss << f.rdbuf();
        // UTF-8 (BOM ok)
        std::string raw = ss.str();
        size_t i0 = 0;
        if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF) i0 = 3;
        int n = MultiByteToWideChar(CP_UTF8, 0, raw.c_str() + (int)i0, (int)(raw.size() - i0), nullptr, 0);
        std::wstring text(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, raw.c_str() + (int)i0, (int)(raw.size() - i0), text.data(), n);

        RoomTemplate t;
        std::vector<std::wstring> lines;
        size_t pos = 0;
        while (pos <= text.size()) {
            size_t nl = text.find(L'\n', pos);
            std::wstring line = (nl == std::wstring::npos) ? text.substr(pos) : text.substr(pos, nl - pos);
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            pos = (nl == std::wstring::npos) ? text.size() + 1 : nl + 1;
            if (line == L"----") { // body follows
                while (pos <= text.size()) {
                    size_t nl2 = text.find(L'\n', pos);
                    std::wstring l2 = (nl2 == std::wstring::npos) ? text.substr(pos) : text.substr(pos, nl2 - pos);
                    if (!l2.empty() && l2.back() == L'\r') l2.pop_back();
                    pos = (nl2 == std::wstring::npos) ? text.size() + 1 : nl2 + 1;
                    if (!l2.empty()) lines.push_back(l2);
                }
                break;
            }
            size_t eq = line.find(L'=');
            if (eq != std::wstring::npos) {
                std::wstring k = line.substr(0, eq), v = line.substr(eq + 1);
                if (k == L"name") t.name = v;
                else if (k == L"tags") t.tags = v;
                else if (k == L"width") t.w = _wtoi(v.c_str());
            }
        }
        t.rows = std::move(lines);
        if ((int)t.rows.size() != kRoomH || t.w <= 0) {
            LOG_WARN(L"level: template %s has %zu rows (need %d) w=%d, skipped",
                     t.name.c_str(), t.rows.size(), kRoomH, t.w);
            continue;
        }
        for (auto& r : t.rows)
            if ((int)r.size() != t.w) {
                LOG_WARN(L"level: template %s row width mismatch (%zu != %d), skipped", t.name.c_str(), r.size(), t.w);
                t.w = -1;
                break;
            }
        if (t.w > 0) out.push_back(std::move(t));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    LOG_INFO(L"level: loaded %zu room templates", out.size());
    return !out.empty();
}

static const RoomTemplate* pickTemplate(const std::vector<const RoomTemplate*>& pool, std::mt19937& rng) {
    if (pool.empty()) return nullptr;
    std::uniform_int_distribution<size_t> d(0, pool.size() - 1);
    return pool[d(rng)];
}

// ---------------- stamping ----------------

// Visual frame ids inside the (tinted) pixel-platformer tile atlas (20x9 grid, 18px, 1px gap).
namespace vis {
    constexpr int groundTop = 1;    // grass top
    constexpr int groundFill = 21;  // dirt
    constexpr int brickA = 12, brickB = 13; // red bricks -> tinted
    constexpr int platform = 48;    // wood platform
    constexpr int torch = 129;
    constexpr int sign = 84;
    constexpr int column = 136;     // tree trunk as pillar deco
}

static bool isMarker(wchar_t c) {
    switch (c) {
        case L'P': case L'E': case L'C': case L'H': case L'W': case L'X': case L'S': case L'B':
            return true;
    }
    return false;
}

bool generateLevel(const std::wstring& assetRoot, int levelIndex,
                   int roomsMin, int roomsMax, std::mt19937& rng, GeneratedLevel& out) {
    std::vector<RoomTemplate> all;
    if (!loadTemplates(core::joinPath(assetRoot, L"rooms"), all)) return false;

    std::vector<const RoomTemplate*> startP, combatP, platformP, bonusP, endP, bossP;
    for (auto& t : all) {
        auto has = [&](const wchar_t* tag) { return t.tags.find(tag) != std::wstring::npos; };
        if (has(L"start")) startP.push_back(&t);
        if (has(L"combat")) combatP.push_back(&t);
        if (has(L"platform")) platformP.push_back(&t);
        if (has(L"bonus")) bonusP.push_back(&t);
        if (has(L"end")) endP.push_back(&t);
        if (has(L"boss")) bossP.push_back(&t);
    }
    bool bossLevel = (levelIndex >= 3) && !bossP.empty();
    if (startP.empty() || combatP.empty() || endP.empty()) {
        LOG_FATAL(L"level: missing required template tags (start/combat/end)");
        return false;
    }

    std::uniform_int_distribution<int> roomCountD(roomsMin, roomsMax);
    int total = roomCountD(rng);

    // Build sequence of templates.
    std::vector<const RoomTemplate*> seq;
    seq.push_back(pickTemplate(startP, rng));
    int middle = total - 2; // minus start and end/boss
    int bonusAt = bonusP.empty() ? -1 : (int)(1 + rng() % (unsigned)std::max(1, middle));
    for (int i = 0; i < middle; ++i) {
        if (i == bonusAt) { seq.push_back(pickTemplate(bonusP, rng)); continue; }
        // mix combat & platform rooms 65/35
        bool useCombat = platformP.empty() || (rng() % 100) < 65;
        seq.push_back(pickTemplate(useCombat ? combatP : platformP, rng));
    }
    seq.push_back(bossLevel ? pickTemplate(bossP, rng) : pickTemplate(endP, rng));

    // Total width.
    int totalW = 2; // left boundary
    for (auto* t : seq) totalW += t->w;
    totalW += 2;    // right boundary

    out = GeneratedLevel{};
    out.level = levelIndex;
    out.map.resize(totalW, kRoomH);

    std::uniform_int_distribution<int> coinVar(0, 2);
    std::uniform_int_distribution<int> mirrorD(0, 1);

    int x0 = 2;
    int roomIdx = 0;
    for (auto* t : seq) {
        bool mirror = mirrorD(rng) == 1 && t->tags.find(L"boss") == std::wstring::npos;
        RoomInfo ri; ri.tileX0 = x0; ri.tileW = t->w; ri.tags = t->tags;
        out.rooms.push_back(ri);

        for (int ry = 0; ry < kRoomH; ++ry) {
            for (int rx = 0; rx < t->w; ++rx) {
                int sx = mirror ? (t->w - 1 - rx) : rx;
                wchar_t c = t->rows[ry][sx];
                int gx = x0 + rx, gy = ry;
                if (c == L'#') {
                    // top edge if tile above is empty in template
                    wchar_t above = (ry > 0) ? t->rows[ry - 1][sx] : L'#';
                    int v = (above == L'.' || isMarker(above))
                            ? vis::groundTop
                            : ((rng() % 4 == 0) ? vis::brickA + (int)(rng() % 2) : vis::groundFill);
                    out.map.set(gx, gy, SSolid, v);
                } else if (c == L'=') {
                    out.map.set(gx, gy, SPlatform, vis::platform);
                } else if (isMarker(c)) {
                    SpawnPoint sp;
                    sp.x = (gx + 0.5f) * kTilePx;
                    sp.y = (gy + 1.0f) * kTilePx; // bottom of the tile
                    sp.roomIndex = roomIdx;
                    switch (c) {
                        case L'P': sp.kind = SpawnPoint::PlayerStart; break;
                        case L'E': sp.kind = SpawnPoint::Enemy; break;
                        case L'C': sp.kind = SpawnPoint::CoinLine; break;
                        case L'H': sp.kind = SpawnPoint::Potion; break;
                        case L'W': sp.kind = SpawnPoint::WeaponRack; break;
                        case L'X': sp.kind = SpawnPoint::Portal; break;
                        case L'S': sp.kind = SpawnPoint::Saw; break;
                        case L'B': sp.kind = SpawnPoint::BossArena; break;
                    }
                    out.spawns.push_back(sp);
                }
            }
        }

        if (t->tags.find(L"boss") != std::wstring::npos) {
            out.hasBoss = true;
            out.bossArena = Rect{x0 * kTilePx, 0, t->w * kTilePx, kRoomH * kTilePx};
        }
        x0 += t->w;
        ++roomIdx;
    }

    // Boundary walls: two solid columns on each side, full height.
    for (int gy = 0; gy < kRoomH; ++gy) {
        for (int bx = 0; bx < 2; ++bx) {
            out.map.set(bx, gy, SSolid, vis::brickA);
            out.map.set(totalW - 1 - bx, gy, SSolid, vis::brickA);
        }
    }
    // Safety floor across the whole map just in case (row kRoomH-1 already solid in templates;
    // we do NOT add a global floor: pits are intentional in platform rooms. Falling out of the
    // world is handled by gameplay as pit damage.)

    (void)coinVar;
    LOG_INFO(L"level: generated level %d with %zu rooms, %d x %d tiles, %zu spawns",
             levelIndex, out.rooms.size(), totalW, kRoomH, out.spawns.size());
    return true;
}

// ---------------- TileMap impl ----------------

void TileMap::resize(int w_, int h_) {
    w = w_; h = h_;
    solid.assign((size_t)w * h, SEmpty);
    visual.assign((size_t)w * h, -1);
    visualBg.assign((size_t)w * h, -1);
}
uint8_t TileMap::solidAt(int tx, int ty) const {
    if (tx < 0 || tx >= w) return SSolid; // side walls
    if (ty < 0) return SEmpty;
    if (ty >= h) return SEmpty;           // pits
    return solid[(size_t)ty * w + tx];
}
int TileMap::visualAt(int tx, int ty) const {
    if (tx < 0 || tx >= w || ty < 0 || ty >= h) return -1;
    return visual[(size_t)ty * w + tx];
}
void TileMap::set(int tx, int ty, uint8_t s, int vis) {
    if (tx < 0 || tx >= w || ty < 0 || ty >= h) return;
    solid[(size_t)ty * w + tx] = s;
    visual[(size_t)ty * w + tx] = vis;
}

// ---------------- tile rendering ----------------

void drawTiles(render::Renderer& r, render::TextureStore& store, const TileMap& map,
               const std::wstring& atlasPath, float camX, float camY, float tintMul) {
    render::Texture* tex = store.get(r, atlasPath);
    if (!tex || !tex->bmp()) return;
    // packed atlas: 20 cols x 9 rows of 18px, no spacing
    const int cols = 20, src = 18, gap = 0;

    int tx0 = std::max(0, (int)(camX / kTilePx) - 1);
    int tx1 = std::min(map.w - 1, (int)((camX + render::kLogicalW) / kTilePx) + 1);
    int ty0 = std::max(0, (int)(camY / kTilePx) - 1);
    int ty1 = std::min(map.h - 1, (int)((camY + render::kLogicalH) / kTilePx) + 1);
    if (tintMul <= 0) tintMul = 1;

    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            int v = map.visual[(size_t)ty * map.w + tx];
            if (v < 0) continue;
            int sx = (v % cols) * (src + gap), sy = (v / cols) * (src + gap);
            r.drawBitmap(tex->bmp(), tx * kTilePx, ty * kTilePx, kTilePx, kTilePx,
                         (float)sx, (float)sy, (float)src, (float)src, tintMul);
        }
    }
}

} // namespace game
