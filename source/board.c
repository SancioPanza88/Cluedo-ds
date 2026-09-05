// board.c - schermo superiore: mappa 24x24 a tile 8bpp + 6 pedine sprite
#include <nds.h>
#include <string.h>
#include "board.h"
#include "tiles_data.h"
#include "sprites_data.h"

static int bgBoard = -1, bgOver = -1;
static u16 *sprGfx[7];
static int curX = 0, curY = 0, curVis = 0;

void board_cellPx(int cx, int cy, int *px, int *py) {
    *px = BOARD_PX + cx * CELL_PX - 4;
    *py = cy * CELL_PX - 4;
}

void board_init(void) {
    videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE);
    // Nota: BG2 (art) viene attivato da art_init(), non qui, per non
    // mostrare spazzatura VRAM su un layer non ancora inizializzato.
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);
    // layer 0: tabellone, layer 1: overlay mete, layer 2: art (nascosto)
    bgBoard = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 0, 1);
    bgOver  = bgInit(1, BgType_Text8bpp, BgSize_T_256x256, 4, 2);
    memcpy(bgGetGfxPtr(bgBoard), boardTiles, sizeof(boardTiles));
    memcpy(BG_PALETTE, boardPal, sizeof(boardPal));
    // overlay: tile 0 trasparente, tile 1 pallino oro
    {
        u16 *g = bgGetGfxPtr(bgOver);
        unsigned char *b = (unsigned char *)g;
        memset(b, 0, 128);
        for (int y = 2; y < 6; y++)
            for (int x = 2; x < 6; x++)
                b[64 + y * 8 + x] = 5; // indice palette oro
    }
    setBackdropColor(RGB15(4, 4, 6));
    board_buildMap();
    board_clearReach();
    bgShow(bgBoard);
    bgShow(bgOver);
    board_spritesInit();
}

void board_buildMap(void) {
    u16 *map = bgGetMapPtr(bgBoard);
    for (int i = 0; i < 32 * 32; i++) map[i] = TILE_CORR;
    for (int y = 0; y < NGRID; y++) {
        for (int x = 0; x < NGRID; x++) {
            int tile = TILE_CORR;
            int r = roomAt(x, y);
            if (r >= 0) {
                // bordo se un vicino non e' della stessa stanza
                static const int DX[4] = {1,-1,0,0}, DY[4] = {0,0,1,-1};
                int edge = 0;
                for (int k = 0; k < 4; k++)
                    if (roomAt(x + DX[k], y + DY[k]) != r) { edge = 1; break; }
                tile = edge ? TILE_ROOM_EDGE(r) : TILE_ROOM_FLOOR(r);
            } else if (doorRoom(x, y) >= 0) {
                tile = TILE_DOOR;
            }
            map[(y)*32 + (4 + x)] = (u16)tile;
        }
    }
}

void board_showReach(unsigned char mask[NGRID][NGRID]) {
    u16 *map = bgGetMapPtr(bgOver);
    for (int i = 0; i < 32 * 32; i++) map[i] = 0;
    for (int y = 0; y < NGRID; y++)
        for (int x = 0; x < NGRID; x++)
            // ATTENZIONE: numerazione tile LOCALE dell'overlay (0 vuota, 1 pallino),
            // NON usare TILE_HL (quello e' l'indice nel tileset del tabellone).
            if (mask[y][x]) map[y * 32 + 4 + x] = 1;
}

void board_clearReach(void) {
    u16 *map = bgGetMapPtr(bgOver);
    for (int i = 0; i < 32 * 32; i++) map[i] = 0;
}

void board_spritesInit(void) {
    oamInit(&oamMain, SpriteMapping_1D_128, false);
    memcpy(SPRITE_PALETTE, sprPal, sizeof(sprPal));
    for (int i = 0; i < 6; i++) {
        sprGfx[i] = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
        memcpy(sprGfx[i], sprPawn[i], 256);
    }
    sprGfx[6] = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    memcpy(sprGfx[6], sprCursor, 256);
    board_hideSprites();
}

void board_drawPawns(const Game *g) {
    static const int OFF[6][2] = {{-7,-5},{7,-5},{-7,6},{7,6},{0,-10},{0,10}};
    int nocc[9];
    for (int r = 0; r < NROOM; r++) nocc[r] = 0;
    for (int s = 0; s < NSUSP; s++) {
        int px, py;
        const PawnPos *p = &g->pos[s];
        if (p->inRoom) {
            const RoomDef *rm = &ROOMS[p->room];
            int cx = BOARD_PX + ((rm->x0 + rm->x1 + 1) / 2) * CELL_PX;
            int cy = ((rm->y0 + rm->y1 + 1) / 2) * CELL_PX;
            int k = nocc[p->room]++;
            if (k > 5) k = 5;
            px = cx - 8 + OFF[k][0];
            py = cy - 8 + OFF[k][1];
        } else {
            board_cellPx(p->x, p->y, &px, &py);
        }
        oamSet(&oamMain, s, px, py, 1, 0,
               SpriteSize_16x16, SpriteColorFormat_256Color,
               sprGfx[s], -1, false, false, false, false, false);
    }
    // cursore
    {
        int px, py;
        board_cellPx(curX, curY, &px, &py);
        oamSet(&oamMain, 6, px, py, 0, 0,
               SpriteSize_16x16, SpriteColorFormat_256Color,
               sprGfx[6], -1, false, !curVis, false, false, false);
    }
    oamUpdate(&oamMain);
}

void board_hideSprites(void) {
    for (int i = 0; i < 7; i++)
        oamSet(&oamMain, i, 0, 0, 0, 0,
               SpriteSize_16x16, SpriteColorFormat_256Color,
               sprGfx[i], -1, false, true, false, false, false);
    oamUpdate(&oamMain);
}

void board_setCursor(int cx, int cy, int visible) {
    curX = cx; curY = cy; curVis = visible;
}

void board_commit(void) {
    oamUpdate(&oamMain);
}

int board_bgId(void) { return bgBoard; }
int board_overId(void) { return bgOver; }
