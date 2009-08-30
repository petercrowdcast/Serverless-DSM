//	file		: 	vmatrix.cc
//	author	:	Peter Ogilvie
//	date		:	5/30/92
//	notes		:	This program tests the use of and performance of server-less
//					distributed shared memory. It is quite similar to the program of
//					the same name used to test the single server distributed shared
//					memory.

#include <mach/mach.h>
#include <mach/cthreads.h>
#include <stdio.h>
#include <stdlib.h>
#include "sl_dsm_region.h"
#include "mfloat.h"
//#include <builtin.h>
#include <timer.h>

//	function print_matrix()
//	results	:	This function prints a matrix of floats to standard out pointed
//					to by m.  The size of the matrix is specified by size_x and 
//					size_y.
// notes		:	This function is used for debugging.
void print_matrix(mfloat *m, int size_x, int size_y)
{
	int i, limit = size_x * size_y;
	
	for(i = 0; i < limit; i++)
	{
		if(i % size_x == 0)
			cout << "\n";
			
		
	}
	cout << "\n";
}

//	function fill_easy()
//	results	:	Fills the memory pointed to by m with sequential numbers.
//					the size of the matrix is determined by size_x and size_y.
//	notes		:	The matrix used in this function is the first multiplicand.
//					It is filled with sequential numbers to make checking the 
//					results simple, and size independent.  Also the use of 
//					sequential numbers allows pages where problems occure to be 
//					easily calculated.
void fill_easy(mfloat *m, int size_x, int size_y)
{
	int i;
	
	for(i = 0; i < size_x * size_y; i++)
		m[i] = i;
}

//	function fill_zero()
//	results	:	Fills the result area with zeros.
//	notes		:	While not really required since new memory is automaticly
//					zero filled it was felt that this would simulate a broader
//					class of applications in which all memory required some initial
//					state.
void fill_zero(mfloat *m , int size_x, int size_y)
{
	for(int i = 0; i < size_x * size_y; i++)
		m[i] = 0.0;
}



// function fill_identity()
//	results	: fills the square matrix pointed to by m with side size side
//				  with the identity matrix.
// assumes	: Square matrix with side = side
void fill_identity(mfloat *m, int side)
{
	int i;
	
		for(i = 0; i < side * side; i++)
		{
			if(i % (side +1))
				m[i] = 0.0;		
			else
				m[i] = 1.0;	
		}
}

// function pmult_matrix(), parallel matrix multiply.
//	results	:	result = a * b where result, a, b are square matrices of
//					floats.  The portion of the calculation done is determined
//					by the parameters start and stop.  For the full matrix:
//					start = 0 stop = side_a * side_a.
// Assumes: Square matrix
void pmult_matrix(mfloat *result, mfloat *a, int side_a, 
							    mfloat *b, int side_b, 
							    int start, int stop)
{
	int i, j, pos1, pos2;
	
	
	for(i = start; i < stop; i++)
	{
		pos1 = i - (i % side_a);
		pos2 = i % side_b;
		for(j = 0; j < side_b; j++)
		{
			result[i] = result[i] + (a[pos1] * b[pos2]);
			pos1++;
			pos2 += side_b;
		}
	}
}
// function calc_pages()
// returns	:	The number of pages required hold the three floating point
//					matrices used in the program.  (Two for the source and one for
//					the result.) Where size is the number of elements in one side of
//					of a square matrix.
// assumes	:	size > 0
// notes		:	May allocate one more page than needed.  This extra page may
//					be used to hold event_counters.
inline int calc_pages(int size)
{
	return (size * size * sizeof(mfloat)  * 3 /vm_page_size) + 1;
}

// function start_index
// returns	:	The starting index for use by pmult_matrix, where client_no
//					is the client number of this client, number_clients is the 
//					number of clients being used for this computation, and size
//					is the size of one side of the square matrix.
inline int start_index(int client_no, int number_clients, int size)
{
	return client_no * size * size / number_clients;
}

// function stop_index
// returns	:	The ending index for use by pmult_matrix, where client_no
//					is the client number of this client, number_clients is the 
//					number of clients being used for this computation, and size
//					is the size of one side of the square matrix.
inline int stop_index(int client_no, int number_clients, int size)
{
	return (client_no + 1) * size * size / number_clients;
}

//	function check_results()
//	returns	: TRUE if the result matrix pointed to by m contains the correct
//				  results which is a sequence of sequential numbers from 
//				  0...size * size.  Where size = the size of one side of the
//				  square matrix.
int check_results(mfloat *m,int size)
{
	int i, limit = size * size;
	boolean_t worked = TRUE;
	
	for(i = 0; i < limit; i++)
		if(float(m[i]) != i) {
			cerr << m[i] << " != " << i << "\n";
			worked = FALSE;
		} 
		
	return worked;
}

extern "C" int gethostname(char *, int);

	
main(int argc, char *argv[])
{
	char name[64];
	timer init_timer, check_timer;

// get the arguments from the command line.
	if(argc != 4) {
		cerr << "usage: vmatrix <number of clients> <size of square matrix>\
		<client_no> \n";
		exit(-1);
	}
	int number_clients = atoi(argv[1]), size = atoi(argv[2]);
	int client_no = atoi(argv[3]);
	
	
// pointers which point to the matrices
	mfloat *m, *x, *y;
	
	int elements = size * size;
	int *iptr;
	sl_dsm_region	d(calc_pages(size), number_clients, client_no);
	double atime;

// Message to sent events to server for barrier events
	port_t  event_port;
	Cport_allocate(&event_port);
	Dsm_msg event_msg(client_no, d.get_sync_port() , event_port);


	
   gethostname(name,64);
	cout << "client " << client_no << " is on " << name << "\n";
	iptr = d.get_iptr();
	
	
// Set the pointers of the matrices to point into distributed shared memory.
	m = (mfloat *) iptr;
	x = m + elements;
	y = x + elements;


// Initialization.  If fewer than three clients client 0 dose all inits else
// init is shared by client 0 1, and 2.
int wait_count;
if(number_clients >= 3) {
   wait_count = 3;
   if(client_no == 0) // client 1 does the init
   {
		init_timer.set();
      fill_easy(x, size, size);
      event_msg.send_control(INIT_DONE);
		cout << "init time = " << init_timer.get_elapsed_time() << " msec\n";
   }
   if(client_no == 1 )
   {
		init_timer.set();
      fill_identity(y, size);
      event_msg.send_control(INIT_DONE);
		cout << "init time = " << init_timer.get_elapsed_time() << " msec\n";
   }
   if(client_no == 2 )
   {
		init_timer.set();
      fill_zero(m, size, size);
      event_msg.send_control(INIT_DONE);
		cout << "init time = " << init_timer.get_elapsed_time() << " msec\n";
   }
}
else
{
   wait_count = 1;
   if(client_no == 0)
   {
		init_timer.set();
      fill_easy(x, size, size);
      fill_identity(y, size);
      fill_zero(m, size, size);
      event_msg.send_control(INIT_DONE);
		cout << "init time = " << init_timer.get_elapsed_time() << " msec\n";
   }
}
   event_msg.wait_control(WAIT_INIT, wait_count , d.get_sync_port());


	
// This is the calculation
	pmult_matrix(m, x, size, y, size,
					 start_index(client_no, number_clients, size),
					 stop_index(client_no, number_clients, size));

		
	if(client_no == 0) // client 0 checks the results
	{
		event_msg.wait_control(CLEAN_UP);
		check_timer.set();

		if(check_results(m, size))
			cerr << "\nmult worked\n";
		else
			cerr << "mult failed\n";
		
		cout << "Check time = " << check_timer.get_elapsed_time() << "msec\n";
	}

	event_msg.wait_control(CLIENT_DONE);

	cout  << "client " << client_no << " done.\n";
}
