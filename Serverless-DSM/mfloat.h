// class	: mfloat
// author	:	Peter Ogilvie
// notes		:	This class overcomes the mach exception bug by transparently
// causing writes of floats to be done four times.  When (or if) the
// exception byg is fixed on the NeXT programs which use mfloats can change
// to the use of floats without furthur modification. 
// Use this class just as you would a float but take care when using
// perincreament/ postincreament operators as surprising results may occur.

#ifndef MFLOAT
#define MFLOAT


#include <stream.h>

extern int write_flag;

class mfloat
{
	friend ostream& operator << (ostream&, mfloat&);
	
	private:
		float	data;
	
	public:
		mfloat() {}
		
	   float operator = (float value);

		operator float()	{ return data; }
		
		
};



inline float mfloat::operator = (float value)
{
	write_flag = 1;
	data = value;
	data = value;
	data = value;
	data = value;
	write_flag = 0;
	if(data != value)
		cerr << "###MFLOAT OPERATOR = FAILED VALUE### " << value << "\n";
	return data;
}

ostream& operator << (ostream& os, mfloat& f)
{
	os << f.data;
	return os;
}

#endif
