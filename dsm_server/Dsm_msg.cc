// file :  Dsm_msg.cc
// author	: Peter Ogilvie
// notes		: This file contains the some of the implementation for class
//            Dsm_msg.  Since most of the methods are so short they are inlined
//            and their implementations are in the header file.

#include "Dsm_msg.h"

Dsm_msg::Dsm_msg(int whoami,port_t r_port, port_t l_port)
{
	// local copies of local and remote ports needed because Mach messes with
	// them after message passing.

	remote_port = r_port;
	local_port = l_port;
	the_client_no = whoami;
	m.h.msg_remote_port								= remote_port;
  	m.h.msg_local_port								= local_port;
  	m.h.msg_id											= INVALID; 
  	m.h.msg_size										= sizeof(dsm_msg);
  	m.h.msg_type										= MSG_TYPE_NORMAL;
  	m.h.msg_simple										= FALSE;

  	m.client_data.msg_type_name					= MSG_TYPE_INTEGER_32;
  	m.client_data.msg_type_size					= 32;  // bits
  	m.client_data.msg_type_number					= 2;   
  	m.client_data.msg_type_inline					= TRUE; 
  	m.client_data.msg_type_longform				= FALSE; 
  	m.client_data.msg_type_deallocate			= FALSE; 
  	m.client_no											= INVALID;
  	m.page_no											= INVALID;

  	m.page_data.msg_type_name						= MSG_TYPE_INTEGER_32;
  	m.page_data.msg_type_size						= 32;  // bits
  	m.page_data.msg_type_number					= 1; 
  	m.page_data.msg_type_inline					= FALSE; 
  	m.page_data.msg_type_longform					= FALSE; 
  	m.page_data.msg_type_deallocate				= FALSE; 
  	m.page												= PAGE_NULL;
}

void Dsm_msg::set_page(vm_address_t *page)
{
		if(page == PAGE_NULL)
		{
				m.page_data.msg_type_number = 1;
				m.page = &dummy_page;
		}
		else
		{
				m.page_data.msg_type_number = PAGE_DATA_SIZE;
				m.page = page;
		}
		 
}
