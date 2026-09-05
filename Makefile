# Makefile - Cluedo DS (BlocksDS, solo ARM9 + ARM7 precompilato)
# Compilazione locale:  make   (dentro Wonderful Toolchain Shell / container)
# Pulizia:              make clean
# Output: cluedo-ds.nds
BLOCKSDS ?= /opt/blocksds/core
BLOCKSDSEXT ?= /opt/blocksds/external

NAME := cluedo-ds
GAME_TITLE := Cluedo DS
GAME_SUBTITLE := Delitto a Villa Nera
GAME_AUTHOR := Villa Nera
GAME_ICON := icon.gif

SOURCEDIRS := source
INCLUDEDIRS := source
GFXDIRS := graphics

# ARM7 precompilato di default (fornisce anche i servizi audio FIFO
# usati dalle API sound PSG di libnds; MaxMod non e' usato dal gioco).
ARM7ELF := $(BLOCKSDS)/sys/arm7/main_core/arm7_maxmod.elf

# Solo libnds: niente MaxMod, niente FAT (tutto e' compilato nella ROM,
# NitroFS non serve -> nessun problema DLDI/argv su flashcart).
LIBS := -lnds9
LIBDIRS := $(BLOCKSDS)/libs/libnds

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
