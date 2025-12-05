# Quark Language

> This langauge is in early stages of development, everything is subject to change

To build the compiler (default: `qc`):

```sh
$ make build
or 
$ make build out=PATH
```

Compile a `.qk` file to C:

```sh
$ ./qc main.qk -o main.c
```

## Hello World

```quark
import lib::io;

print("Hello World");
```
