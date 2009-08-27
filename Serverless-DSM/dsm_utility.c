//  dsm_ultility.c

#include "dsm_utility.h"

/*
void page_info(vm_address_t addr)
{

  vm_address_t   reg_addr = addr;
  vm_size_t      reg_size;
  vm_prot_t      reg_prot, reg_max_prot, reg_prot2, reg_max_prot2;
  vm_inherit_t   reg_inher;
  boolean_t      shared;
  port_t         object_name;
  vm_offset_t    offset;
  kern_return_t  r;

  printf("before vm_region reg_addr = 0x%x\n", (unsigned int) reg_addr);

  r = vm_region(task_self(), &reg_addr, &reg_size, &reg_prot,
                &reg_max_prot, &reg_inher, &shared, &object_name, &offset);

  if(r != KERN_SUCCESS)
    mach_error("vm_region", r);

   printf("after vm_region reg_addr = 0x%x\n", (unsigned int) reg_addr);
   printf("current protection is %d\n max protection is %d\n", reg_prot,
         reg_max_prot);

  printf("object_name %d\n", object_name);

}

*/
boolean_t examine_address(vm_address_t start_addr, int dsm_size,
                          vm_address_t fault_addr)
{
   vm_address_t end_addr = start_addr + (vm_page_size * dsm_size);
   boolean_t result;

   result = (start_addr <= fault_addr) && (fault_addr <= end_addr);

   if(!result) 
     printf("Address out dsm ranage: fault addr = 0x%x\n", fault_addr);

   return result;

}

int address_to_page(vm_address_t start_addr, vm_address_t faulting_addr)
{
   vm_address_t offset = faulting_addr - start_addr;
   int page_no;

   page_no = offset / vm_page_size;

   return page_no;
}
int* page_to_address(int page, vm_address_t vmem)
{
  int *addr;

   addr = (int*) vmem;

   addr = addr + (PAGE_DATA_SIZE * page);

return addr;
}


void page_copy(int page_index, int *source_page, vm_address_t vmem)
{
 int *dest_page = page_to_address(page_index, vmem);
 int i;

  for(i = 0; i < PAGE_DATA_SIZE; i++)
      dest_page[i] = source_page[i];
}

