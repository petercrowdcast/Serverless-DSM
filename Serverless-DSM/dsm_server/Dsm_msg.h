// class		:	Dsm_msg
// author	:	Peter Ogilvie
// date		:	4/13/92
// notes		:	This class provides communication for both the single server
//             and server-less systems. It provides methods to assist in 
//             sending/receiving both control and page information.

#ifndef DSM_MSG
#define DSM_MSG
#include <mach/message.h>
#include <C_mach_interface.h>

#define PAGE_NULL					(vm_address_t *)0
#define PAGE_DATA_SIZE			(vm_page_size/sizeof(vm_address_t))
#define INVALID				   -1	

// Structure for a msg (may or may not contain page data)
typedef struct {
  msg_header_t  h;
  msg_type_t    client_data;
  int           client_no;
  int           page_no;
  msg_type_t    page_data;
  vm_address_t  *page; 
}dsm_msg;


class Dsm_msg {

	private:
		dsm_msg m; 
		port_t remote_port;
		port_t local_port;
		int the_client_no;

		vm_address_t dummy_page;
		
	public:
		Dsm_msg(int whoami, port_t remote_port, port_t local_port);
		// results	: A Dsm_msg is constructed. Whoami is the client number
		//            of the creator of the message, remote_port is the port
		//            where the message should be sent and local_port is a
	   //            local port where a reply to this message should go.  The
		//            remote client will get send rights to this port.  If
		//            there will be not reply to the message (no confirmation)
		//            then PORT_NULL may be used for local port.
		// assumes  : whoami is a valid client number, remote_port and local_port
		//            are valid ports.

/*
These are the basic action methods of Dsm_msg.  Each assumes that all 
necessary data has been set.  They are rarely used directly but are instead
called by higher level methods.
*/
		void send();
		void receive();
		void rpc();

/* get_x(), set_x()
These are the Dsm_msg access methods.  Their use should be self explanatory.
*/

		int get_msg_size()			{ return m.page_data.msg_type_number;		}
		int get_request()				{ return m.h.msg_id;								}
		int get_page_no()				{ return m.page_no;								}
		int get_client_no()			{ return m.client_no;							}
		vm_address_t* get_page_ptr()	{ return m.page;								}
		port_t get_rport()			{ return m.h.msg_remote_port;					}

		void set_request(int request)
											{ m.h.msg_id = request;							}
		void set_page_no(int page_no)
											{ m.page_no = page_no;							}
		void set_client_no(int client_no)
											{ m.client_no = client_no;					}

		void set_page(vm_address_t *page);

		void set_rport(port_t rport)
											{ remote_port = rport;				}

/* wait_control()
This when a client (or server) must wait for confirmation that a remote action
has taken place before it can proceed.  For example:  wait_control would
be used to send page invalidation requests. Valid requests are given in 
dsm_server.h and sl_dsm_region.h
*/
		void wait_control(int request, int the_page, port_t who);
		void wait_control(int request);

/* send_control()
Is used to send control information were conformation that the receiver
received the message and did the action is not required.  Send control is
typically used for confirmations such as invalidation done.
*/
		void send_control(int request, int the_page, port_t who);
		void send_control(int request, port_t who);
		void send_control(int request);

/* send_page()
Used to send pages in response to a get_page(). Send page will not block
the sender
*/

 		void send_page(int request, int the_page, vm_address_t *p, port_t who);
 		void send_page(int request, int the_page, vm_address_t *p);

/* get_page()
Used to request page data.  The calling client will block until the data
is received.
*/
		void get_page(int request, int the_page, port_t who);
		void get_page(int request, int the_page);
};
inline void Dsm_msg::send()
{
	m.h.msg_remote_port = remote_port;
	m.client_no = the_client_no;
	Cmsg_send(&m.h);						
}
inline void Dsm_msg::receive()
{
	m.h.msg_local_port = local_port;
	Cmsg_receive(&m.h);				
}
inline void Dsm_msg::rpc()					
{
	m.h.msg_remote_port = remote_port;
	m.h.msg_local_port = local_port;
	m.client_no = the_client_no;
	Cmsg_rpc(&m.h, sizeof(dsm_msg));
}

inline void Dsm_msg::wait_control(int request, int the_page, port_t who)
{
	m.h.msg_id = request;
	remote_port = who;
	m.page_no = the_page;
	set_page(PAGE_NULL);
	rpc();
}

inline void Dsm_msg::wait_control(int request)
{
	m.h.msg_id = request;
	m.page_no = 0;
   set_page(PAGE_NULL);
	rpc();
}

inline void Dsm_msg:: send_control(int request, int the_page, port_t who)
{
	m.h.msg_id = request;
	remote_port = who;
	m.page_no = the_page;
	set_page(PAGE_NULL);
	send();
}
inline void Dsm_msg::send_control(int request, port_t who)
{
	m.h.msg_id = request;
	remote_port = who;
	m.page_no = 0;
	set_page(PAGE_NULL);
	send();
}
inline void Dsm_msg::send_control(int request)
{
	m.h.msg_id = request;
	m.page_no = 0;
	set_page(PAGE_NULL);
	send();
}

inline void Dsm_msg::send_page(int request, int the_page, vm_address_t *p,
										port_t who)
{
	m.h.msg_id = request;
	remote_port = who;
	m.page_no = the_page;
	set_page(p);
	send();
}
inline void Dsm_msg::send_page(int request, int the_page, vm_address_t *p)
{
	m.h.msg_id = request;
	m.page_no = the_page;
	set_page(p);
	send();
}

inline void Dsm_msg::get_page(int request,int the_page, port_t who)
{
	m.h.msg_id = request;
	remote_port = who;
	m.page_no = the_page;
	set_page(PAGE_NULL);
	rpc();
}

inline void Dsm_msg::get_page(int request, int the_page)
{
	m.h.msg_id = request;
	m.page_no = the_page;
	set_page(PAGE_NULL);
	rpc();
}

#endif
