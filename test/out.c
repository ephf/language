#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
struct Vec__int32_t { int32_t* data; size_t size; size_t capacity; ; };

int main();
struct Vec__int32_t Vec__int32_t__new();


int main() {
    struct Vec__int32_t numbers;
    (numbers = Vec__int32_t__new());
}


struct Vec__int32_t Vec__int32_t__new() {
    return (struct Vec__int32_t) { malloc(sizeof(int32_t)), 0, 1 };
}
