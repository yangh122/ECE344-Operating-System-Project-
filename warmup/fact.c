#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


int helperfunction(int input){
    if(input == 1){
        return 1;
    }
    return input*helperfunction(input - 1);
}

int main(int argc, char **argv)
{
 
    if(argc !=2 ){
        printf("Huh?\n");
       return 0;
    }
      
    for(int i = 0; i< strlen(argv[1]); i++){
        if(isdigit(argv[1][i]) == 0){
            printf("Huh?\n");
            return 0;
        }
    }
    
    int x = atoi(argv[1]);
    if(x == 0 || x < 0){
       printf("Huh?\n");
       return 0;
    }
    else if(x >12){
        printf("Overflow\n");
        return 0;
    }
    
    int ans = helperfunction(x);
    printf("%d\n", ans);
     
     return 0;
}


