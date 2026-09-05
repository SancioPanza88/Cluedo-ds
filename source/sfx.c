// sfx.c - bip e jingle con i canali PSG/Noise hardware (niente MaxMod)
#include <nds.h>
#include "sfx.h"

void sfx_init(void) {
    soundEnable();
}

static void tone(int freq, int frames) {
    int ch = soundPlayPSG(DutyCycle_50, (u16)freq, 100, 64);
    for (int i = 0; i < frames; i++) {
        swiWaitForVBlank();
        oamUpdate(&oamMain);
    }
    if (ch >= 0) soundKill(ch);
}

static void noise(int freq, int frames) {
    int ch = soundPlayNoise((u16)freq, 100, 64);
    for (int i = 0; i < frames; i++) {
        swiWaitForVBlank();
        oamUpdate(&oamMain);
    }
    if (ch >= 0) soundKill(ch);
}

void sfx_click(void) { tone(600, 3); }
void sfx_step(void) { tone(520, 5); }
void sfx_door(void) { tone(330, 9); tone(495, 12); }
void sfx_card(void) { tone(700, 6); tone(900, 8); }
void sfx_bad(void) { tone(220, 18); tone(150, 26); }
void sfx_dice(void) {
    noise(500, 4); noise(700, 4); noise(900, 4); noise(650, 5);
}
void sfx_win(void) {
    static const int N[6] = {523, 659, 784, 1046, 784, 1046};
    for (int i = 0; i < 6; i++) tone(N[i], 10);
}
