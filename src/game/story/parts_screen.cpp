#include "parts_screen.h"

#include <math.h>

#include "../../audio/sound.h"
#include "../extras.h"
#include "../text.h"
#include "../ui/window.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

#include "parts_tables.inc"

const float kPi = 3.14159265f;
const char kPartsPictures[] = "ITEM/PARTS.GSL";
// Body 149 and the PC's name for it (the Body 149 extra): its texture has a battery cover with the Choro Q logo and
// "TAKARA", and the model is a box with two sticks, a Choro Q remote control (GUESS: what it was meant to be).
const int kBody149 = 149;
const char kBody149Name[] = "Q Remote";

// The list's rows (D_0029F370): categories 1-14, then the body (0). A teammate's list stops after Sticker.
const char* const kCategories[15] = {GT_003336E0_5,    GT_003336D8_6,        GT_003336D0_7, GT_002EBC80_12, GT_002EBC70_8,
                                     GT_003336C8_5,    GT_003336C0_5,         GT_003336B8_6,  GT_002EBC60_8,     GT_002EBC50_13,
                                     GT_00333350_7,  GT_003336A8_7,       GT_003336A0_4,    GT_00333698_5,        GT_00333690_4};

// The part flags a fitted part brings (func_0023C418).
// (kPartBigTires, 0x400, is car_physics.h's.)
const uint16_t kPartEngineType = 0x3, kPartLights = 0x10, kPartWingSet = 0x1000, kPartSpecial = 0x2040,
               kPartOptions = 0x0324, kPartSticker = 0x4000;

uint32_t Grey(int c) { return 0x80000000u | (uint32_t)c << 16 | (uint32_t)c << 8 | (uint32_t)c; }

// A card's 2D picture (func_00243C50: D_002A8C00's base, stride and palette by category): PARTS.GSL entry and
// palette for the item, or -1 for the categories that show a model.
int PictureEntry(int cat, int item, int* palette) {
    *palette = 0;
    switch (cat) {
    case 1: return item;
    case 2: return 13 + item;
    case 3: *palette = item; return 25;
    case 4: *palette = item; return 30;
    case 5: return 26 + item;
    case 6: *palette = item; return 31;
    case 10: return 47 + item;
    case 13: return 32 + item;
    case 14: return 50 + item;
    default: return -1;
    }
}

// Part_NthOwned's list (func_0023BF28) of kind `cat` in the current record. Bodies: each one owned, in body order.
// Parts: for each item 0-15, one entry per copy owned past the number of the cars in `mask` (bits 0-2; a teammate's
// car only counts when there's that teammate) that have it fitted, except that from Lights on the default part (item
// 0) isn't counted against them; with mask bit 3, only the items with a price.
void Part_OwnedList(int cat, int mask, std::vector<int>& out) {
    const SaveRecord& r = Save_Current();
    out.clear();
    if (cat == 0) {
        for (int b = 0; b < 151; ++b)
            if (r.items[0][b >> 5] >> (b & 31) & 1) out.push_back(b);
        return;
    }
    for (int i = 0; i < 16; ++i) {
        if ((mask & 8) && Part_Price(cat, i) == 0) continue;
        int used = 0;
        if (i != 0 || cat < 8)
            for (int c = 0; c < 3; ++c)
                if ((mask >> c & 1) && (c == 0 || r.teammates[c - 1][0]) && r.cars[c].parts[cat] == i) ++used;
        for (int j = used; j < 5; ++j)
            if (r.items[cat][j] >> i & 1) out.push_back(i);
    }
}

}  // namespace

int Part_Price(int cat, int item) {
    if (cat == 0) return 500;
    return cat < 15 && item >= 0 && item < kPartCounts[cat] ? kPartTables[cat][item].price : 0;
}

int Trade_Price(int cat, int item, int player, bool selling) {
    const int d = Save_Score(player) - Save_Score(player ^ 1), v = Part_Price(cat, item);
    return selling ? v * (3000 + d) / 4000 : v * (2000 - d) / 2000;
}

void Part_Fit(SaveCar& car, int cat, int item) {
    car.parts[cat] = (uint8_t)item;
    const int value = cat >= 1 && cat < 15 && item < kPartCounts[cat] ? kPartTables[cat][item].value : 0;
    uint16_t& f = car.partFlags;
    switch (cat) {
    case 1: f = (uint16_t)(value == 1 ? f | kPartBigTires : f & ~kPartBigTires); break;
    case 2: f = (uint16_t)((f & ~kPartEngineType) | (value == 1 ? 1 : value == 2 ? 2 : 0)); break;
    case 7: car.wheel = (uint8_t)item; break;
    case 8: f = (uint16_t)(item ? f | kPartLights : f & ~kPartLights); break;
    case 9: f = (uint16_t)(item ? f | kPartWingSet : f & ~kPartWingSet); break;
    case 10: f = (uint16_t)((f & ~kPartSpecial) | (value & 0xFFFF)); break;
    case 11:
        car.billboard = (uint8_t)(value >> 16);
        f = (uint16_t)((f & ~kPartOptions) | (value & 0xFFFF));
        break;
    case 12: f = (uint16_t)(item ? f | kPartSticker : f & ~kPartSticker); break;
    default: break;
    }
}

const char* Part_Name(int cat, int item) {
    // Body 149's name in D_002ED720 is empty (it's in no shop, talk or driver record); the Body 149 extra sells it,
    // so it has the PC's name, kept after the extra is turned off for a save that has it.
    if (cat == 0 && item == kBody149) return kBody149Name;
    if (cat == 0) return item >= 0 && item < 151 ? kBodyNames[item] : "";
    if (cat == 15) return Item_Name(item);
    return cat < 15 && item >= 0 && item < kPartCounts[cat] ? kPartTables[cat][item].name : "";
}

PartsScreen::PartsScreen(gfx::Draw2D& draw, const std::string& dataDir, Cars& cars)
    : d_(draw), dataDir_(dataDir), cars_(cars), picture_(nullptr), car_(0), row_(-1), remake_(true),
      carousel_(false), fitted_(0), sel_(0), scroll_(0), frames_(0), changed_(false), shop_(0), town_(0),
      shopState_(kShopBrowse), shopIndex_(0), shopItem_(0), price_(0), age_(0) {}

// func_0022BB88's first frame: the list (func_00205DA0: 6 rows shown, 15 for the player, 12 for a teammate), at the
// top left, style 1, opening, on the player's pad.
void PartsScreen::Start(int car, const gfx::Texture* picture, int side, bool versus) {
    car_ = car;
    picture_ = picture;
    side_ = side & 1;
    pad_ = side_;
    versus_ = versus;
    shop_ = 0;
    trade_ = false;
    list_ = Window();
    list_.SetList(kCategories, car ? 12 : 15, 6);
    list_.Place(5);
    list_.style = 1;
    list_.pad = (unsigned char)side_;
    list_.state = Window::kOpening;
    row_ = -1;
    remake_ = true;
    carousel_ = false;
    changed_ = false;
    Light_Setup(light_, 3);
}

// func_0023C150 / func_0023BF28 in the factory's mode. Bodies: each one owned (the 160-bit mask of items[0]), in
// body order. Parts: for each item 0-15, one entry per copy owned that the team's other cars don't have fitted (the
// copy words from the number they use; a teammate's car only counts when there's that teammate), except that from
// Lights on the default part (item 0) isn't counted against them. The carousel starts on the fitted part.
bool PartsScreen::Parts_StartCarousel() {
    const SaveCar& car = Save_Current().cars[car_];
    const int cat = Category();
    Part_OwnedList(cat, ~(1 << car_) & 7, entries_);
    fitted_ = cat == 0 ? car.body : car.parts[cat];
    if (entries_.empty()) return false;
    sel_ = 0;
    for (size_t k = 0; k < entries_.size(); ++k)
        if (entries_[k] == fitted_) {
            sel_ = (int)k;
            break;
        }
    scroll_ = 0;
    frames_ = 0;
    if (cat == 0) Parts_LoadBodies();
    return true;
}

// The body cards show those cars, each in its body's own paint (save +0xD70): their models are read meanwhile
// (Cars_Request; Parts_TurnCarousel puts them on the card), a card staying empty until its car is in, as on the PS2
// (which loads them into eight slots as they come round, func_0020DC50).
void PartsScreen::Parts_LoadBodies() {
    const SaveRecord& r = Save_Current();
    static CarRecord records[151];
    std::vector<RaceCar> cars;
    for (int b : entries_) {
        records[b].body = b;
        records[b].paint = r.bodyPaint[b] & 0xFFFFFF;
        RaceCar c = RaceCar();
        c.record = &records[b];
        cars.push_back(c);
    }
    cars_.Cars_Request(d_, dataDir_, cars.data(), (int)cars.size());
}

// Up to 16 cards; with more entries (bodies) the cards are a ring around the one in front, 8 ahead and 8 behind,
// card k holding entry k's place (sel & 15 the front).
int PartsScreen::Parts_Entry(int card) const {
    const int count = (int)entries_.size(), n = count < 16 ? count : 16;
    if (count <= 16) return card;
    int off = (card - sel_ % n + n) % n;
    if (off >= 8) off -= 16;
    return ((sel_ + off) % count + count) % count;
}

// func_0022BB88's handling of the carousel's Cross: a body swaps the car's body, keeping each body's own paint
// (save +0xD70); a horn plays its sound (func_002083C0) and is fitted; anything else is fitted. The carousel is then
// made again around the new part.
void PartsScreen::Parts_Pick(int item) {
    SaveRecord& r = Save_Current();
    SaveCar& car = r.cars[car_];
    const int cat = Category();
    if (cat == 0) {
        r.bodyPaint[car.body] = car.paint;
        car.body = (uint8_t)item;
        car.paint = r.bodyPaint[item];
    } else {
        if (cat == 13) audio::Sound_PlaySe(kParts13[item].value);
        Part_Fit(car, cat, item);
    }
    changed_ = true;
    remake_ = true;
}

void PartsScreen::Parts_DrawCard(int entry, int x, int y, int size, uint32_t grey) {
    const int cat = Category(), item = entries_[entry];
    // The frame: the room picture's card art, texels (0, 224)-(192, 384), over 256 x 216 at full size.
    if (picture_)
        d_.Sprite(*picture_, (float)(x - 128 * size / 256), (float)y, (float)(2 * (128 * size / 256)),
                  (float)(216 * size / 256), 0, 224, 192, 160, grey);
    // The name, scaled by 5/4 of the card, green for the fitted part.
    const char* name = Part_Name(cat, item);
    const int scale = size * 5 / 4;
    const int width = Text_Width(name) * 5 / 4 * size / 256;
    Text_PrintScaled(d_, x - width / 2, y + size * 14 / 256, name, item == fitted_ ? 5 : 0, scale);
    // The picture, a square of half the card's width, or the car.
    int palette;
    int picture = PictureEntry(cat, item, &palette);
    if (picture >= 0) {
        // UV 0-1: the engines' 128 x 64 pictures are squeezed into the square too.
        const gfx::Texture& t = d_.Get(kPartsPictures, picture, palette);
        d_.Sprite(t, (float)(x - size / 4), (float)(y + size / 4), (float)(size / 2), (float)(size / 2), 0, 0,
                  (float)t.width, (float)t.height, grey);
        return;
    }
    // The card's camera (D_002A8CF0): at (0, 1, -60) from the car, scaled with the card, turned by pitch 0x0C00 and
    // yaw 0x6280, focal length 3564, centred on the card. The Wheel card's (D_002A8CF0[7]) is at (0, 0, -15),
    // pitch 0 and yaw 0x4000: WHEEL.BIN's hub face on.
    const bool wheelCard = cat == 7;
    const float a = wheelCard ? 0.0f : 0x0C00 * kPi / 32768.0f, b = (wheelCard ? 0x4000 : 0x6280) * kPi / 32768.0f;
    const float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
    const float right[3] = {cb, 0, -sb}, up[3] = {sb * sa, ca, cb * sa}, fwd[3] = {sb * ca, -sa, cb * ca};
    const float back = (wheelCard ? -15.0f : -60.0f) * 256.0f / size;
    gfx::Camera3D cam;
    for (int k = 0; k < 3; ++k) {
        cam.right[k] = right[k];
        cam.up[k] = up[k];
        cam.forward[k] = fwd[k];
        cam.eye[k] = (wheelCard ? 0.0f : up[k]) + back * fwd[k];
    }
    cam.focalX = 3564.0f;
    cam.focalY = 3564.0f * 0.94f;
    cam.centerX = (float)x;
    cam.centerY = (float)(y + size / 2);
    Light_SetHighlight(light_, cam.forward);
    // A body card is that body in its own paint; the others are the car as it is with this part fitted, in black
    // but for the part (Lights, Options), or with its shape (Wing Set, Sticker). Special Parts' and the Wheel's
    // cards keep the car's colours.
    const SaveRecord& r = Save_Current();
    if (wheelCard) {
        // func_00244F60's category 7: the hub of the item alone, both paints the edited car's wheel colour.
        d_.ClearDepth();
        cars_.Car_DrawWheelCard(d_, cam, light_, item, r.cars[car_].wheelColor);
        return;
    }
    SaveCar shown = r.cars[car_];
    CarRecord record;
    record.paint = shown.paint & 0xFFFFFF;
    record.body = shown.body;
    record.name = "";
    if (cat == 0) {
        record.body = item;
        record.paint = r.bodyPaint[item] & 0xFFFFFF;
    } else {
        Part_Fit(shown, cat, item);
    }
    record.wheelColor = shown.wheelColor;
    record.wheel = shown.wheel;   // (a Wheels card shows its own)
    record.billboard = shown.billboard;
    RaceCar car = RaceCar();
    car.record = &record;
    car.partFlags = shown.partFlags;
    d_.ClearDepth();
    cars_.Car_DrawCard(d_, cam, light_, car, cat == 8 || cat == 9 || cat == 11 || cat == 12);
}

// func_00246250's placement: card k of n on a ring, a = 2 pi ((k - sel) mod n + scroll / 8) / n; x = 240 + 112 sin a,
// top y = 240 + (n + 24) cos a - n, size 160 + (n + 32) cos a - n (one more on the right half), darker away from
// the front. Drawn from the back, left before right.
void PartsScreen::Parts_DrawCards() {
    const int count = (int)entries_.size(), n = count < 16 ? count : 16, front = sel_ % n;
    struct Place {
        int x, y, size;
        uint32_t grey;
    } place[16];
    for (int k = 0; k < n; ++k) {
        int pos = (k + n - front) % n;
        float a = (int16_t)(((pos * 8 + scroll_) << 13) / n) * kPi / 32768.0f;
        Place& p = place[k];
        p.size = 160 + (int)(cosf(a) * (n + 32.0f)) - n + (a > 0 ? 1 : 0);
        p.x = 240 + (int)(sinf(a) * 112.0f);
        p.y = 240 + (int)(cosf(a) * (n + 24.0f)) - n;
        p.grey = Grey(a != 0.0f ? (int)(cosf(a) * 16) + 0x60 : 0x80);
    }
    for (int k = n / 2; k >= 0; --k) {
        int left = (n + front - k) % n, right = (front + k) % n;
        Parts_DrawCard(Parts_Entry(left), place[left].x, place[left].y, place[left].size, place[left].grey);
        if (right != left)
            Parts_DrawCard(Parts_Entry(right), place[right].x, place[right].y, place[right].size, place[right].grey);
    }
}

// The carousel's frame: Right and Left (with repeat) turn it a card (SE 0x24) when it has two or more; a turn
// plays out over 8 frames. Then the cards.
void PartsScreen::Parts_TurnCarousel(const platform::Input& input) {
    const int count = (int)entries_.size();
    if (count >= 2 && scroll_ == 0) {
        uint32_t r = input.Repeated(pad_);
        if (r & platform::kRight) {
            sel_ = (sel_ + 1) % count;
            scroll_ = 8;
            audio::Sound_PlaySe(0x24);
        } else if (r & platform::kLeft) {
            sel_ = sel_ ? sel_ - 1 : count - 1;
            scroll_ = -8;
            audio::Sound_PlaySe(0x24);
        }
    }
    if (scroll_ != 0) {
        frames_ = 0;
        scroll_ += scroll_ < 0 ? 1 : -1;
    }
    if (Category() == 0) cars_.Cars_Stream(d_, 3.0);
    Parts_DrawCards();
}

// The description (func_0022E960), four lines in palette 10 in a style-1 frame; none for bodies, wheels, stickers,
// horns and meters.
void PartsScreen::Parts_DrawDescription(int item) {
    const int cat = Category();
    if (cat == 0 || cat == 7 || cat >= 12) return;
    Window_DrawCellFrame(d_, 0x11, 0xC, 0x1E, 0x11, 1);
    for (int i = 0; i < 4; ++i) Text_Print(d_, 0x12, 0xD + i, kPartTables[cat][item].desc[i], 0xA);
}

// Everything the screen does is on its player's record (2 Player's other player's, for the factory's screen there).
int PartsScreen::Update(const platform::Input& input) {
    const int current = Save_CurrentPlayer();
    Save_SetCurrent(side_);
    const int r = Run(input);
    Save_SetCurrent(current);
    return r;
}

int PartsScreen::Run(const platform::Input& input) {
    if (shop_) return Shop_Update(input);
    changed_ = false;
    list_.Update(d_, input);   // (its Cross and Triangle aren't used: the carousel takes them)
    // A new row, or a fit, makes the carousel again; the list waits while it starts. Wheel has none with Big tires
    // fitted, nor Meter in 2 Player (the list then keeps the input, and Triangle does nothing until the row moves).
    const int row = list_.sel;
    if ((remake_ && list_.state == Window::kOpen) || row != row_) {
        row_ = row;
        remake_ = false;
        carousel_ = !(Category() == 7 && (Save_Current().cars[car_].partFlags & kPartBigTires)) &&
                    !(versus_ && Category() == 14);
        if (carousel_) {
            // With nothing to list, the carousel ends at once, and the screen with it.
            if (!Parts_StartCarousel()) return -1;
            list_.state |= Window::kInactive;
        }
    }
    if (!carousel_ || row_ < 0) return 0;

    Parts_TurnCarousel(input);
    const int cat = Category(), item = entries_[sel_];
    Parts_DrawDescription(item);
    // Once settled, the list has its input back; Cross fits the part in front (not the fitted one: SE 0x1A, but the
    // horn plays itself instead), Triangle leaves (SE 0x1B).
    int result = 0;
    if (frames_ != 0) {
        list_.state &= ~Window::kInactive;
        uint32_t p = input.Pressed(pad_);
        if (p & platform::kCross) {
            if (item != fitted_) {
                if (cat != 13) audio::Sound_PlaySe(0x1A);
                Parts_Pick(item);
            }
        } else if (p & platform::kTriangle) {
            audio::Sound_PlaySe(0x1B);
            result = -1;
        }
    }
    ++frames_;
    return result;
}


void Money_DrawAfter(gfx::Draw2D& d, int price, int age, bool wallet) {
    const SaveRecord& r = Save_Current();
    int after = r.money - price;
    if (after > 999999) after = 999999;
    if (after < 0) after = 0;   // (the PS2 hands Money_Draw the negative: what it shows then isn't checked)
    if (wallet) {
        Money_Draw(d, 0x1B8, 0x152, r.money, r.currency);
        Money_Draw(d, 0x1B8, 0x182, after, r.currency);
        Icon_Draw(d, 0x204, ((age >> 1) & 7) + 0x171, 7);
    } else {
        Money_Draw(d, 0x1B8, 0x4A, after, r.currency);
        Icon_Draw(d, 0x204, ((age >> 1) & 7) + 0x39, 7);
    }
}

namespace {

int Digits(int n) {
    int k = 1;
    while (n >= 10) {
        n /= 10;
        ++k;
    }
    return k;
}

// The notices' messages (D_002DC7F8) by mode: 0-3 a shop's, 4-7 a trade's.
const char* const kNotices[8] = {GT_0032F348_16,      GT_0032F338_11,       GT_0032F320_16,
                                 GT_0032F308_21, GT_0032F2F0_17, GT_0032F338_11,
                                 GT_0032F320_16,      GT_0032F308_21};

}  // namespace

// func_00269E88: the Parts Shop (func_00269568) and the Recycle Shop (func_00268E40) start with their category
// list, the 14 part categories (D_002DC8E0 / D_002DC870, 6 rows shown, style 1, top left, opening; a trade's on either
// pad and 0x18 half-lines lower), and the carousel of the first row; the Body Shop (func_00269900) has only the
// carousel, of the bodies it has that the player doesn't.
void PartsScreen::StartShop(int kind, int town, const gfx::Texture* picture, int side, bool trade) {
    shop_ = kind;
    trade_ = trade;
    town_ = town >= 0 && town < 10 ? town : 0;
    picture_ = picture;
    side_ = side & 1;
    pad_ = trade ? platform::kEitherPad : side_;
    versus_ = trade;
    car_ = 0;
    changed_ = false;
    shopState_ = kShopBrowse;
    shopIndex_ = 0;
    remake_ = true;
    carousel_ = false;
    soldOut_ = false;
    dialog_[0].open = dialog_[1].open = false;
    age_ = 0;
    list_ = Window();
    if (kind == 1 || kind == 4) {
        list_.SetList(kCategories, 14, 6);
        list_.Place(5);
        list_.style = 1;
        list_.state = Window::kOpening;
        list_.pad = (unsigned char)pad_;
        if (trade) list_.y += 0x18;
        row_ = -1;
    } else {
        row_ = 14;   // Category() 0, the bodies
    }
    Light_Setup(light_, 3);
}

namespace {

const int kMyCity = 9;
// D_002DC7A8: by town 0-8, the place of its Parts Shop and of its Body Shop (-1: none).
const int kShopPlace[9][2] = {
    {-1, -1}, {1, 2}, {1, 2}, {1, 2}, {-1, -1}, {1, -1}, {1, 2}, {1, 2}, {1, 2},
};
// What the story adds to My City's shops: func_00267C30's switch on the category, with MyCity_BodyStockAdd
// (0x00267A80), MyCity_HornStockAdd (0x002678E0) and MyCity_MeterStockAdd (0x002679C8) as rows. Each is the item
// (body or part index) of category `cat` once stamp `id` is earned, or (`flag`) event flag `id` set. (The meters'
// function also asks for stamp 47 and drops the answer: the Life Meter, item 6, is never stocked.)
struct ShopUnlock {
    uint8_t cat, item;
    bool flag;
    uint8_t id;
};
const ShopUnlock kMyCityUnlocks[] = {
    {0, 29, false, 42},   {0, 30, false, 84},  {0, 33, false, 65},  {0, 36, false, 63},  {0, 79, true, 0x5D},
    {0, 80, true, 0x5E},  {0, 82, false, 89},  {0, 83, true, 0x5F}, {0, 84, false, 44},  {0, 85, true, 0x5C},
    {0, 86, false, 9},    {0, 88, false, 48},  {0, 148, false, 100},
    {1, 11, true, 0x60},  {2, 10, false, 46},  {3, 4, true, 0x63},  {4, 5, true, 0x62},   {5, 3, true, 0x61},
    {6, 3, true, 0x64},   {10, 2, false, 81},
    {13, 5, false, 92},   {13, 6, false, 39},  {13, 7, false, 97},  {13, 8, false, 19},  {13, 9, false, 67},
    {13, 10, false, 89},  {13, 13, false, 59},
    {14, 2, false, 90},   {14, 3, false, 44},  {14, 5, false, 17},  {14, 7, false, 14},  {14, 8, false, 89},
};
const int kWingSet = 9, kSpecial = 10, kOptions = 11, kMeters = 14;
const int kLifeMeter = 6;
const int kSeaTemple = 18;   // area 18 place 0: Seidon's temple under the sea
const int kPoliceLightPlace = 4;

// MyCity_ShopStockAdd (func_00267C30): into `m` (category `cat`'s mask; for the bodies five words), every other
// town's Parts Shop (or Body Shop) stock once that shop has been found, then what the story has unlocked.
void MyCity_ShopStockAdd(uint32_t* m, int cat) {
    for (int t = 0; t < 9; ++t) {
        int place = kShopPlace[t][cat == 0 ? 1 : 0];
        if (place < 0 || Place_Hidden(t, place)) continue;
        if (cat == 0)
            for (int w = 0; w < 5; ++w) m[w] |= kShopBodies[t][w];
        else
            m[0] |= kShopParts[t][cat];
    }
    for (const ShopUnlock& u : kMyCityUnlocks)
        if (u.cat == cat && (u.flag ? Save_EventFlag(u.id) : Stamp_Earned(u.id))) m[u.item >> 5] |= 1u << (u.item & 31);
    // The places: the Wing Set and the Propeller once the sea temple is found, the Police Light once My City's
    // place 4 is.
    if ((cat == kWingSet || cat == kSpecial) && !Place_Hidden(kSeaTemple, 0)) m[0] |= 1u << 1;
    if (cat == kOptions && !Place_Hidden(kMyCity, kPoliceLightPlace)) m[0] |= 1u << 3;
    // The Life Meter (meter 6), an extra: MyCity_MeterStockAdd (0x002679C8) asks Stamp_Earned(47), "Cleared the
    // Which-way? Race!", at 0x00267A2C and drops the answer, so no shop ever stocks it. docs/extras.md.
    if (cat == kMeters && Extras_On(kExtraLifeMeter) && Stamp_Earned(47)) m[0] |= 1u << kLifeMeter;
    // Body 149 (CAR4/Q149.BIN), an extra: always in My City's Body Shop, as body 28 is (its own row), at the price
    // of every body. Nothing in the game gives it. docs/extras.md.
    if (cat == 0 && Extras_On(kExtraBody149)) m[kBody149 >> 5] |= 1u << (kBody149 & 31);
}

}  // namespace

// What the carousel lists. The Parts Shop and the Body Shop: func_00267F48 for category `cat` of town_'s shop, the
// items of its mask in order (My City's grown by MyCity_ShopStockAdd), for bodies only those the player hasn't got.
// The Recycle Shop: the player's own parts that have a price, a copy each, the copies on the team's three cars not
// counted (Part_NthOwned's mask 0xF).
void PartsScreen::Shop_Stock(int cat) {
    if (shop_ == 4) {
        Part_OwnedList(cat, 0xF, entries_);
        return;
    }
    const SaveRecord& r = Save_Current();
    entries_.clear();
    uint32_t m[5] = {};
    if (cat == 0)
        for (int w = 0; w < 5; ++w) m[w] = kShopBodies[town_][w];
    else
        m[0] = kShopParts[town_][cat];
    if (town_ == kMyCity) MyCity_ShopStockAdd(m, cat);
    // The All Parts cheat: every body (but 149, the extra's), or every part with a price.
    if (Extras_On(kCheatAllParts)) {
        if (cat == 0)
            for (int b = 0; b < kBody149; ++b) m[b >> 5] |= 1u << (b & 31);
        else
            for (int i = 0; i < 32 && i < kPartCounts[cat]; ++i)
                if (Part_Price(cat, i) > 0) m[0] |= 1u << i;
    }
    if (cat == 0) {
        for (int b = 0; b < 151; ++b)
            if ((m[b >> 5] >> (b & 31) & 1) && !(r.items[0][b >> 5] >> (b & 31) & 1)) entries_.push_back(b);
    } else {
        for (int i = 0; i < 32 && i < kPartCounts[cat]; ++i)
            if (m[0] >> i & 1) entries_.push_back(i);
    }
}

// The windows of func_00268428 and func_00268890: the lines, each centred with spaces on the widest (in spaces), then
// "Yes" (and "No", the cursor on it); centred on (320, 112), or `offset` pixels to one side; read on `owner`'s pad.
void PartsScreen::Shop_OpenWindow(Dialog& dg, const char* const* lines, int count, bool yesNo, int owner, int offset) {
    const int space = Text_Width(" ");
    int widest = 0;
    for (int i = 0; i < count; ++i)
        if (Text_Width(lines[i]) / space > widest) widest = Text_Width(lines[i]) / space;
    dg.w = Window();
    for (int i = 0; i < count; ++i) {
        dg.lines[i] = std::string((widest - Text_Width(lines[i]) / space) / 2, ' ') + lines[i];
        dg.w.AddLine(dg.lines[i].c_str(), 0);
    }
    dg.w.AddLine("Yes", kLineSelectable);
    if (yesNo) {
        dg.w.AddLine("No", kLineSelectable);
        dg.w.sel = 1;
    }
    dg.w.x = (short)(dg.w.x + offset);
    dg.w.pad = (unsigned char)owner;
    dg.owner = owner;
    dg.price = -1;
    dg.age = 0;
    dg.open = true;
}

// func_00268428, a notice by mode (D_002DC7F8): 0-3 a shop's one line; in a trade, the player's name over it, the
// seller's for 4 "Cannot sell this!" and 5 "Installing!", the buyer's for 6 "Need more money!" (with the buying
// price below) and 7 "Cannot carry anymore!" (with the part's name), 150 pixels to that player's side.
void PartsScreen::Shop_Notice(int mode, int item) {
    Dialog& dg = dialog_[0];
    dialog_[1].open = false;
    if (mode < 4) {
        const char* lines[1] = {kNotices[mode]};
        Shop_OpenWindow(dg, lines, 1, false, side_, 0);
    } else {
        const int who = mode >= 6 ? side_ ^ 1 : side_;
        const char* lines[3] = {Save_Record(who).name, kNotices[mode], mode == 7 ? Part_Name(Category(), item) : ""};
        Shop_OpenWindow(dg, lines, mode >= 6 ? 3 : 2, false, who, who ? 0x96 : -0x96);
        if (mode == 6) dg.price = Trade_Price(Category(), item, who, false);
    }
    shopState_ = kShopNotice;
}

// func_00268890, the Yes / No by mode (D_002DC818): 0 sell, 1 buy: the part's name, "Sell ?" or "Buy ?" and two
// empty lines, the price (half the part's for selling) on the third; 2 a trade's seller, 3 its buyer: the player's
// name first, the price (Trade_Price) on the fourth, 150 pixels to that player's side.
void PartsScreen::Shop_Ask(Dialog& dg, int mode, int item) {
    const int cat = Category();
    const char* ask = mode == 0 || mode == 2 ? GT_0032F390_14 : GT_0032F370_14;
    if (mode < 2) {
        const char* lines[4] = {Part_Name(cat, item), ask, "", ""};
        Shop_OpenWindow(dg, lines, 4, true, side_, 0);
        dg.price = mode == 0 ? Part_Price(cat, item) / 2 : Part_Price(cat, item);
        dg.priceLine = 2;
    } else {
        const int who = mode == 3 ? side_ ^ 1 : side_;
        const char* lines[5] = {Save_Record(who).name, Part_Name(cat, item), ask, "", ""};
        Shop_OpenWindow(dg, lines, 5, true, who, who ? 0x96 : -0x96);
        dg.price = Trade_Price(cat, item, who, mode == 2);
        dg.priceLine = 3;
    }
}

// From a window's second frame: the price on its line, centred with the owner's currency 2 pixels after it.
void PartsScreen::Shop_DrawWindow(Dialog& dg) {
    if (dg.age++ == 0 || dg.price < 0) return;
    const char* unit = Save_Record(dg.owner).currency;
    const int digits = Digits(dg.price) * 16;
    const int x = dg.w.x + (dg.w.w - (digits + Text_Width(unit) + 2)) / 2 + digits;
    const int y = (dg.w.y + dg.priceLine * 12) * 2;
    Number_PrintRight(d_, x, y, dg.price, 0);
    Text_PrintAt(d_, x + 2, y, unit, 0);
}

// The carousel's Cross in a shop (state 1): the carousel goes and the list waits. Buying: "Need more money!" if the
// money is short, "Cannot carry anymore!" for a fifth copy (or a body owned), else the item's name and "Buy ?" over
// Yes / No. Selling: "Cannot buy this!" for a part with no price, "Installing!" for one not listed (both only by the
// code: the list has neither), else "Sell ?". Trading: the same checks on the buyer's money and room, then a window
// for each player.
void PartsScreen::Shop_Pick(int item) {
    const int cat = Category();
    shopItem_ = item;
    shopIndex_ = sel_;
    price_ = Part_Price(cat, item);
    carousel_ = false;
    list_.state |= Window::kInactive;
    if (shop_ != 4) {
        if (Save_Current().money < price_) {
            Shop_Notice(2, item);
        } else if (Save_ItemFull(cat, item)) {
            Shop_Notice(3, item);
        } else {
            Shop_Ask(dialog_[0], 1, item);
            dialog_[1].open = false;
            pending_ = 1;
            shopState_ = kShopConfirm;
        }
        return;
    }
    // Part_OwnedIndex: the item's first entry.
    shopIndex_ = -1;
    for (size_t k = 0; k < entries_.size(); ++k)
        if (entries_[k] == item) {
            shopIndex_ = (int)k;
            break;
        }
    const int buyer = side_ ^ 1;
    if (price_ == 0) {
        Shop_Notice(trade_ ? 4 : 0, item);
    } else if (shopIndex_ < 0) {
        Shop_Notice(trade_ ? 5 : 1, item);
    } else if (!trade_) {
        Shop_Ask(dialog_[0], 0, item);
        dialog_[1].open = false;
        pending_ = 1;
        shopState_ = kShopConfirm;
    } else if (Save_Record(buyer).money < Trade_Price(cat, item, buyer, false)) {
        Shop_Notice(6, item);
    } else {
        Save_SetCurrent(buyer);
        const bool full = Save_ItemFull(cat, item);
        Save_SetCurrent(side_);
        if (full) {
            Shop_Notice(7, item);
        } else {
            Shop_Ask(dialog_[0], 2, item);
            Shop_Ask(dialog_[1], 3, item);
            pending_ = 2;
            shopState_ = kShopConfirm;
        }
    }
    if (shopIndex_ < 0) shopIndex_ = 0;
}

// ItemCarousel_Task's box between the cards and the description (style 2). Buying (cells 0x11-0x1E, rows 9-0xC):
// "You have it." (the Body Shop) or "You have N" (copies), or "You don't have it."; "Price" with the price
// (palette 6 if it's more than the money) and the currency; and the money it would leave (Money_DrawAfter). Selling
// (rows 0xA-0xC): "Selling" at half the price, and the money it would make. Trading: the buyer's name, what they
// have and "Buying" with their price (rows 8-0xC), over it the seller's name and "Selling" with theirs (rows 5-8).
void PartsScreen::Shop_DrawPrice(int item) {
    const SaveRecord& r = Save_Current();
    const int cat = Category(), price = Part_Price(cat, item);
    auto have = [&](int row) {
        const int n = Save_HasItem(cat, item);
        if (n == 0)
            Text_Print(d_, 0x12, row, GT_003275B8_18, 0);
        else if (shop_ == 2)
            Text_Print(d_, 0x12, row, GT_00327598_12, 0);
        else
            Number_PrintRight(d_, Text_PrintAt(d_, 0x186, 0x7D * 2, GT_003275A8_9, 0) + 0x10, 0x7D * 2, n, 0);
    };
    if (trade_) {
        const int buyer = side_ ^ 1;
        const SaveRecord& b = Save_Record(buyer);
        const int buy = Trade_Price(cat, item, buyer, false), sell = Trade_Price(cat, item, side_, true);
        Window_DrawCellFrame(d_, 0x11, 8, 0x1E, 0xC, 2);
        Text_Print(d_, 0x12, 9, b.name, 0);
        Save_SetCurrent(buyer);
        have(0xA);
        Save_SetCurrent(side_);
        Text_Print(d_, 0x13, 0xB, GT_00334388_6, 0);
        Number_PrintRight(d_, 0x226, 0x89 * 2, buy, b.money >= buy ? 0 : 6);
        Text_Print(d_, 0x1B, 0xB, b.currency, 0);
        Window_DrawCellFrame(d_, 0x11, 5, 0x1E, 8, 2);
        Text_Print(d_, 0x12, 6, r.name, 0);
        Text_Print(d_, 0x13, 7, GT_00334390_7, 0);
        Number_PrintRight(d_, 0x226, 0x59 * 2, sell, 0);
        Text_Print(d_, 0x1B, 7, r.currency, 0);
    } else if (shop_ == 4) {
        Window_DrawCellFrame(d_, 0x11, 0xA, 0x1E, 0xC, 2);
        Text_Print(d_, 0x13, 0xB, GT_00334390_7, 0);
        Number_PrintRight(d_, 0x226, 0x89 * 2, price / 2, 0);
        Text_Print(d_, 0x1B, 0xB, r.currency, 0);
        Money_DrawAfter(d_, -(price / 2), age_, false);
    } else {
        Window_DrawCellFrame(d_, 0x11, 9, 0x1E, 0xC, 2);
        have(0xA);
        Text_Print(d_, 0x13, 0xB, GT_00334380_5, 0);
        Number_PrintRight(d_, 0x226, 0x89 * 2, price, r.money >= price ? 0 : 6);
        Text_Print(d_, 0x1B, 0xB, r.currency, 0);
        Money_DrawAfter(d_, price, age_, false);
    }
}

// While the windows wait: Wallet_Draw at the bottom right, the money now and after the deal. A trade shows both
// players', player 1's at x 0x28 and player 2's at 0x1B8 (Money_DrawPlayer), each with the arrow between.
void PartsScreen::Shop_DrawWallets() {
    const int cat = Category();
    if (!trade_) {
        Money_DrawAfter(d_, shop_ == 4 ? -(Part_Price(cat, shopItem_) / 2) : price_, age_, true);
        return;
    }
    for (int p = 0; p < 2; ++p) {
        const SaveRecord& r = Save_Record(p);
        const int change = p == side_ ? -Trade_Price(cat, shopItem_, p, true) : Trade_Price(cat, shopItem_, p, false);
        int after = r.money - change;
        if (after > 999999) after = 999999;
        const int x = p ? 0x1B8 : 0x28;
        Money_Draw(d_, x, 0x152, r.money, r.currency);
        Money_Draw(d_, x, 0x182, after, r.currency);
        Icon_Draw(d_, x + 0x4C, ((age_ >> 1) & 7) + 0x171, 7);
    }
}

// Every Yes is in. Buying: the item is the player's, paid for (SE 0x305); a bought body leaves the stock, the
// carousel staying on the same place (or the one before, for the last). Selling: the copy goes (Save_TakeItem) for
// half its price; trading, to the other player at the buying price, the seller getting the selling price. A sold
// part's last listed copy moves the carousel back one.
void PartsScreen::Shop_Sell() {
    const int cat = Category(), item = shopItem_;
    if (shop_ != 4) {
        Item_Give(cat, item);
        Money_Pay(price_);
        if (cat == 0) {
            Shop_Stock(0);
            if (shopIndex_ != 0 && shopIndex_ >= (int)entries_.size()) --shopIndex_;
        }
        return;
    }
    if (!trade_) {
        Save_TakeItem(cat, item);
        Money_Pay(-(price_ / 2));
    } else {
        const int buyer = side_ ^ 1;
        const int sell = Trade_Price(cat, item, side_, true), buy = Trade_Price(cat, item, buyer, false);
        Save_TakeItem(cat, item);
        Money_Pay(-sell);
        Save_SetCurrent(buyer);
        Item_Give(cat, item);
        Money_Pay(buy);
        Save_SetCurrent(side_);
    }
    Shop_Stock(cat);
    if (shopIndex_ != 0 && shopIndex_ >= (int)entries_.size()) --shopIndex_;
}

// One frame of a shop's screen. The carousel is ItemCarousel_Task in the shops' modes: no part is "fitted" (every
// name white, Cross on any card), and it starts on the entry picked last. -1 once left: Triangle on the list or (the
// Body Shop) the carousel, or the Body Shop's notice that it has nothing left.
int PartsScreen::Shop_Update(const platform::Input& input) {
    changed_ = false;
    ++age_;
    // The list (Window_UpdateList): a new row makes its carousel from its first entry; Triangle leaves.
    if (shop_ == 1 || shop_ == 4) {
        if (list_.Update(d_, input) == -1) {
            audio::Sound_PlaySe(0x1B);
            return -1;
        }
        if (list_.sel != row_) {
            row_ = list_.sel;
            shopIndex_ = 0;
            remake_ = true;
        }
    }
    const int cat = Category();
    switch (shopState_) {
    case kShopBrowse: {
        if (remake_ && (shop_ == 2 || list_.state == Window::kOpen)) {
            remake_ = false;
            Shop_Stock(cat);
            carousel_ = !entries_.empty();
            if (carousel_) {
                fitted_ = -1;
                sel_ = shopIndex_ < (int)entries_.size() ? shopIndex_ : 0;
                scroll_ = 0;
                frames_ = 0;
                if (cat == 0) Parts_LoadBodies();
                list_.state |= Window::kInactive;
            } else if (shop_ == 2) {
                // BodyShop_SoldOutTask: "We don't have any more / bodies to sell." and Yes, which leaves.
                const char* lines[2] = {GT_0032F3A0_22, GT_0032F3B8_18};
                Dialog& dg = dialog_[0];
                dg.w = Window();
                for (int i = 0; i < 2; ++i) dg.w.AddLine(lines[i], 0);
                dg.w.AddLine("Yes", kLineSelectable);
                dg.open = true;
                dg.price = -1;
                dg.age = 0;
                soldOut_ = true;
                shopState_ = kShopNotice;
            }
            // (A category with nothing in it has no carousel until the row moves.)
        }
        if (!carousel_) break;
        Parts_TurnCarousel(input);
        const int item = entries_[sel_];
        Shop_DrawPrice(item);
        Parts_DrawDescription(item);
        if (frames_ != 0) {
            list_.state &= ~Window::kInactive;
            uint32_t p = input.Pressed(pad_);
            if (p & platform::kCross) {
                // SE 0x1A, but none for a horn.
                if (cat != 13) audio::Sound_PlaySe(0x1A);
                Shop_Pick(item);
            } else if ((p & platform::kTriangle) && shop_ == 2) {
                audio::Sound_PlaySe(0x1B);
                return -1;
            }
        }
        ++frames_;
        break;
    }
    case kShopNotice: {
        // Any answer: SE 0x1A, the window gone, the carousel made again on the same entry.
        Dialog& dg = dialog_[0];
        int r = dg.w.Update(d_, input);
        Shop_DrawWindow(dg);
        if (r == 0) break;
        audio::Sound_PlaySe(0x1A);
        dg.open = false;
        if (soldOut_) return -1;
        shopState_ = kShopBrowse;
        remake_ = true;
        list_.state &= ~Window::kInactive;
        break;
    }
    case kShopConfirm: {
        // Each window goes at once on its answer: Yes SE 0x1A, No or Triangle SE 0x1B (a No sends the other one
        // away too). Then the deal, or back to the carousel.
        if (pending_ > 0) Shop_DrawWallets();
        for (Dialog& dg : dialog_) {
            if (!dg.open) continue;
            int r = dg.w.Update(d_, input);
            Shop_DrawWindow(dg);
            if (r == 0) continue;
            dg.open = false;
            if (r == 1 && dg.w.sel == 0) {
                audio::Sound_PlaySe(0x1A);
                if (pending_ > 0) --pending_;
            } else {
                audio::Sound_PlaySe(0x1B);
                pending_ = -1;
            }
        }
        if (pending_ > 0) break;
        if (pending_ == 0) Shop_Sell();
        dialog_[0].open = dialog_[1].open = false;
        shopState_ = kShopBrowse;
        remake_ = true;
        list_.state &= ~Window::kInactive;
        break;
    }
    }
    return 0;
}

}  // namespace game
