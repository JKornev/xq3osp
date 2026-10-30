# qagame: the OSP 1.03a server module (vm/qagame.qvm in zz-osp-server3a.pk3).

# qagame's sources are the shared game directory itself.
SRCDIR ?= $(GAMEDIR)
# Link order (build/srcs.mk): OSP's game.q3asm order with OSP's files inserted.
MOD_SRC ?= $(QA_SRC)
# id's game.sh / Conscript pass no module define; QAGAME (as in baseq3a's config.mk) only
# selects a Com_Printf text inside an #ifdef _DEBUG block of bg_misc.c, so it does not change
# the output.
MOD_CFLAGS ?= -DQAGAME
# OSP 1.03a was built on "Mar 04 2003" (zero-padded day, as the MSVC runtime's ctime prints
# it); g_main.c puts __DATE__ into the lit segment.
Q3CPP_DATE ?= Mar 04 2003

# SHA-256 of the released qagame.qvm (`make verify`).
EXPECTED_SHA256 ?= 7ce6a6ac1fcb444206461e19129d4f5cd1c4ba79a3cbf881653e6450b8277160
