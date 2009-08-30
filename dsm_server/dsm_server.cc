// dsm_server.cc
// 	author	: Peter Ogilvie
// 	date		: 3/1/92
// 	notes		: This file contains the code for a centralized server
//					  distributed memory management system. It is highly
//					  simplified since it only manages one address space and
//					  uses largely synchronous communication.

#include "dsm_server.h"
#include "Dsm_msg.h"
#include <stream.h>
#include <timer.h>


extern "C" {
	void exit();
}
int client_no = 1;		//  Stands for the server itself.
client_table_entry client_table[MAX_CLIENTS +1];
page_table_entry   page_table[DSM_PAGE_COUNT];
vm_address_t       the_pages;


port_t        server_port;

main()
{
	int 			the_client, number_of_clients, Q_len;
	intqueue 	done_Q;
	intqueue 	wait_Q;
	port_t 		done_client;
	boolean_t	clean_up_flag = FALSE, init_count = 0;

	int count = 0;
   server_port = dsm_init();

	timer calc_time;

	Dsm_msg 		done_msg(SERVER, PORT_NULL, server_port);
	Dsm_msg 		wait_msg(SERVER, PORT_NULL, server_port);
	Dsm_msg 		clean_up_msg(SERVER, PORT_NULL, server_port);
   Dsm_msg      m(SERVER, PORT_NULL, server_port);

cout << "server done init\n";
   while(1)
   {
#ifdef DEBUG
      printf("Server ready to receive\n");
#endif
		m.receive();

// Get the client number and check it.

		the_client = m.get_client_no();

		if(((the_client < 0) || (the_client > MAX_CLIENTS + 1))
			  && (m.get_request() != CLIENT_INIT))
		{
			cout << "Invalid client no " << the_client << "\n";
			cout << "Request = " << m.get_request() << "\n";
			exit(-1);
		}



      switch(m.get_request())
      {
	 		case CLIENT_INIT    :
				number_of_clients = m.get_client_no();
				do_client_init(m.get_rport());
			break;
	 
	 		case EXCEPTION_INIT :
#ifdef DEBUG
				cout << "Got exception init from client " << the_client << "\n";
#endif
				client_table[the_client].except_port = m.get_rport();
		   break;

	 		case RESPONDER_INIT :
#ifdef DEBUG
				cout << "Got responder init from client " << the_client << "\n"; 
				cout << "responder's port is " << m.get_rport() << "\n";
#endif
         	client_table[the_client].respond_port = m.get_rport();
		   break;

      	case READ_FAULT     :
				do_read_fault(m.get_page_no(), the_client);
         break;

      	case WRITE_FAULT    :
				do_write_fault(m.get_page_no(), the_client);
         break;

			case TEST:
				cout << "Got a test message\n";
				m.send();
			break;

			case INIT_DONE :
#ifdef DEBUG
				cout << "Got init_done from client " << the_client << "\n";
#endif
				init_count++;
			break;

			case WAIT_INIT	:
#ifdef DEBUG
				cout << "Got wait msg from client " << the_client << "\n";
#endif
				wait_Q.enq(m.get_rport());

				if(init_count == m.get_page_no())
				{
					calc_time.set(); // start calc timer when init done;
					while(!wait_Q.empty())
					{
					   wait_msg.send_control(INIT_DONE, wait_Q.deq());
					}
				}
			break;

			case CLIENT_DONE:
cout << "Got client done message from " << the_client << "\n";
				done_Q.enq(m.get_rport());
				Q_len = done_Q.length();

				if(Q_len == number_of_clients -1 || number_of_clients == 1)
					cout << "Calc time = "<< calc_time.get_elapsed_time() << "\n";

				if(Q_len == number_of_clients)
				{
					while(!done_Q.empty())
					{
						done_client = done_Q.deq();
						done_msg.send_control(CLIENT_DONE, done_client);
					}
					Cvm_deallocate(the_pages, vm_page_size * DSM_PAGE_COUNT);
					cout << "server done\n";
					exit(1);
				}	
				else if(Q_len == (number_of_clients -1) && clean_up_flag)
					clean_up_msg.send();
				
			break;
					
			case CLEAN_UP :
#ifdef DEBUG
				cout << "Got clean up message\n";
#endif
				if(done_Q.length() == (number_of_clients - 1))
				{
					clean_up_msg.set_rport(m.get_rport());
					clean_up_msg.set_request(CLEAN_UP);
					clean_up_msg.set_page_no(0);
					clean_up_msg.set_page(PAGE_NULL);
					clean_up_msg.send();
				}

				else if(!clean_up_flag)
				{
					clean_up_flag = TRUE;
					clean_up_msg.set_rport(m.get_rport());
					clean_up_msg.set_request(CLEAN_UP);
					clean_up_msg.set_page_no(0);
					clean_up_msg.set_page(PAGE_NULL);
				}
			break;
					
	 
			default :
				cerr << "dsm_server bad request = "<< m.get_request() << "\n";
		   exit(-1);
      }
   }
}

port_t dsm_init()
{
  port_t           server_port;
  kern_return_t    r;
  int              i, j;
  vm_address_t              *aptr;


// Allocate the pages.

  Cvm_allocate(&the_pages, vm_page_size * DSM_PAGE_COUNT);


// Initailize the client table
  for(i = 1; i < MAX_CLIENTS + 1; i++)  {
     client_table[i].except_port = PORT_NULL;
     client_table[i].respond_port = PORT_NULL;
   }

// Initailize the page table

  for(i = 0; i < DSM_PAGE_COUNT; i++) {
    page_table[i].state = INIT;
    page_table[i].owner = SERVER;
  } 

// Allocate a port for server communication.

  Cport_allocate(&server_port);
 
//  Load the pages with initial values for testing.
   for(i = 0; i < DSM_PAGE_COUNT; i++)
   {
     aptr = page_to_address(i);
     for(j = 0; j < PAGE_DATA_SIZE; j++)
        aptr[j] = (i + 1);
   }
  
// Register new port with net name server.

   Cnetname_check_in("dsm_server", server_port);


// Return new port so that server can receive on it.

  return server_port;
}



vm_address_t* page_to_address(int page)
{
  vm_address_t *addr;

  addr = (vm_address_t*) the_pages;

  addr = addr + (PAGE_DATA_SIZE * page);

  return addr;
}



void do_client_init(port_t remote_port)
{
   Dsm_msg  msg_xmt(client_no, remote_port, PORT_NULL);
   kern_return_t r;

   if(client_no >= MAX_CLIENTS + 1) {
      cout <<"do_client_init() too many clients\n"; exit(-1);
   }

	msg_xmt.set_request(CLIENT_INIT);
	msg_xmt.set_page(PAGE_NULL);
              
   // Tell the client what number they are.
	msg_xmt.send();
#ifdef DEBUG
cout << "Got client init for client " << client_no << "\n";
#endif
   client_no++;
}
      

void do_read_fault(int the_page, int the_client)
{
   Dsm_msg msg_xmt(SERVER, client_table[the_client].except_port, server_port);
	int the_owner;
	port_t service_port;

	Cport_allocate(&service_port);
	Dsm_msg ser_msg(SERVER, PORT_NULL, service_port);
   
#ifdef DEBUG
printf("Got to do_read_fault page = %d from client %d\n", the_page, the_client);
#endif

   if((the_page >= 0) && (the_page < DSM_PAGE_COUNT))
   {
	   msg_xmt.set_request(READ_FAULT);
		msg_xmt.set_page_no(the_page);
		msg_xmt.set_page(page_to_address(the_page));
		page_table[the_page].Q.enq(the_client);
		
      switch(page_table[the_page].state)
      {
         case INIT :   
			case READ :
				page_table[the_page].state = READ;
				msg_xmt.send();
	  		break;
	  
	  		case WRITE :
				page_table[the_page].state = READ;
				the_owner = page_table[the_page].owner;

				ser_msg.get_page(READ_DATA_REQUESTED, the_page,
									  client_table[the_owner].respond_port);

				page_table[the_page].Q.enq(the_owner);

#ifdef DEBUG
				cout << "Write page owned by client " << the_owner
	 			<< " with res port " << client_table[the_owner].respond_port <<
				" and exp port " << client_table[the_owner].except_port << "\n";
#endif

				page_copy(the_page, (vm_address_t *) ser_msg.get_page_ptr());
#ifdef DEBUG
				cout << "got page back from " << the_owner <<
				"with message " << msg_xmt.get_request() << "\n";

#endif
				msg_xmt.set_rport(client_table[the_client].except_port);
			   msg_xmt.send();
		   break;

	  		default :
				cerr << "do_read_fault() unk state\n";
				exit(-1);
     	}
		
   }
   else {
     cerr << "do_read_fault() page index out of bounds\n";
	  exit(-1);
   }

	Cport_deallocate(service_port);
}

void page_copy(int page_index, vm_address_t *source_page)
{
 vm_address_t *dest_page = page_to_address(page_index);
 int i;

  for(i = 0; i < PAGE_DATA_SIZE; i++)
     dest_page[i] = source_page[i];

	  //source_page[0] = 57;

	// dealocate the page form the message.
	Cvm_deallocate(*source_page, vm_page_size);
}



void do_write_fault(int the_page, int the_client)
{
  Dsm_msg msg_xmt(SERVER, client_table[the_client].except_port, server_port);
  int read_client, a_client, client_has_data = FALSE;
  intqueue *theQ;
  int old_owner, new_owner;

  port_t service_port;
  Cport_allocate(&service_port);
  Dsm_msg inval_msg(SERVER, PORT_NULL, service_port);


#ifdef DEBUG
cout << "do_write_fault called with client " <<
the_client << " page " << the_page << "\n"<< "the page state is "<<
page_table[the_page].state << "\n";
#endif

  if((the_page >= 0) && (the_page < DSM_PAGE_COUNT)) 
  {
     switch(page_table[the_page].state)
     {
			case INIT: 
				page_table[the_page].owner = the_client;
				page_table[the_page].state = WRITE;
				msg_xmt.send_page(WRITE_DATA_PROVIDED, the_page,
										page_to_address(the_page),
										client_table[the_client].except_port);
        	break;

			case WRITE: 
#ifdef DEBUG
		 		cout << "WRITE FAULT ON WRITE PAGE\n";
#endif
				old_owner = page_table[the_page].owner;
				new_owner = the_client;
				page_table[the_page].owner = new_owner;

				inval_msg.get_page(WRITE_DATA_REQUESTED, the_page,
									  client_table[old_owner].respond_port);
				
				page_copy(the_page, (vm_address_t *) inval_msg.get_page_ptr());

				msg_xmt.send_page(WRITE_DATA_PROVIDED, the_page,
										page_to_address(the_page),
										client_table[the_client].except_port);

       		break;

       	case READ :
				theQ = &page_table[the_page].Q;
				page_table[the_page].state = WRITE;
				page_table[the_page].owner = the_client;

				while(!theQ->empty())
				{
					a_client = theQ->deq();
#ifdef DEBUG
					cout << "a_client = " << a_client << "\n";
#endif
					if(a_client == the_client)
						client_has_data = TRUE;
					else
					{
#ifdef DEBUG
						cout << "Sending page invalid msg to client "
						<< a_client << "\n";
#endif
						inval_msg.wait_control(PAGE_INVALID,
							the_page, client_table[a_client].respond_port);
					}
				}

				if(client_has_data)
				{
#ifdef DEBUG
					cout << "write perm granted\n";
#endif
					msg_xmt.send_control(WRITE_PERM_GRANTED, the_page, 
												client_table[the_client].except_port);
				}
				else
				{
#ifdef DEBUG
					cout << "Write data provided\n";
#endif
					msg_xmt.send_page(WRITE_DATA_PROVIDED, the_page,
											page_to_address(the_page),
											client_table[the_client].except_port);
				}
	 		break;


			default:
				cerr << "do_write_fault unk state\n";
				exit(-1);
			break;
     }
  }
  else
  {
    cerr << "do_write_fault page_index out of bounds\n";
	 exit(-1);
  }

	Cport_deallocate(service_port);


}
