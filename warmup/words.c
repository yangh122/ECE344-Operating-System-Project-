#include "common.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    int i;
	for(i = 1; i< argc; i++){
		//if(*argv[i] != ' '){
			//printf(argv[i]);	
		//}
		//else if(*argv[i] == ' '){
			//printf("1");
		//}
		printf("%s\n",argv[i]);
	}
	return 0;
}
