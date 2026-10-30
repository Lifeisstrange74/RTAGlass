// Q's Factory's Change parts: a high-level reimplementation of the scene's parts window func_0022BB88 (a scrolling
// category list at the top left) with its item carousel func_00246250 in the factory's mode (a card for each owned
// copy of the category's parts, with the part's picture or the car, and the part's description beside it). Cross
// fits the part in front (Part_Fit, func_0023C418); Triangle leaves. The same carousel serves the shops of
// func_00269E88: the towns' Parts Shops and Body Shops (func_00269568 and func_00269900: the shop's stock, its price
// and a window to buy what's in front) and My City's Recycle Shop (func_00268E40: the player's own parts, sold at
// half price), which is also 2 Player's Trade Items, where one player sells a part to the other. 2 Player's Change
// Parts card opens the factory's screen for either player's cars. See docs/parts.md.
#pragma once

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../race/car.h"
#include "../race/race_light.h"
#include "../save/save_data.h"
#include "../ui/window.h"

namespace game {

// func_0023C418: fits `item` of category `cat` on `car` (parts[cat], and the part flags, wheel and billboard that
// go with it). Bodies (category 0) are swapped by the parts window itself (PartsScreen).
void Part_Fit(SaveCar& car, int cat, int item);
// The name of `item` of category `cat` (func_00243468; a body's is its Q number).
const char* Part_Name(int cat, int item);
// Item_Name: kind 15's things (D_002F0810 + 8 n: "Voucher", "Coin Radar" ...; field_menu_screens.cpp), 0-47.
const char* Item_Name(int n);
// Part_Price (func_00243670): a part's price (its table's +8); every body is 500.
int Part_Price(int cat, int item);
// func_00243A78: 2 Player's price of a part for `player`, by the two players' Save_Score difference d (theirs less
// the other's): selling (Part_Price x (3000 + d) / 4000) or buying (x (2000 - d) / 2000).
int Trade_Price(int cat, int item, int player, bool selling);
// The money after paying `price` (capped at 999999; a negative price is money in), drawn as Money_DrawAfter
// (func_0022E8C0) does below the scene's money box, or (`wallet`) as Wallet_Draw (func_0022E7F0) does at the bottom
// right under the money now; the arrow between bobs by `age`.
void Money_DrawAfter(gfx::Draw2D& d, int price, int age, bool wallet);

class PartsScreen {
public:
    PartsScreen(gfx::Draw2D& draw, const std::string& dataDir, Cars& cars);

    // The parts of car `car` of player `side`'s team (D_0177EA50: 0 the player, 1 and 2 the teammates), on that
    // player's pad. `picture` is the room's picture, whose corner holds the cards' art. In 2 Player (`versus`) the
    // Meter row has no carousel.
    void Start(int car, const gfx::Texture* picture, int side = 0, bool versus = false);
    // func_00269E88's screens for player `side`: Room_SceneKind `kind` 1 the Parts Shop, 2 the Body Shop (both
    // with `town`'s stock), 4 the Recycle Shop; with `trade`, the Recycle Shop as 2 Player's Trade Items
    // (func_00269E50), `side` selling to the other player.
    void StartShop(int kind, int town, const gfx::Texture* picture, int side = 0, bool trade = false);
    // One frame, drawn over the room. 0 while it runs, -1 once left (Triangle). Changed() says that the car's
    // look changed on this frame (its body, or a part drawn on it), for the scene to rebuild it.
    int Update(const platform::Input& input);
    bool Changed() const { return changed_; }

private:
    int Category() const { return row_ == 14 ? 0 : row_ + 1; }
    int Run(const platform::Input& input);      // Update with side_'s record current
    bool Parts_StartCarousel();                 // func_00246250's first frame; false: nothing to list
    int Parts_Entry(int card) const;            // the entry card `card` shows
    void Parts_DrawCards();
    void Parts_DrawCard(int entry, int x, int y, int size, uint32_t grey);
    void Parts_Pick(int item);                  // func_0022BB88 on the carousel's Cross
    void Parts_LoadBodies();
    void Parts_TurnCarousel(const platform::Input& input);   // func_00246250's turn, and the cards drawn
    void Parts_DrawDescription(int item);       // func_0022E960
    // The shops.
    struct Dialog {
        Window w;
        std::string lines[5];
        bool open = false;
        int owner = 0;         // whose pad and money (+0x0C)
        int price = 0;         // shown on line priceLine, with the owner's currency; -1 none
        int priceLine = 2;
        int age = 0;
    };
    void Shop_Stock(int cat);                   // entries_: what the shop sells (func_00267F48), or has to sell
    int Shop_Update(const platform::Input& input);
    void Shop_Pick(int item);                   // the carousel's Cross: the window to buy or sell it, or why not
    void Shop_Notice(int mode, int item);       // func_00268428
    void Shop_Ask(Dialog& dg, int mode, int item);   // func_00268890
    void Shop_OpenWindow(Dialog& dg, const char* const* lines, int count, bool yesNo, int owner, int offset);
    void Shop_DrawWindow(Dialog& dg);           // the price on its line
    void Shop_DrawPrice(int item);              // ItemCarousel_Task's box under the description
    void Shop_DrawWallets();                    // the money before and after, while the windows wait
    void Shop_Sell();                           // the Yes(es): func_00268E40 state 3

    gfx::Draw2D& d_;
    std::string dataDir_;
    Cars& cars_;
    RaceLight light_;
    const gfx::Texture* picture_;
    int car_;
    int side_ = 0;       // the record (and pad) the screen is for: +0x1C bit 0 (bit 4 for the factory's)
    int pad_ = 0;        // the pad the list and the carousel read
    bool versus_ = false;
    Window list_;
    int row_;            // P+0x27: the row the carousel is for, -1 none
    bool remake_;        // P+2 == 0: make the carousel again (at the start and after a fit)
    bool carousel_;      // the child is there (some rows have none)
    std::vector<int> entries_;   // S+0x5420: the items (bodies) listed, a card each
    int fitted_;         // S+0x5404: the category's fitted item
    int sel_;            // C+0x27: the entry in front
    int scroll_;         // C+0x02: +-8 when a turn starts, a step a frame back to 0
    int frames_;         // C+0x08: frames since the carousel started or last turned
    bool changed_;
    // The shops (the shop task's own fields).
    enum ShopState { kShopBrowse = 1, kShopNotice = 2, kShopConfirm = 3 };
    int shop_;           // Room_SceneKind: 0 the factory's Change parts, 1 the Parts Shop, 2 the Body Shop, 4 recycling
    bool trade_ = false; // +0x1C bit 1: 2 Player's trading
    int town_;           // info+0x23: whose stock
    int shopState_;      // +0x25
    int shopIndex_;      // +0x27: the entry the carousel starts on (the one last picked)
    int shopItem_;       // the item the windows are about
    int price_;
    int pending_ = 0;    // +0x20: the Yes answers still to come (-1: a No)
    bool soldOut_ = false;   // the Body Shop's notice that it has nothing (which leaves)
    Dialog dialog_[2];   // the notice or the windows to buy or sell (two in a trade: the seller's, the buyer's)
    int age_;            // the frame count the money arrows bob by
};

}  // namespace game
