#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>


// Return a random capital letter char or ' ' (space)
int rand_char() {
    char char_digit;
    int rand_int = rand() % 27;
    if (rand_int == 26) {
        char_digit = ' ';
    }
    else {
        char_digit = 'A' + rand_int;
    }
    return char_digit;
}


int main(int argc, char *argv[]) {
    srand(time(NULL));
    // Check usage & args
    if (argc < 2) {
        fprintf(stderr,"USAGE: %s keylength (int greater than 0)\n", argv[0]);
        exit(1);
    }

    int keylength = strtol(argv[1], NULL, 10);

    if (keylength < 1) {
        fprintf(stderr,"USAGE: %s keylength (int greater than 0)\n", argv[0]);
        exit(1);
    }

    char key[keylength + 1];

    for (int i = 0; i < keylength; i++) {
        key[i] = rand_char();
    }
    key[keylength] = '\0';
    printf("%s\n", key);

}
