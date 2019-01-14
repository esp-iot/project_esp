#include <stdio.h>


int dec2hex(int n)
{
	if(n>15)
		dec2hex(n/16);
	
	return n; 
}

