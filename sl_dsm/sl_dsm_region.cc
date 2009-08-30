/*
	file		: sl_dsm_region
	author	: Peter Ogilvie
	date		: 6/9/92
	notes		: Implementation of server-less distributed shared memory.
*/

#include "sl_dsm_region.h"

#ifdef _NeXT_CPP_CLASSVAR_BUG
// This is a know NeXT C++ bug,  link errors result when class variables
// are used. When this compiler bug is fixed this code should be removed 
// the the corresponding code in class sl_dsm_region should be reactivated 

static vm_address_t vmem;
static unsigned page_count;
static ptable_entry		*page_table;
static port_t			*responder_ports;
// just required for debugging
		static unsigned			number_of_clients;

//		static port_t			*exception_ports;

// the below should be static but is declared extern for use in header  

static port_t			sync_port;	
static unsigned		self;

#endif /* _NeXT_CPP_CLASSVAR_BUG */



void debug_page_io(char *debug_msg, Dsm_msg& m, int who)
{
#ifdef DEBUG

	float *fptr = (float *) m.get_page_ptr();

	cout << debug_msg <<
	" msg_request = " << m.get_request() <<
	" remote client = " << who <<
	" page no. = " << m.get_page_no() <<
	" msg_value = " << fptr[1] << " " << fptr[1024] << "\n";

#endif
}

#ifdef _NeXT_CPP_CLASSVAR_BUG
unsigned sl_dsm_region::Self() { return self; }
port_t sl_dsm_region::get_sync_port() { return sync_port; }
#endif /* _NeXT_CPP_CLASSVAR_BUG */

int sl_dsm_region::rport(port_t r)
{
	for(int i = 0; i < number_of_clients; i++)
		if(responder_ports[i] == r)
			return i;

	return -1;
}

static ptable_entry *page_table = NULL;

// function hash()
//	returns	: Client number which owns the page page_no.
// notes	: This function is used to distribute ownershipe of pages 
//		  through out the system.
static inline unsigned hash(unsigned page_no, unsigned client_count)
{
		return page_no % client_count;
}

sl_dsm_region::sl_dsm_region()
{
// Empty constructor so the management threads may have access to 
// sl_dsm_region methods.
}
sl_dsm_region::sl_dsm_region(unsigned pcount, unsigned ccount,
								unsigned client_number)
{

	if(!vmem) // Protect agains clinet trying to create mult regions.
	{
		self = client_number;
		init_ports(ccount);
		number_of_clients = ccount;
//		start_timer();
		Cvm_allocate(&vmem, vm_page_size * pcount);
		Cvm_protect(vmem, vm_page_size * pcount, VM_PROT_NONE);
		page_count = pcount;
		page_table = new ptable_entry[pcount];

		if(!page_table) {
			cerr << "ERROR : allocation of memory failed during\
			construction of sl_dsm_region\n";
			exit(-1);
		}

		ptable_entry	*p = page_table;

		for(int i = 0; i < page_count; i++, p++)
		{
			p->lock = mutex_alloc();
			p->owner = hash(i, ccount);

			if(p->owner == self)
			{  // This client manages this page.
#ifdef DEBUG
				cout << "client " << self << " owns page " << i << "\n";
#endif
				p->time_stamp.set();
				protect_page(i, VM_PROT_ALL);
				p->state = WRITE;
				p->readers = new intqueue();
			}
			else
			{
				p->state = NO_ACCESS;
				p->readers = NULL;
			}
			p->writer = PORT_NULL;
		}


		lookup_ports(ccount);
		init(ccount, responder_ports[self]);
	}
}

void sl_dsm_region::init_ports(unsigned client_count)
{

	if(self == 0)  // Only one sync thread per system resides on client 0.
		cthread_detach(cthread_fork((cthread_fn_t) 
							sync_thread, (any_t) client_count));
	responder_ports = new port_t[client_count];

	if(!(responder_ports))
	{
		cerr << "ERROR: Memory allocation for port array failed\n";
		exit(-1);
	}

	sync_port = PORT_NULL;

	for(int i = 0; i < client_count; i++)
	{
		responder_ports[i] = PORT_NULL;
	}


// Allocate the local responder port

	Cport_allocate(&responder_ports[self]);

// Check the port in with the Mach netname server.
	String number = dec(self);
	String res_port_name = "res_port" + number;

	Cnetname_check_in((char *) res_port_name, responder_ports[self]);
}

void sl_dsm_region::lookup_ports(unsigned client_count)
{
	kern_return_t		r;
	boolean_t			done = FALSE;
	String 				number;
	String 				res_names[client_count];
	String				sync = "sync";

	for(int i = 0; i < client_count; i++)
	{
		number = dec(i);
		res_names[i] = "res_port" + number;
	}

	for(int retries = 0; retries < MAX_RETRIES || !done; retries++)
	{
		done = TRUE;

		for(int i = 0; i < client_count; i++)
		{
			if(sync_port == PORT_NULL)
				Rnetname_lookup((char *) sync, &sync_port);
			if(responder_ports[i] == PORT_NULL)
			{
				r = Rnetname_lookup((char *) res_names[i], &responder_ports[i]);

				if(r != KERN_SUCCESS) {
					done = FALSE;
				}
			}
		}
	}
}
int* sl_dsm_region::get_iptr()
{
   return (int *) vmem;

}

 
boolean_t sl_dsm_region::check_address(vm_address_t addr)
{
   vm_address_t end_addr;
  
   end_addr = vmem + (vm_page_size * page_count);

   boolean_t result = (vmem <= addr) && (addr <= end_addr);

   if(result == FALSE) {
      printf("Address out of dsm range 0x%x\n",addr);
      printf("start of range is 0x%x\n",vmem);
      cout << "page count = " << page_count << "\n";
   }
    
   return result;
}
 
unsigned sl_dsm_region::get_page_index(vm_address_t addr)
{
   vm_address_t offset = addr - vmem;

   unsigned page_index = offset / vm_page_size;

   return page_index;

}                    
                     
vm_address_t sl_dsm_region::get_page_address(unsigned page_index)
{                                                             
   vm_address_t addr;                                         
                     
   addr = vmem + (vm_page_size * page_index);
                                             
   return addr;                              
}              
               
void sl_dsm_region::copy_page(int page_index, vm_address_t page)
{                                                            
   int *dest_page = (int *) get_page_address(page_index);    
   int *source_page = (int *) page;                      

	dest_page[0] = 15;
                                                         
   for(int i = 0; i < vm_page_size / sizeof(int); i++)
      dest_page[i] = source_page[i];                  

                                                      
  // Cvm_deallocate((vm_address_t) source_page, vm_page_size);
}                                                           
                                                            
void sl_dsm_region::temp_copy(vm_address_t *temp_buf, unsigned page_index)
{                                                                      
   int *source_page = (int *) get_page_address(page_index);            
                                                           
   for(int i = 0; i < vm_page_size / sizeof(int); i++)     
      temp_buf[i] = source_page[i];                        
}                                  
                                
void sl_dsm_region::protect_page(int page_index, vm_prot_t protection)
{                                                                  
   vm_address_t addr = get_page_address(page_index);               
#ifdef DEBUG
  
   cout << "protect_page " << protection << "\n";
#endif
  
   Cvm_protect(addr, vm_page_size, protection);
} 

void sl_dsm_region::read_fault(unsigned page_index, unsigned client, port_t who)
{
	ptable_entry			*page = &page_table[page_index];
	vm_address_t			*page_data;

#ifdef DEBUG
	
cout << "READ_FAULT on client: " << self << " on page: " << page_index <<
" page owner is " << page->owner << "page state is " << page->state <<"\n";
#endif

	port_t temp_port;

	Cport_allocate(&temp_port);
	
	Dsm_msg					m(self, PORT_NULL, temp_port);

	page_data = (vm_address_t *) get_page_address(page_index);


	
	if(page->owner == self)
	{	// this client manages this page
		mutex_lock(page->lock);

// delta page time lock
		while ((page->time_stamp.get_elapsed_time() < delta) &&
			    (page->state == WRITE)) 
			Cthread_yield();

		switch(page->state)
		{
		
			case NO_ACCESS :	// single writer is another client
				m.get_page(READ_DATA_REQUESTED, page_index, page->writer);
debug_page_io("read_fault NO_ACCESS got_page ", m , rport(page->writer));
				page->readers->enq(page->writer);
				page->writer = PORT_NULL;
				page->state = READ;
				protect_page(page_index, VM_PROT_ALL);
// race with main thread possible here!
				copy_page(page_index, (vm_address_t) m.get_page_ptr());
				protect_page(page_index, VM_PROT_READ);
			
				if(client != self)
				{
					page->readers->enq(responder_ports[client]);
					m.send_page(READ_DATA_PROVIDED, page_index, page_data, who);
debug_page_io("read_fault NO_ACCESS sent page ", m, who);
				}
			break;
		
			case READ :	// This client is one of the readers.
				if(client != self)
				{
					page->readers->enq(responder_ports[client]);
					m.send_page(READ_DATA_PROVIDED, page_index, page_data, who);
debug_page_io("read_fault READ send page ", m, who);
				}
				else
				{
#ifdef DEBUG
					cout << "client " << self <<
					" had a read fault on a read page " << page_index << ".\n";
#endif
				}
			break;
		
			case WRITE :  // This client is the single writer.
				if(client != self)
				{
					page->state = READ;
					page->writer = PORT_NULL;
					protect_page(page_index, VM_PROT_READ);
					page->readers->enq(responder_ports[client]);
					m.send_page(READ_DATA_PROVIDED, page_index, page_data, who);
debug_page_io("read_fault WRITE sent page ", m, who);
				}
				else
				{
					cerr << "ERROR: client " << self <<
					" had a read fault on write page " << page_index << ".\n";
					exit(-1);
				}
			break;
			
			default :
				cerr << "ERROR: bad page state " << page->state <<
				" on read fault " << " on page " << page_index <<
				" on client " << self << ".\n";
				exit(-1);
			break;
		}
		page->time_stamp.set();
		mutex_unlock(page->lock);
	}
	else
	{	// owner of this page is another client, pass the request along
		m.get_page(READ_FAULT,page_index, responder_ports[page->owner]);
debug_page_io("read_fault remote owner got page ", m, page->owner);
		mutex_lock(page->lock);
		protect_page(page_index, VM_PROT_ALL);
		copy_page(page_index, (vm_address_t) m.get_page_ptr());
		protect_page(page_index, VM_PROT_READ);
		mutex_unlock(page->lock);
	}
#ifdef DEBUG
	cout << "client " << self << " done with read fault\n";
#endif
	Cport_deallocate(temp_port);
}

void sl_dsm_region::write_fault(unsigned page_index, unsigned client, port_t who)
{
	ptable_entry			*page = &page_table[page_index];
	port_t					local_port, a_client;
	intqueue				*the_readers;
	vm_address_t			temp[page_int_size];
	boolean_t				client_has_data = FALSE;
	
#ifdef DEBUG
cout << "WRITE_FAULT on client: " << self << " on page: " << page_index <<
" page owner is " << page->owner << " page state is " << page->state <<  "\n";
#endif
	Cport_allocate(&local_port);

	port_t temp_port;
	Cport_allocate(&temp_port);
	
	Dsm_msg					m(self, PORT_NULL, temp_port);

	
	if(page->owner == self)
	{	// this client manages this page
	mutex_lock(page->lock);

// delta page time lock.
		while(page->time_stamp.get_elapsed_time() < delta)
			Cthread_yield();

		switch(page->state)
		{
		
			case NO_ACCESS :	// single writer is another client
				// invalidate the single writer
				m.get_page(WRITE_DATA_REQUESTED, page_index, page->writer);
debug_page_io("write_fault NO_ACCESS got page ", m, rport(page->writer));
				
				if(client == self)  
				{  // this client had a write fault
					page->state = WRITE;
					page->writer = responder_ports[self];
					protect_page(page_index, VM_PROT_ALL);
					copy_page(page_index, (vm_address_t) m.get_page_ptr());
				}
				else
				{  // some other client had a write fault
					page->writer = responder_ports[client];
					m.send_page(WRITE_DATA_PROVIDED, page_index,
									m.get_page_ptr(), who);
debug_page_io("write_fault NO_ACCESS sent page ", m, who);
				}
			break;
		
			case READ :
				the_readers = page->readers;
				while(!the_readers->empty())
				{
					a_client = the_readers->deq();
					
					if(a_client != responder_ports[client])
						m.wait_control(PAGE_INVALID, page_index, a_client);
					else
						client_has_data = TRUE;	
				}
				if(client == self)
				{	// This client is one of the readers and wants WRITE privilege.
					page->state = WRITE;
					page->writer = responder_ports[self];
					protect_page(page_index, VM_PROT_ALL);
				}
				else if(client_has_data)
				{
					page->state = NO_ACCESS;
					page->writer = responder_ports[client];
					protect_page(page_index, VM_PROT_NONE);
					m.send_control(WRITE_PERM_GRANTED, page_index,
										who); 
				}
				else
				{	// Another client wants write privilege
					page->state = NO_ACCESS;
					page->writer = responder_ports[client];
					protect_page(page_index, VM_PROT_READ);
					temp_copy(temp, page_index);
					protect_page(page_index, VM_PROT_NONE);
					m.send_page(WRITE_DATA_PROVIDED, page_index,
									temp, who);
debug_page_io("write_fault READ sent page ", m, who);
				}
					
			break;
		
			case WRITE :  // This client is the single writer.	
			
				if(client != self)
				{
					page->state = NO_ACCESS;
					page->writer = responder_ports[client];
				// Down grade to read to make a temp copy which will be up to date
					protect_page(page_index, VM_PROT_READ);
					temp_copy(temp, page_index);
					protect_page(page_index, VM_PROT_NONE);
					m.send_page(WRITE_DATA_PROVIDED, page_index, temp,
									who);
debug_page_io("write_fault WRITE sent page ", m, who);
				}
				else
				{
					cerr << "ERROR: client " << self <<
					" had a write fault on write page " << page_index << ".\n";
					exit(-1);
				}
			break;
			
			default :
				cerr << "ERROR: bad page state " << page->state <<
				" on read fault " << " on page " << page_index <<
				" on client " << self << ".\n";
				exit(-1);
			break;
		}
	page->time_stamp.set();
	mutex_unlock(page->lock);
	}
	else
	{	// owner of this page is another client; pass the request along
		m.get_page(WRITE_FAULT,page_index, responder_ports[page->owner]);

		mutex_lock(page->lock);
		page->state = WRITE;
		page->writer = responder_ports[self];
		protect_page(page_index, VM_PROT_ALL);
		if(m.get_request() == WRITE_DATA_PROVIDED) {
debug_page_io("write_fault remote owner got page ", m, page->owner);
			copy_page(page_index, (vm_address_t) m.get_page_ptr());
		}
		else if(m.get_request() == WRITE_PERM_GRANTED)
			cout << "Remote write fault write perm granted\n";

		mutex_unlock(page->lock);
	}
#ifdef DEBUG
	cout << "client " << self << " done with write fault\n";
#endif
	Cport_deallocate(temp_port);
}


void sl_dsm_region::get_port(unsigned who)
{
	String client_name = "client", number = dec(who);

	client_name += number;

	Cnetname_lookup((char *)client_name, &responder_ports[who]);
}


void sl_dsm_region::invalidate(unsigned page_index, port_t who)
{
	Dsm_msg m(self, who, PORT_NULL);
	ptable_entry *page = &page_table[page_index];

	mutex_lock(page->lock);

	page->state = NO_ACCESS;

	protect_page(page_index, VM_PROT_NONE);

	m.send_control(INVALID_DONE);

	mutex_unlock(page->lock);
}

void sl_dsm_region::send_read_page(unsigned page_index, port_t who)
{

	Dsm_msg m(self, who, PORT_NULL);
	ptable_entry *page = &page_table[page_index];
	vm_address_t *page_data = (vm_address_t *) get_page_address(page_index);

	mutex_lock(page->lock);

	page->state = READ;
	protect_page(page_index, VM_PROT_READ);

	m.send_page(READ_DATA_PROVIDED, page_index, page_data, who);
   debug_page_io("send_read_page() sent page ", m, who);

	mutex_unlock(page->lock);
}

void sl_dsm_region::send_write_page(unsigned page_index, port_t who)
{
	Dsm_msg m(self, who, PORT_NULL);
	vm_address_t temp[page_int_size];

	ptable_entry *page = &page_table[page_index];
	vm_address_t *page_data = (vm_address_t *) get_page_address(page_index);

	mutex_lock(page->lock);

	page->state = NO_ACCESS;
	page->writer = who;
	protect_page(page_index, VM_PROT_READ);
	temp_copy(temp, page_index);
	protect_page(page_index, VM_PROT_NONE);

	m.send_page(WRITE_DATA_PROVIDED, page_index, temp, who);
   debug_page_io("send_write_page() sent page ", m, who);

	mutex_unlock(page->lock);
}

