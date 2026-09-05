// board.h - tabellone sullo schermo superiore (tilemap + sprite)
#pragma once
#include "game.h"

#define BOARD_PX 32   // offset X in pixel della griglia 24x24 (192x192)
#define CELL_PX 8

void board_init(void);        // video main, layer 0/1, sprite, palette
void board_buildMap(void);    // disegna stanze/corridoi/porte nella mappa
void board_showReach(unsigned char mask[NGRID][NGRID]); // evidenzia mete
void board_clearReach(void);
void board_spritesInit(void);
void board_drawPawns(const Game *g); // posiziona le 6 pedine + commit OAM
void board_hideSprites(void);
void board_setCursor(int cx, int cy, int visible); // cursore su cella
void board_commit(void); // oamUpdate
int board_bgId(void);   // id layer tabellone (per art.c)
int board_overId(void); // id layer overlay (per art.c)
// conversione cella -> pixel (angolo sup-sx dello sprite 16x16 centrato)
void board_cellPx(int cx, int cy, int *px, int *py);
