// ui.h - interfaccia testuale sullo schermo inferiore (console + touch)
#pragma once

// Un frame: VBlank + scanKeys + oamUpdate. Usare ovunque al posto delle
// chiamate grezze (scanKeys va chiamato 1 volta per frame nel game loop).
void ui_frame(void);
// Contatore frame globali (utile come seed RNG).
extern unsigned int ui_frames;
void ui_clear(void);
// Menu verticale: D-pad + A, B annulla (se allowCancel), TAP sulla riga.
// Ritorna indice scelto o -1.
int ui_menu(const char *title, const char *items[], int n, int allowCancel);
// Scelta SI/NO. Ritorna 1 = si.
int ui_yesno(const char *question);
// Animazione dadi, ritorna totale (e i singoli in d1,d2).
int ui_dice(int *d1, int *d2);
// Messaggio + attesa A/TAP.
void ui_msg(const char *title, const char *body);
// Mostra la mano del giocatore.
void ui_showHand(const void *gamePtr);
// Taccuino interattivo (21 carte, A = cambia stato, B = esci).
void ui_notebook(void *gamePtr);
// Attende A/START/TAP.
void ui_waitKey(void);
