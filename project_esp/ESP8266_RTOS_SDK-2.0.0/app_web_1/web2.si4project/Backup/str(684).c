#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main()
{

	char *urlparam = "ssid=301&password=mwl.17521705230" ;

	char *p = NULL, *q = NULL,*k = NULL;
	char ssid[10], password[15];
	char*temp = NULL;
	int i = 0;
	char test[128] = "" , pass_word[128] = "";

	
	memset(ssid, 0, sizeof(ssid));
	memset(password, 0, sizeof(password));
	
	p = (char *)strstr(urlparam, "ssid=");
	q = (char *)strstr(urlparam, "password=");
	k = (char *)strstr(urlparam, "&");

	printf("----p = %s , q + 9 = %s\n",p,q +9);

	//test = p ;
	memcpy(test, p + 5,sizeof(test));
	
	//printf("----test = %s \n",test);
	for(i ; i < strlen(test) ; i++){
		printf("test[%d]=%d\n",i,test[i]);
		if(test[i] != 38)
			pass_word[i] = test[i];
		else
			break;
	}

	printf("pass_word = %s\n",pass_word);
	//temp = strtok(p,"&");
	
	
	if ( p == NULL || q == NULL ){
		printf("-----return\n");
		return;
	}

#if 0	
	memcpy(ssid, p + 5, k - p - 10);
	memcpy(password, q + 5, strlen(urlparam) - (q - urlparam) - 5);
	//printf("ssid[%s]password[%s]\r\n", ssid, password);
#endif
}
