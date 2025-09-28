#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main();
void inner__auto();
void outer__int32_t();
void outer__uint32_t();


int main() {
    outer__int32_t();
    outer__uint32_t();
}


void inner__auto() {
}


void outer__int32_t() {
    inner__auto();
}


void outer__uint32_t() {
    inner__auto();
}
