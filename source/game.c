// game.c - implementazione logica (port fedele di cluedo.html)
#include "game.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char *SUSP_NAMES[NSUSP] = {
    "Amelia Rosso", "Col. Senape", "Dott.ssa Bianca",
    "Rev. Verdi", "Cont. Pavone", "Prof. Prugna"
};
const char *WEAP_NAMES[NWEAP] = {
    "Candelabro", "Pugnale", "Tubo di piombo",
    "Pistola", "Corda", "Chiave inglese"
};
// Ordine stanze = indice tile/gfx: cucina, ballo, serra, pranzo, biliardo,
// biblioteca, salotto, ingresso, studio (stesso di cluedo.html).
const RoomDef ROOMS[NROOM] = {
    {"Cucina",        0,0, 5,5,   {{2,6},{6,2},{-1,-1},{-1,-1}},     2, 8},
    {"Sala da Ballo", 9,0, 14,6,  {{11,7},{8,3},{15,3},{-1,-1}},    3, -1},
    {"Serra",         19,0, 23,4, {{19,5},{18,2},{-1,-1},{-1,-1}},  2, 6},
    {"Sala da Pranzo",0,9, 5,15,  {{6,10},{3,8},{3,16},{-1,-1}},    3, -1},
    {"Sala Biliardo", 9,9, 14,14, {{9,8},{14,15},{8,12},{15,12}},   4, -1},
    {"Biblioteca",    18,8, 23,15,{{17,10},{20,7},{20,16},{-1,-1}}, 3, -1},
    {"Salotto",       0,18, 5,23, {{2,17},{6,20},{-1,-1},{-1,-1}},  2, 2},
    {"Ingresso",      9,18,14,23, {{11,17},{8,20},{15,20},{-1,-1}}, 3, -1},
    {"Studio",        18,18,23,23,{{20,17},{17,20},{-1,-1},{-1,-1}},2, 0},
};
const int STARTS[NSUSP][2] = {{7,1},{16,1},{23,6},{7,23},{16,23},{0,16}};
const char *AI_NAMES[5] = {"Marple", "Sherlock", "Poirot", "Colombo", "Wolfe"};

static void shuffle_int(int *a, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1), t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

void game_setup(Game *g, int mySusp, int nplayers) {
    memset(g, 0, sizeof(*g));
    g->nplayers = nplayers;
    g->turnCount = 1;
    g->winner = -1;
    // umano = giocatore 0
    snprintf(g->players[0].name, sizeof(g->players[0].name), "Detective");
    g->players[0].susp = mySusp;
    g->players[0].ai = 0;
    // IA prendono gli altri sospetti in ordine
    int ai = 0;
    for (int s = 0; s < NSUSP && ai < nplayers - 1; s++) {
        if (s == mySusp) continue;
        Player *p = &g->players[1 + ai];
        snprintf(p->name, sizeof(p->name), "%s", AI_NAMES[ai % 5]);
        p->susp = s;
        p->ai = 1;
        ai++;
    }
    // busta
    g->envS = CARD_S(rand() % NSUSP);
    g->envW = CARD_W(rand() % NWEAP);
    g->envR = CARD_R(rand() % NROOM);
    // mazzo e distribuzione round-robin
    int deck[NCARDS], nd = 0;
    for (int c = 0; c < NCARDS; c++) {
        if (c == g->envS || c == g->envW || c == g->envR) continue;
        deck[nd++] = c;
    }
    shuffle_int(deck, nd);
    for (int i = 0; i < nd; i++) {
        Player *p = &g->players[i % nplayers];
        p->hand |= CARD_BIT(deck[i]);
        p->seen |= CARD_BIT(deck[i]);
    }
    // taccuino: le mie carte sono escluse
    for (int c = 0; c < NCARDS; c++)
        if (g->players[0].hand & CARD_BIT(c)) g->notes[c] = 2;
    // pedine alle partenze
    for (int s = 0; s < NSUSP; s++) {
        g->pos[s].inRoom = 0;
        g->pos[s].x = STARTS[s][0];
        g->pos[s].y = STARTS[s][1];
    }
    g->turn = rand() % nplayers;
}

int game_activeCount(const Game *g) {
    int n = 0;
    for (int i = 0; i < g->nplayers; i++) if (!g->players[i].out) n++;
    return n;
}

int game_nextTurn(Game *g) {
    for (int k = 0; k < g->nplayers; k++) {
        g->turn = (g->turn + 1) % g->nplayers;
        if (g->turn == 0) g->turnCount++;
        if (!g->players[g->turn].out) return g->turn;
    }
    return -1;
}

const char *card_name(int id, char *buf, int buflen) {
    if (id < 6) snprintf(buf, buflen, "%s", SUSP_NAMES[id]);
    else if (id < 12) snprintf(buf, buflen, "%s", WEAP_NAMES[id - 6]);
    else snprintf(buf, buflen, "%s", ROOMS[id - 12].name);
    return buf;
}

int roomAt(int x, int y) {
    for (int r = 0; r < NROOM; r++) {
        const RoomDef *rm = &ROOMS[r];
        if (x >= rm->x0 && x <= rm->x1 && y >= rm->y0 && y <= rm->y1) return r;
    }
    return -1;
}

int doorRoom(int x, int y) {
    for (int r = 0; r < NROOM; r++) {
        const RoomDef *rm = &ROOMS[r];
        for (int d = 0; d < rm->ndoors; d++)
            if (rm->doors[d][0] == x && rm->doors[d][1] == y) return r;
    }
    return -1;
}

int walkable(int x, int y) {
    return x >= 0 && y >= 0 && x < NGRID && y < NGRID && roomAt(x, y) < 0;
}

int bfsReach(const Game *g, int pi, int steps, unsigned char mask[NGRID][NGRID]) {
    static int dist[NGRID][NGRID];
    for (int y = 0; y < NGRID; y++)
        for (int x = 0; x < NGRID; x++) { dist[y][x] = -1; mask[y][x] = 0; }
    // code dei nodi in static: lo stack DTCM del DS e' piccolo
    static int qx[NGRID * NGRID], qy[NGRID * NGRID];
    int qh = 0, qt = 0;
    const PawnPos *p = &g->pos[g->players[pi].susp];
    if (p->inRoom) {
        const RoomDef *rm = &ROOMS[p->room];
        for (int d = 0; d < rm->ndoors; d++) {
            int x = rm->doors[d][0], y = rm->doors[d][1];
            if (dist[y][x] < 0) { dist[y][x] = 0; qx[qt] = x; qy[qt] = y; qt++; }
        }
    } else {
        dist[p->y][p->x] = 0; qx[qt] = p->x; qy[qt] = p->y; qt++;
    }
    static const int DX[4] = {1,-1,0,0}, DY[4] = {0,0,1,-1};
    while (qh < qt) {
        int x = qx[qh], y = qy[qh], dd = dist[y][x]; qh++;
        if (dd >= steps) continue;
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!walkable(nx, ny) || dist[ny][nx] >= 0) continue;
            dist[ny][nx] = dd + 1; qx[qt] = nx; qy[qt] = ny; qt++;
        }
    }
    int n = 0;
    for (int y = 0; y < NGRID; y++)
        for (int x = 0; x < NGRID; x++) {
            if (dist[y][x] <= 0) continue; // 0 = partenza (resta = voce menu)
            mask[y][x] = 1; n++;
        }
    return n;
}

int roomDist(int sx, int sy, int room) {
    static int dist[NGRID][NGRID];
    for (int y = 0; y < NGRID; y++)
        for (int x = 0; x < NGRID; x++) dist[y][x] = -1;
    const RoomDef *rm = &ROOMS[room];
    unsigned int targets[4];
    int nt = 0;
    for (int d = 0; d < rm->ndoors; d++)
        targets[nt++] = (unsigned int)(rm->doors[d][1] * NGRID + rm->doors[d][0]);
    static int qx[NGRID * NGRID], qy[NGRID * NGRID];
    int qh = 0, qt = 0;
    if (sx < 0 || sy < 0 || sx >= NGRID || sy >= NGRID) return 99;
    dist[sy][sx] = 0; qx[qt] = sx; qy[qt] = sy; qt++;
    static const int DX[4] = {1,-1,0,0}, DY[4] = {0,0,1,-1};
    while (qh < qt) {
        int x = qx[qh], y = qy[qh], dd = dist[y][x]; qh++;
        for (int t = 0; t < nt; t++)
            if (targets[t] == (unsigned int)(y * NGRID + x)) return dd;
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (nx < 0 || ny < 0 || nx >= NGRID || ny >= NGRID || dist[ny][nx] >= 0) continue;
            int r = roomAt(nx, ny);
            if (r >= 0 && r != room) continue;
            dist[ny][nx] = dd + 1; qx[qt] = nx; qy[qt] = ny; qt++;
        }
    }
    return 99;
}

unsigned int wantMask(int sid, int wid, int rid) {
    return CARD_BIT(CARD_S(sid)) | CARD_BIT(CARD_W(wid)) | CARD_BIT(CARD_R(rid));
}

unsigned int handMatches(unsigned int hand, unsigned int want) {
    return hand & want;
}

int findDisprover(const Game *g, int suggester, unsigned int want) {
    for (int i = 1; i <= g->nplayers; i++) {
        int p = (suggester + i) % g->nplayers;
        if (g->players[p].hand & want) return p;
    }
    return -1;
}

int pickAiCard(unsigned int matches) {
    int ids[3], n = 0;
    for (int c = 0; c < NCARDS && n < 3; c++)
        if (matches & CARD_BIT(c)) ids[n++] = c;
    if (!n) return -1;
    return ids[rand() % n];
}

int accuseCorrect(const Game *g, int sid, int wid, int rid) {
    return CARD_S(sid) == g->envS && CARD_W(wid) == g->envW && CARD_R(rid) == g->envR;
}

int aiUnknown(const Player *p, int cat, int *outIds) {
    int n = 0;
    int base = cat == 0 ? 0 : cat == 1 ? 6 : 12;
    int cnt = cat == 2 ? NROOM : NSUSP;
    if (cat == 1) cnt = NWEAP;
    for (int i = 0; i < cnt; i++) {
        int id = base + i;
        if (!(p->seen & CARD_BIT(id))) {
            if (outIds) outIds[n] = id;
            n++;
        }
    }
    return n;
}

int aiConfident(const Player *p, int turnCount, int *s, int *w, int *r) {
    int us[6], uw[6], ur[9];
    int ns = aiUnknown(p, 0, us), nw = aiUnknown(p, 1, uw), nr = aiUnknown(p, 2, ur);
    int lim = turnCount >= 28 ? 2 : 1; // come in cluedo.html: dopo un po' osano
    if (ns <= lim && nw <= lim && nr <= lim && ns > 0 && nw > 0 && nr > 0) {
        *s = us[0] - 0; *w = uw[0] - 6; *r = ur[0] - 12;
        return 1;
    }
    return 0;
}

// cella di riferimento della pedina (per distanze IA)
static void pawnCell(const Game *g, int pi, int *ox, int *oy) {
    const PawnPos *p = &g->pos[g->players[pi].susp];
    if (p->inRoom) {
        const RoomDef *rm = &ROOMS[p->room];
        *ox = (rm->x0 + rm->x1) / 2; *oy = rm->y1 + 1;
    } else { *ox = p->x; *oy = p->y; }
}

int aiTargetRoom(const Game *g, int pi) {
    const Player *p = &g->players[pi];
    const PawnPos *pp = &g->pos[p->susp];
    int here = pp->inRoom ? pp->room : -1;
    // se sono in una stanza non ancora esclusa, resto
    if (here >= 0 && !(p->seen & CARD_BIT(CARD_R(here)))) return here;
    int cx, cy; pawnCell(g, pi, &cx, &cy);
    int best = -1, bestd = 999;
    for (int pass = 0; pass < 2 && best < 0; pass++) {
        bestd = 999;
        for (int r = 0; r < NROOM; r++) {
            if (r == here) continue;
            int unseen = !(p->seen & CARD_BIT(CARD_R(r)));
            if (pass == 0 && !unseen) continue;
            int d = roomDist(cx, cy, r);
            if (d < bestd) { bestd = d; best = r; }
        }
    }
    return best < 0 ? (here >= 0 ? here : 0) : best;
}

void aiStepToward(Game *g, int pi, int goalRoom, int steps) {
    PawnPos *p = &g->pos[g->players[pi].susp];
    int cx, cy;
    if (p->inRoom) {
        // esce dalla porta migliore
        const RoomDef *rm = &ROOMS[p->room];
        int bi = 0, bd = 999;
        for (int d = 0; d < rm->ndoors; d++) {
            int dd = goalRoom >= 0 ? roomDist(rm->doors[d][0], rm->doors[d][1], goalRoom) : 0;
            if (dd < bd) { bd = dd; bi = d; }
        }
        cx = rm->doors[bi][0]; cy = rm->doors[bi][1];
    } else { cx = p->x; cy = p->y; }
    static const int DX[4] = {1,-1,0,0}, DY[4] = {0,0,1,-1};
    for (int i = 0; i < steps; i++) {
        int ox[4], oy[4], od[4], no = 0;
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (!walkable(nx, ny)) continue;
            ox[no] = nx; oy[no] = ny;
            od[no] = goalRoom >= 0 ? roomDist(nx, ny, goalRoom) : 0;
            no++;
        }
        if (!no) break;
        int pick = 0;
        if (rand() % 100 < 75) {
            for (int k = 1; k < no; k++) if (od[k] < od[pick]) pick = k;
        } else pick = rand() % no;
        cx = ox[pick]; cy = oy[pick];
        int dr = doorRoom(cx, cy);
        if (dr >= 0 && dr == goalRoom) {
            p->inRoom = 1; p->room = dr;
            return;
        }
    }
    p->inRoom = 0; p->x = cx; p->y = cy;
}

void aiPickSuggest(const Player *p, int *sid, int *wid) {
    int us[6], uw[6];
    int ns = aiUnknown(p, 0, us), nw = aiUnknown(p, 1, uw);
    *sid = ns ? us[0] - 0 : rand() % NSUSP;
    *wid = nw ? uw[0] - 6 : rand() % NWEAP;
}
