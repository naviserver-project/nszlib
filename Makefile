ifndef NAVISERVER
  NAVISERVER  = /usr/local/ns
endif

#
# Module name
#
MOD      =  nszlib.so

#
# Objects to build.
#
OBJS     = nszlib.o

MODLIBS	 = -lz

include $(NAVISERVER)/include/Makefile.module
