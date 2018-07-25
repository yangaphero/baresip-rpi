#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= sdl2
$(MOD)_SRCS	+= sdl.c
$(MOD)_CFLAGS	:= -g -O2 -D_REENTRANT -I/usr/local/include/SDL2 -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux  -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DHAVE_OPENGL -DHAVE_SDL_TTF -g
$(MOD)_LFLAGS	+= -L/usr/local/lib -Wl,-rpath,/usr/local/lib -Wl,--enable-new-dtags -lSDL2

include mk/mod.mk
