#
# module.mk
#
# Copyright (C) 2013-2018 studio-link.de
#

MOD		:= webapp
$(MOD)_SRCS	+= webapp.c account.c contact.c chat.c vumeter.c videosrc.c 
#$(MOD)_SRCS	+= mono.c record.c routing.c option.c 
$(MOD)_SRCS	+= ws_baresip.c ws_contacts.c ws_meter.c ws_calls.c ws_chat.c ws_videosrc.c 
#$(MOD)_SRCS	+= ws_rtaudio.c ws_options.c 

$(MOD)_SRCS	+= websocket.c utils.c
$(MOD)_LFLAGS   += -lm 
#-lFLAC
$(MOD)_CFLAGS	:= -isystem /home/pi/baresip-master/modules/webapp/flac-1.3.2/include
include mk/mod.mk
