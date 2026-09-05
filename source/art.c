// art.c - layer 2: un ritratto/stanza alla volta, palette ricaricata ogni volta
#include <nds.h>
#include <string.h>
#include "art.h"
#include "board.h"
#include "game.h"
#include "tiles_data.h"

#include "suspect_rosso.h"
#include "suspect_senape.h"
#include "suspect_bianca.h"
#include "suspect_verdi.h"
#include "suspect_pavone.h"
#include "suspect_prugna.h"
#include "room_cucina.h"
#include "room_ballo.h"
#include "room_serra.h"
#include "room_pranzo.h"
#include "room_biliardo.h"
#include "room_biblioteca.h"
#include "room_salotto.h"
#include "room_ingresso.h"
#include "room_studio.h"
#include "title_logo.h"

typedef struct {
    const unsigned int *tiles;
    unsigned int tilesLen;   // byte (define *_TilesLen di grit)
    const unsigned short *map;
    unsigned int mapLen;     // byte (define *_MapLen di grit)
    int mw, mh;              // mappa in tile
    const unsigned short *pal;
    unsigned int palLen;     // byte (define *_PalLen di grit)
} Art;

static const Art SUSP_ART[6] = {
    {suspect_rossoTiles, suspect_rossoTilesLen, suspect_rossoMap, suspect_rossoMapLen, 8, 8, suspect_rossoPal, suspect_rossoPalLen},
    {suspect_senapeTiles, suspect_senapeTilesLen, suspect_senapeMap, suspect_senapeMapLen, 8, 8, suspect_senapePal, suspect_senapePalLen},
    {suspect_biancaTiles, suspect_biancaTilesLen, suspect_biancaMap, suspect_biancaMapLen, 8, 8, suspect_biancaPal, suspect_biancaPalLen},
    {suspect_verdiTiles, suspect_verdiTilesLen, suspect_verdiMap, suspect_verdiMapLen, 8, 8, suspect_verdiPal, suspect_verdiPalLen},
    {suspect_pavoneTiles, suspect_pavoneTilesLen, suspect_pavoneMap, suspect_pavoneMapLen, 8, 8, suspect_pavonePal, suspect_pavonePalLen},
    {suspect_prugnaTiles, suspect_prugnaTilesLen, suspect_prugnaMap, suspect_prugnaMapLen, 8, 8, suspect_prugnaPal, suspect_prugnaPalLen},
};
static const Art ROOM_ART[9] = {
    {room_cucinaTiles, room_cucinaTilesLen, room_cucinaMap, room_cucinaMapLen, 16, 12, room_cucinaPal, room_cucinaPalLen},
    {room_balloTiles, room_balloTilesLen, room_balloMap, room_balloMapLen, 16, 12, room_balloPal, room_balloPalLen},
    {room_serraTiles, room_serraTilesLen, room_serraMap, room_serraMapLen, 16, 12, room_serraPal, room_serraPalLen},
    {room_pranzoTiles, room_pranzoTilesLen, room_pranzoMap, room_pranzoMapLen, 16, 12, room_pranzoPal, room_pranzoPalLen},
    {room_biliardoTiles, room_biliardoTilesLen, room_biliardoMap, room_biliardoMapLen, 16, 12, room_biliardoPal, room_biliardoPalLen},
    {room_bibliotecaTiles, room_bibliotecaTilesLen, room_bibliotecaMap, room_bibliotecaMapLen, 16, 12, room_bibliotecaPal, room_bibliotecaPalLen},
    {room_salottoTiles, room_salottoTilesLen, room_salottoMap, room_salottoMapLen, 16, 12, room_salottoPal, room_salottoPalLen},
    {room_ingressoTiles, room_ingressoTilesLen, room_ingressoMap, room_ingressoMapLen, 16, 12, room_ingressoPal, room_ingressoPalLen},
    {room_studioTiles, room_studioTilesLen, room_studioMap, room_studioMapLen, 16, 12, room_studioPal, room_studioPalLen},
};

static int bgArt = -1;

void art_init(void) {
    // attiva anche BG2 sul main engine e prepara il layer dedicato
    videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_BG2_ACTIVE);
    bgArt = bgInit(2, BgType_Text8bpp, BgSize_T_256x256, 8, 3);
    bgHide(bgArt);
}

static void art_show(const Art *a) {
    memcpy(bgGetGfxPtr(bgArt), a->tiles, a->tilesLen);
    memcpy(BG_PALETTE, a->pal, a->palLen);
    u16 *map = bgGetMapPtr(bgArt);
    for (int i = 0; i < 32 * 32; i++) map[i] = 0;
    int ox = (32 - a->mw) / 2, oy = (24 - a->mh) / 2;
    for (int r = 0; r < a->mh; r++)
        for (int c = 0; c < a->mw; c++)
            map[(oy + r) * 32 + ox + c] = a->map[r * a->mw + c];
    bgHide(board_bgId());
    bgHide(board_overId());
    board_hideSprites();
    setBackdropColor(RGB15(0, 0, 0));
    bgShow(bgArt);
}

void art_showSuspect(int i) {
    if (i < 0 || i > 5) return;
    art_show(&SUSP_ART[i]);
}

void art_showRoom(int i) {
    if (i < 0 || i > 8) return;
    art_show(&ROOM_ART[i]);
}

void art_showTitle(void) {
    static const Art TITLE = {title_logoTiles, title_logoTilesLen, title_logoMap,
                              title_logoMapLen, 32, 24, title_logoPal, title_logoPalLen};
    art_show(&TITLE);
}

void art_hide(const void *gamePtr) {
    const Game *g = (const Game *)gamePtr;
    bgHide(bgArt);
    memcpy(BG_PALETTE, boardPal, sizeof(boardPal));
    setBackdropColor(RGB15(4, 4, 6));
    bgShow(board_bgId());
    bgShow(board_overId());
    board_drawPawns(g);
}
