
// Can the C++ compiler on the NeXT handle static variables?

#include <iostream.h> 

class stest {

	private:
	 	static int i;

	public:
		stest() { i = 0; }
};

static int stest::i;

main()
{
	cout << "wow it works!\n";
}
