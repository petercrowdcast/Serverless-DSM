/*
dsm_server.h
	author 	:	Peter Ogilvie
	date	 	: 	3/1/92
	modified	: 	5/13/92 Changed MAX_CLIENTS for variable no of clients
*/

#ifndef DSM_SERVER
#define DSM_SERVER

#include <stdio.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/netname.h>
#define c_plusplus
#include <intqueue.h>
#include <C_mach_interface.h>


#define MAX_CLIENTS         8
#define DSM_PAGE_COUNT      390 // Big enough to 512 X 512 + a little extra
#define SERVER              0

// types of requests
#define CLIENT_INIT				0
#define EXCEPTION_INIT			1    
#define RESPONDER_INIT			2
#define READ_FAULT				3
#define WRITE_FAULT				4
#define WRITE_DATA_PROVIDED	5
#define CLIENT_DONE				6
#define PAGE_INVALID				7
#define WRITE_PERM_GRANTED		8
#define TEST						9
#define READ_DATA_REQUESTED	10
#define WRITE_DATA_REQUESTED	11
#define READ_DATA_PROVIDED		12
#define INVALID_DONE				13
#define CLEAN_UP					14
#define INIT_DONE					15
#define WAIT_INIT					16
	

// page states
#define INIT                0
#define READ                1
#define WRITE               2
#define READ_WAIT           3
#define WRITE_WAIT          4


// Structure for the client table
typedef struct {
  port_t except_port;
  port_t respond_port;
}client_table_entry;


// Structure for page table
typedef struct {
  int       state;
  int        owner; 
  intqueue      Q;
}page_table_entry;

// dsm_server functions

port_t dsm_init();
void do_client_init(port_t);
void do_read_fault(int the_page, int the_client);
void do_write_fault(int the_page, int the_client);
vm_address_t* page_to_address(int page);
void page_copy(int page_index, vm_address_t *source_page);


#endif

