#include "save_data.h"

#include <string.h>

#include "../../audio/sound.h"
#include "../extras.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

#include "save_tables.inc"

SaveRecord g_records[2];
SaveRecord* g_current = &g_records[0];
bool g_testRecord = false;   // record 0 is the test menu's (Save_SetTestRecord)

// Save_Init's area words (+0xBD8 on): the places in each area still to be found.
const uint32_t kAreaHidden[22] = {0,   0x3DFFF, 0x7FBFF, 0x1FFFFF, 0xFBF, 0xF,  0xFFFFF, 0x3FFFF,
                                  0x17F, 0x7FFFDF, 0xF, 0xF,  0xF,  0x1F, 7,   0x3F,
                                  3,   1,       1,       0xF,      1,     1};

const char* const kPlaces[24] = {" 1ST", " 2ND", " 3RD", " 4TH", " 5TH", " 6TH", " 7TH", " 8TH",
                                 " 9TH", "10TH", "11TH", "12TH", "13TH", "14TH", "15TH", "16TH",
                                 "17TH", "18TH", "19TH", "20TH", "21ST", "22ND", "23RD", "24TH"};

}  // namespace

SaveRecord& Save_Record(int player) { return g_records[player & 1]; }
SaveRecord& Save_Current() { return *g_current; }
void Save_SetCurrent(int player) { g_current = &g_records[player & 1]; }
int Save_CurrentPlayer() { return g_current == &g_records[1] ? 1 : 0; }
void Save_SetTestRecord(bool on) { g_testRecord = on; }
bool Save_TestRecord() { return g_testRecord; }

void Save_ResetButtons(SaveRecord& r) {
    // Cross, Square, Circle, R2, L1, L2, R1, Triangle, R1, Select (PS2 pad bits).
    static const uint16_t kButtons[10] = {0x40, 0x80, 0x20, 2, 4, 1, 8, 0x10, 8, 0x100};
    memcpy(r.buttons, kButtons, sizeof(kButtons));
}

void Save_Init(SaveRecord& r) {
    memset(&r, 0, sizeof(r));
    Save_ResetButtons(r);
    for (int i = 0; i < 3; ++i)
        for (int c = 0; c < 26; ++c) {
            r.adventurePlace[i][c] = kNoRecordPlace;
            r.adventureLap[i][c] = kNoRecordTime;
            r.adventureTotal[i][c] = kNoRecordTime;
        }
    r.rallyBest = kNoRecordTime;
    for (int32_t& t : r.figure8Best) t = kNoRecordTime;
    for (int8_t& h : r.golfHole) h = 0x7F;
    r.golfTotal = 0x7F;
    for (SaveQuickRace& q : r.quickRace) {
        q.bestLap = q.bestTotal = kNoRecordTime;
        q.bestPlace = kNoRecordPlace;
    }
    for (SaveVersusCourse& v : r.versus) v.bestLap = v.bestTotal = kNoRecordTime;
    r.myCity |= 0x100;
    memcpy(r.areaHidden, kAreaHidden, sizeof(kAreaHidden));
    for (int kind = 1; kind <= 14; ++kind) r.items[kind][0] = 1;   // one of each kind's first part
    r.townsUnvisited = 0x3FF;
    r.radioStation = 2;
    r.radioVolume = 0x5A;
    r.pickupsLeft[0] = ~0ull;
    r.pickupsLeft[1] = 0xFFFFFFFFFull;   // 100 bits in all
    strcpy(r.name, GT_002EB470_12);
    strcpy(r.currency, "gold");
    for (int t = 0; t < 2; ++t) memcpy(r.teammateSpeeds[t], kTeammateSpeeds, sizeof(kTeammateSpeeds));
}

void Save_InitBoth() {
    g_testRecord = false;
    Save_Init(g_records[0]);
    Save_Init(g_records[1]);
    SaveRecord& r = g_records[0];
    r.money = 1000;
    memcpy(r.bodyPaint, kBodyPaint, sizeof(kBodyPaint));
    r.cars[0].paint = kBodyPaint[0];
}

void Save_SetStartingCar(int body) {
    SaveRecord& r = g_records[0];
    r.items[0][body >> 5] |= 1u << (body & 31);   // func_0023D2C0(0, body): kind 0, the bodies, is a plain bit set
    r.bodyPaint[body] = kBodyPaint[body];
    r.cars[0].parts[0] = (uint8_t)body;
    r.cars[0].body = (uint8_t)body;
    r.cars[0].paint = kBodyPaint[body];
}

int Save_HasItem(int kind, int n) {
    const SaveRecord& r = Save_Current();
    if (kind < 0 || kind > 15 || n < 0) return 0;
    if (kind == 0 || kind == 15) return n < 160 && ((r.items[kind][n >> 5] >> (n & 31)) & 1);
    if (n >= 32) return 0;
    int copies = 0;
    for (int w = 0; w < 5; ++w) copies += (r.items[kind][w] >> n) & 1;
    return copies;
}

bool Save_ItemFull(int kind, int n) {
    const SaveRecord& r = Save_Current();
    if (kind < 0 || kind > 15 || n < 0) return false;
    if (kind == 0 || kind == 15) return n < 160 && ((r.items[kind][n >> 5] >> (n & 31)) & 1);
    return n < 32 && ((r.items[kind][4] >> n) & 1);
}

bool Pickup_Taken(int i) {
    const SaveRecord& r = Save_Current();
    if (i == 0) return (r.pickupsLeft[0] | r.pickupsLeft[1]) == 0;
    --i;
    if (i < 0 || i >= 128) return false;
    return ((r.pickupsLeft[i >> 6] >> (i & 63)) & 1) == 0;
}

int Pickups_Left() {
    const SaveRecord& r = Save_Current();
    int n = 0;
    for (uint64_t w : r.pickupsLeft)
        for (; w; w &= w - 1) ++n;
    return n;
}

void Pickup_Take(int i) {
    if (i >= 0 && i < 128) Save_Current().pickupsLeft[i >> 6] &= ~(1ull << (i & 63));
}

void Mail_Send(int m) {
    SaveRecord& r = Save_Current();
    if (r.mailCount == 128) {
        memmove(r.mail, r.mail + 1, 127);
        r.mail[127] = (uint8_t)m;
    } else if (r.mailCount >= 0 && r.mailCount < 128) {
        r.mail[r.mailCount++] = (uint8_t)m;
    }
}

int Mail_SendRandom(int base, int range, int seed) {
    const SaveRecord& r = Save_Current();
    auto inBox = [&](int m) {
        for (int i = 0; i < r.mailCount && i < 128; ++i)
            if ((r.mail[i] & 0x7F) == m) return true;
        return false;
    };
    int m = seed % range + base;
    for (int i = 1; inBox(m) && i <= range; ++i) m = (seed + i) % range + base;
    Mail_Send(m);
    return m;
}

bool Place_Hidden(int town, int place) {
    if (town < 0 || town >= 22 || place < 0 || place >= 32) return false;
    return ((Save_Current().areaHidden[town] >> place) & 1) != 0;
}

void Place_MarkFound(int town, int place) {
    // D_002A2B20: towns 1-9's stamps, whose text the book shows once the town is found.
    static const uint8_t kTownStamp[10] = {0, 1, 10, 23, 52, 91, 60, 72, 85, 50};
    if (town < 0 || town >= 22 || place < 0 || place >= 32) return;
    SaveRecord& r = Save_Current();
    r.areaHidden[town] &= ~(1u << place);
    if (place == 0) {
        // The town visited. With all of 0x25E visited (towns 1-4, 6 and 9) resident 0x19 moves into My City.
        r.townsUnvisited &= (uint16_t)~(1u << town);
        if (!(r.townsUnvisited & 0x25E)) Resident_MoveIn(0x19);
    }
    if (town >= 1 && town < 10) {
        int s = kTownStamp[town] - 1;
        r.stampShown[s >> 6] |= 1ull << (s & 63);
    }
}

void Save_TakeItem(int kind, int n) {
    SaveRecord& r = Save_Current();
    if (kind < 0 || kind > 15 || n < 0) return;
    if (kind == 0 || kind == 15) {
        if (n < 160) r.items[kind][n >> 5] &= ~(1u << (n & 31));
        return;
    }
    if (n >= 32) return;
    for (int w = 4; w >= 0; --w)
        if (r.items[kind][w] & (1u << n)) {
            r.items[kind][w] &= ~(1u << n);
            return;
        }
}

void Item_Give(int kind, int n) {
    SaveRecord& r = Save_Current();
    if (kind < 0 || kind > 15 || n < 0) return;
    if (kind == 0 || kind == 15) {
        if (n < 160) r.items[kind][n >> 5] |= 1u << (n & 31);
        return;
    }
    if (n >= 32) return;
    for (int w = 0; w < 5; ++w)
        if (!(r.items[kind][w] & (1u << n)) || w == 4) {
            r.items[kind][w] |= 1u << n;
            return;
        }
}

namespace {
int Bits128(const uint64_t* w) {
    int n = 0;
    for (int i = 0; i < 2; ++i)
        for (uint64_t v = w[i]; v; v >>= 1) n += (int)(v & 1);
    return n;
}
}  // namespace

int Save_Score(int player) {
    const SaveRecord& r = Save_Record(player);
    int score = (r.teammates[0][0] ? 10 : 0) + (r.teammates[1][0] ? 10 : 0);
    for (int t = 0; t < 10; ++t)
        if (!(r.townsUnvisited >> t & 1)) score += 2;
    score += r.license * 20 + (100 - Bits128(r.pickupsLeft)) * 2;
    for (int i = 0; i < 25; ++i) {
        static const int kPoints[6] = {8, 4, 4, 3, 2, 1};
        const int place = r.adventurePlace[0][i];
        if (place < 6) score += kPoints[place];
    }
    return score + Bits128(r.stampEarned) * 2 + Bits128(r.photoTaken) * 2;
}

bool Money_Pay(int amount) {
    SaveRecord& r = Save_Current();
    if (r.money < amount) return false;
    r.money -= amount;
    if (r.money > 999999) r.money = 999999;
    if (amount) audio::Sound_PlaySe(0x305);
    return true;
}

bool Stamp_Earned(int s) {
    const SaveRecord& r = Save_Current();
    if (s == 0) return r.stampEarned[0] == ~0ull && (r.stampEarned[1] & 0xFFFFFFFFFull) == 0xFFFFFFFFFull;
    --s;
    return (r.stampEarned[s >> 6 & 1] >> (s & 63) & 1) != 0;
}

bool Stamp_Queue(int id) {
    SaveRecord& r = Save_Current();
    if (id <= 0 || id > 100 || r.stampQueueCount >= 8 || r.stampQueueCount < 0) return false;
    if ((r.stampEarned[(id - 1) >> 6] >> ((id - 1) & 63)) & 1) return false;
    for (int i = 0; i < r.stampQueueCount; ++i)
        if (r.stampQueue[i] == id) return false;
    r.stampQueue[r.stampQueueCount++] = (uint8_t)id;
    // The mail some stamps bring: Tim's thanks, Otomi's, Butch's, Wonder Realty's and Forest's.
    switch (id) {
    case 5: Mail_Send(0x7B); break;
    case 17: Mail_Send(0x7C); break;
    case 30: Mail_Send(0x7D); break;
    case 50: Mail_Send(0x1B); break;
    case 100: Mail_Send(0x7F); break;
    }
    return true;
}

int Stamp_Dequeue() {
    SaveRecord& r = Save_Current();
    if (r.stampQueueCount <= 0) return 0;
    int id = r.stampQueue[0];
    memmove(r.stampQueue, r.stampQueue + 1, 7);
    r.stampQueue[7] = 0;
    --r.stampQueueCount;
    return id;
}

void Stamp_Earn(int n) {
    SaveRecord& r = Save_Current();
    if (n < 0 || n >= 128) return;
    r.stampEarned[n >> 6] |= 1ull << (n & 63);
    r.stampShown[n >> 6] |= 1ull << (n & 63);
}

void Town_FoundStamp(int town) {
    static const uint8_t kTownStamp[10] = {0, 1, 10, 23, 52, 91, 60, 72, 85, 50};
    if (town >= 0 && town < 10 && Save_Current().areaHidden[town] == 0) Stamp_Queue(kTownStamp[town]);
}

void Team_Join(int slot, int group, int index, uint32_t paint, int body) {
    SaveRecord& r = Save_Current();
    if (slot < 1 || slot > 2) return;
    SaveCar& c = r.cars[slot];
    c.paint = paint;
    c.body = (uint8_t)body;
    r.teammates[slot - 1][0] = (int8_t)group;
    r.teammates[slot - 1][1] = (int8_t)index;
    memcpy(r.teammateSpeeds[slot - 1], kTeammateSpeeds, sizeof(kTeammateSpeeds));   // Ai_CopySpeedTables
    for (int t = 1; t <= 7; ++t) Item_Give(t, c.parts[t]);
}

void Team_Leave(int slot) {
    SaveRecord& r = Save_Current();
    if (slot < 1 || slot > 2) return;
    for (int t = 1; t <= 7; ++t) Save_TakeItem(t, r.cars[slot].parts[t]);
    r.teammates[slot - 1][0] = 0;   // (the index byte stays)
}

void Photo_Take(int p) {
    SaveRecord& r = Save_Current();
    if (p >= 1 && p <= 100) {
        int k = p - 1;
        memcpy(&r.photos[k], &r.cars[0], sizeof(r.photos[k]));
        r.photoTaken[k >> 6] |= 1ull << (k & 63);
        r.photoCursor = (int16_t)k;
        if (r.photoTaken[0] == ~0ull && (r.photoTaken[1] & 0xFFFFFFFFFull) == 0xFFFFFFFFFull) Stamp_Queue(0x60);
    }
    r.stampShown[1] |= 1ull << 31;   // (always: stamp 96's text)
}

void Save_AddDistance(int32_t odometer) {
    SaveRecord& r = Save_Current();
    int32_t v = odometer / 0x8000;
    r.distance += v;
    if ((r.cars[0].partFlags & 0x200) && r.cars[0].billboard < 5) r.sponsorDistance[r.cars[0].billboard] += v;
}

bool Save_Beaten(int group, int index) {
    int w = group == 13 ? 9 : group - 1;
    if (w < 0 || w >= 10 || index < 0 || index >= 64) return false;
    return ((Save_Current().beaten[w] >> index) & 1) != 0;
}

int Team_State() {
    const SaveRecord& r = Save_Current();
    int n = (r.teammates[0][0] != 0) + (r.teammates[1][0] != 0);
    return n == 0 ? 1 : n == 1 ? 2 : 0;
}

namespace {

// D_002A2B70: My City's residents by (group, index); (0, 0) for the slots that aren't anyone's. D_002A2B38: the
// mail each one's moving in sends (-1 none).
const int8_t kResidents[25][2] = {{1, 29}, {6, 34}, {1, 30}, {7, 33}, {0, 0},  {0, 0},  {1, 31}, {0, 0}, {7, 35},
                                  {0, 0},  {6, 35}, {3, 39}, {3, 38}, {3, 37}, {2, 35}, {1, 32}, {4, 25}, {3, 35},
                                  {2, 36}, {4, 26}, {7, 36}, {1, 33}, {2, 37}, {3, 36}, {6, 36}};
const int8_t kResidentMail[26] = {3,  6,  7,  8,  -1, 4,  9,  -1, 10, -1, 11, 12, 13,
                                  14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};
const uint32_t kCityFull = 0x3FFFFFF;

}  // namespace

int Resident_Of(int town, int place) {
    if (town == 0 && place == 0) return -1;
    for (int k = 0; k < 25; ++k)
        if (kResidents[k][0] == town && kResidents[k][1] == place) return k;
    return -1;
}

bool Resident_MovedIn(int k) { return k >= 0 && k < 32 && ((Save_Current().myCity >> k) & 1) != 0; }

void Resident_MoveIn(int k) {
    SaveRecord& r = Save_Current();
    if (k < 0 || k >= 26) return;
    if (!(r.myCity & (1u << k))) {
        r.myCity |= 1u << k;
        if (kResidentMail[k] >= 0) {
            Mail_Send(kResidentMail[k]);
            if (k == 0) r.stampShown[0] |= 1ull << 48;   // stamp 49's text
        }
        // Gonzo (resident 0) opens My City's Q's Factory and, his mail 3 says, the Endurance Run with it. The
        // Endurance Mail extra sends Wonder Realty's mail 5, "You can now play Endurance Run!", then too: the
        // original never sends it (docs/extras.md).
        if (k == 0 && Extras_On(kExtraMail5)) Mail_Send(5);
    }
    // Thirteen in: bit 9 as well. All but bit 4 in: the city's complete, stamp 0x33 and mails 0x1C and 0x1D.
    int in = 0;
    for (uint32_t b = r.myCity; b; b &= b - 1) ++in;
    if (in == 13) r.myCity |= 0x200;
    if (r.myCity == (kCityFull & ~0x10u)) {
        r.myCity = kCityFull;
        Stamp_Queue(0x33);
        Mail_Send(0x1C);
        Mail_Send(0x1D);
    }
}

bool Save_EventFlag(int f) { return (Save_Current().eventFlags[f >> 6 & 1] >> (f & 63) & 1) != 0; }

void Save_SetEventFlag(int f, bool on) {
    uint64_t bit = 1ull << (f & 63);
    uint64_t& w = Save_Current().eventFlags[f >> 6 & 1];
    w = on ? w | bit : w & ~bit;
}

namespace {

const int8_t kRallyRoute[8] = {3, 4, 6, 1, 2, 9, 0, 0};   // D_002A2B30
int32_t g_rallyTimer;
int g_rallyDest;
int32_t g_rallySplits[7];
bool g_rallyRunning;

}  // namespace

void Rally_Checkpoint(int n) {
    if (n < 0 || n > 6) return;
    if (n != 0) {
        g_rallySplits[n - 1] = g_rallyTimer;
        if (n == 6) {
            int32_t total = 0;
            for (int i = 0; i < 6; ++i) total += g_rallySplits[i];
            g_rallySplits[6] = total;
            SaveRecord& r = Save_Current();
            if ((uint32_t)total < (uint32_t)r.rallyBest) r.rallyBest = total;
        }
    }
    g_rallyTimer = 0;
    g_rallyDest = kRallyRoute[n];
}

int Rally_NextCheckpoint() {
    for (int i = 0; i < 6; ++i)
        if (kRallyRoute[i] == g_rallyDest) return i;
    return 6;
}

bool Rally_AtDestination(int town, int place) { return town == g_rallyDest && place == 0; }
int Rally_Destination() { return g_rallyDest; }
int32_t Rally_Split(int i) { return i >= 0 && i < 7 ? g_rallySplits[i] : 0; }
int32_t Rally_Timer() { return g_rallyTimer; }
void Rally_SetRunning(bool on) { g_rallyRunning = on; }
bool Rally_Running() { return g_rallyRunning; }

void Rally_Tick() {
    if (g_rallyRunning) ++g_rallyTimer;
}

void Save_InitVersusPreset() {
    // {body of the car, its teammates' cars} and the teammates' (group, index), per player.
    static const uint8_t kBodies[2][3] = {{0x2D, 0x49, 0x68}, {0, 0x69, 0x8D}};
    static const int8_t kTeammates[2][2][2] = {{{1, 0x21}, {7, 0x0B}}, {{7, 0x1F}, {4, 0x1A}}};
    for (int p = 0; p < 2; ++p) {
        SaveRecord& r = g_records[p];
        strcpy(r.name, p ? GT_002EB490_8 : GT_002EB480_8);   // D_002EB480, D_002EB490
        for (int c = 0; c < 3; ++c) {
            r.cars[c].body = kBodies[p][c];
            r.cars[c].paint = kBodyPaint[kBodies[p][c]];
        }
        memcpy(r.teammates, kTeammates[p], sizeof(r.teammates));
    }
}

const char* Save_TownName(int town) {
    static const char* const kTowns[22] = {
        GT_00328770_9, GT_00328760_10, GT_00328750_9, GT_00328740_9, GT_00328730_15, GT_00328720_13, GT_00328710_14,
        GT_00328700_13, GT_00328488_10, GT_003345E8_7, "???", GT_003345D8_6, "UFO", GT_003345C8_5, GT_003286F0_10, "022", "110", "120",
        "202", "212", "221", "232"};
    return town >= 0 && town < 22 ? kTowns[town] : "";
}

void Results_UpdateQuickRace(SaveRecord& r, int card, int bestLap, int totalTime, int place) {
    Results_UpdateQuickRace(r.quickRace[card], bestLap, totalTime, place);
}

void Results_UpdateQuickRace(SaveQuickRace& q, int bestLap, int totalTime, int place) {
    if (place < q.bestPlace) q.bestPlace = (int16_t)place;
    if (totalTime < q.bestTotal) q.bestTotal = totalTime;
    if (bestLap < q.bestLap) q.bestLap = bestLap;
    q.attempts = (int16_t)(q.attempts + 1);
}

namespace {

// One of Results_UpdateRecords' top-3 inserts: `time` goes in before the first entry that's empty or slower (a tie
// goes after), the rest moving down and the last dropping off, tagged `who`.
void Versus_InsertTop3(uint32_t list[3], int32_t time, uint8_t who) {
    uint32_t entry = (uint32_t)who << 24 | ((uint32_t)time & 0xFFFFFF);
    for (int i = 0; i < 3; ++i) {
        int32_t old = (int32_t)(list[i] << 8) >> 8;
        if ((list[i] & 0xFFFFFF) && time >= old) continue;
        for (int k = 2; k > i; --k) list[k] = list[k - 1];
        list[i] = entry;
        return;
    }
}

void Versus_Count(uint8_t& n) {
    if (n < 0xFF) ++n;
}

}  // namespace

void Results_UpdateVersus(int row, const VersusRace& race, int outcome, const int rival[2]) {
    // Each player's own bests on the row.
    for (int p = 0; p < 2; ++p) {
        SaveVersusCourse& c = g_records[p].versus[row];
        if (race.total[p] < c.bestTotal) c.bestTotal = race.total[p];
        if (race.bestLap[p] < c.bestLap) c.bestLap = race.bestLap[p];
    }
    // Each side's races against the other.
    for (int p = 0; p < 2; ++p) Versus_Count(g_records[p].rivals[rival[p]].races[row]);
    // Both records' top-3 lists, laps then totals: player 1's time, then player 2's, each tagged 0xFF in its own
    // player's record and with the rival index in the other's.
    for (int p = 0; p < 2; ++p) {
        SaveVersusCourse& c = g_records[p].versus[row];
        for (int k = 0; k < 2; ++k) Versus_InsertTop3(c.topLaps, race.bestLap[k], k == p ? 0xFF : (uint8_t)rival[p]);
    }
    for (int p = 0; p < 2; ++p) {
        SaveVersusCourse& c = g_records[p].versus[row];
        for (int k = 0; k < 2; ++k) Versus_InsertTop3(c.topTotals, race.total[k], k == p ? 0xFF : (uint8_t)rival[p]);
    }
    // The win: the Tunnel Race (row 6) goes by place (a tie to player 2); every other row by the outcome.
    int winner = row == 6 ? (race.place[0] >= race.place[1] ? 1 : 0) : outcome == 2 ? 0 : outcome == 1 ? 1 : -1;
    if (winner >= 0) Versus_Count(g_records[winner].rivals[rival[winner]].wins[row]);
}

void Versus_FindRivals(int rival[2]) {
    for (int p = 0; p < 2; ++p) {
        SaveRecord& r = g_records[p];
        const SaveRecord& other = g_records[p ^ 1];
        int i = 0;
        while (i < 20 && r.rivals[i].name[0] && r.rivals[i].id != other.id) ++i;
        if (i == 20) {
            i = 19;
            memcpy(r.rivals[19].name, GT_00333870_7, 8);   // D_00333870, an 8-byte copy
        } else if (!r.rivals[i].name[0]) {
            r.rivals[i].id = other.id;
            strcpy(r.rivals[i].name, other.name);
        }
        rival[p] = i;
    }
}

const char* Save_PlaceName(int place) { return place >= 0 && place < 24 ? kPlaces[place] : " --"; }

}  // namespace game
