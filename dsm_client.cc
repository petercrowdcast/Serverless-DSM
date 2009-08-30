#include "dsm_region.h"
#include <stream.h>
#include <cthreads.h>


extern int client_no;
extern mutex_t responder_done;
extern int write_flag;


void set_range(dsm_region &d , vm_prot_t prot, int start, int stop);


main()
{
    int             i;
    int             *iptr, j;
	 dsm_region			d;
   
/*  set the write flage to read */

cout << "back from dsm_region constructor\n";
     set_range(d, VM_PROT_NONE, 0, 9);

     write_flag = FALSE;
     iptr = d.get_iptr();

     for(i = 0; i < ((vm_page_size/4) * 10); i += 2048)
        printf("%d ", iptr[i]);
      
     write_flag = TRUE;

     for(i = 0; i < ((vm_page_size/4) * 10); i += 2048) {
        *iptr = 7;
        *iptr = 8;
        *iptr = 9;
        iptr += 2048;
   }
   cout << "Client " << client_no << " SET DONE\n"; 


}

void set_range(dsm_region &d, vm_prot_t prot, int start, int stop)
{
	int i;
	
	for (i = start; i <= stop; i++)
		d.protect_page(i, prot);
}
