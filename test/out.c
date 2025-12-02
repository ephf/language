#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
struct Rc__int32_t { int32_t* ptr; size_t counter; };

int main();
int32_t* reference__int32_t(struct Rc__int32_t);
struct Rc__int32_t alloc_rc__int32_t(size_t);


int main() {
    struct Rc__int32_t rc;
    int32_t* ref;
    (rc = alloc_rc__int32_t(4));
    (ref = reference__int32_t(rc));
}



int32_t* reference__int32_t(struct Rc__int32_t rc) {
    ((rc . counter) = ((rc . counter) + 1));
    return (rc . ptr);
}

struct Rc__int32_t alloc_rc__int32_t(size_t size) {
    return (struct Rc__int32_t) { malloc(size), 0 };
}
