// main.c - Cluedo DS "Delitto a Villa Nera": flusso di gioco
// Sopra (main): tabellone. Sotto (sub): console + menu touch/D-pad.
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "board.h"
#include "ui.h"
#include "art.h"
#include "sfx.h"

static Game G;
static int fastMode = 0;

static void waitN(int n) {
    if (fastMode) n = n / 6 + 1;
    for (int i = 0; i < n; i++) ui_frame();
}

// Schermata titolo (sopra: logo, sotto: menu). Il seed RNG deriva dai
// frame trascorsi (ui_frames): piu' pensi, piu' e' casuale.
static void helpScreen(void) {
    ui_msg("COME SI GIOCA 1/2",
           "Scopo: scopri CHI, CON COSA\ne DOVE prima delle IA.\n\nD-pad: cursore e menu\nA/TAP: conferma\nB: indietro\nSTART: conferma");
    ui_msg("COME SI GIOCA 2/2",
           "Tira, muovi sulle caselle\noro ed entra in stanza:\nIPOTESI o PASSAGGIO.\nGli altri smentiscono.\nCerto? ACCUSA!\nSbagli = eliminato.\nTaccuino: ?=ignota *=sosp.\nV=esclusa X=no.");
}

static void titleScreen(void) {
    static const char *items[] = {"NUOVA INDAGINE", "COME SI GIOCA"};
    art_showTitle();
    for (;;) {
        int c = ui_menu("DELITTO A VILLA NERA\nCluedo DS", items, 2, 0);
        srand(ui_frames * 2654435761u + 0x9E3779B9u);
        if (c == 0) return;
        helpScreen();
    }
}

static void redrawPawns(void) {
    board_drawPawns(&G);
}

// ---------- risoluzione ipotesi (umano o IA) ----------
static void resolveSuggestion(int suggester, int sid, int wid, int rid) {
    char a[40], b[40], c[40];
    G.suggCount++;
    G.pos[sid].inRoom = 1;
    G.pos[sid].room = rid;
    redrawPawns();
    sfx_card();
    card_name(CARD_S(sid), a, sizeof(a));
    card_name(CARD_W(wid), b, sizeof(b));
    card_name(CARD_R(rid), c, sizeof(c));
    {
        char t[96];
        snprintf(t, sizeof(t), "%s ipotizza:\n%s + %s\nin %s", G.players[suggester].name, a, b, c);
        ui_clear();
        printf("%s\n", t);
    }
    waitN(50);
    unsigned int want = wantMask(sid, wid, rid);
    int shower = findDisprover(&G, suggester, want);
    if (shower < 0) {
        ui_msg("Nessuno smentisce!", "Forse sei vicino alla verita'...");
        return;
    }
    Player *sh = &G.players[shower], *sg = &G.players[suggester];
    unsigned int matches = handMatches(sh->hand, want);
    int card = -1;
    if (sh->ai) {
        card = pickAiCard(matches);
    } else {
        // l'umano sceglie cosa mostrare
        char items[3][40], *it[3];
        int m[3], n = 0;
        for (int cd = 0; cd < NCARDS && n < 3; cd++)
            if (matches & CARD_BIT(cd)) {
                card_name(cd, items[n], sizeof(items[n]));
                it[n] = items[n]; m[n] = cd; n++;
            }
        char t[64];
        snprintf(t, sizeof(t), "%s ti chiama in causa!\nScegli carta da mostrare:", sg->name);
        int pick = ui_menu(t, (const char **)it, n, 0);
        card = m[pick];
        sfx_card();
    }
    sh->seen |= CARD_BIT(card);
    sg->seen |= CARD_BIT(card);
    card_name(card, a, sizeof(a));
    if (!sg->ai) {
        // rivelazione al giocatore umano
        int isS = card < 6, isR = card >= 12;
        if (isS) art_showSuspect(card);
        else if (isR) art_showRoom(card - 12);
        G.players[0].seen |= CARD_BIT(card);
        G.notes[card] = 2;
        {
            char t[96];
            snprintf(t, sizeof(t), "%s ti mostra:", sh->name);
            ui_msg(t, a);
        }
        if (isS || isR) art_hide(&G);
        redrawPawns();
    } else if (!sh->ai) {
        ui_clear();
        printf("Hai mostrato una carta\nin segreto a %s.\n", sg->name);
        waitN(40);
    } else {
        ui_clear();
        printf("%s mostra una carta\nin segreto a %s.\n", sh->name, sg->name);
        waitN(40);
    }
}

// ---------- accusa ----------
static void gameOverScreen(void) {
    char a[40], b[40], c[40];
    card_name(G.envS, a, sizeof(a));
    card_name(G.envW, b, sizeof(b));
    card_name(G.envR, c, sizeof(c));
    art_showSuspect(G.envS);
    {
        char t[128];
        snprintf(t, sizeof(t), "LA BUSTA RIVELA:\n\n%s\ncon %s\nin %s", a, b, c);
        ui_msg(G.winner >= 0 ? "CASO RISOLTO" : "CASO CHIUSO", t);
    }
    art_showRoom(G.envR - 12);
    waitN(90);
    art_hide(&G);
    redrawPawns();
}

static void resolveAccuse(int pi, int sid, int wid, int rid) {
    char a[40], b[40], c[40];
    card_name(CARD_S(sid), a, sizeof(a));
    card_name(CARD_W(wid), b, sizeof(b));
    card_name(CARD_R(rid), c, sizeof(c));
    if (accuseCorrect(&G, sid, wid, rid)) {
        ui_clear();
        printf("%s accusa:\n%s + %s\nin %s\n\nCOLPEVOLE!", G.players[pi].name, a, b, c);
        G.over = 1;
        G.winner = pi;
        if (!G.players[pi].ai) sfx_win(); else sfx_bad();
        waitN(90);
        gameOverScreen();
    } else {
        sfx_bad();
        G.players[pi].out = 1;
        ui_clear();
        printf("%s accusa:\n%s + %s\nin %s\n\nSBAGLIATO! Eliminato.\n", G.players[pi].name, a, b, c);
        printf("Le sue carte restano\nin gioco per smentire.\n");
        waitN(80);
        if (!G.players[pi].ai) {
            fastMode = 1;
            ui_msg("Eliminato!", "Guarda le IA darsi\nbattaglia in velocita'.");
        }
        if (game_activeCount(&G) == 0) {
            G.over = 1;
            G.winner = -1;
            gameOverScreen();
        }
    }
}

// ---------- menu scelta carta con segni V (escluse) ----------
static int chooseCardMenu(const char *title, int cat, const Player *knower) {
    // cat 0=sospetti 1=armi 2=stanze
    static char items[9][40];
    static const char *it[9];
    char nm[40];
    int n = cat == 2 ? NROOM : NSUSP;
    if (cat == 1) n = NWEAP;
    for (int i = 0; i < n; i++) {
        int id = cat == 0 ? CARD_S(i) : cat == 1 ? CARD_W(i) : CARD_R(i);
        card_name(id, nm, sizeof(nm));
        snprintf(items[i], sizeof(items[i]), "%s %s",
                 (knower->seen & CARD_BIT(id)) ? "V" : " ", nm);
        it[i] = items[i];
    }
    int pick = ui_menu(title, it, n, 1);
    return pick; // indice 0..n-1 o -1
}

// ---------- ipotesi del giocatore ----------
static void humanSuggest(int pi) {
    int room = G.pos[G.players[pi].susp].room;
    char t[64];
    snprintf(t, sizeof(t), "IPOTESI in %s:", ROOMS[room].name);
    int sid = chooseCardMenu(t, 0, &G.players[0]);
    if (sid < 0) return;
    int wid = chooseCardMenu("Arma?", 1, &G.players[0]);
    if (wid < 0) return;
    resolveSuggestion(pi, sid, wid, room);
}

// ---------- accusa del giocatore ----------
static void humanAccuse(int pi) {
    int sid = chooseCardMenu("Accusa - CHI?", 0, &G.players[0]);
    if (sid < 0) return;
    int wid = chooseCardMenu("Accusa - CON COSA?", 1, &G.players[0]);
    if (wid < 0) return;
    int rid = chooseCardMenu("Accusa - DOVE?", 2, &G.players[0]);
    if (rid < 0) return;
    if (!ui_yesno("Confermi? Se sbagli\nsei ELIMINATO!")) return;
    resolveAccuse(pi, sid, wid, rid);
}

// ---------- movimento del giocatore (cursore tra le mete) ----------
static int humanMove(int pi) {
    // ritorna: 0 mosso/restato in stanza, 1 turno finito altrove
    PawnPos *pp = &G.pos[G.players[pi].susp];
    int wasRoom = pp->inRoom ? pp->room : -1;
    unsigned char mask[NGRID][NGRID];
    int n = bfsReach(&G, pi, G.dice, mask);
    if (n == 0) {
        if (wasRoom >= 0) return 0; // resta: menu stanza
        ui_msg("Nessuna meta.", "Passo il turno...");
        return 1;
    }
    // lista mete per scorrimento D-pad
    static int lx[NGRID * NGRID], ly[NGRID * NGRID];
    int nl = 0;
    for (int y = 0; y < NGRID && nl < NGRID * NGRID; y++)
        for (int x = 0; x < NGRID && nl < NGRID * NGRID; x++)
            if (mask[y][x]) { lx[nl] = x; ly[nl] = y; nl++; }
    board_showReach(mask);
    int cur = 0;
    // parti dalla meta piu' vicina alla pedina
    {
        int px = pp->inRoom ? ROOMS[pp->room].doors[0][0] : pp->x;
        int py = pp->inRoom ? ROOMS[pp->room].doors[0][1] : pp->y;
        int bd = 9999;
        for (int i = 0; i < nl; i++) {
            int d = abs(lx[i] - px) + abs(ly[i] - py);
            if (d < bd) { bd = d; cur = i; }
        }
    }
    int blink = 0;
    for (;;) {
        board_setCursor(lx[cur], ly[cur], (blink++ / 18) % 2 == 0);
        board_drawPawns(&G);
        ui_clear();
        {
            int dr = doorRoom(lx[cur], ly[cur]);
            if (dr >= 0) printf("Dadi:%d Meta %d/%d\n%s -> ENTRA in\n%s\n", G.dice, cur + 1, nl,
                                 G.players[pi].name, ROOMS[dr].name);
            else printf("Dadi:%d Meta %d/%d\n%s: cella [%d,%d]\n", G.dice, cur + 1, nl,
                         G.players[pi].name, lx[cur], ly[cur]);
        }
        printf("\nFrecce:scorri A:vai\n%s", wasRoom >= 0 ? "B:resta qui" : " ");
        ui_frame();
        unsigned int down = keysDown();
        if (down & KEY_LEFT) { cur = (cur + nl - 1) % nl; sfx_click(); }
        if (down & KEY_RIGHT) { cur = (cur + 1) % nl; sfx_click(); }
        // passo verticale ridotto se le mete sono poche (evita indici negativi)
        { int step = nl < 5 ? 1 : 5;
          if (down & KEY_UP) { cur = (cur + nl - step) % nl; sfx_click(); }
          if (down & KEY_DOWN) { cur = (cur + step) % nl; sfx_click(); } }
        // Nota: il touch legge solo lo schermo sotto (UI): qui si usa A/frecce.
        if (down & KEY_A) break;
        if ((down & KEY_B) && wasRoom >= 0) {
            board_clearReach();
            board_setCursor(0, 0, 0);
            redrawPawns();
            return 0;
        }
    }
    int tx = lx[cur], ty = ly[cur];
    int dr = doorRoom(tx, ty);
    board_clearReach();
    board_setCursor(0, 0, 0);
    if (dr >= 0) {
        pp->inRoom = 1; pp->room = dr;
        sfx_door();
    } else {
        pp->inRoom = 0; pp->x = tx; pp->y = ty;
        sfx_step();
    }
    redrawPawns();
    return 0;
}

// ---------- menu azione in stanza (giocatore) ----------
static void humanRoomMenu(int pi) {
    int room = G.pos[G.players[pi].susp].room;
    art_showRoom(room);
    for (;;) {
        static const char *base[] = {"IPOTESI", "TACCUINO", "MANO", "ACCUSA", "FINE TURNO"};
        static const char *withP[] = {"IPOTESI", "PASSAGGIO SEGRETO", "TACCUINO", "MANO", "ACCUSA", "FINE TURNO"};
        int hasP = ROOMS[room].passage >= 0;
        char t[48];
        snprintf(t, sizeof(t), "Sei in %s:", ROOMS[room].name);
        int c = ui_menu(t, hasP ? withP : base, hasP ? 6 : 5, 0);
        if (hasP && c == 1) {
            int dest = ROOMS[room].passage;
            G.pos[G.players[pi].susp].room = dest;
            room = dest;
            sfx_door();
            art_showRoom(room);
            redrawPawns();
            continue;
        }
        int k;
        if (!hasP) k = c;
        else if (c == 0) k = 0; // IPOTESI e' sempre prima, passaggio = 1
        else k = c - 1;
        if (k == 0) { humanSuggest(pi); if (G.over || G.players[pi].out) break; continue; }
        if (k == 1) { ui_notebook(&G); continue; }
        if (k == 2) { ui_showHand(&G); continue; }
        if (k == 3) { humanAccuse(pi); break; }
        break; // FINE TURNO
    }
    art_hide(&G);
    redrawPawns();
}

// ---------- turno giocatore ----------
static void humanTurn(int pi) {
    static const char *m0[] = {"TIRA I DADI", "TACCUINO", "MANO", "ACCUSA"};
    static const char *m1[] = {"TACCUINO", "MANO", "ACCUSA", "FINE TURNO"};
    int rolled = 0;
    for (;;) {
        char t[64];
        snprintf(t, sizeof(t), "Turno %d: %s [dado?]", G.turnCount, G.players[pi].name);
        int c = ui_menu(rolled ? "Cosa fai?" : t, rolled ? m1 : m0, 4, 0);
        if (!rolled && c == 0) {
            sfx_dice();
            int d1, d2;
            G.dice = ui_dice(&d1, &d2);
            rolled = 1;
            {
                int r = humanMove(pi);
                if (G.over) return;
                if (r == 1) return; // nessuna meta, turno finito
            }
            PawnPos *pp = &G.pos[G.players[pi].susp];
            if (pp->inRoom) { humanRoomMenu(pi); return; }
            continue; // in corridoio: solo accusa o fine turno
        }
        int k = rolled ? c : c - 1;
        // m0: 1=taccuino 2=mano 3=accusa ; m1: 0=tacc 1=mano 2=accusa 3=fine
        if (!rolled) {
            if (c == 1) { ui_notebook(&G); continue; }
            if (c == 2) { ui_showHand(&G); continue; }
            humanAccuse(pi); return;
        } else {
            if (k == 0) { ui_notebook(&G); continue; }
            if (k == 1) { ui_showHand(&G); continue; }
            if (k == 2) { humanAccuse(pi); return; }
            return; // FINE TURNO
        }
    }
}

// ---------- turno IA ----------
static void aiTurn(int pi) {
    Player *p = &G.players[pi];
    char t[64];
    snprintf(t, sizeof(t), "Turno %d: %s indaga...", G.turnCount, p->name);
    ui_clear();
    printf("%s\n", t);
    waitN(40);
    int s = 0, w = 0, r = 0;
    if (aiConfident(p, G.turnCount, &s, &w, &r) && (rand() % 100 < (G.turnCount >= 28 ? 50 : 90))) {
        ui_clear();
        printf("%s e' certo...\nACCUSA!\n", p->name);
        waitN(50);
        resolveAccuse(pi, s, w, r);
        return;
    }
    {
        int d1, d2;
        sfx_dice();
        G.dice = ui_dice(&d1, &d2);
    }
    int here = G.pos[p->susp].inRoom ? G.pos[p->susp].room : -1;
    int cx, cy;
    {
        const PawnPos *pp = &G.pos[p->susp];
        if (pp->inRoom) {
            const RoomDef *rm = &ROOMS[pp->room];
            cx = (rm->x0 + rm->x1) / 2; cy = rm->y1 + 1;
        } else { cx = pp->x; cy = pp->y; }
    }
    int goal = aiTargetRoom(&G, pi);
    if (here >= 0 && goal == here) {
        ui_clear();
        printf("%s resta in\n%s.\n", p->name, ROOMS[goal].name);
    } else if (goal >= 0 && roomDist(cx, cy, goal) <= G.dice) {
        G.pos[p->susp].inRoom = 1;
        G.pos[p->susp].room = goal;
        sfx_door();
        ui_clear();
        printf("%s entra in\n%s.\n", p->name, ROOMS[goal].name);
    } else {
        if (here >= 0 && ROOMS[here].passage >= 0 && (rand() % 100) < 60) {
            int dest = ROOMS[here].passage;
            if (!(p->seen & CARD_BIT(CARD_R(dest))) || (rand() % 100) < 30) {
                G.pos[p->susp].room = dest;
                ui_clear();
                printf("%s usa il\npassaggio -> %s.\n", p->name, ROOMS[dest].name);
                sfx_door();
                redrawPawns();
                waitN(40);
                goto afterMove;
            }
        }
        aiStepToward(&G, pi, goal, G.dice);
        sfx_step();
        ui_clear();
        printf("%s si aggira\nper Villa Nera...\n", p->name);
    }
    redrawPawns();
    waitN(40);
afterMove:
    {
        PawnPos *pp = &G.pos[p->susp];
        if (pp->inRoom) {
            int sid, wid;
            aiPickSuggest(p, &sid, &wid);
            resolveSuggestion(pi, sid, wid, pp->room);
            if (G.over || p->out) return;
            int s2 = 0, w2 = 0, r2 = 0;
            if (aiConfident(p, G.turnCount, &s2, &w2, &r2)) {
                int ns = aiUnknown(p, 0, NULL), nw = aiUnknown(p, 1, NULL), nr = aiUnknown(p, 2, NULL);
                if (ns == 1 && nw == 1 && nr == 1 && (rand() % 100) < 85) {
                    ui_clear();
                    printf("%s ha capito tutto!\nACCUSA!\n", p->name);
                    waitN(40);
                    resolveAccuse(pi, s2, w2, r2);
                    return;
                }
            }
        }
    }
}

// ---------- setup ----------
static void setupGame(void) {
    static const char *counts[] = {"3 (tu + 2 IA)", "4 (tu + 3 IA)", "5 (tu + 4 IA)"};
    int sel = 0, shown = -1;
    // pedina
    for (;;) {
        ui_clear();
        printf("DELITTO A VILLA NERA\n\nScegli pedina:\n<FRECCE> cambia\n\n");
        printf("  %s\n  %s\n", SUSP_NAMES[sel],
                sel == 0 ? "la femme fatale" : sel == 1 ? "il colonnello" :
                sel == 2 ? "la governante" : sel == 3 ? "il reverendo" :
                sel == 4 ? "la contessa" : "il professore");
        printf("\nA:conferma");
        if (sel != shown) { art_showSuspect(sel); shown = sel; }
        ui_frame();
        unsigned int down = keysDown();
        if (down & KEY_LEFT) { sel = (sel + NSUSP - 1) % NSUSP; sfx_click(); }
        if (down & KEY_RIGHT) { sel = (sel + 1) % NSUSP; sfx_click(); }
        if ((down & KEY_A) || (down & KEY_START)) { sfx_click(); break; }
    }
    art_hide(&G);
    board_hideSprites(); // G non e' ancora inizializzato: nascondi tutto
    int ci = ui_menu("Investigatori al tavolo:", counts, 3, 0);
    int nplayers = 3 + ci;
    game_setup(&G, sel, nplayers);
    fastMode = 0;
    redrawPawns();
    {
        char t[64];
        snprintf(t, sizeof(t), "Indaga, %s!", SUSP_NAMES[sel]);
        ui_msg("Il Conte Nero e' morto.", t);
    }
    ui_showHand(&G);
    ui_msg("LE 9 STANZE",
           "Cucina, Sala da Ballo, Serra\nSala da Pranzo, Sala Biliardo\nBiblioteca, Salotto\nIngresso, Studio");
}
}

// ---------- main ----------
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    videoSetModeSub(MODE_0_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    consoleDemoInit();
    board_init();
    art_init();
    sfx_init();
    for (;;) {
        titleScreen();
        setupGame();
        while (!G.over) {
            Player *p = &G.players[G.turn];
            if (p->out) {
                if (game_nextTurn(&G) < 0) { G.over = 1; G.winner = -1; gameOverScreen(); }
                continue;
            }
            if (p->ai) aiTurn(G.turn);
            else humanTurn(G.turn);
            if (!G.over) {
                if (game_nextTurn(&G) < 0) { G.over = 1; G.winner = -1; gameOverScreen(); }
            }
        }
        ui_clear();
        if (G.winner >= 0 && !G.players[G.winner].ai)
            printf("HAI VINTO, detective!\n");
        else if (G.winner >= 0)
            printf("%s vince!\nSara' per la prossima.\n", G.players[G.winner].name);
        else
            printf("Tutti eliminati...\n");
        printf("\nSTART: nuova indagine");
        for (;;) {
            ui_frame();
            if (keysDown() & KEY_START) break;
        }
    }
    return 0;
}
