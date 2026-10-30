# Build configuration shared by all modules (modelled on ec-/baseq3a build/linux-qvm/config.mk).
# Paths are relative to this directory (build/). Every variable can be overridden on the
# make command line, e.g.  make MODULE=ui OUT=/tmp/vm

ROOT := ..

# Module to build: qagame (default), ui or cgame. Its settings (source dir, link order,
# cflags, build date, expected SHA-256) are in build/<module>.mk; the link orders in
# build/srcs.mk.
MODULE ?= qagame
ifeq ($(filter $(MODULE),qagame ui cgame),)
$(error unknown MODULE=$(MODULE), use qagame, ui or cgame)
endif

# Shared source directory: id's code/game layout. qagame's own sources live here too; the
# ui and cgame builds take the shared files (bg_misc, bg_lib, q_math, q_shared, ...) from
# here.
GAMEDIR ?= $(ROOT)/src/game

# Directory holding menudef.h. id's ai_*.c / g_cmds.c include "../../ui/menudef.h"; a small
# include shim under $(OUT) makes that path resolve wherever GAMEDIR lives.
UIDIR ?= $(GAMEDIR)/../ui

# Output directory: $(OUT)/<module>.qvm (+ .map) and $(OUT)/<module>/*.asm.
OUT ?= vm

include $(MODULE).mk

# Toolchain (installed on PATH inside the xq3osp-build Docker image).
Q3LCC ?= q3lcc
Q3ASM ?= q3asm

# Flags as in id's code/game/game.sh / q3_ui.sh for 1.32:
#   lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g -I<dirs>
# id's lcc driver defaults to an x86 target, so -Wf-target=bytecode is required.
# MOD_CFLAGS (per module, build/<module>.mk) adds -DQAGAME / -DQ3UI / -DCGAME.
LCCFLAGS ?= -DQ3_VM -S -Wf-target=bytecode -Wf-g
MOD_CFLAGS ?=
MOD_INCLUDES ?=
ASMFLAGS ?=

# __DATE__ / __TIME__ as q3cpp should see them (qagame's g_main.c puts __DATE__ into the
# "gamedate" cvar and a G_Printf, so it lands in the lit segment). Pinned through the
# LD_PRELOAD shim docker/cppdate.c (installed in the image as /opt/q3tools/lib/cppdate.so),
# which replaces libc ctime() in q3cpp. Q3CPP_DATE is set per module (build/<module>.mk);
# the time is not used by any module. Format: Q3CPP_DATE exactly 11 chars, Q3CPP_TIME 8
# chars. Set CPPDATE_SO= (empty) to get the real current date instead.
Q3CPP_TIME ?= 12:00:00
CPPDATE_SO ?= /opt/q3tools/lib/cppdate.so

include srcs.mk
