#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int trim(char s[]) {

    int n;

    for (n = strlen(s) - 1; n >= 0; n--) 
        if (s[n] != ' ' && s[n] != '\t' && s[n] != '\n')
            break;
    s[n+1] = '\0';
    return n;

    

}

int main(void) {


    return 0;


}