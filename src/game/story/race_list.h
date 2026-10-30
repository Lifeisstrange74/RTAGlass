// The Adventure's races: the table every town's Q's Factory picks from (D_002BDDD0), the towns' ranges of it
// (D_002BE000) and the course names (D_002BE398). See docs/messages.md ("The end codes' windows").
#pragma once

namespace game {

// A row of D_002BDDD0 (16 bytes; +8 and +12 two functions, not used here) with its setup (+4).
struct AdventureRace {
    unsigned char course;   // +0: the course id (Race_CourseName)
    unsigned char cars;     // +1: 24, or 2 for the race against the president
    unsigned char laps;     // +2
    unsigned char rank;     // +3: the license it needs: 0 C (none), 1 B, 2 A, 4 none shown; also its prize class
    // The setup's +0: the opponents, (group, index) of D_002C22C8 for cars - 1 cars (Race_CreateCars).
    const unsigned char (*opponents)[2];
    // The setup's +4: every opponent's parts record (Car_Init; its +0xE the part flags).
    unsigned char parts[16];
};

// Races 0-27 are the towns', 28-34 the GrandPrix's stages, 35-55 the events (kFirstEvent on, D_002BE018).
const int kAdventureRaces = 56;
const int kTownRaceRows = 28, kFirstEvent = 0x23;
const AdventureRace& AdventureRace_Get(int race);
// The town's races (D_002BE000[town]): the first and how many (0 for town 0, the garage).
void AdventureRace_Town(int town, int* first, int* count);
// The rank text (D_002A2B08): " RANK C", " RANK B", " RANK A", "GrandPrix", "".
const char* AdventureRace_RankName(int rank);

// Race_Outcome (0x0023D8C8, D_00335704): how the last Adventure race went, as Race_SetupTask keeps it for the talk
// after it (pre-code 0x0B): 0 retired, else what its results told (AdventureResults::Update). -1 reads it.
int Race_Outcome(int set = -1);

// The race names by course id (D_002BE398); 0x0B and 0x10 share 0x06's and 0x0F's.
const char* Race_CourseName(int id);

}  // namespace game
