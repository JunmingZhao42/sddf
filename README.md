# Brief Intro
This is a learning exercise to get acquainted with the sDDF. A basic serial driver has been implemented in pancake, with printf, getchar and scanf.

## Building the cake compiler
Included in this repo is a tar for the cake compiler. Extract this file and run the
following commands:
```
cd cake-x64-64/
make cake
```

## Building the sDDF

    $ cd echo_server
    $ make BUILD_DIR=<path/to/build> MICROKIT_SDK=<path/to/microkit/sdk> CAKE_COMPILER=<path/to/cake/compiler/binary> MICROKIT_BOARD=imx8mm MICROKIT_CONFIG=(release/debug)

## Notes on building Pancake
We will need to modify an output file, and recompile. This is because of the auto-genrated assembly code outputted by the cake compiler wishing to call
to a `cml_exit` function rather than return from where it was called, which is the behaviour we want.

<!-- TODO: Remove this: -->

In `serial.S`, please add line
```
cdecl(ret_third): .quad 0
```
right after line
```
cdecl(ret_base): .quad 0
```

Replace `cake_enter` with:
```
cake_enter:
     str    x30, [sp, #-32]!
     str    x28, [sp, #-32]!
     str    x27, [sp, #-32]!
     str    x25, [sp, #-32]!
     _ldrel x9, cdecl(ret_stack)
     ldr    x25, [x9]
     cbz    x25, cake_err3
     str    xzr, [x9]
     _ldrel x9, cdecl(ret_base)
     ldr    x28, [x9]
     cbz    x28, cake_err3
     str    xzr, [x9]
     _ldrel x9, cdecl(ret_third)
     ldr    x27, [x9]
     _ldrel x30, cake_ret
     br     x10
     .p2align 4
```

Replace `cake_return` with:

```
cake_return:
     _ldrel x9, cdecl(ret_stack)
     str    x25, [x9]
     _ldrel x9, cdecl(ret_base)
     str    x28, [x9]
     _ldrel x9, cdecl(ret_third)
     str    x27, [x9]
     ldr    x25, [sp], #32
     ldr    x27, [sp], #32
     ldr    x28, [sp], #32
     ldr    x30, [sp], #32
     ret
     .p2align 4
```
## Supported Boards

### iMX8MM-EVK

