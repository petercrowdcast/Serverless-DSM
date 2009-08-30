#  Makefile with suffix rule for C++.
.SUFFIXES : .cc

GNU      = /usr/local/gnu/lib
GINCLUDE = /usr/include/g++
GLIB     = ${GNU}/libg++.a
INCLUDE  = -I$(HOME)/lib/include
SL_OBJS  = sl_dsm_region.o sl_dsm.o
OBJS     =  $(SL_OBJS) ../dsm2/dsm_server/Dsm_msg.o  $(HOME)/lib/C_mach_interface.o $(HOME)/lib/intqueue.o
DEFINES  = -D_NeXT_CPP_CLASSVAR_BUG
CFLAGS   = -g $(DEFINES)

.cc.o :
	cc++ -I${GINCLUDE} -c $<

vmatrix : vmatrix.cc sl_dsm_region.h sl_dsm_region.o sl_dsm.o vmatrix.o
	cc++ ${CFLAGS} ${INCLUDE} -o vmatrix vmatrix.o ${OBJS} -lg++

vmatrix.o : vmatrix.cc
	cc++ ${CFLAGS} ${INCLUDE} -c vmatrix.cc

sl_dsm.o : sl_dsm.cc ../dsm2/dsm_server/Dsm_msg.h
	cc++ ${CFLAGS} ${INCLUDE}  -c sl_dsm.cc

sl_dsm_region.o	: sl_dsm_region.h sl_dsm_region.cc
	cc++ ${CFLAGS}  ${INCLUDE}  -c sl_dsm_region.cc

hello : $(OBJS)
	cc++ -I${GINCLUDE} -o $@ ${OBJS} -lg++

clean:	
	rm -f $(SL_OBJS) hello vmatrix

clobber:
	rm -f