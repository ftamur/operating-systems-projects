#include <stdio.h>


int main() {

    FILE *fptr = stdin;
    char line[1000];

    fgets(line, sizeof(line), fptr);
    printf("I am in the for_input: %s", line);


    return 0;
}