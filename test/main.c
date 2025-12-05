#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
struct Vec__int32_t { int32_t* data; size_t size; size_t capacity; };

int main();
struct Vec__int32_t Vec__int32_t__new();
void Vec__int32_t__reserve(struct Vec__int32_t*, size_t);
void Vec__int32_t__push(struct Vec__int32_t*, int32_t);


int main() {
    struct Vec__int32_t numbers;
    (numbers = Vec__int32_t__new());
    Vec__int32_t__push((&numbers), 5);
}


struct Vec__int32_t Vec__int32_t__new() {
    return (struct Vec__int32_t) { .data = malloc(sizeof(int32_t)), .size = 0, .capacity = 1 };
}


void Vec__int32_t__reserve(struct Vec__int32_t* self, size_t n) {
    if((((self -> size) + n) <= (self -> capacity))) 
        return ;
    while(((self -> capacity) < ((self -> size) + n))) 
    {
        ((self -> capacity) = ((self -> capacity) * 2));
    }
    ((self -> data) = realloc((self -> data), (self -> capacity)));
}


void Vec__int32_t__push(struct Vec__int32_t* self, int32_t item) {
    Vec__int32_t__reserve(self, 1);
    ((*((self -> data) + (self -> size))) = item);
    ((self -> size) = ((self -> size) + 1));
}
