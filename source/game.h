// game.h - dati e logica di Cluedo (port fedele di cluedo.html)
// Puro C standard: nessuna dipendenza da libnds (solo stdbool.h).
// Nota: tutte le stringhe sono SENZA accenti (il font console DS e' ASCII).
#pragma once
#include <stdbool.h>

#define NSUSP 6
#define NWEAP 6
#define NROOM 9
#define NCARDS 21
#define NGRID 24
#define MAXP 5

// id carte: 0..5 sospetti, 6..11 armi, 12..20 stanze
#define CARD_S(i) (i)
#define CARD_W(i) (6 + (i))
#define CARD_R(i) (12 + (i))
#define CARD_BIT(id) (1u << (id))

typedef struct {
    const char *name;
    int x0, y0, x1, y1;
    int doors[4][2];
    int ndoors;
    int passage; // indice stanza collegata dal passaggio segreto, -1 se nessuno
} RoomDef;

typedef struct {
    char name[24];
    int susp;              // 0..5 indice sospetto/pedina
    int ai;                // 0 umano, 1 IA
    unsigned int hand;     // bitmask carte in mano
    unsigned int seen;     // bitmask carte escluse (mano + viste)
    int out;               // eliminato
} Player;

typedef struct {
    int inRoom; // 0 cella, 1 stanza
    int room;   // indice stanza se inRoom
    int x, y;   // cella se !inRoom
} PawnPos;

typedef struct {
    Player players[MAXP];
    int nplayers;
    int envS, envW, envR;  // id carte nella busta
    PawnPos pos[NSUSP];    // posizione di TUTTE le 6 pedine
    int turn;              // indice in players
    int dice;
    int turnCount;
    int suggCount;
    int over;
    int winner;            // indice giocatore o -1
    unsigned char notes[NCARDS]; // taccuino umano: 0 ignota,1 sospetta,2 esclusa,3 no
} Game;

extern const char *SUSP_NAMES[NSUSP];
extern const char *WEAP_NAMES[NWEAP];
extern const RoomDef ROOMS[NROOM];
extern const int STARTS[NSUSP][2];
extern const char *AI_NAMES[5];

// --- setup / stato ---
void game_setup(Game *g, int mySusp, int nplayers);
int game_activeCount(const Game *g);
int game_nextTurn(Game *g); // avanza a prossimo non eliminato, ritorna indice
const char *card_name(int id, char *buf, int buflen); // scrive in buf, ritorna buf

// --- mappa ---
int roomAt(int x, int y);   // indice stanza o -1
int doorRoom(int x, int y); // indice stanza di cui (x,y) e' porta, o -1
int walkable(int x, int y);
// mask[y][x] = 1 raggiungibile con `steps` passi (BFS). Ritorna n. celle.
int bfsReach(const Game *g, int pi, int steps, unsigned char mask[NGRID][NGRID]);
int roomDist(int sx, int sy, int room); // passi minimi a una porta della stanza

// --- ipotesi / accusa ---
unsigned int wantMask(int sid, int wid, int rid);
int findDisprover(const Game *g, int suggester, unsigned int want); // indice o -1
unsigned int handMatches(unsigned int hand, unsigned int want);
int pickAiCard(unsigned int matches); // una carta a caso tra quelle che smentiscono
int accuseCorrect(const Game *g, int sid, int wid, int rid);

// --- IA (stessa logica di cluedo.html) ---
int aiUnknown(const Player *p, int cat, int *outIds); // cat 0=s,1=w,2=r; ritorna n
int aiConfident(const Player *p, int turnCount, int *s, int *w, int *r);
int aiTargetRoom(const Game *g, int pi); // stanza meta (puo' essere quella attuale)
void aiStepToward(Game *g, int pi, int goalRoom, int steps);
void aiPickSuggest(const Player *p, int *sid, int *wid);
