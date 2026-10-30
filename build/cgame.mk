# cgame: the OSP 1.03a client module (vm/cgame.qvm in zz-osp-pak3.pk3, 2003-02-28).

SRCDIR ?= $(ROOT)/src/cgame
# Link order (build/srcs.mk).
MOD_SRC ?= $(CG_SRC)
# id's cgame.sh and Conscript pass -DCGAME (bg_misc.c tests QAGAME only inside #ifdef _DEBUG).
MOD_CFLAGS ?= -DCGAME
# cg_consolecmds.c, cg_draw.c include "../ui/ui_shared.h", which includes "keycodes.h";
# id's cgame.sh / Conscript find it with -I../../q3_ui. Here the same file is in src/ui.
MOD_INCLUDES ?= -I$(abspath $(GAMEDIR)/../ui)
# The cgame contains no __DATE__; pinned anyway.
Q3CPP_DATE ?= Feb 28 2003

# SHA-256 of the released cgame.qvm (`make verify`).
EXPECTED_SHA256 ?= 48e17665b0b274baa95670364364be373fbbda4aea6e74c1fb587aaa5feb33b3
