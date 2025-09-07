#include <stdint.h>
#include <stdio.h>
int main();


int main() {
    struct Test { int32_t data; } x;
    int32_t y;
    (x = (struct Test) { .data = 4 });
    (y = (x . data));
}
