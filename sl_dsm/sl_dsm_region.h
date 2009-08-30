/*
	class		:	sl_dsm_region
	author		:	Peter Ogilvie
	date		:	6/9/92
	notes		:	This class addes coherency management to dsm_region so that
				a server is no longer required in the system.
*/

#ifndef SL_DSM_REGION
#define SL_DSM_REGION

#include <C_mach_interface.h>
#include <mach/cthreads.h>

#include "../dsm2/dsm_server/Dsm_msg.h"
#include <stream.h>
#include <String.h>
#include <intqueue.h>
//	#include <builtin.h>
#include <timer.h>

#define address_null  (vm_address_t) 0
#define page_int_size (vm_page_size/sizeof(int))   // Number of int in a page.

// called by sl_dsm_region constructor to start threads.
void init(unsigned int, port_t respond_port);
int sync_thread(unsigned client_count);

enum page_state { NO_ACCESS, READ, WRITE };

#define delta 0 			   // Minimum time which a page must remain on a client.
								   // This is a tunable parameter.
#define NUM_RESPONDERS  3	// number of responder threads for each client.


// types of requests (same as single server)
#define SYNC				  -1
#define CLIENT_INIT            0
#define EXCEPTION_INIT         1
#define RESPONDER_INIT         2
#define READ_FAULT             3
#define WRITE_FAULT            4
#define WRITE_DATA_PROVIDED    5
#define CLIENT_DONE            6
#define PAGE_INVALID           7
#define WRITE_PERM_GRANTED     8
#define TEST                   9
#define READ_DATA_REQUESTED    10
#define WRITE_DATA_REQUESTED   11
#define READ_DATA_PROVIDED     12
#define INVALID_DONE           13
#define CLEAN_UP               14
#define INIT_DONE              15
#define WAIT_INIT              16


const MAX_RETRIES = 3;		// number of times to try looking for a remote port.

// Structure used for record keeping for each page in the page table.
typedef struct {
	mutex_t		lock;		// Only one thread at a time may modify a page.
	timer		time_stamp;	// Records how long a page has been on owner.
	page_state	state;		
	unsigned	owner;		// Client number of the owner of this page.
	port_t		writer;		// Port of the single writer.
	intqueue	*readers;	// This of ports of readers of this page.
}ptable_entry;

class sl_dsm_region {

	private:
#ifndef _NeXT_CPP_CLASSVAR_BUG
      static vm_address_t vmem;
      // Class variable which points to client's locally allocated memory.
  
      static unsigned page_count;
      // Class variable contains the number of pages in the distributed shared
      // memory.  Pages are indexed from 0 to page_count - 1.

		static ptable_entry		*page_table;
		static port_t			*responder_ports;
//		static port_t			*exception_ports;
		static port_t			sync_port;
		static unsigned			self;
// just required for debugging
		static unsigned			number_of_clients;
#endif /* _NeXT_CPP_CLASSVAR_BUG */
		void init_ports(unsigned client_count);
		//	results	:	Memory for responder_ports table is allocated.
		//			The local responder port is allocated, filled into
		//			the table and checked in with the Mach network
		//			name server. Client_count is the number of clients
		//			in the system.
		//	assumes	:	Nothing.
		//	notes		:	If memory allocation, port allocation, or checking local
		//				names in fails the program will exit.

		void lookup_ports(unsigned client_count);
		//	results	:	Responder ports for the other clients are
		//			filled into their respective tables. Client_count is the
		//			number of clients in the system.
		//	assumes	:	Init_ports has been previously called.
		//	notes		:	Lookup_ports attempts to find ports of other clients
		//				if attempts fail.  The number of retries lookup_ports
		//				will attempt may be set by resetting the constant 
		//				MAX_RETRIES.

		int rport(port_t r);
		// returns :		Returns the client number of a given responder port.
		// notes  :  		This function is only used for debugging so the responder
		//			port number are transformed into more understandable and
		//			constant client numbers.
	
	public:
		sl_dsm_region(unsigned pcount, unsigned ccount, unsigned client_number);
		//	results	:	A server-less region of distributed shared memory is 
		//			created.  Where: pcount = number of virtual memory pages
		//			in the region, ccount = number of clients in the system,
		//			and client_number is the unique id for the calling client.
		//			Where 0 <= client_number < ccount
		//	assumes	:	Parameters are consistent on each client on which a region
		//			is created.  I.e. each should create a region with the
		//			same pcount and ccount and unique client_number where each
		//			client_number in the range of ccount is represented.
		// notes :		This and get_iptr() are the only methods which should
		//			be called directly by clients.


		sl_dsm_region();
		//	notes		:	This contructory is provided to allow the management
		//					threads (exc_thread, responder_thread) access to 
		//					sl_dsm_region methods and class variables
#ifndef _NeXT_CPP_CLASSVAR_BUG	
		port_t get_sync_port() { return sync_port; }
		// returns	:	the sync server port.  This server handles events in the
		//					system.
#else
		port_t get_sync_port();
		// returns	:	the sync server port.  This server handles events in the
		//					system.
#endif /* _NeXT_CPP_CLASSVAR_BUG */
		int* get_iptr();
		// 
      //	returns  :	An integer pointer to vmem so that the client may change
      // 	         the contens of the memory pointed to by vmem.
      // assumes  :	Nothing.
      // notes    :	Although the contents which vmem points to may be changed
      //					as a result of this call the address which vmem points to
      //					is protected.

		boolean_t check_address(vm_address_t addr);
		//	returns	:	TRUE if addr is within the managed address space.  FALSE
		//					otherwise.
                    
      unsigned get_page_index(vm_address_t addr);
		//	returns  :	The page index in which addr resides.
		//	assumes  :	addr is within the bounds of the managed address space.
                                                                            
      vm_address_t get_page_address(unsigned page_index);
		//	returns  :	The starting address of the page specified by page index.
      // assumes  :	0 <= page_index < page_count                             
                                                
      void copy_page(int page_index, vm_address_t page);
		//	results  :	Physical copy of page is made to managed memory specified
		//					by page_index. 
		//	assumes  :	0 <= page_index < page_count                             
		//					page is an area of memory which is the size of a machine
		//					page.                                                   
                         
      void temp_copy(vm_address_t *temp, unsigned page_index);
		//	results	:	Physical copy of page specified by page_index is made
		//					to memory pointed to by temp which will usually reside
		//					on a stack.
		//	notes		:	Temp_copies are required to avoid some race conditions.
                                                              
      void protect_page(int page_index, vm_prot_t protection);
		//	results  : Page specified by page_index is given access protection
		//					specified by protection.                               
		//	assumes  : 0 <= page_index < page_count
		//	notes    : protection is given by the following from <vm/vm_prot.h>
		//						VM_PROT_READ   : read only permission                  
		//						VM_PROT_ALL	   : read and write permission             
		//						VM_PROT_NONE   : No access allowed       

		
		void read_fault(unsigned page_index, unsigned client, port_t who);
		//	results	:	Page specified by page_index is mapped into memory area
		//					pointed to by class variable vmem. This page will contain
		//					data reflecting the most recent write in the system.
		//					Access protection is set to read only for this page.
		//	assumes	:	Page_index is within the managed area and client is a
		//					valid client id.
		//	notes		:	Read_fault() is called by both the exception and responder
		//					threads.
		

		void write_fault(unsigned page_index, unsigned client, port_t who);
		//	results	:	Page specified by page_index is mapped into memory area
		//					pointed to by class variable vmem. This page will contain
		//					data reflecting the most recent write in the system.
		//					Access protection is set to read/write for this page.
		//	assumes	:	Page_index is within the managed area and client is a
		//					valid client id.
		//	notes		:	Write_fault() is called by both the exception and
		//					responder threads.


		void invalidate(unsigned page_index, port_t who);
		// results	:	Page specified by page_index has protection set to no
		//					access. A confirmation message is sent to the client
		//					listening on port who.
		//	assumes	: 	Page_index is within the managed area and who is a
		//					currently valid port number
		// notes		:	This method is called by the responder thread in response
		//					to invalidation requests form managing clients.


		void send_read_page(unsigned page_index, port_t who);
		// results	:	Page specified by page_index has protection changed to
		//					read only and data in the page is sent to managing client
		//					receiving on port who.
		//	assumes	:	Page_index is within the managed area and who is a 
		//					currently valid port number.
		// notes		:	This method is called by the responder thread in response
		//					to SEND_READ_PAGE requests.


		void send_write_page(unsigned page_index, port_t who);
		// results	:	Page specified by page_index has protection changed to
		//					no access and data in the page is sent to
		//					managing client receiving on port who.
		//	assumes	:	Page_index is within the managed area and who is a 
		//					currently valid port number.
		// notes		:	This method is called by the responder thread in response
		//					to SEND_WRITE_PAGE requests.

#ifndef _NeXT_CPP_CLASSVAR_BUG
		unsigned Self() { return self; }
		// returns	: Self which is the client id of this client.
#else
		unsigned Self();
		// returns	: Self which is the client id of this client.
#endif /* _NeXT_CPP_CLASSVAR_BUG */
		void get_port(unsigned who);
		//	results	:	Responder port for client number who is found by asking
		//					net name server (nmserve). 
		//	assumes	:	Who is a valid client number.
		// notes		: 	Will try to find port three times and exit on failure.

};


#endif

