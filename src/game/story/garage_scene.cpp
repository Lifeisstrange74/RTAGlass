#include "garage_scene.h"

#include <math.h>
#include <string.h>

#include "../../audio/sound.h"
#include "../race/race.h"
#include "../save/save_data.h"
#include "parts_screen.h"
#include "race_list.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const float kPi = 3.14159265f;
const float kAngle = kPi / 32768.0f;
const gfx::Rect kFrame = {0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight};
const char kGarageFile[] = "SHOP/GARAGE.BIN";

// D_0029F3F0: the garage's camera, the eye at (0, 1.3, -22.9) turned by the pitch about x, looking at the origin.
const float kCameraEye[3] = {0.0f, 1.3f, -22.9f};
const float kCameraFocal = 1782.0f;
const int kCameraPitch = 0xE80;

// The car drives in (sub-state 0): from (-1, -6), heading atan2(1, 4) (func_00274AF8), 0.02 along x and 0.08 along z a
// frame (D_00332E2C, D_00332E30) for 50 frames.
const float kCarStart[2] = {-1.0f, -6.0f};
const float kCarStep[2] = {0.02f, 0.08f};
const int kDriveFrames = 50;

// D_0029F440: Go play's games, by the building that runs each (the list shows a game once its place is found), and
// their names (D_002BE398 by D_002BE018's course).
struct Game {
    int town, place;
    const char* name;
};
const Game kGames[20] = {
    {2, 8, GT_00328548_17}, {2, 17, GT_00328538_12},   {3, 10, GT_00328518_9},
    {9, 18, GT_003284D8_11},      {7, 13, GT_00328468_10},     {8, 4, GT_00328458_15},
    {2, 9, GT_00328560_21}, {4, 5, GT_00328528_13}, {5, 3, GT_00334558_4},
    {3, 8, GT_00328508_8},          {3, 6, GT_003284F8_8},        {3, 7, GT_00334550_6},
    {8, 6, GT_003284E8_12},      {7, 5, GT_003284C8_15}, {9, 10, GT_003284B8_15},
    {4, 7, GT_003284A8_14},    {6, 18, GT_00334548_7},        {1, 17, GT_00328498_14},
    {6, 7, GT_00328478_11},       {7, 15, GT_00334540_7},
};

// func_0022C648's sprites, all from the garage's sheet (000.dds, TEX0 at 0x1F2E): a trophy (0xBA, 0x4F, 30 x 32) for
// each of the 24 town races whose best place is 1st (save +0xFF0), eight a row at D_0029F508's heights; the Grand
// Prix's picture with event flag 87; and the things from the stories (kind 15 items 10-22) on show (D_0029F468).
const int kTrophyRows[3] = {189, 144, 94};
struct Decoration {
    int x, y, u, v, w, h;
};
const Decoration kDecorations[13] = {
    {80, 200, 337, 137, 52, 52},  {524, 238, 117, 114, 54, 50}, {59, 207, 50, 97, 56, 80},
    {400, 34, 187, 2, 116, 76},   {530, 73, 592, 0, 48, 168},   {354, 153, 408, 184, 36, 34},
    {580, 54, 417, 0, 48, 130},   {414, 226, 408, 136, 76, 40}, {542, 301, 1, 95, 46, 80},
    {34, 269, 306, 4, 110, 124},  {556, 182, 470, 2, 72, 124},  {496, 228, 520, 137, 56, 26},
    {59, 113, 544, 2, 48, 122},
};

}  // namespace

GarageScene::GarageScene(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), dataDir_(dataDir), roomShown_(-1), car_(), body_(), record_(), mail_(draw, dataDir), state_(0), age_(0),
      ending_(-1), fade_(0), result_(0), goTown_(0), goPlace_(0) {}

void GarageScene::Garage_LoadRoom() {
    int n = Save_Current().homeInterior % 10;
    if (n == roomShown_) return;
    std::string dir = dataDir_ + "\\models\\SHOP\\GARAGE\\room" + std::to_string(n);
    room_.Load(dir + ".obj", d_);
    roomB_.Load(dir + "_b.obj", d_);
    roomShown_ = n;
}

void GarageScene::Garage_PlaceCar(float x, float z) {
    body_.pos[0] = car_.pos[0] = x;
    body_.pos[2] = car_.pos[2] = z;
}

void GarageScene::Start() {
    // func_0022CD38's first frame: the camera (Camera_BuildMatrices with D_0029F3F0), the room's light (Light_Setup 1)
    // and highlight.
    const float a = kCameraPitch * kAngle, ca = cosf(a), sa = sinf(a);
    const float right[3] = {1, 0, 0}, up[3] = {0, ca, sa}, fwd[3] = {0, -sa, ca};
    for (int k = 0; k < 3; ++k) {
        camera_.right[k] = right[k];
        camera_.up[k] = up[k];
        camera_.forward[k] = fwd[k];
        camera_.eye[k] = kCameraEye[0] * right[k] + kCameraEye[1] * up[k] + kCameraEye[2] * fwd[k];
    }
    camera_.focalX = kCameraFocal;
    camera_.focalY = kCameraFocal * 0.94f;
    camera_.centerX = gfx::Draw2D::kWidth / 2.0f;
    camera_.centerY = gfx::Draw2D::kHeight / 2.0f;
    Light_Setup(light_, 1);
    Light_SetHighlight(light_, camera_.forward);
    roomShown_ = -1;
    Garage_LoadRoom();

    // The player's car (D_0177EA50[0], the save's car 0), heading atan2(1, 4), its wing put away (part flag 8 becomes
    // 4, 0x0022CE54).
    uint16_t& wing = Save_Current().cars[0].partFlags;
    if (wing & kPartWingOut) wing = (uint16_t)((wing & ~kPartWingOut) | kPartWingFolded);
    const SaveRecord& r = Save_Current();
    const SaveCar& c = r.cars[0];
    record_.paint = c.paint & 0xFFFFFF;
    record_.body = c.body;
    record_.wheelColor = c.wheelColor;
    record_.wheel = c.wheel;
    record_.billboard = c.billboard;
    record_.name = r.name;
    car_ = RaceCar();
    car_.record = &record_;
    car_.town = -1;
    car_.partFlags = c.partFlags;
    memset(&body_, 0, sizeof body_);
    for (int k = 0; k < 3; ++k) body_.body[k][k] = 1.0f;
    for (int i = 0; i < 7; ++i) body_.probe[i][1] = body_.probe[i][3] = 0.0f;
    float yaw = atan2f(1.0f, 4.0f), s = sinf(yaw), co = cosf(yaw);
    const float cr[3] = {co, 0, -s}, cu[3] = {0, 1, 0}, cf[3] = {s, 0, co};
    for (int k = 0; k < 3; ++k) {
        body_.m[0][k] = cr[k];
        body_.m[1][k] = cu[k];
        body_.m[2][k] = cf[k];
    }
    car_.yaw = yaw;
    body_.bodyHeight = (car_.partFlags & kPartBigTires) ? kBigTiresLift : 0.0f;
    car_.physics = &body_;
    Garage_PlaceCar(kCarStart[0], kCarStart[1]);
    cars_.Load(d_, dataDir_, &car_, 1);

    // The door's SE 0x32 and the buildings' song 0x0A0B0102.
    audio::Sound_PlaySe(0x32);
    audio::Sound_LoadSong(0x0A0B0102);
    audio::Sound_StartSong();

    // The menu (style 2, top left): Check E-mail, Redecorate, Go play, Go outside.
    menu_ = Window();
    menu_.AddLine(GT_002EBCB0_12, kLineSelectable);
    menu_.AddLine(GT_002EBCC0_10, kLineSelectable);
    menu_.AddLine(GT_003336F0_7, kLineSelectable);
    menu_.AddLine(GT_002EBCD0_10, kLineSelectable);
    menu_.style = 2;
    menu_.Place(5);
    menu_.state = Window::kOpening;

    // Place_MarkFound for (0, 0): the garage visited, so its door and Warp's entry open.
    Place_MarkFound(0, 0);
    // func_0022D1E0, on the first visit: stamp 40, the first interior (Stylish Pattern) and mails 2, 1 and 0.
    if (!Stamp_Earned(40)) {
        Stamp_Queue(40);
        Item_Give(15, 0);
        Mail_Send(2);
        Mail_Send(1);
        Mail_Send(0);
    }
    state_ = kDriveIn;
    age_ = 0;
    ending_ = -1;
    fade_ = 0;
    result_ = 0;
}

void GarageScene::Garage_OpenList(int kind) {
    // Redecorate (func_0022C8A0): the interiors owned (kind 15 items 0-9), then Quit, the cursor on the one shown.
    // Go play (func_0022CA28): the games whose buildings are found, then Quit.
    listNames_.clear();
    listIds_.clear();
    int cursor = 0;
    for (int i = 0; i < (kind == kRedecorate ? 10 : 20); ++i) {
        if (kind == kRedecorate) {
            if (!Save_HasItem(15, i)) continue;
            if (i == Save_Current().homeInterior) cursor = (int)listIds_.size();
            listNames_.push_back(Item_Name(i));
        } else {
            if (Place_Hidden(kGames[i].town, kGames[i].place)) continue;
            listNames_.push_back(kGames[i].name);
        }
        listIds_.push_back(i);
    }
    listNames_.push_back(GT_002FF078_4);
    list_ = Window();
    list_.SetList(listNames_.data(), (int)listNames_.size(), 10);
    list_.style = 2;
    list_.Place(5);
    list_.sel = (signed char)cursor;
    list_.state = Window::kOpening;
    menu_.state |= Window::kInactive;
    state_ = kind;
}

void GarageScene::Garage_DrawSprites() {
    const gfx::Texture& sheet = d_.Get(kGarageFile, 0);
    const SaveRecord& r = Save_Current();
    for (int i = 0; i < 24; ++i) {
        if (r.adventurePlace[0][i] != 0) continue;
        int col = i % 8;
        float x = (float)(col < 4 ? 0x80 + 0x20 * col : 0x186 + 0x20 * (col - 4));
        d_.Sprite(sheet, x, (float)kTrophyRows[i / 8], 0x1E, 0x20, 0xBA, 0x4F, 0x1E, 0x20);
    }
    if (Save_EventFlag(87)) d_.Sprite(sheet, 0xF2, 0x16, 0x9C, 0x5C, 0, 0, 0x9C, 0x5C);
    for (int k = 0; k < 13; ++k) {
        if (!Save_HasItem(15, 10 + k)) continue;
        const Decoration& s = kDecorations[k];
        d_.Sprite(sheet, (float)s.x, (float)s.y, (float)s.w, (float)s.h, (float)s.u, (float)s.v, (float)s.w,
                  (float)s.h);
    }
}

void GarageScene::Garage_Draw() {
    // func_0022CC38: the room under the room camera, lit as the cars are, its paint colours D_0029F420 (1, 1, 1)
    // and (0, 0, 0) (func_00226A18); the car, then the sprites.
    gfx::DrawStyle style;
    gfx::Lighting lighting = Light_CarSet(light_, 0);
    for (int k = 0; k < 3; ++k) {
        lighting.paint[0][k] = 1.0f;
        lighting.paint[1][k] = 0.0f;
    }
    style.lighting = &lighting;
    room_.Draw(d_, camera_, kFrame, style);
    // Chain B: the furniture's shadows, black at half alpha (vf17 = (1, 1, 1, 0.5), D_0029F420; program 0x50
    // blended).
    style.alpha = 0.5f;
    roomB_.Draw(d_, camera_, kFrame, style);
    d_.ClearDepth();
    cars_.Cars_Draw(d_, camera_, light_, &car_, 1);
    Garage_DrawSprites();
}

int GarageScene::Update(const platform::Input& input) {
    // func_0022C418 after Go outside: the last picture darkened for 64 frames, then the song stops.
    if (ending_ >= 0) {
        d_.BeginFrameFromLast(0.0f);
        d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, 0x08000000);
        d_.EndFrame(0);
        if (++ending_ < 0x40) return 0;
        audio::Sound_StopSong();
        return result_;
    }
    d_.BeginFrame(0x80000000);
    if (state_ == kMail) {
        // Sub-state 4: the room and the car go on under the mail reader (func_00264498), the menu neither drawn nor
        // run; then sub-state 1 again.
        Garage_Draw();
        if (mail_.Update(input)) {
            state_ = kMenu;
            menu_.state &= ~Window::kInactive;
        }
        d_.EndFrame(Race::kFrameBlend);
        return 0;
    }
    if (state_ == kDriveIn) {
        if (age_ < kDriveFrames) {
            Garage_PlaceCar(body_.pos[0] + kCarStep[0], body_.pos[2] + kCarStep[1]);
            float v = sqrtf(kCarStep[0] * kCarStep[0] + kCarStep[1] * kCarStep[1]);
            body_.wheelAngle[2] += v / 0.38f;
            body_.wheelAngle[0] += v / 0.33f;
        } else {
            state_ = kMenu;
        }
    }
    Garage_Draw();
    int leave = 0;
    // The menu runs before its list: the list draws over it, and a press that closes the list isn't read again by
    // the menu it hands back to (Triangle would leave the garage, Cross open the list again).
    int r = menu_.Update(d_, input);
    if (state_ == kMenu) {
        int item = menu_.sel;
        if (r == -1 || (r == 1 && item == 3)) {
            audio::Sound_PlaySe(0x1A);
            menu_.state = Window::kClosing;
        } else if (r == 1 && item == 0) {
            // (No SE: only Go outside and cancelling play 0x1A here.)
            mail_.Start();
            menu_.state |= Window::kInactive;
            state_ = kMail;
        } else if (r == 1 && item == 1) {
            audio::Sound_PlaySe(0x1A);
            Garage_OpenList(kRedecorate);
        } else if (r == 1 && item == 2) {
            audio::Sound_PlaySe(0x1A);
            Garage_OpenList(kGoPlay);
        } else if (r == -2) {
            // Go outside: the parent is told 10 and Room_EndTask takes over.
            result_ = kDriveOut;
            audio::Sound_FadeOutSong();
            ending_ = 0;
        }
    } else if (state_ == kRedecorate || state_ == kGoPlay) {
        int r = list_.Update(d_, input);
        int n = list_.sel;
        if (r == 1 && n < (int)listIds_.size()) {
            audio::Sound_PlaySe(0x1A);
            int id = listIds_[n];
            if (state_ == kRedecorate) {
                // A new interior: the room is loaded again (the menu stays live meanwhile).
                Save_Current().homeInterior = (uint8_t)id;
                Garage_LoadRoom();
                state_ = kMenu;
                menu_.state &= ~Window::kInactive;
            } else {
                // Into that game's building, as a door would (info+0x23/+0x21, the parent told 11).
                goTown_ = kGames[id].town;
                goPlace_ = kGames[id].place;
                leave = kGoTo;
            }
        } else if (r == 1 || r == -1) {
            audio::Sound_PlaySe(0x1B);
            state_ = kMenu;
            menu_.state &= ~Window::kInactive;
        }
    }
    if (fade_ < 32) d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)((32 - fade_) * 4) << 24);
    ++fade_;
    d_.EndFrame(Race::kFrameBlend);
    ++age_;
    if (leave) {
        audio::Sound_FadeOutSong();
        audio::Sound_StopSong();
        return leave;
    }
    return 0;
}

}  // namespace game
