// file     : dsm.cc
// author   : Peter Ogilvie
// date     : 3/10/92
// notes    : This file contains the threads which make the system work.
//				  The function init() is called by the dsm_region constructor.
#include <mach/cthreads.h>
#include <mach/message.h>
#include <mach/mach.h>
#include <stdio.h>
#include <mach/exception.h>
#include "C_mach_interface.h"
#include "dsm_region.h"
#include "dsm_server/Dsm_msg.h"
#include "dsm_server/dsm_server.h"
#include <stream.h>
#include <builtin.h>

void handle_exception(vm_address_t addr);
any_t responder_thread(port_t server_port);


typedef struct {
    port_t old_exc_port;
    port_t clear_port;
    port_t exc_port;
} ports_t;

port_t server_port, handle_exc_port;

volatile boolean_t  pass_on = FALSE;
mutex_t             printing, alock;
condition_t				done;
int client_no;
ports_t ports;

extern "C" {
	boolean_t exc_server(msg_header_t*, msg_header_t*);
	kern_return_t catch_exception_raise(port_t,port_t, port_t, int, int, int);
	void mach_NeXT_exception(char *, int, int, int);
}

/* Listen on the exception port. */
any_t exc_thread(ports_t *port_p)
{
    kern_return_t   r;
    char           *msg_data[2][64];
    msg_header_t   *imsg = (msg_header_t *)msg_data[0],
                   *omsg = (msg_header_t *)msg_data[1];

		Cport_allocate(&handle_exc_port);
		Dsm_msg m(client_no, server_port, handle_exc_port);
		m.send_control(EXCEPTION_INIT);



    /* Wait for exceptions. */
    while (1) {
        imsg->msg_size = 64;
        imsg->msg_local_port = port_p->exc_port;
        Cmsg_receive(imsg);

            /* Give the message to the Mach exception server. */
            if (exc_server(imsg, omsg)) {
                /* Send the reply message that exc_serv gave us. */
		Cmsg_send(omsg);
            }
            else { /* exc_server refused to handle imsg. */
                mutex_lock(printing);
                printf("exc_server didn't like the message\n");
                mutex_unlock(printing);
                exit(2);
            }
        

        /* Pass the message to old exception handler, if necessary. */
        if (pass_on == TRUE) {    
            imsg->msg_remote_port = port_p->old_exc_port;
            imsg->msg_local_port = port_p->clear_port;
            Cmsg_send(imsg);
        }
    }
}

int write_flag;

/* 
 * catch_exception_raise() is called by exc_server().  The only
 * exception it can handle is EXC_BAD_ACCESS.  
 * This function is called by the Mach exc_server().
 */
kern_return_t catch_exception_raise(port_t exception_port, 
    port_t thread, port_t task, int exception, int code, int subcode)
{
  int prot;
  int page_index;
  vm_address_t bad_addr = (vm_address_t) subcode;
  dsm_region d;
  Dsm_msg m(client_no, server_port, handle_exc_port);

    if (exception == EXC_BAD_ACCESS && d.check_address(bad_addr)) {
        /* Handle the exception so that the program can continue. */
      

		page_index = d.get_page_index(bad_addr);
      if(write_flag) {
#ifdef DEBUG
			cout << "write fault on client " << client_no << " on page " <<
			page_index << "\n";
#endif
			m.get_page(WRITE_FAULT, page_index);
			d.protect_page(page_index, VM_PROT_ALL);
			if(m.get_request() == WRITE_PERM_GRANTED) {
#ifdef DEBUG
				cout << "client: " << client_no <<
				" Got write perm for a page I have\n";
#endif
         }
			else if(m.get_request() == WRITE_DATA_PROVIDED) {
#ifdef DEBUG
				cout << "Got a write page\n";
#endif
				d.copy_page(page_index, (vm_address_t) m.get_page_ptr());
			}
		 	else
				cout << "Got a bad request from server on write fault\n";
			
      }
      else {
#ifdef DEBUG
			cout << "read fault on client " << client_no << " on page " <<
			page_index << "\n";
#endif
			m.get_page(READ_FAULT, page_index);
		  	d.protect_page(page_index, VM_PROT_ALL);
		  	d.copy_page(page_index, (vm_address_t) m.get_page_ptr());
		  	d.protect_page(page_index, VM_PROT_READ);
      }
#ifdef DEBUG
      printf("bad address = 0x%x\n", subcode);
#endif
      
      return KERN_SUCCESS;
    }
    else { /* Pass the exception on to the old port. */
        pass_on = TRUE;
        mach_NeXT_exception("Forwarding exception", exception, 
            code, subcode);
        return KERN_FAILURE;  /* Couldn't handle this exception. */
    }
}

// thread responder_thread
// This thread receives requests from the server.
any_t responder_thread(port_t server_port)
{
	port_t respond_port;
	dsm_region region;
   unsigned page_index;
	vm_prot_t prot;

	Cport_allocate(&respond_port);

	Dsm_msg m(client_no, server_port, respond_port);

	m.send_control(RESPONDER_INIT);
	condition_signal(done);

#ifdef DEBUG
cout << "past the condition signal\n";
#endif

	while(1)
	{
		m.receive();
		m.set_rport(m.get_rport());

		page_index = m.get_page_no();

		switch(m.get_request())
		{
			case PAGE_INVALID:
#ifdef DEBUG
				cout << "Responder thread of client_no " << client_no
				<< "  PAGE_INVALID on page " << page_index << "\n";
#endif
				m.send_control(INVALID_DONE);
				prot = VM_PROT_NONE;
			break;

			case READ_DATA_REQUESTED:
#ifdef DEBUG
				cout << "Responder thread of client_no " << client_no
				<< " READ_DATA_REQUESTED on page " << page_index << "\n";
#endif
				prot = VM_PROT_READ;
				m.send_page(READ_DATA_PROVIDED, page_index,
				(vm_address_t *) region.get_page_address(page_index));
			break;

			case WRITE_DATA_REQUESTED:
#ifdef DEBUG
				cout << "Responder thread of client_no " << client_no
				<< " WRITE_DATA_REQUESTED on page " << page_index << "\n";
#endif
				prot = VM_PROT_NONE;
				m.send_page(WRITE_DATA_PROVIDED, page_index,
				(vm_address_t *) region.get_page_address(page_index));

			break;
			
			default:  
				cout << "Responder thread got bad request\n";
				exit(-1);
		}
		region.protect_page(page_index, prot); // THIS MUST CHANGE!!!
	}

}


init(unsigned number_of_clients)
{
    int					i;
    int          	   *iptr, j;
	 port_t				a_port;


    Cport_allocate(&a_port);
    
    printing = mutex_alloc();
    alock = mutex_alloc();
	 done = condition_alloc();

// get the server and handle exception ports.

	Cnetname_lookup("dsm_server", &server_port);
	Dsm_msg client_no_msg(number_of_clients, server_port, a_port);
	client_no_msg.set_request(CLIENT_INIT);
	client_no_msg.set_page_no(0);
	client_no_msg.set_page(PAGE_NULL);
	client_no_msg.rpc();

	client_no = client_no_msg.get_client_no();


	Cport_allocate(&handle_exc_port);


    /* Save the old exception port for this task. */
    Ctask_get_exception_port(&(ports.old_exc_port));


    /* Create a new exception port for this task. */
    Cport_allocate(&(ports.exc_port));

    Ctask_set_exception_port((ports.exc_port));

    /* Fork the thread that listens to the exception port. */
    cthread_detach(cthread_fork((cthread_fn_t)exc_thread, (any_t)&ports));
    cthread_detach(cthread_fork((cthread_fn_t)responder_thread,
	                (any_t)server_port));

	 ports.clear_port = Cthread_self();

	 condition_wait(done, alock);
   

}

