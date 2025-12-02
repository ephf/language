#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main();
struct HeapPointer__int32_t { int32_t* pointer; };
struct HeapPointer__int32_t alloc__int32_t(size_t);


int main() {
    struct HeapPointer__int32_t x;
    (x = alloc__int32_t(4));
    free((x . pointer));
    ((x . pointer) = malloc(4));
}


struct HeapPointer__int32_t alloc__int32_t(size_t size) {
    return (struct HeapPointer__int32_t) { malloc(size) };
}
