ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

# Location of the ChartDirector library
CD       = /usr/local/chartdir
CDFLAGS  = -I$(CD)/include
CDLIBS   = -L$(CD)/lib -Wl,-R$(CD)/lib -lchartdir

#
# Module name
#
MOD      =  nschartdir.so

#
# Objects to build.
#
MODOBJS     = nschartdir.o
CFLAGS	 = $(CDFLAGS)
MODLIBS	 = $(CDLIBS)

include  $(NAVISERVER)/include/Makefile.module

CC	= g++
LD	= g++
LDSO	= g++ -pipe -shared -nostartfiles

