// Roulette (course 0x18, ACTION/A06; the Adventure's race 44, Sandpolis 8): a betting loop. On the board the player
// picks a number, a sixth or a colour and a wager; then drives the car off the hub of the spinning wheel in place of
// the ball, the wheel catches it at its edge and carries it round, and the pocket it stops over pays. A high-level
// reimplementation of the event task func_00260DC0 and its children: the board (func_00261AC8), the wager
// (func_00261638), the small windows and the result (func_00261DE0). See docs/minigames.md.
#include <math.h>
#include <string.h>

#include <algorithm>

#include "../../audio/sound.h"
#include "../hud/hud_games.h"
#include "../save/save_data.h"
#include "../story/parts_screen.h"
#include "../text.h"
#include "race.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const float kPi = 3.14159f;   // D_00333178, D_00333184, D_00333188, D_00333190
const float kHub[3] = {400.0f, 0.0f, 400.0f};   // D_00329250: the wheel's centre
const float kStart[3] = {400.0f, 5.25f, 400.0f};   // the car on the hub, facing +z
// The car leaving radius 11.7 (D_0033318C) is caught: the overhead camera and the ride. The ride pulls it to
// radius 15, 1/60 a frame (D_0033317C), and towards its pocket's middle 10 a frame.
const float kCatch = 11.7f, kRideRadius = 15.0f, kRideStep = 1.0f / 60.0f;
const int kPocket = 0xCCC;   // 20 pockets round the wheel
// D_002D9DB8: each pocket's number, 0 for "0", "00" and the sliver at the end.
const int kPockets[21] = {0, 6, 8, 18, 10, 4, 12, 16, 14, 2, 0, 5, 7, 17, 9, 3, 11, 15, 13, 1, 0};
// D_002D9968: the board's 23 bets (18 numbers, three sixths, BLACK and RED): the ball marker's place, the box lit
// on a win, the payout, the cursor's neighbours (up, down, left, right) and the d-pad repeats that don't wrap there.
struct Box {
    int cursorX, cursorY, x, y, w, h, payout, up, down, left, right;
    uint32_t noRepeat;   // the PS2's pad bits: 0x1000 up, 0x4000 down, 0x8000 left, 0x2000 right
};
const Box kBoard[23] = {
    {156, 128, 112, 128, 128, 40, 18, 15, 3, 18, 1, 0x1000},    {300, 128, 256, 128, 128, 40, 18, 16, 4, 0, 2, 0x1000},
    {444, 128, 400, 128, 128, 40, 18, 17, 5, 1, 21, 0x1000},    {156, 176, 112, 176, 128, 40, 18, 0, 6, 18, 4, 0},
    {300, 176, 256, 176, 128, 40, 18, 1, 7, 3, 5, 0},           {444, 176, 400, 176, 128, 40, 18, 2, 8, 4, 21, 0},
    {156, 224, 112, 224, 128, 40, 18, 3, 9, 19, 7, 0},          {300, 224, 256, 224, 128, 40, 18, 4, 10, 6, 8, 0},
    {444, 224, 400, 224, 128, 40, 18, 5, 11, 7, 21, 0},         {156, 272, 112, 272, 128, 40, 18, 6, 12, 19, 10, 0},
    {300, 272, 256, 272, 128, 40, 18, 7, 13, 9, 11, 0},         {444, 272, 400, 272, 128, 40, 18, 8, 14, 10, 22, 0},
    {156, 320, 112, 320, 128, 40, 18, 9, 15, 20, 13, 0},        {300, 320, 256, 320, 128, 40, 18, 10, 16, 12, 14, 0},
    {444, 320, 400, 320, 128, 40, 18, 11, 17, 13, 22, 0},       {156, 368, 112, 368, 128, 40, 18, 12, 0, 20, 16, 0x4000},
    {300, 368, 256, 368, 128, 40, 18, 13, 1, 15, 17, 0x4000},   {444, 368, 400, 368, 128, 40, 18, 14, 2, 16, 22, 0x4000},
    {64, 156, 64, 128, 40, 88, 3, 20, 19, 21, 0, 0x9000},       {64, 252, 64, 224, 40, 88, 3, 18, 20, 21, 6, 0x8000},
    {64, 348, 64, 320, 40, 88, 3, 19, 18, 22, 12, 0xC000},      {536, 176, 536, 128, 40, 136, 2, 22, 22, 5, 18, 0x3000},
    {536, 320, 536, 272, 40, 136, 2, 21, 21, 14, 20, 0x6000},
};
const int kBlack = 21;
const int kMaxBet = 10000, kMinMoney = 10;
// The board's pictures: SCOREA06.GSL 000 (the board, T8 640 x 448) and 001 (the same lit, the ball marker in its
// top left 40 x 40).
const char kBoardFile[] = "SYS/SCOREA06.GSL";
const uint32_t kSeOk = 0x1A, kSeCancel = 0x1B, kSeMove = 0x1D;

// The angle (16-bit) of the car about the hub, from +z towards +x.
int16_t HubAngle(const float* pos) {
    return (int16_t)(int)(atan2f(pos[0] - kHub[0], pos[2] - kHub[2]) / kPi * 32768.0f);
}

void DrawBoard(gfx::Draw2D& d, int picture, int u, int v, int w, int h, int x, int y) {
    d.Sprite(d.Get(kBoardFile, picture, 0), (float)x, (float)y, (float)w, (float)h, (float)u, (float)v, (float)w,
             (float)h);
}

}  // namespace

// Roulette_Wheel (func_002609D0): the wheel turned by its speed, matrices 0-3 all its turn about y at the hub
// (A06's objects 0-3: the ring and the floor).
void Race::Roulette_Wheel() {
    RouletteState& r = roulette_;
    r.wheel = (uint16_t)(r.wheel + r.speed);
    float a = (int16_t)r.wheel * kPi / 32768.0f, c = cosf(a), s = sinf(a);
    const float m[12] = {c, 0, s, kHub[0], 0, 1, 0, kHub[1], -s, 0, c, kHub[2]};
    for (int i = 0; i < 4; ++i) objects_.SetObject(i, m);
    r.drawCanvas = true;
}

// Roulette_CarRide (func_00260B48): the car carried by the wheel, its radius drawn to 15 and its angle to its pocket's
// middle, turned with the wheel each frame; its own physics goes on under it with the brake held.
void Race::Roulette_CarRide() {
    RouletteState& r = roulette_;
    CarBody& me = bodies_[0];
    float v[3] = {me.pos[0] - kHub[0], 0, me.pos[2] - kHub[2]};
    float len = sqrtf(v[0] * v[0] + v[2] * v[2]);
    if (len > 0) v[0] /= len, v[2] /= len;
    if (len < kRideRadius) len = std::min(len + kRideStep, kRideRadius);
    else if (len > kRideRadius) len = std::max(len - kRideStep, kRideRadius);
    int16_t a = (int16_t)(int)(atan2f(v[0], v[2]) / kPi * 32768.0f);
    int16_t rel = (int16_t)((uint16_t)(a - r.wheel + kPocket / 2) % kPocket - kPocket / 2);
    int adj = rel < -10 ? 10 : rel > 10 ? -10 : 0;
    float t = (int16_t)(r.speed + adj) * kPi / 32768.0f, c = cosf(t), s = sinf(t);
    float x = v[0] * len, z = v[2] * len;
    float pos[3] = {c * x + s * z + kHub[0], me.pos[1], -s * x + c * z + kHub[2]};
    Car_SetPlace(me, pos, (uint16_t)(me.yaw + r.speed));
}

// Roulette_SetResult (func_00262358): the pocket's number, its colour (0 black, 1 red; none for 0) and whether the
// bet won.
void Race::Roulette_SetResult(int pocket) {
    RouletteState& r = roulette_;
    r.number = kPockets[pocket];
    r.won = false;
    if (r.number == 0) {
        r.colour = -1;
        return;
    }
    r.colour = pocket < 10 ? pocket % 2 : (pocket + 1) % 2;
    int b = r.bet;
    if (b < 18) r.won = b == r.number - 1;
    else if (b < 21) r.won = b == (r.number - 1) / 6 + 18;
    else r.won = b == r.colour + kBlack;
}

// func_00260DC0's frame, after the cars. 0: the song (playing through the betting), the bet on BLACK, the money
// to compare; 1: black, no green, the view off (race flag 0x2000), the board; 2: on its answer, quit (the outcome)
// or the car on the hub, the wheel's turn and speed from the frame count; 3: 33 frames, then green; 4: off the hub
// the overhead camera, the HUD off and the brake; 5: the wheel slows 3 a frame, carrying the car, and stopped gives
// the pocket; 7: 128 frames still; 8: the fade over 64, then the view off and the result; 9: WIN!/LOSE on its word,
// and the next round.
bool Race::Race_RouletteTask() {
    RouletteState& r = roulette_;
    r.drawCanvas = false;
    uint64_t frames = Info_Frames();
    switch (r.state) {
    case 0:
        audio::Sound_LoadSong(0x0A100201);   // BGM_05
        audio::Sound_StartSong();
        r.bet = kBlack;
        r.startMoney = Save_Current().money;
        r.state = 1;
        // fall through
    case 1:
        event_.fadeAll = 0x80;
        go_ = false;
        overhead_ = false;
        Roulette_HideView(true);
        r.child = kRouletteBoard;
        r.childState = 0;
        r.childCount = 0;
        r.childFrame = 0;
        r.state = 2;
        // fall through
    case 2: {
        if (!r.pulse) break;
        r.pulse = false;
        event_.fadeAll = 0x80;
        if (r.pulseValue == -1) {
            // Roulette_SetOutcome (func_00262458): the money against what it was.
            int money = Save_Current().money;
            result_.outcome = money < r.startMoney ? 1 : money > r.startMoney ? 2 : 3;
            return true;
        }
        game_.hidden = false;
        braked_[0] = false;
        r.state = 3;
        r.count = 0;
        Roulette_HideView(false);
        Car_Reset(bodies_[0], kStart, 0);
        Car_WorldPosition(bodies_[0], cars_[0].pos);
        for (HudElement& e : elements_) e = HudElement();   // HudCanvas_InitRoulette (func_00235008)
        r.wheel = (uint16_t)((frames % 20) * kPocket);
        r.speed = (uint16_t)(((frames >> 5) & 0xF) * 12 + 0x300);
        break;
    }
    case 3:
        if (++r.wait == 0x21) {
            r.wait = 0;
            go_ = true;
            r.state = 4;
        }
        Roulette_Wheel();
        break;
    case 4: {
        float dx = bodies_[0].pos[0] - kHub[0], dz = bodies_[0].pos[2] - kHub[2];
        if (sqrtf(dx * dx + dz * dz) > kCatch) {
            overhead_ = true;
            game_.hidden = true;
            braked_[0] = true;
            r.state = 5;
            r.count = 0;
        }
        Roulette_Wheel();
        break;
    }
    case 5:
        r.speed = (uint16_t)(r.speed - 3);
        if ((int16_t)r.speed < 0) {
            r.state = 6;
            r.speed = 0;
            r.count = 0;
            int16_t a = HubAngle(bodies_[0].pos);
            Roulette_SetResult((uint16_t)(a - r.wheel + kPocket / 2) / kPocket);
        }
        Roulette_CarRide();
        Roulette_Wheel();
        break;
    case 6:
        r.count = 0;
        r.state = 7;
        // fall through
    case 7:
        if (r.count++ < 0x80) {
            Roulette_Wheel();
            break;
        }
        r.count = 0;
        r.state = 8;
        // fall through
    case 8:
        event_.fadeAll = r.count << 1;
        if (r.count++ < 0x40) {
            Roulette_Wheel();
            break;
        }
        Roulette_HideView(true);
        r.child = kRouletteResult;
        r.childState = 0;
        r.childCount = 0;
        r.childFrame = 0;
        r.state = 9;
        // fall through
    case 9:
        if (r.pulse) {
            r.pulse = false;
            if (r.pulseValue == 0) {
                Event_SetLose(0);
            } else if (r.pulseValue == 1) {
                Event_SetWin(0);
            } else if (r.pulseValue == 2) {
                elements_[kElemWin].type = elements_[kElemLose].type = 0;
            } else {
                event_.fadeAll = 0x80;
                r.state = 1;
            }
        }
        r.drawCanvas = true;
        break;
    default:
        break;
    }
    return false;
}

// Race flag 0x2000 (Race_HideView, func_0024FB38): no 3D view and the camera still, with the cars' sounds muted.
void Race::Roulette_HideView(bool on) {
    hideView_ = on;
    if (on) sounds_[0].Mute();
    else sounds_[0].Loud();
}

// The board's children, drawn over the race's frame: the board (func_00261AC8) with the wager (func_00261638) and its
// windows, and the result (func_00261DE0). Each tells its parent through `pulse` as the game's task +0x24 and +4 do.
void Race::Roulette_DrawScreens(const platform::Input& in) {
    RouletteState& r = roulette_;
    if (r.child == kRouletteNone) return;
    const int frame = r.childFrame++;
    const Box& bet = kBoard[r.bet];
    const int money = Save_Current().money;
    // Fade_SetOverlay's black over the board as it ends.
    int fade = 0;
    d_.Pillarbox();   // the board is a 4:3 picture: a wide screen's sides are black
    if (r.child == kRouletteBoard) {
        DrawBoard(d_, 0, 0, 0, 640, 448, 0, 0);
        // The ball marker blinks 16 frames in 32 while the bet is being picked.
        if (!(r.childState == 2 && !(frame & 0x10))) DrawBoard(d_, 1, 0, 0, 40, 40, bet.cursorX, bet.cursorY);
        switch (r.childState) {
        case 0:
            if (money < kMinMoney) {
                Roulette_OpenWindow(kRouletteNoMoney);
                r.childState = 1;
            } else {
                r.childState = 2;
            }
            break;
        case 1:
            if (Roulette_UpdateWindow(in) != kWindowOpen) {
                audio::Sound_FadeOutSong();
                r.childState = 4;
                r.childResult = -1;
            }
            break;
        case 2: {
            // Roulette_MoveCursor (func_00261378): a press moves to the neighbour; a repeat too, but where the
            // box's no-repeat bit is set.
            uint32_t pressed = in.Pressed(0), repeated = in.Repeated(0);
            const struct {
                uint32_t key, ps2;
                int Box::*to;
            } dirs[4] = {{platform::kUp, 0x1000, &Box::up}, {platform::kDown, 0x4000, &Box::down},
                         {platform::kLeft, 0x8000, &Box::left}, {platform::kRight, 0x2000, &Box::right}};
            int b = r.bet;
            for (const auto& k : dirs) {
                if ((pressed & k.key) || ((repeated & k.key) && !(bet.noRepeat & k.ps2))) {
                    b = bet.*k.to;
                    break;
                }
            }
            if (b != r.bet) audio::Sound_PlaySe(kSeMove);
            r.bet = b;
            if (pressed & platform::kCross) {
                audio::Sound_PlaySe(kSeOk);
                r.wager = RouletteWager();
                r.childState = 3;
            } else if (pressed & platform::kTriangle) {
                audio::Sound_PlaySe(kSeCancel);
                audio::Sound_FadeOutSong();
                r.childState = 4;
                r.childResult = -1;
            }
            break;
        }
        case 3: {
            int w = Roulette_WagerTask(in);
            if (w == -1) {
                r.childState = 2;
            } else if (w == 1) {
                r.childResult = 0;
                r.childState = 4;
            }
            break;
        }
        case 4:
            fade = r.childCount << 1;
            if (++r.childCount == 0x41) {
                r.childCount = 0;
                r.pulse = true;
                r.pulseValue = r.childResult;
                r.child = kRouletteNone;
            }
            break;
        default:
            break;
        }
        // Money_DrawBox (func_0022E7C0), but while the wager draws its own.
        if (r.childState != 3) Money_Draw(d_, 0x1B8, 0x1A, money, Save_Current().currency);
    } else {
        DrawBoard(d_, 0, 0, 0, 640, 448, 0, 0);
        // The three winning bets' boxes lit, blinking 32 frames in 64; the ball marker on the bet.
        if (r.number != 0 && (frame & 0x20)) {
            const int lit[3] = {r.number - 1, (r.number - 1) / 6 + 18, r.colour + kBlack};
            for (int k : lit) DrawBoard(d_, 1, kBoard[k].x, kBoard[k].y, kBoard[k].w, kBoard[k].h, kBoard[k].x,
                                        kBoard[k].y);
        }
        DrawBoard(d_, 1, 0, 0, 40, 40, bet.cursorX, bet.cursorY);
        bool skip = (in.Pressed(0) & (platform::kCross | platform::kTriangle)) != 0;
        switch (r.childState) {
        case 0:
            r.childState = 1;
            // fall through
        case 1:
        case 2:
            if (skip) {
                audio::Sound_PlaySe(kSeOk);
                r.childCount = 0x80;
            }
            if (r.childCount++ > 0x80) {
                r.childCount = 0;
                if (r.childState == 1) {
                    r.pulse = true;
                    r.pulseValue = r.won ? 1 : 0;
                }
                ++r.childState;
            }
            break;
        case 3: {
            int win = r.won ? r.wager.amount * bet.payout : 0;
            Money_DrawAfter(d_, money - std::min(money + win, 999999), frame, true);
            if (skip) {
                audio::Sound_PlaySe(kSeOk);
                r.childCount = 0xC0;
            }
            if (r.childCount++ > 0xC0) {
                r.childCount = 0;
                Money_Pay(-win);
                r.childState = 4;
                r.pulse = true;
                r.pulseValue = 2;
            }
            break;
        }
        case 4:
            fade = r.childCount;
            if (r.childCount++ > 0x80) {
                r.pulse = true;
                r.pulseValue = -1;
                r.child = kRouletteNone;
            }
            break;
        default:
            break;
        }
    }
    if (fade > 0) d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)(fade < 0x80 ? fade : 0x80) << 24);
}

void Race::Race_DrawGameScreens(const platform::Input& in) {
    if (task_ == kTaskRoulette) Roulette_DrawScreens(in);
    else if (task_ == kTaskFigure8) Figure8_DrawScreens(in);
    else if (task_ == kTaskGolf) Golf_DrawScreens(in);
}

// The small windows (func_002624B8, func_002625A0, func_00261508): a line or two and Yes (and No), centred.
void Race::Roulette_OpenWindow(int which) {
    RouletteState& r = roulette_;
    r.window = Window();
    r.windowKind = which;
    switch (which) {
    case kRouletteNoMoney:
        r.window.AddLine(GT_003292B0_33, 0);   // D_003292B0
        r.window.AddLine("Yes", kLineSelectable);
        r.window.y = 0x9E;
        break;
    case kRouletteNeedBet:
        r.window.AddLine(GT_003292D8_18, 0);   // D_003292D8
        r.window.AddLine("Yes", kLineSelectable);
        r.window.y = 0x77;
        break;
    default:
        r.window.AddLine(GT_00334EA0_6, 0);   // D_00334EA0
        r.window.AddLine("Yes", kLineSelectable);
        r.window.AddLine("No", kLineSelectable);
        r.window.y = 0x77;
        break;
    }
}

// The window's frame: kWindowOpen while it waits, else its answer: the cursor's line (Start's cancel is No), and
// "Please make a bet." always 1. Start's No sounds 0x1B, the rest 0x1A.
int Race::Roulette_UpdateWindow(const platform::Input& in) {
    RouletteState& r = roulette_;
    int u = r.window.Update(d_, in);
    if (u == 0) return kWindowOpen;
    int answer = r.window.sel;
    if (r.windowKind == kRouletteStart && u == -1) answer = 1;
    if (r.windowKind == kRouletteNeedBet) answer = 1;
    audio::Sound_PlaySe(r.windowKind == kRouletteStart && answer != 0 ? kSeCancel : kSeOk);
    return answer;
}

// Roulette_WagerTask (func_00261638): the amount, a digit at a time (Left/Right pick the digit, its tens upward;
// Up/Down change it, wrapping between 0 and the most on a fresh press), up to the money or 10000. Cross asks
// "Start?" (or "Please make a bet." for 0) and takes the bet; Triangle backs out. -1 backed out, 1 the bet taken,
// 0 still going.
int Race::Roulette_WagerTask(const platform::Input& in) {
    RouletteState& r = roulette_;
    RouletteWager& w = r.wager;
    const int money = Save_Current().money;
    const Box& bet = kBoard[r.bet];
    int result = 0;
    if (w.frame++ == 0) return 0;   // nothing on its first frame
    // The window, the amount and the money after a win, drawn before the frame's input.
    Window_DrawFrame(d_, 0x78, 0x3E, 0x190, 0x24, 0);
    // (Text_PrintAt's y is in half-lines on the PS2.)
    Text_PrintAt(d_, 0x78, 0x3E * 2, GT_003292F0_28, 0);   // D_003292F0
    Text_PrintAt(d_, 0x158, 0x4D * 2, Save_Current().currency, 0);
    switch (w.state) {
    case 0: {
        w.amount = 0;
        w.max = std::min(money, kMaxBet);
        int n[10];
        w.cols = Hud_Digits(w.max, n) - 1;   // func_0024F4E0
        w.pos = 0;
        w.state = 1;
        // fall through
    }
    case 1: {
        int step = 10;
        for (int k = 0; k < w.pos; ++k) step *= 10;
        if (w.cols > 0) {
            int c = Menu_MoveCursorGrid(in, 0, w.cols - w.pos - 1, w.cols, 1, false);
            w.pos = w.cols - c - 1;
            step = 10;
            for (int k = 0; k < w.pos; ++k) step *= 10;
        }
        uint32_t pressed = in.Pressed(0), repeated = in.Repeated(0);
        int before = w.amount;
        if (w.amount == w.max) {
            if (pressed & platform::kUp) w.amount = 0;
        } else if (repeated & platform::kUp) {
            w.amount = std::min(w.amount + step, w.max);
        }
        if (w.amount == 0) {
            if (pressed & platform::kDown) w.amount = w.max;
        } else if (repeated & platform::kDown) {
            w.amount = std::max(w.amount - step, 0);
        }
        if (w.amount != before) audio::Sound_PlaySe(kSeMove);
        if (pressed & platform::kCross) {
            audio::Sound_PlaySe(kSeOk);
            Roulette_OpenWindow(w.amount == 0 ? kRouletteNeedBet : kRouletteStart);
            w.state = 2;
        } else if (pressed & platform::kTriangle) {
            audio::Sound_PlaySe(kSeCancel);
            result = -1;
        }
        break;
    }
    case 2: {
        int answer = Roulette_UpdateWindow(in);
        if (answer == kWindowOpen) break;
        if (r.windowKind == kRouletteNeedBet || answer == 1) {
            w.state = 1;
        } else {
            Money_Pay(w.amount);   // the bet taken (SE 0x305)
            result = 1;
        }
        break;
    }
    default:
        break;
    }
    Number_PrintDigits(d_, 0x158, 0x9C, w.amount, w.cols + 1, 0);
    if (w.state != 2) Icon_Draw(d_, 310 - 16 * w.pos, 178 - ((frame_ >> 1) & 7), 6);
    // Wallet_Draw: the money now, and after this bet wins.
    Money_DrawAfter(d_, money - std::min(money + w.amount * (bet.payout - 1), 999999), w.frame, true);
    return result;
}

}  // namespace game
