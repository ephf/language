#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main();
void outer__int32_t();
void inner__int32_t();
void outer__uint32_t();
void inner__uint32_t();


int main() {
    outer__int32_t();
    outer__uint32_t();
}


void outer__int32_t() {
    inner__int32_t();
}


void inner__int32_t() {
}


void outer__uint32_t() {
    inner__uint32_t();
}


void inner__uint32_t() {
}
