//	file		: sl_dsm.cc
//	author	: Peter Ogilvie
// date		: 6/9/92
// notes		: This file contains the threads which make the system work.
//				  It contains a function init() which is called by the
//				  sl_dsm_region constructor.  This function forks off the threads
//				  (exception, responder(s), and sync) with the appopriate
//				  parameters.
#include <mach/cthreads.h>
#include <mach/message.h>
#include <mach/mach.h>
#include <stdio.h>
#include <mach/exception.h>
#include "C_mach_interface.h"
#include "sl_dsm_region.h"
#include "../dsm2/dsm_server/Dsm_msg.h"
#include <stream.h>
#include <String.h>
// #include <builtin.h>
#include <timer.h>

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
ports_t ports;

extern "C" { // required for linking.
	boolean_t exc_server(msg_header_t*, msg_header_t*);
   kern_return_t catch_exception_raise(port_t,port_t, port_t, int, int, int);
	void mach_NeXT_exception(char *, int, int, int);
}

// thread exc_thread
//  This thread is passed port_p on startup which contains the 
//  the old exception port, the clear port, and the new eception port.
//  The old exception port is used when the exception can't be cleared.  This
//  might happen if a protection violation occured outside the dsm area or was
//  an error not managed by the system such as a floating point error.  The
//  clear port is used to signify the exception has been cleared and the kernel
//  should restart the faulting thread.  The new exception thread is where the
//  kernel sends exception messages.
any_t exc_thread(ports_t *port_p)
{
    kern_return_t   r;
    char           *msg_data[2][64];
    msg_header_t   *imsg = (msg_header_t *)msg_data[0],
                   *omsg = (msg_header_t *)msg_data[1];


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

int write_flag;  // This flag is set on writes so writes can be distintished
					  // from reads see discussion of the mach exception bug in 
					  // thesis.

/* 
 * catch_exception_raise() is called by exc_server().  The only
 * exception it can handle is EXC_BAD_ACCESS.  (Memory exception)
 * This function is called by the Mach exc_server().
 */
kern_return_t catch_exception_raise(port_t exception_port, 
    port_t thread, port_t task, int exception, int code, int subcode)
{
  int prot;
  int page_index;
  vm_address_t bad_addr = (vm_address_t) subcode;
  sl_dsm_region d;
  int client_no = d.Self();

    if (exception == EXC_BAD_ACCESS && d.check_address(bad_addr)) {
        /* Handle the exception so that the program can continue. */
      

		page_index = d.get_page_index(bad_addr);
      if(write_flag) {
#ifdef DEBUG
			cout << "write fault on client " << client_no << " on page " <<
			page_index << "\n";
#endif
			d.write_fault(page_index, client_no, PORT_NULL);	
      }
      else {
#ifdef DEBUG
			cout << "read fault on client " << client_no << " on page " <<
			page_index << "\n";
#endif
			d.read_fault(page_index, client_no, PORT_NULL);
      }
#ifdef DEBUG
      printf("bad address = 0x%x\n", subcode);
#endif
      
      return KERN_SUCCESS;
    }
    else { /* Pass the exception on to the old port. */
			cout << "bad address error on client " << client_no << "\n";
        pass_on = TRUE;
        mach_NeXT_exception("Forwarding exception", exception, 
            code, subcode);
        return KERN_FAILURE;  /* Couldn't handle this exception. */
    }
}

// thread responder_thread
// This thread receives requests from other clients in the system.
any_t responder_thread(port_t respond_port)
{
	sl_dsm_region region;
   unsigned page_index;
	port_t who;
	int client_no = region.Self();



	Dsm_msg m(client_no, PORT_NULL, respond_port);

	condition_signal(done);

#ifdef DEBUG
cout << "responder thread " << client_no << " past the condition signal\n";
#endif

	while(1)
	{
		m.receive();
		Cthread_yield();	// Defeat handoff scheduling.
		who = m.get_rport();
		page_index = m.get_page_no();

		switch(m.get_request())
		{
// responder thread acting as client
			case PAGE_INVALID:
#ifdef DEBUG
				cout << "Responder thread of client_no " << client_no
				<< " " << Cthread_self() << "  PAGE_INVALID on page " <<
				page_index << " from " << m.get_client_no() <<"\n";
#endif
				region.invalidate(page_index, who);
			break;

			case READ_DATA_REQUESTED:
#ifdef DEBUG
				cout << "Responder thread of client_no " << client_no
				<< " READ_DATA_REQUESTED on page " << page_index <<
				" from " << m.get_client_no() <<"\n";
#endif
				region.send_read_page(page_index, who);
			break;

			case WRITE_DATA_REQUESTED:
#ifdef DEBUG
				cout << "Responder thread of client_no " << client_no
				<< " WRITE_DATA_REQUESTED on page " << page_index <<
				" from " << m.get_client_no() <<"\n";
#endif
				region.send_write_page(page_index, who);
			break;

// responder thread acting as server
			case READ_FAULT:
#ifdef DEBUG
				cout << "Responder thread of client_no " << client_no <<
				" READ_FAULT on page " << page_index <<
	 			" form " << m.get_client_no() << "\n";
#endif
				region.read_fault(page_index, m.get_client_no(), who);
			break;

			case WRITE_FAULT:
#ifdef DEBUG
				cout << "Responder thread of client_no " << client_no <<
				" WRITE_FAULT on page " << page_index <<
				"form " << m.get_client_no() <<	"\n";
#endif
				region.write_fault(page_index, m.get_client_no(), who);
			break;	

			default:  
				cerr << "client " << client_no <<
				" Responder thread got bad request " << m.get_request() <<
	 			" from client "<< m.get_client_no() << " on page " <<
				page_index << "\n";
				exit(-1);
		}
	}

}


void init(unsigned client_number, port_t respond_port)
{
    int					i;
    int          	   *iptr, j;
	 port_t				a_port;


    Cport_allocate(&a_port);
    
    printing = mutex_alloc();
    alock = mutex_alloc();
	 done = condition_alloc();



	Cport_allocate(&handle_exc_port);


    /* Save the old exception port for this task. */
    Ctask_get_exception_port(&(ports.old_exc_port));


    /* Create a new exception port for this task. */
    Cport_allocate(&(ports.exc_port));

    Ctask_set_exception_port((ports.exc_port));

    /* Fork the thread that listens to the exception port. */
    cthread_detach(cthread_fork((cthread_fn_t)exc_thread, (any_t)&ports));


	 /* Fork the responder thread(s) */
	 for(i = 0; i < NUM_RESPONDERS; i++)
	    cthread_detach(cthread_fork((cthread_fn_t)responder_thread,
	                (any_t)respond_port));

	 ports.clear_port = Cthread_self();

// prevents race of constructor returning before responder_thread completes
// initailization.
	 condition_wait(done, alock);
   

}

// thread sync_thread
// This thread implements builtin barrier events (such as INIT_DONE)
// There is only one such thread in the system and resides on client 0.
// It is also were some of the performace timing is done.
sync_thread(unsigned client_count)
{
	intqueue done_Q;
	intqueue wait_Q;
	port_t sync_port, done_client, client0;
	boolean_t clean_up_flag = FALSE, init_count = 0;
	unsigned the_client, Q_len;

cout << "SYNC_THREAD client_count = " << client_count << "\n";
	String sync = "sync";

	Cport_allocate(&sync_port);
	Cnetname_check_in((char *) sync, sync_port);

	Dsm_msg m(SYNC, PORT_NULL, sync_port);
	Dsm_msg wait_msg(SYNC, PORT_NULL, sync_port);
	Dsm_msg clean_up_msg(SYNC, PORT_NULL, sync_port);
	Dsm_msg done_msg(SYNC, PORT_NULL, sync_port);

	timer calc_timer;

	while(1)
	{
		m.receive();
		the_client = m.get_client_no();

		switch(m.get_request())
		{
			case INIT_DONE :
cout << "Got INIT_DONE msg from " << the_client << "\n";
				init_count++;
		  	break;

			case WAIT_INIT :
cout << "Got WAIT_INIT msg from " << the_client << "\n";
				
				wait_Q.enq(m.get_rport());
				if(init_count == m.get_page_no())
				{
					calc_timer.set();  // start calc timer when init done.
					while(!wait_Q.empty())
						wait_msg.send_control(INIT_DONE, wait_Q.deq());
				}
			break;

			case CLIENT_DONE :
			cerr << "Got CLIENT_DONE msg from " << the_client << "\n";
				done_Q.enq(m.get_rport());
				Q_len = done_Q.length();
				if(the_client == 0)
					client0 = m.get_rport();
            if(Q_len == client_count -1 || client_count == 1)
					cout << "Calc time = " << calc_timer.get_elapsed_time()
					<< " msec\n";

				if(Q_len == client_count)
				{
					while(!done_Q.empty())
					{
						done_client = done_Q.deq();
						if(done_client != client0)
							done_msg.send_control(CLIENT_DONE, done_client);
					 }
					 done_msg.send_control(CLIENT_DONE, client0);
				}	
				else if(Q_len == (client_count -1) && clean_up_flag)
					clean_up_msg.send();

			break;

			case CLEAN_UP :
				cerr << "Got clean up message from " << the_client << "\n";
				cout << "CLEAN_UP remote port is " << m.get_rport() << "\n";
				if(done_Q.length() == (client_count -1))
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
				cerr << "Sync server got bad request\n";
				exit(-1);
			break;
		  }
		}
}

