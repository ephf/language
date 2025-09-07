# Language

**Work in Progress**

This language in transpiled to C using the source code in this repository. You can build it with make:

```sh
$ make build
```

Or use the default make (`make a`) to build and test using the [test/main.lang](test/main.lang) file as the source. (as of now the result will print to stdout)

```sh
$ make
```

## Features

### Command structure

```sh
./c <files> [flags]
```

> Note: the compiler only compiles the first file at the moment

**Flags:**

- `-o <filename>`: sets the output file (currently does nothing).

### Types

This language includes a rust-like type system for primatives. See the source in [main.c](main.c) `insert_std_numerics()` for all types.

Additionally, pointer types can be denoted similarly to C with the `*` symbol.

### Variable Declarations

Again, similar to C we can define variables with a type and an identifier. Unlike C, declarations are expressions with a precedence of 13:

```
i32 x = 15;
u32 y = u32 z = 14;
```

```c
int main() {
    int32_t x;
    uint32_t y;
    uint32_t z;
    (x = 15);
    (y = (z = 14));
}
```

Note that when defining constants with pointers, the `const` keyword will have a low precedence, e.g.

```
const i32* x; // constant pointer
(const i32)* y; // pointer to constant
```

### Function Declarations

Also similar to C:

```
i32 add(i32 a, i32 b) {
    return a + b;
}
```

```c
int main() {
}

int32_t add(int32_t a, int32_t b) {
    return (a + b);
}
```

> Note function declarations will be hoisted

### Generics & Autos

Here is an example of the compiler's ability to keep track of type relations:

```
T echo<T>(T value) {
	return value;
}

i32* x = echo<typeof(echo(15))*>(auto y);
```

```c
int main() {
    int32_t* x;
    int32_t* y;
    (x = echo__int32_t_ptr(y));
}

int32_t echo__int32_t(int32_t value) {
    return value;
}

int32_t* echo__int32_t_ptr(int32_t* value) {
    return value;
}
```

## Ending / Notes

If you want more information, check the source code. I'll hopefully be updating this repo and language regularly for the next couple of weeks.
