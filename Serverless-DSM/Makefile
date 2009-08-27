
LIB		= $(HOME)/lib
MAKE 	= gnumake
CC 		= cc
DEFINES = -D_NeXT_CPP_CLASSVAR_BUG
CFLAGS	= -g $(DEFINES)
INCLUDE = -I$(HOME)/lib/include

dsm_client : dsm_region.h dsm_client.cc dsm_region.o dsm_server/Dsm_msg.o \
dsm_server/Dsm_msg.h dsm.o
	$(CC) $(INCLUDE) -o dsm_client dsm_client.cc $(LIB)/C_mach_interface.o dsm_region.o dsm_server/Dsm_msg.o dsm.o -lg++

dsm.o : dsm.cc dsm_server/Dsm_msg.h
	$(CC) $(INCLUDE) $(CFLAGS) -c dsm.cc

dsm_region.o	: dsm_region.h dsm_region.cc
	$(CC)  $(INCLUDE) $(CFLAGS) -c dsm_region.cc

dsm_server/Dsm_msg.o	: dsm_server/Dsm_msg.h dsm_server/Dsm_msg.cc
	$(CC) $(INCLUDE) $(CFLAGS) -o dsm_server/Dsm_msg.o -c dsm_server/Dsm_msg.cc

matrix : matrix.cc dsm_region.h dsm_region.o dsm.o dsm_server/Dsm_msg.h dsm_server/Dsm_msg.o mfloat.h
	$(CC)  $(INCLUDE) $(CFLAGS) -o matrix matrix.cc $(LIB)/C_mach_interface.o dsm_region.o dsm_server/Dsm_msg.o dsm.o

vmatrix : vmatrix.cc dsm_region.h dsm_region.o dsm.o dsm_server/Dsm_msg.h dsm_server/Dsm_msg.o mfloat.h
	$(CC)  $(INCLUDE) $(CFLAGS) -o vmatrix vmatrix.cc  $(LIB)/C_mach_interface.o dsm_region.o dsm_server/Dsm_msg.o dsm.o -lg++

	
vmatrix_ec : vmatrix_ec.cc dsm_region.h dsm_region.o dsm.o dsm_server/Dsm_msg.h \
dsm_server/Dsm_msg.o mfloat.h event_counter.h
	$(CC) $(INCLUDE) $(CFLAGS) -o vmatrix_ec vmatrix_ec.cc C_mach_interface.o dsm_region.o \
	dsm_server/Dsm_msg.o dsm.o $(LIB)/C_mach_interface.o

clean :
	rm -f *.o

