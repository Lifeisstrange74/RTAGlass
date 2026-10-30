// The save data: a reimplementation of the game's player records (D_0177F760, two of 0x3448 bytes) and of what the
// memory card keeps of them. On the PS2 the card's /BASLUS-20398/BASLUS-20398 file is one player's record, copied
// byte for byte (func_00228708 writes it, func_00228510 reads it; there is no checksum or version), next to the
// icon files. The PC keeps the same record, field for field, in a text file (save_file.cpp). See docs/save.md
// for every field and how it was found.
//
// SaveRecord has the PS2's layout exactly (little endian, both), so a record read from a PS2 card is the same bytes.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace game {

#pragma pack(push, 1)

// A car: the player's (slot 0) or a teammate's (1, 2). The first 12 bytes are what a photo keeps.
struct SaveCar {
    uint32_t paint;       // +0x00: RGB in bits 0-23 (six 4-bit colour indices), bits 24-31 a copy of `wheelColor`
    uint8_t body;         // +0x04: the body (CARn/Qnn.BIN)
    uint8_t wheelColor;   // +0x05: the wheels' colour, 0-11 (the Paint Shop's Wheel; TIRE.BIN's palette)
    uint8_t wheel;        // +0x06: a copy of parts[7]
    uint8_t billboard;    // +0x07: the sponsor of the Options part's billboard (0-4)
    uint16_t partFlags;   // +0x08: 0x1/0x2 engine type, 0x4 Flight Wing, 0x10 lights, 0x20 police light, 0x40
                          // propeller, 0x100 water ski, 0x200 billboard, 0x400 big tires, 0x1000 wing set, 0x2000
                          // jet turbine, 0x4000 sticker
    uint8_t unused0A[2];
    uint8_t parts[16];    // +0x0C: the part fitted in each category: 0 the starting body, 1 tires, 2 engine,
                          // 3 chassis, 4 transmission, 5 steering, 6 brakes, 7 wheels, 8 lights, 9 wing set,
                          // 10 special part, 11 option, 12 sticker, 13 horn, 14 meter (15 unused)
};

struct SavePhoto {        // a picture taken: the player's car's first 12 bytes then
    uint32_t paint;
    uint8_t body, wheelColor, wheel, billboard;
    uint16_t partFlags;
    uint8_t unused[2];
};

// 2 Player: another save met in a race (func_00237AC0 finds or adds it).
struct SaveVersusRival {
    uint64_t id;          // its record's +0x3438
    char name[20];        // empty: a free entry (the 20th becomes "ANYBODY" when all are taken)
    uint8_t wins[10];     // races won against it, by 2 Player course row
    uint8_t races[10];    // races run against it
};

// 2 Player: a course row's records. A top-3 entry is a time (bits 0-23, frames) and whose (bits 24-31: 0xFF this
// record's player, else a rivals[] index); 0 is empty.
struct SaveVersusCourse {
    uint32_t topLaps[3];
    uint32_t topTotals[3];
    int32_t bestLap;      // this record's player's own
    int32_t bestTotal;
};

// Quick Race: a course card's records (CourseInfo_DrawRecords shows them).
struct SaveQuickRace {
    int32_t bestLap;      // frames
    int32_t bestTotal;    // frames, from green to the line
    int16_t attempts;     // races finished
    int16_t bestPlace;    // 0 = 1st; 0xFF none
};

struct SaveRecord {
    SaveCar cars[3];                 // 0x0000
    SavePhoto photos[100];           // 0x0054: the photo album
    uint8_t unused504[4];            // 0x0504
    uint64_t photoTaken[2];          // 0x0508: bit n-1: photo n taken (100)
    uint64_t stampEarned[2];         // 0x0518: stamp n earned (100)
    uint64_t stampShown[2];          // 0x0528: stamp n's text shown in the book
    uint64_t eventFlags[2];          // 0x0538: the event scripts' flags (128)
    uint64_t pickupsLeft[2];         // 0x0548: the field's pickups (D_002A3970) not yet collected (100; all set)
    uint64_t beaten[10];             // 0x0558: opponents placed behind, a word per group, a bit per index
    uint8_t mail[128];               // 0x05A8: the mail box, oldest first: bits 0-6 the mail, bit 7 read
    int32_t mailCount;               // 0x0628
    char name[19];                   // 0x062C: "UnnamedRacer" (name entry: up to 9)
    char currency[5];                // 0x063F: "gold" (up to 4)
    uint8_t unused644[2];            // 0x0644
    char greeting[6];                // 0x0646: (up to 5)
    uint8_t unused64C[5];            // 0x064C
    int8_t license;                  // 0x0651: 0 none, 1 B, 2 A, 3 Super A
    uint8_t homeInterior;            // 0x0652: the garage's Redecorate choice
    uint8_t unused653;               // 0x0653
    int32_t money;                   // 0x0654: up to 999999
    int32_t distance;                // 0x0658: metres driven
    int32_t sponsorDistance[5];      // 0x065C: metres driven with each billboard sponsor, not yet paid
    uint32_t myCity;                 // 0x0670: My City: bit k, who has moved in (0x100 at start)
    int8_t teammates[2][2];          // 0x0674: teammate 1, 2: (group, index) of D_002C22C8; 0 none
    SaveVersusRival rivals[20];      // 0x0678
    SaveVersusCourse versus[10];     // 0x0A38
    SaveQuickRace quickRace[8];      // 0x0B78: by course card
    uint32_t areaHidden[22];         // 0x0BD8: a word per area, a bit per place not yet found
    uint32_t items[16][5];           // 0x0C30: kind 0 bodies owned and 15 flags (160 bits); 1-14 parts owned,
                                     // 0-5 of each (a bit per item, one word per copy)
    uint32_t bodyPaint[151];         // 0x0D70: each body's paint, kept while another is fitted
    uint8_t unusedFCC[0x24];         // 0x0FCC
    uint8_t adventurePlace[3][26];   // 0x0FF0: Adventure records by course: the player, teammate 1, teammate 2;
    uint8_t unused103E[2];           //         best place 0 = 1st, 0xFF none
    int32_t adventureLap[3][26];     // 0x1040: best lap
    int32_t adventureTotal[3][26];   // 0x1178: best total time
    int32_t rallyBest;               // 0x12B0: Q's Rally (frames)
    int32_t figure8Best[5];          // 0x12B4: Figure 8, steps 1-5
    int32_t skiJumpBest;             // 0x12C8: metres
    int32_t curlingBest;             // 0x12CC: points
    int8_t golfHole[9];              // 0x12D0: strokes over par (0x7F none)
    int8_t golfTotal;                // 0x12D9
    uint16_t townsUnvisited;         // 0x12DA: a bit per town (D_002BE438)
    int8_t saveTown;                 // 0x12DC: where it was saved
    int8_t radioStation;             // 0x12DD: 0 off
    int16_t radioVolume;             // 0x12DE: 0-0x80
    int16_t stampCursor;             // 0x12E0
    int16_t photoCursor;             // 0x12E2: the photo album's (0-99; Photo_Take sets it)
    uint8_t unused12E4[4];           // 0x12E4
    uint16_t buttons[10];            // 0x12E8: the key setup, pad bits: accelerate, brake, reverse, R2 action, L1,
                                     // L2, horn, camera, lights (= horn), map
    uint8_t unused12FC[4];           // 0x12FC
    uint8_t teammateSpeeds[2][16][256]; // 0x1300: each teammate's AI speed tables (a byte a zone), as they've learned
    int32_t stampQueueCount;         // 0x3300
    uint8_t stampQueue[8];           // 0x3304: stamps waiting to be shown
    int32_t bank;                    // 0x330C: savings
    int32_t gpxStage;                // 0x3310: World GPX stages done (0-6)
    int8_t gpxPoints[7][24];         // 0x3314: [stage][car]
    uint8_t unused33BC[4];           // 0x33BC
    uint8_t adventureTeammates[26][2][2];  // 0x33C0: the teammates' (group, index) each Adventure record was set with
    uint8_t unused3428[0x10];        // 0x3428
    uint64_t id;                     // 0x3438: name entry's: bytes 3-7 the name's first five characters
    uint8_t unused3440[8];           // 0x3440
};

#pragma pack(pop)

static_assert(sizeof(SaveCar) == 0x1C, "SaveCar");
static_assert(sizeof(SaveVersusRival) == 0x30, "SaveVersusRival");
static_assert(offsetof(SaveRecord, mail) == 0x5A8, "mail");
static_assert(offsetof(SaveRecord, license) == 0x651, "license");
static_assert(offsetof(SaveRecord, versus) == 0xA38, "versus");
static_assert(offsetof(SaveRecord, quickRace) == 0xB78, "quickRace");
static_assert(offsetof(SaveRecord, bodyPaint) == 0xD70, "bodyPaint");
static_assert(offsetof(SaveRecord, adventurePlace) == 0xFF0, "adventurePlace");
static_assert(offsetof(SaveRecord, buttons) == 0x12E8, "buttons");
static_assert(offsetof(SaveRecord, teammateSpeeds) == 0x1300, "teammateSpeeds");
static_assert(offsetof(SaveRecord, adventureTeammates) == 0x33C0, "adventureTeammates");
static_assert(sizeof(SaveRecord) == 0x3448, "SaveRecord: the memory card file's size");

const int kNoRecordTime = 0x7FFFFFFF;
const int kNoRecordPlace = 0xFF;

// The two records (player 1, player 2) and the current one (D_0177EA50).
SaveRecord& Save_Record(int player);
SaveRecord& Save_Current();
void Save_SetCurrent(int player);
int Save_CurrentPlayer();   // which record Save_Current is (0 or 1)
// The PC's guard for the developers' test menu (flow/test_menu.h): record 0 holds its complete record, not the
// player's. Set by the test menu; cleared by Save_InitBoth and when the title's main menu opens. While it's set the
// save point's overwrite question treats any save in the slot as someone else's ("This is not your saved data.",
// No first).
void Save_SetTestRecord(bool on);
bool Save_TestRecord();

// func_002277B0: a new record: all zero, then the defaults (names, records none yet, the key setup, the teammates'
// speed tables).
void Save_Init(SaveRecord& r);
// func_00229FD8: both records new; player 1's gets 1000 money and every body's own paint (Quick Race, New Game).
void Save_InitBoth();
// func_00229F20: the default key setup.
void Save_ResetButtons(SaveRecord& r);
// Save_InitVersusPreset (func_00227B28): 2 Player's Race Right-Away records, after Save_InitBoth: "PLAYER 1" in
// body 0x2D and "PLAYER 2" in body 0, each with two teammates and their cars, all in their bodies' own paint.
void Save_InitVersusPreset();
// The save's town names (D_002BE438, by +0x12DC): "My Garage", "Peach Town" ... ("" past the English ones).
const char* Save_TownName(int town);
// func_00229F70: player 1 starts with `body` (New Game): owned, fitted, in its own paint.
void Save_SetStartingCar(int body);

// The Adventure's progress in the current record.
// Save_HasItem (func_0023D488): kinds 0 (bodies) and 15 (things) are a bit each (1 owned); of kinds 1-14, how many
// copies of the part are owned (0-5).
int Save_HasItem(int kind, int n);
// func_0023D548: no more of item (kind, n) can be had: a body or thing owned, a part's fifth copy.
bool Save_ItemFull(int kind, int n);
// The field's ChoroQ coins (save +0x548, a bit per coin still out). Pickup_Taken (func_0023DBF0): coin i (1-100)
// taken; 0: all of them. Pickups_Left (func_0023D968): how many are still out. Pickup_Take: coin i (0-99) taken.
bool Pickup_Taken(int i);
int Pickups_Left();
void Pickup_Take(int i);
// Mail_Send (func_0023DF38): mail m into the box (save +0x5A8), the oldest dropped when all 128 are in.
void Mail_Send(int m);
// func_0023C6B0: one of mails base .. base + range - 1 sent, from seed % range on, the first not in the box (the last
// tried if all are). The fan mail (func_0023DFA0: 0x1F, 49 of them) and the mileage mail (func_0023DFC0: 0x50, 43).
int Mail_SendRandom(int base, int range, int seed);
// Place_Hidden (func_0023D5F8): `place` of `town` not yet found (save +0xBD8, a bit per place).
bool Place_Hidden(int town, int place);
// Place_MarkFound (func_0023C7B0), as a building's scene ends: the place found; for place 0 the town visited
// (+0x12DA); and a town's stamp text (D_002A2B20) shown in the book.
void Place_MarkFound(int town, int place);
// Save_TakeItem (func_0023D390): item (kind, n) gone; of a part, its last copy.
void Save_TakeItem(int kind, int n);
// Item_Give (func_0023D2C0): item (kind, n) owned: kinds 0 and 15 a bit; a part one more copy (of 5).
void Item_Give(int kind, int n);
// Save_Score (func_0023D058): how far record `player` is in the Adventure, for 2 Player's trading prices: 10 a
// teammate, 2 a town visited (of the first 10), 20 a license level, 2 a ChoroQ coin taken, by each of the first 25
// Adventure races' best place 8 (1st), 4 (2nd, 3rd), 3, 2, 1 (4th-6th), 2 a stamp earned and 2 a photo taken.
int Save_Score(int player);
// Money_Pay (func_0023DC58): `amount` paid (negative: earned, up to 999999), SE 0x305 if it isn't 0. False, and
// nothing paid, if the money is short.
bool Money_Pay(int amount);
// The stamps waiting for the book (save +0x3300, 8 at most, first in first out). Stamp_Queue (func_0023C8A8): stamp
// `id` (1-100) queued unless it's earned or queued already (stamps 5, 17, 30, 50 and 100 send their mail); false if
// not. Stamp_Dequeue (func_0023D818): the next one, 0 none. Stamp_Earn (func_0023D6E0): stamp n + 1 earned and its
// text shown. Town_FoundStamp (func_0023D878): every place of a town 1-9 found queues its stamp (D_002A2B20).
bool Stamp_Queue(int id);
// Stamp_Earned (func_0023D628): stamp s (1-based) earned; 0: all of them.
bool Stamp_Earned(int s);
int Stamp_Dequeue();
void Stamp_Earn(int n);
void Town_FoundStamp(int town);
// The team (save +0x674). Team_Join (func_0023DA58): (group, index) joins as teammate `slot` (1 or 2): that car
// takes the person's paint and body (keeping the last teammate's parts), its speed tables start over; then
// (func_00239378) the car's parts 1-7 are added to the items. Team_Leave (func_00239508's Yes): teammate `slot`'s
// parts 1-7 are taken out of the items and the slot emptied (its car stays as it was).
void Team_Join(int slot, int group, int index, uint32_t paint, int body);
void Team_Leave(int slot);
// Photo_Take (func_0023C5C0): photo p (1-100) of the player's car as it is now (its first 12 bytes) taken; all 100
// queue stamp 96.
void Photo_Take(int p);
// func_0022E718, as the field hands over: the car's odometer (+0x204, 1/32768 m) into the save's distance and, with
// a billboard (part flag 0x200), its sponsor's distance.
void Save_AddDistance(int32_t odometer);
// Save_Beaten (func_0023DCC0): driver record (group, index) placed behind the player in an Adventure race (+0x558: a
// word per group, My City's 13 in word 9).
bool Save_Beaten(int group, int index);
// Team_State (func_0023DBC8): 1 no teammates yet, 2 one, 0 the team is full.
int Team_State();
// My City's residents (D_002A2B70: 25 people of the other towns, by (group, index)): Resident_Of the one (town,
// place) is, or -1; Resident_MovedIn (func_0023DE30) bit k of +0x670; Resident_MoveIn (func_0023CB68) moves k in,
// with their mail (D_002A2B38), and the city's own bits and mail as it fills.
int Resident_Of(int town, int place);
bool Resident_MovedIn(int k);
void Resident_MoveIn(int k);

// The event scripts' flags (save +0x538): Save_EventFlag (func_0023D778), Save_SetEventFlag/Save_ClearEventFlag.
bool Save_EventFlag(int f);
void Save_SetEventFlag(int f, bool on);

// Q's Rally, while event flag 80 is up (the Rally Center's sign-up): the route D_002A2B30 (Sandpolis, Chestnut
// Canyon, White Mountain, Peach Town, Fuji City, My City's Q's Factory). Not saved: the timer D_00335730 (frames,
// shown as game-clock h:mm), the destination D_00335734 and the splits D_017870C8 (D_01787008[0x30..0x36]).
// Rally_Checkpoint (func_0023DD30): checkpoint n's split (n 1-6; at 6 the total and the best, save +0x12B0), the
// timer back to 0 and the next destination. Rally_NextCheckpoint (func_0023DDB8): the destination's index 0-5 (6
// none). Rally_AtDestination (func_0023DE10): (town, place) is the destination's Q's Factory. Rally_Split: split i
// (6 the total). The timer runs while the field's race flag 0x1000 is up (Rally_SetRunning): set as the car drives
// out of a building with flag 80, cleared by a door and by Warp; Rally_Tick on the field, in its talks and on the
// Radio screen.
void Rally_Checkpoint(int n);
int Rally_NextCheckpoint();
bool Rally_AtDestination(int town, int place);
int Rally_Destination();
int32_t Rally_Split(int i);
int32_t Rally_Timer();
void Rally_SetRunning(bool on);
bool Rally_Running();
void Rally_Tick();

// func_00237C48 (the race's records into the save), Quick Race (mode 1): card `card`'s best lap, best total and
// best place, each kept if this race beat it, and one more attempt. Race_SetupTask calls it when the player has
// finished (after the replay, if there is one); Retire and falling off don't.
void Results_UpdateQuickRace(SaveRecord& r, int card, int bestLap, int totalTime, int place);
// The same into one card's records (the PC's Test Course keeps its own, outside the save: course_select.h).
void Results_UpdateQuickRace(SaveQuickRace& q, int bestLap, int totalTime, int place);

// A 2 Player race's outcome for the records, by player: place (0 = 1st), total time and best lap (frames).
struct VersusRace {
    int place[2];
    int32_t total[2];
    int32_t bestLap[2];
};

// func_00237C48's 2 Player branch (mode & 2), on course row `row`: each record's best lap and best total on the row,
// both players' times into both records' top-3 lists (the record's own player tagged 0xFF, the other by its rival
// index), one more race against the rival on each side, and a win for the winner: by place on row 6, else by the
// outcome (Race_Outcome: 2 player 1, 1 player 2). The records only reach the files through Save&Exit.
void Results_UpdateVersus(int row, const VersusRace& race, int outcome, const int rival[2]);

// Versus_FindRivals (func_00237AC0), Versus_Task's first frame: each player's rivals[] entry for the other player's
// record (by its +0x3438 id), or the first free entry, given that id and the other's name. With all 20 taken it's the
// last, renamed "ANYBODY" (id and counts kept). rival[p] is player p's index (pad +0x1C, +0x1E).
void Versus_FindRivals(int rival[2]);

// The race results' place names (D_002A8ED0): " 1ST" .. "24TH".
const char* Save_PlaceName(int place);

}  // namespace game
