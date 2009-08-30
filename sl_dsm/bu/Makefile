HOME    = /lore/home2/mnop/pogilvie
GNU     = /usr/local/gnu/lib
PLIB    = ${HOME}/lib/plib.a
UNIX	  = /usr/include/ansi
CPFLAGS = -g 
INCLUDE = -I${UNIX} -I${HOME}/lib/include 


OBJS    =  sl_dsm_region.o \
          ../dsm2/dsm_server/Dsm_msg.o sl_dsm.o ${PLIB} \
          ${HOME}/lib/C_mach_interface.o 



vmatrix : vmatrix.cc sl_dsm_region.h sl_dsm_region.o sl_dsm.o vmatrix.o
	c++ ${CPFLAGS} ${INCLUDE} -o vmatrix vmatrix.o ${OBJS}

vmatrix.o : vmatrix.cc
	c++ ${CPFLAGS} ${INCLUDE} -c vmatrix.cc

fmatrix : fmatrix.cc sl_dsm_region.h sl_dsm_region.o sl_dsm.o
	c++ ${CPFLAGS} ${INCLUDE} -o fvmatrix fvmatrix.cc ${OBJS}


sl_dsm.o : sl_dsm.cc ../dsm2/dsm_server/Dsm_msg.h
	c++ ${CPFLAGS} ${INCLUDE}  -c sl_dsm.cc

sl_dsm_region.o	: sl_dsm_region.h sl_dsm_region.cc
	c++ ${CPFLAGS} ${INCLUDE}  -c sl_dsm_region.cc


test_client : test_client.cc sl_dsm_region.h sl_dsm_region.o sl_dsm.o
	c++  ${CPFLAGS} ${INCLUDE} -o test_client test_client.cc ${OBJS}
  

