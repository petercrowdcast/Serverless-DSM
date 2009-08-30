#include <stream.h>
#include <builtin.h>


main()
{
	start_timer();

	cout <<  "time = " << return_elapsed_time(0) << "\n";



   for(int i = 0; i < 1000000; i++)
		;

	cout <<  "time = " << return_elapsed_time(0) << "\n";

}

