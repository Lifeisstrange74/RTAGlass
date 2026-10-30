#include "field_world.h"

#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <vector>

#include <windows.h>

#include "../../gfx/textures/texture_set.h"
#include "../race/course.h"
#include "../save/save_data.h"

namespace game {

namespace {

const float kFixed = 20971.52f;          // D_00332C20: fixed point per unit
const int32_t kWorldMask = 0x0FFFFFFF;   // eight blocks each way
const float kBlock = 1600.0f;
const float kNoCeiling = 10000.0f;
// A block is read once the car comes this near it (its nearest edge): the view's reach (ViewReach, about 1000) and
// the way a fast car covers while the block is read and built.
const float kLoadReach = 1400.0f;
// It's let go only once the car is this far, so a car going to and fro over an edge doesn't have it read again and
// again.
const float kKeepReach = 2000.0f;
const int kMaxThreads = 3;

const gfx::Rect kFrame = {0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight};

// A fixed-point difference the nearer way round the world, in units.
float Wrapped(int32_t d) {
    d &= kWorldMask;
    if (d >= 0x08000000) d -= 0x10000000;
    return d / kFixed;
}

// Nothing shows past the distance fade (alpha 0 at a view depth of `end`). A point in the frame is at least the cosine
// of its angle off the view axis of its distance deep, so nothing further from the eye than `end` over that at the
// frame's corner can show; the PC leaves it out (drawing all of every block near was 1500 draws a frame).
float ViewReach(const gfx::Camera3D& c, float end) {
    float tx = std::max(fabsf(kFrame.x0 - c.centerX), fabsf(kFrame.x1 - c.centerX)) / c.focalX;
    float ty = std::max(fabsf(kFrame.y0 - c.centerY), fabsf(kFrame.y1 - c.centerY)) / c.focalY;
    return end * sqrtf(1 + tx * tx + ty * ty);
}

// The squared distance from the eye to the nearest point of the box (lo, hi) moved by (dx, 0, dz).
float BoxDistance2(const float* eye, const float* lo, const float* hi, float dx, float dz) {
    float o[3] = {dx, 0, dz}, d2 = 0;
    for (int k = 0; k < 3; ++k) {
        float d = std::max(0.0f, std::max(lo[k] + o[k] - eye[k], eye[k] - (hi[k] + o[k])));
        d2 += d * d;
    }
    return d2;
}

// How far the fixed-point position is from the block (0 inside it), the nearer way round the world, in units.
float BlockDistance(int block, int32_t x, int32_t z) {
    int32_t ox, oz;
    FieldWorld::Field_BlockOrigin(block, &ox, &oz);
    float dx = std::max(0.0f, fabsf(Wrapped(x - ox - FieldWorld::kBlockFixed / 2)) - kBlock / 2);
    float dz = std::max(0.0f, fabsf(Wrapped(z - oz - FieldWorld::kBlockFixed / 2)) - kBlock / 2);
    return sqrtf(dx * dx + dz * dz);
}

}  // namespace

int FieldWorld::Field_BlockAt(int32_t x, int32_t z) {
    int row = (z >> 25) & 7, odd = row & 1;
    int column = (((x >> 24) - odd) >> 1) & 7;
    return (row >> 1) * 16 + column * 2 + odd;
}

void FieldWorld::Field_BlockOrigin(int block, int32_t* x, int32_t* z) {
    int row = ((block >> 4) << 1) | (block & 1), column = (block >> 1) & 7;
    *x = column * kBlockFixed + (row & 1) * (kBlockFixed / 2);
    *z = row * kBlockFixed;
}

std::string FieldWorld::Field_BlockFile(int block) {
    char name[4] = {(char)('0' + ((block >> 4) & 3)), (char)('0' + ((block >> 2) & 3)),
                    (char)('0' + (((block & 2) >> 1) | ((block & 1) << 1))), 0};
    return name;
}

void FieldWorld::Field_BlockOffset(int from, int to, float* dx, float* dz) {
    int32_t fx, fz, tx, tz;
    Field_BlockOrigin(from, &fx, &fz);
    Field_BlockOrigin(to, &tx, &tz);
    *dx = Wrapped(tx - fx);
    *dz = Wrapped(tz - fz);
}

// The loading threads: each reads the nearest block asked of them (FieldWorld::Field_Read) and hands it back.
struct FieldWorld::Loader {
    const FieldWorld* world;
    std::vector<HANDLE> threads;
    HANDLE work = nullptr;   // set while `asked` has blocks
    CRITICAL_SECTION lock;
    std::map<int, float> asked;   // waiting, by their distance from the car
    std::vector<std::unique_ptr<Read>> done;
    std::map<std::string, bool> translucent;   // the pictures read so far, and which have see-through texels
    bool quit = false;

    static DWORD WINAPI Run(void* self) {
        Loader& l = *static_cast<Loader*>(self);
        for (;;) {
            WaitForSingleObject(l.work, INFINITE);
            EnterCriticalSection(&l.lock);
            if (l.quit) {
                LeaveCriticalSection(&l.lock);
                return 0;
            }
            int block = l.Nearest();
            if (block < 0) {
                ResetEvent(l.work);
                LeaveCriticalSection(&l.lock);
                continue;
            }
            LeaveCriticalSection(&l.lock);
            std::unique_ptr<Read> r = l.world->Field_Read(block);
            EnterCriticalSection(&l.lock);
            l.done.push_back(std::move(r));
            LeaveCriticalSection(&l.lock);
        }
    }

    // Takes the nearest waiting block off `asked` (under the lock); -1 if none.
    int Nearest() {
        auto best = asked.end();
        for (auto it = asked.begin(); it != asked.end(); ++it)
            if (best == asked.end() || it->second < best->second) best = it;
        if (best == asked.end()) return -1;
        int block = best->first;
        asked.erase(best);
        return block;
    }
};

FieldWorld::FieldWorld(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), dataDir_(dataDir), loader_(new Loader) {
    for (float& d : distance_) d = 1e30f;
    loader_->world = this;
    InitializeCriticalSection(&loader_->lock);
    loader_->work = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    // A thread a processor, the main thread's aside: reading a block is most of a processor's work for a while.
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    int threads = std::min(std::max((int)info.dwNumberOfProcessors - 1, 1), kMaxThreads);
    for (int i = 0; i < threads; ++i) {
        HANDLE t = CreateThread(nullptr, 0, &Loader::Run, loader_.get(), 0, nullptr);
        if (!t) continue;
        SetThreadPriority(t, THREAD_PRIORITY_BELOW_NORMAL);
        loader_->threads.push_back(t);
    }
}

FieldWorld::~FieldWorld() {
    EnterCriticalSection(&loader_->lock);
    loader_->quit = true;
    SetEvent(loader_->work);
    LeaveCriticalSection(&loader_->lock);
    for (HANDLE t : loader_->threads) {
        WaitForSingleObject(t, INFINITE);
        CloseHandle(t);
    }
    CloseHandle(loader_->work);
    DeleteCriticalSection(&loader_->lock);
}

void FieldWorld::Field_LoadSky() {
    if (sky_.Empty()) sky_.Load(dataDir_ + "\\models\\FLD\\sky.obj", d_);
    if (clouds_.Empty()) clouds_.Load(dataDir_ + "\\models\\COURSE\\C20\\clouds.obj", d_);
    if (horizon_.Empty()) horizon_.Load(dataDir_ + "\\models\\COURSE\\C20\\horizon.obj", d_);
}

void FieldWorld::Field_SetCourse(int course) {
    course_ = course;
    if (course < 0 || course == courseLoaded_) return;
    // Loader_LoadCourse (job 0x35) into slot 2: the course's blocks, sky and PMP; the field's clouds ring.
    courseWorld_.Load(d_, dataDir_, course);
    courseWorld_.Field_LoadClouds(d_, dataDir_);
    if (!courseGround_.Load(dataDir_ + "\\collision\\" + Course::File(course) + ".pmp"))
        fprintf(stderr, "no collision data for course %d\n", course);
    courseLoaded_ = course;
}

std::unique_ptr<FieldWorld::Read> FieldWorld::Field_Read(int block) const {
    std::unique_ptr<Read> r(new Read);
    r->block = block;
    std::string file = Field_BlockFile(block);
    std::string dir = dataDir_ + "\\models\\FLD\\" + file + "\\";
    // The cells, block 64, the objects, then block 11's tables and their blocks 64.
    const int kCity = kCells + 1 + kObjects;
    std::vector<gfx::Mesh> meshes(kCity + (block == kCityBlock ? 2 * kCityTables : 0));
    std::set<std::string> paths;
    for (int c = 0; c < (int)meshes.size(); ++c) {
        char name[24];
        if (c <= kCells) snprintf(name, sizeof name, "%02d.obj", c);
        else if (c < kCity) snprintf(name, sizeof name, "obj%d.obj", c - kCells - 1);
        else if (c < kCity + kCityTables) snprintf(name, sizeof name, "city_%02d.obj", c - kCity);
        else snprintf(name, sizeof name, "city_%02d_64.obj", c - kCity - kCityTables);
        gfx::LoadObj(dir + name, meshes[c]);
        for (const gfx::MeshPart& p : meshes[c].parts)
            if (!p.texture.empty()) paths.insert(p.texture);
    }
    // The pictures no block has read yet: their pixels go to the main thread, and whether they have see-through
    // texels decides the models' drawing order (gfx::Model::Prepare).
    std::map<std::string, bool> seen;
    EnterCriticalSection(&loader_->lock);
    for (const std::string& path : paths) {
        auto it = loader_->translucent.find(path);
        if (it != loader_->translucent.end()) seen[path] = it->second;
    }
    LeaveCriticalSection(&loader_->lock);
    for (const std::string& path : paths) {
        if (seen.count(path)) continue;
        Picture p = {path, {}, 0, 0};
        bool read = gfx::LoadDds(path, p.pixels, p.width, p.height);
        seen[path] = read && gfx::PixelsTranslucent(p.pixels);
        if (read) r->pictures.push_back(std::move(p));
    }
    EnterCriticalSection(&loader_->lock);
    for (const Picture& p : r->pictures) loader_->translucent[p.path] = seen[p.path];
    LeaveCriticalSection(&loader_->lock);
    auto translucent = [&](const std::string& path) {
        auto it = seen.find(path);
        return it != seen.end() && it->second;
    };
    for (int c = 0; c < kCells; ++c) {
        // A cell with nothing (or broken faces) is an empty section, so that sections stay cells.
        if (meshes[c].indices.empty() || !gfx::Model::Prepare(meshes[c], translucent, r->cells)) {
            gfx::PreparedModel::Section none = {(uint32_t)r->cells.parts.size(), 0, {0, 0, 0}, {0, 0, 0}};
            r->cells.sections.push_back(none);
        }
        meshes[c] = gfx::Mesh();
    }
    // The cells' parts by texture across the block, so the cells in view that share a picture draw as one.
    gfx::Model::MergeSections(r->cells);
    if (!meshes[kCells].indices.empty()) gfx::Model::Prepare(meshes[kCells], translucent, r->last);
    for (int n = 0; n < kObjects; ++n) r->objects[n] = std::move(meshes[kCells + 1 + n]);
    if (block == kCityBlock) {
        // One section a table (an empty one where the table has nothing), merged as the cells are.
        for (int i = 0; i < 2 * kCityTables; ++i) {
            gfx::PreparedModel& to = i < kCityTables ? r->city : r->cityLast;
            gfx::Mesh& m = meshes[kCity + i];
            if (m.indices.empty() || !gfx::Model::Prepare(m, translucent, to)) {
                gfx::PreparedModel::Section none = {(uint32_t)to.parts.size(), 0, {0, 0, 0}, {0, 0, 0}};
                to.sections.push_back(none);
            }
            m = gfx::Mesh();
        }
        gfx::Model::MergeSections(r->city);
        gfx::Model::MergeSections(r->cityLast);
        r->cityGround.resize(kResidents);
        for (int k = 0; k < kResidents; ++k) {
            char name[24];
            snprintf(name, sizeof name, "\\city_%02d.pmp", k);
            if (!r->cityGround[k].Load(dataDir_ + "\\collision\\FLD\\" + file + name))
                fprintf(stderr, "no collision data for My City's building %d\n", k);
        }
    }
    if (!r->ground.Load(dataDir_ + "\\collision\\FLD\\" + file + ".pmp"))
        fprintf(stderr, "no collision data for world block %s\n", file.c_str());
    gfx::Mesh map;   // blocks 112 and 201 (the sea) have none
    if (gfx::LoadObj(dir + "map.obj", map)) r->map.Load(map);
    return r;
}

bool FieldWorld::Field_Build(Building& b, int steps) {
    int pictures = (int)b.read->pictures.size();
    for (; b.next < pictures + 2 && steps > 0; ++b.next, --steps) {
        if (b.next < pictures) {
            Picture& p = b.read->pictures[b.next];
            d_.GetFile(p.path, p.pixels, p.width, p.height);
            std::vector<uint32_t>().swap(p.pixels);
        } else if (b.next == pictures) {
            b.block->cells.Load(std::move(b.read->cells), d_);
        } else {
            b.block->last.Load(std::move(b.read->last), d_);
            for (int n = 0; n < kObjects; ++n)
                if (!b.read->objects[n].indices.empty()) b.block->objects[n].Load(std::move(b.read->objects[n]), d_);
            if (!b.read->city.sections.empty()) {
                b.block->city.Load(std::move(b.read->city), d_);
                b.block->cityLast.Load(std::move(b.read->cityLast), d_);
            }
        }
    }
    if (b.next < pictures + 2) return false;
    b.block->ground = std::move(b.read->ground);
    b.block->cityGround = std::move(b.read->cityGround);
    b.block->map = std::move(b.read->map);
    return true;
}

void FieldWorld::Field_Forget(const Building& b) {
    EnterCriticalSection(&loader_->lock);
    for (size_t i = (size_t)b.next; i < b.read->pictures.size(); ++i)
        loader_->translucent.erase(b.read->pictures[i].path);
    LeaveCriticalSection(&loader_->lock);
}

void FieldWorld::Field_TakeReads() {
    std::vector<std::unique_ptr<Read>> done;
    EnterCriticalSection(&loader_->lock);
    done.swap(loader_->done);
    LeaveCriticalSection(&loader_->lock);
    for (std::unique_ptr<Read>& r : done) {
        int w = r->block;
        asked_.erase(w);
        Building fresh = {std::move(r), std::unique_ptr<Block>(new Block), 0};
        if (distance_[w] >= kKeepReach || blocks_.count(w) || building_.count(w)) {
            Field_Forget(fresh);   // let go of meanwhile
            continue;
        }
        building_.emplace(w, std::move(fresh));
    }
}

void FieldWorld::Field_LoadNow(int w) {
    while (!blocks_.count(w)) {
        Field_TakeReads();
        auto b = building_.find(w);
        if (b != building_.end()) {
            Field_Build(b->second, 1 << 30);
            blocks_[w] = std::move(b->second.block);
            building_.erase(b);
            return;
        }
        // Still waiting for a thread: read it here unless one has started on it.
        EnterCriticalSection(&loader_->lock);
        bool waiting = loader_->asked.erase(w) != 0;
        LeaveCriticalSection(&loader_->lock);
        if (waiting || !asked_.count(w)) {
            asked_.erase(w);
            Building fresh = {Field_Read(w), std::unique_ptr<Block>(new Block), 0};
            building_.emplace(w, std::move(fresh));
        } else {
            Sleep(1);
        }
    }
}

void FieldWorld::Field_LoadAround(int32_t x, int32_t z, Wait wait) {
    if (OnCourse()) return;   // the course is all in
    for (int b = 0; b < kBlocks; ++b) distance_[b] = BlockDistance(b, x, z);
    for (auto it = blocks_.begin(); it != blocks_.end();)
        it = distance_[it->first] < kKeepReach ? std::next(it) : blocks_.erase(it);
    for (auto it = building_.begin(); it != building_.end();) {
        if (distance_[it->first] < kKeepReach) {
            ++it;
        } else {
            Field_Forget(it->second);
            it = building_.erase(it);
        }
    }
    // The threads' queue: what's near and not in goes on it, what's far comes off, the rest by its new distance.
    EnterCriticalSection(&loader_->lock);
    for (auto it = loader_->asked.begin(); it != loader_->asked.end();) {
        if (distance_[it->first] < kKeepReach) {
            it->second = distance_[it->first];
            ++it;
        } else {
            asked_.erase(it->first);
            it = loader_->asked.erase(it);
        }
    }
    for (int b = 0; b < kBlocks; ++b) {
        if (distance_[b] >= kLoadReach || blocks_.count(b) || building_.count(b) || asked_.count(b)) continue;
        loader_->asked[b] = distance_[b];
        asked_.insert(b);
    }
    if (!loader_->asked.empty()) SetEvent(loader_->work);
    LeaveCriticalSection(&loader_->lock);
    // The block the point is in is needed now (all of them behind the loading screen), the main thread helping
    // with the reading.
    int at = Field_BlockAt(x, z);
    if (wait == kWaitHere && !blocks_.count(at)) Field_LoadNow(at);
    if (wait != kWaitAll) return;
    std::vector<int> needed;
    for (int b = 0; b < kBlocks; ++b)
        if (distance_[b] < kLoadReach) needed.push_back(b);
    std::sort(needed.begin(), needed.end(), [&](int a, int b) { return distance_[a] < distance_[b]; });
    for (int w : needed) Field_LoadNow(w);
}

void FieldWorld::Field_Stream(double budgetMs) {
    if (OnCourse()) return;
    Field_TakeReads();
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    while (!building_.empty()) {
        // The nearest first.
        auto it = building_.begin();
        for (auto b = building_.begin(); b != building_.end(); ++b)
            if (distance_[b->first] < distance_[it->first]) it = b;
        for (;;) {
            QueryPerformanceCounter(&now);
            if ((now.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart >= budgetMs) return;
            if (Field_Build(it->second, 1)) break;
        }
        blocks_[it->first] = std::move(it->second.block);
        building_.erase(it);
    }
}

// func_00220938: Sky_Draw, tinted by the time of day's background colour (Light_SkyTint, course -1), and
// Sky_DrawRings: the clouds and stars by the night weight, the band below the horizon by the day's; then the blocks (func_0021FB38, each offset from the car's block by D_0029D080), the
// cars, and each block's block 64 (func_00220030).
void FieldWorld::Field_DrawView(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light, int block,
                                const std::function<void(float)>& cars) const {
    gfx::DrawSourceScope source(d.GetRenderer(), gfx::DrawSource::World);
    if (OnCourse()) {
        float reach = ViewReach(camera, Course_WorldStyle(light).end);
        courseWorld_.Cloud_DrawView(d, camera, light, [&]() {
            if (cars) cars(reach);
        });
        return;
    }
    gfx::DrawStyle sky;
    sky.offset[0] = camera.eye[0];
    sky.offset[2] = camera.eye[2];
    sky.state.depthTest = false;
    sky.state.depthWrite = false;
    Light_SkyTint(light, sky.tint);
    sky_.Draw(d, camera, kFrame, sky);
    sky.tint[0] = sky.tint[1] = sky.tint[2] = 1;
    if (light.weights[2] > 0) {
        sky.alpha = light.weights[2];
        clouds_.Draw(d, camera, kFrame, sky);
    }
    if (light.weights[0] > 0) {
        sky.alpha = light.weights[0];
        horizon_.Draw(d, camera, kFrame, sky);
    }
    gfx::DrawStyle world = Course_WorldStyle(light);
    float reach = ViewReach(camera, world.end), reach2 = reach * reach;
    struct Placed {
        const Block* block;
        float dx, dz;
    };
    std::vector<Placed> placed;
    for (const auto& b : blocks_) {
        Placed p = {b.second.get(), 0, 0};
        Field_BlockOffset(block, b.first, &p.dx, &p.dz);
        float ex = std::max(0.0f, std::max(p.dx - camera.eye[0], camera.eye[0] - (p.dx + kBlock)));
        float ez = std::max(0.0f, std::max(p.dz - camera.eye[2], camera.eye[2] - (p.dz + kBlock)));
        if (ex * ex + ez * ez > reach2) continue;
        placed.push_back(p);
    }
    for (const Placed& p : placed) {
        world.offset[0] = p.dx;
        world.offset[2] = p.dz;
        bool wanted[kCells];
        for (int c = 0; c < kCells; ++c) {
            float lo[3], hi[3];
            wanted[c] = p.block->cells.Bounds(c, lo, hi) && BoxDistance2(camera.eye, lo, hi, p.dx, p.dz) <= reach2;
        }
        p.block->cells.DrawSections(d, camera, kFrame, world, wanted);
        // Field_DrawBlocks (0x0021FEEC): after block 11's cells, each resident's lot or building.
        if (!p.block->city.Empty()) Field_DrawCity(d, camera, world, p.block->city, p.dx, p.dz, reach2);
    }
    if (cars) cars(reach);
    for (const Placed& p : placed) {
        world.offset[0] = p.dx;
        world.offset[2] = p.dz;
        // Field_DrawBlocks64 (0x0022013C): the same tables' blocks 64, not culled.
        if (!p.block->cityLast.Empty()) Field_DrawCity(d, camera, world, p.block->cityLast, p.dx, p.dz, reach2);
        float lo[3], hi[3];
        if (!p.block->last.Bounds(-1, lo, hi) || BoxDistance2(camera.eye, lo, hi, p.dx, p.dz) > reach2) continue;
        p.block->last.Draw(d, camera, kFrame, world);
    }
}

void FieldWorld::Field_CityTables(bool* wanted) {
    for (int k = 0; k < kResidents; ++k) {
        bool in = Resident_MovedIn(k);
        wanted[2 * k] = !in;
        wanted[2 * k + 1] = in;
    }
}

void FieldWorld::Field_DrawCity(gfx::Draw2D& d, const gfx::Camera3D& camera, const gfx::DrawStyle& style,
                                const gfx::Model& model, float dx, float dz, float reach2) {
    bool wanted[kCityTables];
    Field_CityTables(wanted);
    for (int i = 0; i < kCityTables; ++i) {
        float lo[3], hi[3];
        wanted[i] = wanted[i] && model.Bounds(i, lo, hi) && BoxDistance2(camera.eye, lo, hi, dx, dz) <= reach2;
    }
    model.DrawSections(d, camera, kFrame, style, wanted, true);
}

const gfx::Model* FieldWorld::Field_Object(int block, int n) const {
    auto it = blocks_.find(block);
    if (it == blocks_.end() || n < 0 || n >= kObjects || it->second->objects[n].Empty()) return nullptr;
    return &it->second->objects[n];
}

// func_00207280 steps once into the block past the edge the point is beyond (the rows above and below are half a
// block along); the world's fixed point does the same for any point.
int32_t FieldWorld::Field_Ground(int block, float* p, float* ceiling) const {
    if (OnCourse()) return courseGround_.Collision_Ground(p, ceiling);
    int32_t ox, oz;
    Field_BlockOrigin(block, &ox, &oz);
    int32_t x = ox + (int32_t)floorf(p[0] * kFixed), z = oz + (int32_t)floorf(p[2] * kFixed);
    int to = Field_BlockAt(x, z);
    auto it = blocks_.find(to);
    if (it == blocks_.end()) {
        *ceiling = kNoCeiling;
        return -1;
    }
    int32_t tx, tz;
    Field_BlockOrigin(to, &tx, &tz);
    float q[4] = {((x - tx) & kWorldMask) / kFixed, p[1], ((z - tz) & kWorldMask) / kFixed, p[3]};
    const Collision* list[1 + kResidents];
    int n = Field_GroundList(*it->second, list);
    int32_t flags = n == 1 ? list[0]->Collision_Ground(q, ceiling) : Collision::Collision_GroundAll(list, n, q, ceiling);
    if (flags >= 0) {
        p[1] = q[1];
        p[3] = q[3];
    }
    return flags;
}

uint32_t FieldWorld::Field_Posts(int block, const float* pos, uint16_t yaw) const {
    if (OnCourse()) return courseGround_.Collision_Posts(pos, yaw);
    auto it = blocks_.find(block);
    if (it == blocks_.end()) return 0;
    const Collision* list[1 + kResidents];
    return Collision::Collision_PostsAll(list, Field_GroundList(*it->second, list), pos, yaw);
}

int FieldWorld::Field_GroundList(const Block& b, const Collision** list) {
    // The block's own; block 11's adds each moved-in resident's building (Resident_MovedIn).
    int n = 0;
    list[n++] = &b.ground;
    for (int k = 0; k < (int)b.cityGround.size(); ++k)
        if (Resident_MovedIn(k)) list[n++] = &b.cityGround[k];
    return n;
}

// func_00242438 draws the four slots' maps, each offset by its place (func_00221288); the PC draws every loaded
// block's whose square reaches into the window.
void FieldWorld::Field_DrawMaps(gfx::Draw2D& d, const MapView& view, int block, uint32_t tint,
                                const gfx::Rect& clip) const {
    if (OnCourse()) return;   // FieldMap_Task's maps are course -1's
    float k = view.scale / 200.0f, c = cosf(view.angle), s = sinf(view.angle);
    float reach = kBlock * 0.7072f * k * (fabsf(c) + fabsf(s));   // half the block's square across, on the screen
    for (const auto& b : blocks_) {
        if (b.second->map.Empty()) continue;
        float dx, dz;
        Field_BlockOffset(block, b.first, &dx, &dz);
        // The block's middle on the screen (MiniMap's projection), and whether its square could reach the window.
        float a = dx, e = dz;
        float sx = view.x + k * (a * c + e * s), sy = (view.y - 2048.0f) * 2 + 224.0f - 0.94f * k * (e * c - a * s);
        if (sx + reach < clip.x0 || sx - reach > clip.x1 || sy + reach < clip.y0 || sy - reach > clip.y1) continue;
        b.second->map.DrawShape(d, view, tint, dx, dz, clip);
    }
}

}  // namespace game
