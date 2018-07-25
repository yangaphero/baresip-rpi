#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= vidosd
$(MOD)_SRCS	+= vidosd.c panel_osd.c
$(MOD)_LFLAGS	+= $(shell pkg-config --libs cairo)
$(MOD)_CFLAGS	+= $(shell pkg-config --cflags cairo)

include mk/mod.mk
