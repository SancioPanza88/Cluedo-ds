// ui.c - menu console + touch sullo schermo inferiore
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "game.h"

void ui_frame(void) {
    swiWaitForVBlank();
    scanKeys();
    oamUpdate(&oamMain);
}

void ui_clear(void) {
    iprintf("\x1b[2J");
}

static int countRows(const char *s) {
    int n = 1;
    if (!s) return 0;
    for (const char *p = s; *p; p++) if (*p == '\n') n++;
    return n;
}

int ui_menu(const char *title, const char *items[], int n, int allowCancel) {
    int sel = 0;
    if (n <= 0) return -1;
    for (;;) {
        ui_clear();
        int row = 0;
        if (title) { iprintf("%s\n\n", title); row = countRows(title) + 1; }
        int startRow = row;
        for (int i = 0; i < n; i++) {
            iprintf("%c %s\n", i == sel ? '>' : ' ', items[i]);
        }
        iprintf("\n%s", allowCancel ? "A:ok B:indietro TAP:riga" : "A:ok TAP:riga");
        for (;;) {
            ui_frame();
            unsigned int down = keysDown();
            int redraw = 0;
            if (down & KEY_UP) { sel = (sel + n - 1) % n; redraw = 1; }
            if (down & KEY_DOWN) { sel = (sel + 1) % n; redraw = 1; }
            if (down & KEY_A) return sel;
            if (down & KEY_START) return sel;
            if (allowCancel && (down & KEY_B)) return -1;
            if (down & KEY_TOUCH) {
                touchPosition t;
                touchRead(&t);
                int idx = (int)(t.py / 8) - startRow;
                if (idx >= 0 && idx < n) return idx;
            }
            if (redraw) break;
        }
    }
}

int ui_yesno(const char *question) {
    static const char *opts[] = {"SI", "NO"};
    return ui_menu(question, opts, 2, 0) == 0;
}

int ui_dice(int *d1, int *d2) {
    int a = 1, b = 1;
    for (int f = 0; f < 9; f++) {
        ui_clear();
        iprintf("Lancio dadi...\n\n  [ %d ]  [ %d ]\n", a, b);
        a = (a * 5 + 3) % 6 + 1; // animazione (il vero tiro usa rand)
        b = (b * 3 + 5) % 6 + 1;
        for (int w = 0; w < 7; w++) ui_frame();
    }
    a = rand() % 6 + 1;
    b = rand() % 6 + 1;
    ui_clear();
    iprintf("Lancio dadi...\n\n  [ %d ]  [ %d ]\n\nTotale: %d\n", a, b, a + b);
    for (int w = 0; w < 30; w++) ui_frame();
    if (d1) *d1 = a;
    if (d2) *d2 = b;
    return a + b;
}

void ui_msg(const char *title, const char *body) {
    ui_clear();
    if (title) iprintf("%s\n\n", title);
    if (body) iprintf("%s\n", body);
    iprintf("\n-- A:continua --");
    ui_waitKey();
}

void ui_waitKey(void) {
    for (;;) {
        ui_frame();
        unsigned int down = keysDown();
        if ((down & KEY_A) || (down & KEY_START) || (down & KEY_TOUCH)) {
            // anti-rimbalzo: attendi rilascio
            for (;;) {
                ui_frame();
                if (!(keysHeld() & (KEY_A | KEY_START | KEY_TOUCH))) break;
            }
            return;
        }
    }
}

void ui_showHand(const void *gamePtr) {
    const Game *g = (const Game *)gamePtr;
    char buf[40];
    ui_clear();
    iprintf("La tua mano (%s):\n\n", g->players[0].name);
    for (int c = 0; c < NCARDS; c++) {
        if (g->players[0].hand & CARD_BIT(c)) {
            card_name(c, buf, sizeof(buf));
            iprintf("- %s\n", buf);
        }
    }
    iprintf("\n-- A:continua --");
    ui_waitKey();
}

void ui_notebook(void *gamePtr) {
    Game *g = (Game *)gamePtr;
    static const char ST[4] = {'?', '*', 'V', 'X'};
    static char rows[NCARDS][40];
    char name[40];
    int sel = 0, quit = 0;
    while (!quit) {
        for (int c = 0; c < NCARDS; c++) {
            card_name(c, name, sizeof(name));
            const char *cat = c < 6 ? "S" : c < 12 ? "A" : "R";
            snprintf(rows[c], sizeof(rows[c]), "[%c]%s %s", ST[g->notes[c] & 3], cat, name);
        }
        ui_clear();
        // 2 righe header + 1 vuota + 21 carte = 24 righe esatte.
        // L'ultima riga e' stampata SENZA newline finale o la console scorre.
        iprintf("TACCUINO ?=ignota *=sospetta\nV=esclusa X=no A:cambia B:esci\n\n");
        for (int i = 0; i < NCARDS; i++)
            iprintf("%c %s%s", i == sel ? '>' : ' ', rows[i], i < NCARDS - 1 ? "\n" : "");
        for (;;) {
            ui_frame();
            unsigned int down = keysDown();
            int redraw = 0;
            if (down & KEY_UP) { sel = (sel + NCARDS - 1) % NCARDS; redraw = 1; }
            if (down & KEY_DOWN) { sel = (sel + 1) % NCARDS; redraw = 1; }
            if (down & KEY_A) { g->notes[sel] = (g->notes[sel] + 1) & 3; redraw = 1; }
            if ((down & KEY_B) || (down & KEY_START)) { quit = 1; break; }
            if (down & KEY_TOUCH) {
                touchPosition t;
                touchRead(&t);
                int idx = (int)(t.py / 8) - 3;
                if (idx >= 0 && idx < NCARDS) { sel = idx; redraw = 1; }
            }
            if (redraw) break;
        }
    }
}
