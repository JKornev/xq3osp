# ui: the OSP ui module (vm/ui.qvm in zz-osp-pak1.pk3, 2002-01-13).

SRCDIR ?= $(ROOT)/src/ui
# Link order (build/srcs.mk).
MOD_SRC ?= $(UI_SRC)
MOD_CFLAGS ?= -DQ3UI
# The ui contains no __DATE__; pinned anyway.
Q3CPP_DATE ?= Jan 13 2002

# SHA-256 of the released ui.qvm (`make verify`).
EXPECTED_SHA256 ?= 36133dea02f56992a8683f852893a414e330cc87063895671ee268b51891147c
