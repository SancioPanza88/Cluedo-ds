# CLUEDO DS — Delitto a Villa Nera (porting Nintendo DS)

Porting per Nintendo DS del gioco `cluedo.html`: stesse regole, stessi
personaggi/armi/stanze, stessa IA. La presentazione e' adattata all'hardware
(schermi 256x192, niente mouse): **sopra il tabellone, sotto i menu touch**.

## Stato del porting

- Regole 1:1 (busta, distribuzione, ipotesi, smentite, passaggi segreti, accuse).
- Logica IA portata pari pari (deduzione, mete, rischio dopo il turno 28).
- 12 ritratti + 9 stanze generati dagli stessi asset del gioco HTML
  (ridotti a 64x64 / 128x96 a 256 colori: il DS non ha JPEG ne' truecolor).
- Effetti sonori con i canali PSG hardware (nessun file audio).

Differenze volute rispetto all'HTML (limiti hardware veri):
- Niente emoji (il font console e' ASCII) e niente lettere accentate.
- I nomi delle stanze non stanno nei tile 8px: le stanze si riconoscono
  dal colore, il nome e' sempre scritto nello schermo sotto.
- Le armi non hanno ritratto (solo testo).
- Il movimento usa il cursore con D-pad (il touch esiste solo sotto).
- Nome giocatore fisso "Detective" (niente tastiera in setup).

## Comandi

- **D-pad**: muovi cursore / naviga menu / cambia pedina nel setup
- **A / TAP**: conferma, tira i dadi, mostra carta
- **B**: indietro / resta nella stanza
- **START**: conferma nei menu

## Compilare (GitHub Actions, consigliato)

1. Crea un repo con **il contenuto di questa cartella come root**
   (`Makefile`, `source/`, `graphics/`, `icon.gif`, `.github/`).
2. Pusha: la action `Build NDS` compila con `skylyrac/blocksds:slim-latest`.
3. Scarica `cluedo-ds.nds` dagli Artifact.

Compilazione locale: installa BlocksDS (docs: blocksds.skylyrac.net),
apri la shell del toolchain ed esegui `make`.

## Provare la ROM

- Emulatore consigliato: **melonDS** (apri il `.nds` e gioca).
- Hardware: qualsiasi flashcart Slot-1 (il gioco non usa NitroFS/SD,
  quindi nessun problema DLDI/argv).

## Struttura

```
Makefile                  build BlocksDS (ARM9 + ARM7 precompilato)
icon.gif                  banner ROM
source/main.c             flusso di gioco / stati
source/game.h/.c          logica pura (carte, BFS, IA) - niente libnds
source/board.h/.c         tabellone tilemap + sprite pedine (schermo sopra)
source/ui.h/.c            menu console + touch (schermo sotto)
source/art.h/.c           ritratti/stanze a tutto schermo sopra
source/sfx.h/.c           bip PSG
source/tiles_data.h       tiles/palette tabellone (generato, deterministico)
source/sprites_data.h     pedine/cursore (generato, deterministico)
graphics/*.png + *.grit   ritratti 64x64 e stanze 128x96 (grit in build)
tools/gen_assets.ps1      rigenera TUTTO da ../cluedo/assets (JPG originali)
.github/workflows/       CI che produce il .nds
```

Per rigenerare gli asset dopo aver cambiato i JPG:
`powershell -ExecutionPolicy Bypass -File tools\gen_assets.ps1`

## Nota tecnica (onesta)

"1:1 al 100% pixel-identico" su DS e' impossibile per fisica hardware
(2x 256x192 a 15-bit, 4 MB RAM, CPU ARM 67 MHz, touch solo sotto):
questo porting e' 1:1 su **contenuto e regole**, adattato su resa.
