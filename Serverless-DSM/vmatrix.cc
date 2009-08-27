// file     :  vmatrix.cc
// author   :  Peter Ogilvie
// date     :  5/30/92
// notes    :  This program tests the use of and performance of server-less
//             distributed shared memory. It is quite similar to the program of
//             the same name used to test the single server distributed shared
//             memory.


#include <stdio.h>
#include <mach/cthreads.h>
#include <stdlib.h>
#include "dsm_region.h"
#include "dsm_server/dsm_server.h"
#include "dsm_server/Dsm_msg.h"
#include "mfloat.h"
#include <builtin.h>
#include <timer.h>

// function print_matrix()
// results  :  This function prints a matrix of floats to standard out pointed
//             to by m.  The size of the matrix is specified by size_x and
//             size_y.
// notes    :  This function is used for debugging.
void print_matrix(mfloat *m, int size_x, int size_y)
{
	int i, limit = size_x * size_y;
	
	for(i = 0; i < limit; i++)
	{
		if(i % size_x == 0)
			cout << "\n";
			
//		if(i % 100 == 0)
			cout << " " << m[i];
		
	}
	cout << "\n";
}

 
// function fill_easy()
// results  :  Fills the memory pointed to by m with sequential numbers.
//             the size of the matrix is determined by size_x and size_y.
// notes    :  The matrix used in this function is the first multiplicand.
//             It is filled with sequential numbers to make checking the
//             results simple, and size independent.  Also the use of
//             sequential numbers allows pages where problems occure to be
//             easily calculated.
void fill_easy(mfloat *m, int size_x, int size_y)
{
	int i;
	
	for(i = 0; i < size_x * size_y; i++)
		m[i] = i;
}

// function fill_zero()
// results  :  Fills the result area with zeros.
// notes    :  While not really required since new memory is automaticly
//             zero filled it was felt that this would simulate a broader
//             class of applications in which all memory required some initial
//             state.
void fill_zero(mfloat *m , int size_x, int size_y)
{
	for(int i = 0; i < size_x * size_y; i++)
		m[i] = 0.0;
}


// function fill_identity()
// results  : fills the square matrix pointed to by m with side size side
//            with the identity matrix.
// assumes  : Square matrix with side = side
void fill_identity(mfloat *m, int side)
{
	int i;
	
		for(i = 0; i < side * side; i++)
		{
			if(i % (side +1))
				m[i] = 0.0;		// changed form *m++
			else
				m[i] = 1.0;		// changed form *m++
		}
}
// function mult_matrix()
// results	: This function is a plain vanillia matrix multiply
void mult_matrix(mfloat *result, mfloat *a, int size_ax, int size_ay,
							    mfloat *b, int size_bx, int size_by)
{
	int i, j, limit, pos1, pos2;
	mfloat temp;
	
	limit = size_ax * size_ay;
	
	for(i = 0; i < limit; i++)
	{
		pos1 = i - (i % size_ax);
		pos2 = i % size_by;
		for(j = 0; j < size_by; j++)
		{
			result[i] = result[i] + (a[pos1] * b[pos2]);
			pos1++;
			pos2 += size_by;
		}
	}
}

// function pmult_matrix(), parallel matrix multiply.
// results  :  result = a * b where result, a, b are square matrices of
//             floats.  The portion of the calculation done is determined
//             by the parameters start and stop.  For the full matrix:
//             start = 0 stop = side_a * side_a.
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
// notes		:	May allocate one more page than needed.

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
	return (client_no - 1) * size * size / number_clients;
}

// function stop_index
// returns	:	The ending index for use by pmult_matrix, where client_no
//					is the client number of this client, number_clients is the 
//					number of clients being used for this computation, and size
//					is the size of one side of the square matrix.

inline int stop_index(int client_no, int number_clients, int size)
{
	return client_no * size * size / number_clients;
}
int check_results(mfloat *m,int size)
{
	int i, limit = size * size;
	
	for(i = 0; i < limit; i++)
		if(float(m[i]) != i) {
			cerr << m[i] << " != " << i << "\n";
			return FALSE;
		} 
		
	return TRUE;
}

extern "C" int gethostname(char *, int);

// These variables are global so that the threads can access them they are
// defined in dsm.cc
extern int client_no;
extern port_t server_port;
	
main(int argc, char *argv[])
{
	char name[64];
	timer init_timer, check_timer;
	unsigned init_time, check_time;

// get the arguments from the command line.
	if(argc != 3) {
		cerr << "usage: vmatrix <number of clients> <size of squar matrix>\n";
		exit(-1);
	}
	int number_clients = atoi(argv[1]), size = atoi(argv[2]);
	
	
// pointers which point to the matrices
	mfloat *m, *x, *y;
	
	int elements = size * size;
	int *iptr;
	dsm_region	d(calc_pages(size), number_clients);
	double atime;

// Message to sent events to server for barrier events
	port_t  event_port;
	Cport_allocate(&event_port);
	Dsm_msg event_msg(client_no, server_port, event_port);


	
   gethostname(name,64);
	cout << "client " << client_no << " is on " << name << "\n";
	iptr = d.get_iptr();
	
	
// Set the pointer to the matrices to point to distributed shared memory.
	m = (mfloat *) iptr;
	x = m + elements;
	y = x + elements;

	int wait_count;
	
	init_timer.set();

// if there are three or more clients then client 1, 2, 3 do init else
// only client 1 does the init.
if(number_clients >= 3) {
	wait_count = 3;
	if(client_no == 1) // client 1 does the init
	{
 		fill_easy(x, size, size);
      event_msg.send_control(INIT_DONE);
		init_time = init_timer.get_elapsed_time();
		cout << "Init time " << init_time << "msec\n";
	}
	else if(client_no == 2 ) 
	{
      fill_identity(y, size);
      event_msg.send_control(INIT_DONE);
		init_time = init_timer.get_elapsed_time();
		cout << "Init time " << init_time << "msec\n";
	}
	else if(client_no == 3 )
	{
      fill_zero(m, size, size);
      event_msg.send_control(INIT_DONE);
		init_time = init_timer.get_elapsed_time();
		cout << "Init time " << init_time << "msec\n";
	}
}
else
{
	wait_count = 1;
	if(client_no == 1)
	{
		fill_easy(x, size, size);
		fill_identity(y, size);
		fill_zero(m, size, size);
      event_msg.send_control(INIT_DONE);
		init_time = init_timer.get_elapsed_time();
		cout << "Init time " << init_time << "msec\n";
	}
}
	event_msg.wait_control(WAIT_INIT, wait_count , server_port);
	
// This is the calculation.
	pmult_matrix(m, x, size, y, size,
					 start_index(client_no, number_clients, size),
					 stop_index(client_no, number_clients, size));

		
	if(client_no == 1) // client1 checks the results
	{
		check_timer.set();
		event_msg.wait_control(CLEAN_UP);

		if(check_results(m, size))
			cerr << "\nmult worked\n";
		else
			cerr << "mult failed\n";
		
		check_time = check_timer.get_elapsed_time();

		cout << "Check time = " << check_time << "msec\n";
		//print_matrix(m, size, size);
	}

	event_msg.wait_control(CLIENT_DONE);


	cout  << "client " << client_no << " done.\n";
}
