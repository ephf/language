#include <stdint.h>
#include <stdio.h>
int main();
struct Test { int32_t data; } undefined__struct_Test();


int main() {
    struct Test x;
    (x = undefined__struct_Test());
}


struct Test undefined__struct_Test() {
}
