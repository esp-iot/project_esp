#include <stdio.h>


int dec2hex(int n)
{
	if(n>15)
		dec2hex(n/16);
	
	return n; 
}

int StringToInteger(char *p)
{
	int value = 0;
	while (*p != '\0')
	{
		if ((*p >= '0') && (*p <= '9'))
		{
			value = value * 10 + *p - '0';
		}
		p++;
 
	}
	return value;
}




