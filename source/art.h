// art.h - mostra ritratti e stanze sullo schermo superiore (layer dedicato)
#pragma once

void art_init(void);
void art_showSuspect(int i); // 0..5
void art_showRoom(int i);    // 0..8
void art_hide(const void *gamePtr); // ripristina tabellone, palette e pedine
