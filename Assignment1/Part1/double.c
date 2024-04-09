#include <stdio.h>
#include<stdlib.h>
#include<math.h>
#include<unistd.h>
#include<string.h>

void tostring(char str[], unsigned long num){
    unsigned long i, r, len = 0;

    unsigned long n = num;
    while (n != 0){
        len++;
        n = n/10;
    }
    for (i = 0; i < len; i++){
        r = num % 10;
        num = num / 10;
        str[len - (i + 1)] = r + '0';
    }
    str[len] = '\0';
}

int main(int argc, char* argv[]){
    if(argc <2 ){
		printf("Unable to execute\n");
		return 0;
	}
    char* ptr;
	unsigned long val = strtoul(argv[argc-1],&ptr,10);
    if(argc==2){
        val *= 2;
        printf("%lu\n",(unsigned long)val);
    }
    else{
		char str[12];
		tostring(str,val*2);
		argv[argc-1]=str;
        execv(argv[1],argv+1);
        perror("Unable to execute\n");
    }
    return 0;
}

