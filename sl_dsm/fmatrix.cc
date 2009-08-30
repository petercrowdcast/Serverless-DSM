#include <stdio.h>
#include <cthreads.h>
#include <stdlib.h>
#include "sl_dsm_region.h"
#include "mfloat.h"
#include <builtin.h>
#include <timer.h>

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

void fill_easy(mfloat *m, int size_x, int size_y)
{
	int i;
	
	for(i = 0; i < size_x * size_y; i++)
		m[i] = i;	// changed form *m++
}

void fill_zero(mfloat *m , int size_x, int size_y)
{
	for(int i = 0; i < size_x * size_y; i++)
		m[i] = 0.0;
}


// assumes:	Square matrix with side = side
// results:	Square matrix is fill to identity matrix.

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

void pfill_zero(mfloat *m, int start, int stop)
{
	for(int i = start; i < stop; i++)
		m[i] = 0;
}
	
main(int argc, char *argv[])
{
	char name[64];
	timer init_timer, check_timer;

// get the arguments from the command line.
	if(argc != 4) {
		cerr << "usage: vmatrix <number of clients> <size of squar matrix>\
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
	
	
	m = (mfloat *) iptr;
	x = m + elements;
	y = x + elements;

int wait_count;
if(number_clients >= 3) {
   wait_count = 2;
	
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
	/*
   if(client_no == 2 )
   {
		init_timer.set();
      fill_zero(m, size, size);
      event_msg.send_control(INIT_DONE);
		cout << "init time = " << init_timer.get_elapsed_time() << " msec\n";
   }
	*/
}
else
{
   wait_count = 1;
   if(client_no == 1)
   {
	   init_timer.set();
      fill_easy(x, size, size);
      fill_identity(y, size);
    //  fill_zero(m, size, size);
      event_msg.send_control(INIT_DONE);
		cout << "init time = " << init_timer.get_elapsed_time() << " msec\n";
   }
}

	pfill_zero(m, start_index(client_no, number_clients, size),
					  stop_index(client_no, number_clients, size));

   event_msg.wait_control(WAIT_INIT, wait_count , d.get_sync_port());


	

	pmult_matrix(m, x, size, y, size,
					 start_index(client_no, number_clients, size),
					 stop_index(client_no, number_clients, size));

		
	if(client_no == 0) // client1 checks the results
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
