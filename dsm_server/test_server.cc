#include "Dsm_msg.h"
#include "dsm_server.h"
#include <stream.h>
#include "../dsm_region.h"


int send_init(Dsm_msg& M)
{
	M.set_request(CLIENT_INIT);
	M.set_page(PAGE_NULL);

	M.rpc();

	return M.get_client_no();
}


void send_excep_init(Dsm_msg& M)
{
	M.set_request(EXCEPTION_INIT);
	M.set_page(PAGE_NULL);

	M.send();
}
void send_respond_init(Dsm_msg& M)
{
	M.set_request(RESPONDER_INIT);
	M.set_page(PAGE_NULL);

	M.send();
}

void print_page(vm_address_t* page)
{
	//for(int i = 0; i < page_int_size; i++)
		cout << page[0] << "..." << page[2047] << "\n";
}

void send_read_fault(Dsm_msg& M, dsm_region& p, unsigned page_index)
{
	M.set_request(READ_FAULT);
	M.set_page_no(page_index);

	M.rpc();

	print_page(M.m.page);

	cout.flush();

	p.copy_page(page_index, (vm_address_t ) M.get_page());

}

void send_write_fault(Dsm_msg& M, dsm_region& p, unsigned page_index)
{
   M.set_request(WRITE_FAULT);
   M.set_page_no(page_index);

   M.rpc();
           
   cout.flush();        
                
   cout << "the request no I got is " << M.get_request() << "\n"; 
}           
main()
{
	port_t server_port, local_port;
	dsm_region a_page(1);

	Cnetname_lookup("dsm_server", &server_port);

	Cport_allocate(&local_port);

	Dsm_msg m(server_port, local_port);
	Dsm_msg m2(server_port, local_port);

	cout << "sent init client no " << send_init(m) << "\n";
	cout << "sent init client no " << send_init(m2) << "\n";

	send_excep_init(m);
	send_excep_init(m2);

	cout << "sent send_excepinit to sever\n";

	send_respond_init(m);
	send_respond_init(m2);

	cout << "sent respond init to server\n";

	a_page.protect_page(0, VM_PROT_ALL);

	send_read_fault(m, a_page, 0);
	send_read_fault(m2, a_page, 0);

 cout << "sent a read_fault\n";
	send_write_fault(m, a_page, 0);

cout << "sent a write fault\n";
}

