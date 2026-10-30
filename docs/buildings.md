# The Adventure's buildings

Every place with a door is a table of the talks (`data/story/messages.txt`, D_002A7ED0[town][place]; the
town cars' talks share the numbering). Room_SetupTask and Shop_SceneTask run them all the same way: the person, the
talk, and what its end codes start (messages.md). This page is what the places need beyond that.

## Which places count as visited

Place_MarkFound (func_0023C7B0) clears the place's bit of save +0xBD8 (`areaHidden`, a word per area) as its talk
ends, and code 0x14 does it as it sends the car on. Town_FoundStamp (func_0023D878) queues the town's "Visited all
the houses" stamp (D_002A2B20: 1, 10, 23, 52, 91, 60, 72, 85, 50 for towns 1-9) once the town's word is 0. The word
starts with the bits of NewGame_Setup's table (save_data.cpp `kAreaHidden`), which leave out the Quick-Pic Shops:
Peach Town's 0x3DFFF is places 0-17 but 14, and its shops are 18-27.

## The places that start a game (end code 0x03 r)

Races 0-27 are the towns' races (D_002BDDD0), 28-34 the GrandPrix's stages and 35 on the events. Race_Setup takes
an event's row from D_002BE018 + 16 (r - 35): course, cars, laps, rank, the setup (the rival and their parts, or
none for a game played alone) and the event's task at +8. After an event Race_SetupTask shows no results: the fade
goes black, func_00238950 sets Race_Outcome for races 0x26 and 0x28 (the others keep their task's), and the talk
goes on by it (pre-code 0x0B).

| Race | Place | Game | Course | Task | Port |
|---|---|---|---|---|---|
| 35 | Fuji City 8, the King | Sliding Door Race | 0x13 A01 | 0x002595F0 Race_SlidingDoorTask | yes |
| 36 | Fuji City 17, Natsuo | Highway Race | 0x14 A02 | 0x0025FF00 Race_HighwayTask | yes |
| 37 | Sandpolis 10, Johnny | Drag Race | 0x16 A04 | 0x00260638 Race_DragTask | yes |
| 38 | My City 18, Akiban | Tunnel Race | 0x1C A10 | Race_Task (the player drives Akiban's car, body 12, with the row's parts: events.md) | yes |
| 39 | Papaya Island 13, Micky | Beach Flag | 0x25 A19 | 0x002638C0 Race_BeachFlagTask | yes |
| 40 | Cloud Hill 4, Travis | Single Lap Race | 0x26 A20 | Race_Task with SingleLap_FallTask (0x00263C38: both cars on the line at green; the first below 280 decides it, then a 0xC0-frame fade) | yes |
| 41 | Fuji City 9, the maze's guard | Treasure Hunting Maze | 0x12 A00 | 0x0025F6A0 Race_MazeTask | yes |
| 42 | Chestnut Canyon 5 | Rock Climbing | 0x15 A03 | 0x0025FBF8 Race_RockClimbTask | yes |
| 43 | Mushroom Road 3, Jumbo | Golf | 0x17 A05 | 0x00251B60 Race_GolfTask | yes |
| 44 | Sandpolis 8 | Roulette | 0x18 A06 | 0x00260DC0 Race_RouletteTask | yes |
| 45 | Sandpolis 6 | Figure 8 | 0x19 A07 | 0x0025A120 Race_Figure8Task | yes |
| 46 | Sandpolis 7 | Soccer | 0x1A A08 | 0x00254430 Race_SoccerTask | yes |
| 47 | Cloud Hill 6, Smiley | Rainbow Jump | 0x1B A09 | 0x00254D20 Race_RainbowTask | yes |
| 48 | Papaya Island 5 | Obstacle Course | 0x1D A11 | 0x0025EB70 Race_ObstacleTask | yes |
| 49 | My City 10, Dayan | Which-way? Race | 0x1E A12 | 0x00262678 Race_WhichWayTask | yes |
| 50 | Chestnut Canyon 7, Boss Rorke | Volcano Course | 0x1F A13 | 0x0025E630 Race_VolcanoTask | yes |
| 51 | White Mountain 18 | Curling | 0x20 A14 | 0x002631A0 Race_CurlingTask | yes |
| 52 | Peach Town 17, the cave | Barrel Dodging | 0x21 A15 | 0x0025E278 Race_BarrelTask | yes |
| 53 | White Mountain 7 | Ski Jumping | 0x23 A17 | 0x00263E68 Race_SkiJumpTask | yes |
| 54 | Papaya Island 15, Shimisa | Fishing | 0x24 A18 | 0x0025DBB8 Race_FishingTask | yes |

Race 26 is the president's (Cloud Hill 9). Every game is in the port (minigames.md for the Adventure's own, events.md
for those 2 Player shares), and so are the GrandPrix's stages (28-34): every race a table names runs.

## The stops

Towns 10-21 are the places away from the towns. Their building is place 0: Laz (10, the windmills' number), the UFO
(12, by night, not during Q's Rally: flag 0x50 shuts it), Benji (13), Picarl (14) and Seidon (18, the temple under the sea). Their rooms
are the SHOP/T09 ... T20 pictures (those without one share T00's blank). Towns 13 and 18 also have a field talk
(Benji at place 5, Orpheus at place 3).

## My City and My Garage

A My City place opens on the field once its resident has moved in (MyCity_PlaceOpen, func_0023DEB8: D_002A2B58 the
resident of each place, bit k of save +0x670), but for Wonder Realty (6) and the places from 23, which are always
open. Its block (11) has its own model tables and collision for the buildings the residents bring (field.md, "My
City's buildings"), and three objects: Ms. Flower's flowers (stamp 41), the rally's GOAL banner and the gold statue
(stamp 50).

Who moves in: 22 residents by a talk's pre-code 0x17; resident 5 (Quick-Pic Shop No.29) after the Endurance Run
(Results_DrawRace: race 0x18, any finish, with stamp 49); resident 9 when 13 are in; resident 25 (the Rally Center)
once towns 1-4, 6 and 9 are all visited (Place_MarkFound); resident 4 when the rest are in, which completes the city
(0x3FFFFFF: stamp 51, mails 0x1C and 0x1D). Resident 7 is My Garage's building, moved in by Wonder Realty's message 4.

Roberts's Big Tower (place 9): pre-code 0x14 (9, 29) leaves the building at My City's way out 29 (D_002C0698[9] has 30
entries: 23-28 are -1, 29 a way out with no door at (1351.2, 945)), on the tower's roof, with stamp 45. Akiban's Tunnel
Race (place 18): the player races Akiban's own car (D_0177EA5C, body 12) with the race row's parts, in lane 3. My City
Cinemas (place 8) replays the ending with stamp 100 (end code 0x18, result 15; in the port, finish.md "The ending").

**Q's Rally** (the Rally Center, place 20): sign-up sets event flag 80 (0x50) and Rally_Checkpoint(0); the route
D_002A2B30 is Sandpolis, Chestnut Canyon, White Mountain, Peach Town, Fuji City, My City's Q's Factory, each factory's
pre-code 0x0E picking its message by Rally_NextCheckpoint and 0x13 n storing the split. The timer (D_00335730, frames,
shown as game-clock h:mm) runs while race flag 0x1000 is up: set as the car drives out of a building with the flag,
cleared by any door; it ticks on the field, in field talks and on the Radio screen. FieldMap_Task draws "+++ Q's Rally
+++", DESTINATION and TIME; Warp's signs warn "Warning! Warping ends the rally", and warping ends it. The finish (#34)
keeps the best in save +0x12B0 and sends you to the Rally Center: body 88 and stamp 48 the first time. The port:
save_data.cpp (Rally_*), message_task.cpp (0x0E, 0x13, `$T0`-`$T7`), field.cpp, field_map.cpp, field_menu.cpp.

**My Garage** (town 0; Garage_Task 0x0022D1E0, Garage_SceneTask 0x0022CD38; game/story/garage_scene.cpp): reached
by Wonder Realty's end code 0x14 (0, 0), then by its door on block 11 and Warp once visited (bit 0 of +0x12DA). A
3D room, SHOP/GARAGE.BIN chunk n by the interior (+0x652; modelconv's `models/SHOP/GARAGE/roomN.obj` and chain B,
`roomN_b.obj`, the furniture's shadows), camera D_0029F3F0, the car driving in for 50 frames, then sprites from the
chunk's 640 x 256 sheet: a trophy for each of the 24 town races won (+0xFF0), the Grand Prix picture with flag 87,
and the 13 story items on show. The first visit queues stamp 40 and gives the Stylish Pattern and mails 2, 1, 0.
The menu: Check E-mail (the mail reader, Mail_ReaderTask 0x00264498, mail_screen.cpp, texts `data/story/mail.txt`
from D_002DB948), Redecorate (the interiors owned), Go play (the games of D_0029F440 whose buildings are found, straight
into the building) and Go outside (the stamp book, then the field at the garage's door). With stamp 40, every 6000 m
driven sends a mileage mail (SE 0x31E) and a race placed 1st-6th the fan club's stamp 95 and mail, then fan mail.
Chain B is black at half alpha, blended (vf17 = (1, 1, 1, 0.5) from D_0029F420; every room's chain B is program
0x50). The sprites' +0x44 word is their depth: the trophies and decorations 5 and 7 at 0x100000, decoration 11 at
0x200000, the rest at the front; the port draws them all on top. Not checked on the PS2: whether the car ever covers
one.

## The things on the roads

Field_DrawObjects' handlers draw them (field.md, "Objects"): Jones's seven gemstones (items 0x20-0x26, until stamp 9)
and four of the stories' items. The gems are out from the start: Jones's talk (Peach Town 14) only gives the hints
and the reward.

## Not in yet

Nothing of the buildings: Cloud Hill's are in with its field (field.md, "Cloud Hill"), the Secretary (place 8) and
Forest (9) behind the gate that opens after the Grand Prix, and the ending code 0x18 starts is in (finish.md, "The
ending").
