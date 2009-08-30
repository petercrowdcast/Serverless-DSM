#include "sl_dsm_region.h"
#include <mint.h>
#include <event_counter2.h>

main(int argc, char *argv[])
{
	mint *iptr;
	int *ec_loc;

// get the arguments from the command line.
   if(argc != 4) {
      cerr << "usage: vmatrix <number of clients> <size of square matrix> "
      << " <client_no> \n";
      exit(-1);
   }
   int number_clients = atoi(argv[1]), size = atoi(argv[2]);
   int client = atoi(argv[3]);
	int page_count = 3;

 	sl_dsm_region  d(page_count, number_clients, client);

	ec_loc = d.get_iptr();

	event_counter ec(ec_loc, number_clients);

	port_t local_port;
	Cport_allocate(&local_port);
	Dsm_msg sync_msg(client, d.get_sync_port(), local_port);

	if(client == 0)
	{
		// init goes here
		sync_msg.send_control(INIT_DONE);
	}
	else
		sync_msg.wait_control(WAIT_INIT);

cout << "client " << client << " ready to advance the counter\n";
	ec.advance();
	ec.wait();
	
	if(client == 0)
	{
		// cleanup goes here
		sync_msg.wait_control(CLEAN_UP);
	}


	sync_msg.wait_control(CLIENT_DONE);


	cout << "client " << client << "done\n";
}
