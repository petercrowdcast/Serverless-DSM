GNU     = /usr/local/gnu/lib
#PLIB    = $(HOME)/lib/plib.a
UNIX	  = /usr/include/ansi
CPFLAGS = -g -D_NeXT_CPP_CLASSVAR_BUG
INCLUDE = -I${UNIX} -I${HOME}/lib/include 


OBJS    = Dsm_msg.o ${PLIB} ${HOME}/lib/C_mach_interface.o ${HOME}/lib/intqueue.o

dsm_server : dsm_server.h  Dsm_msg.o Dsm_msg.h dsm_server.o
	cc++ ${CPFLAGS} ${INCLUDE} -o dsm_server dsm_server.o ${OBJS} -lg++

dsm_server.o : dsm_server.h dsm_server.cc
	cc++ ${CPFLAGS} ${INCLUDE} -c dsm_server.cc

Dsm_msg.o : Dsm_msg.h Dsm_msg.cc
	cc++ ${CPFLAGS} ${INCLUDE} -c Dsm_msg.cc

clean : 
	rm -f *.o

clobber : clean
	rm -f dsm_server
	

