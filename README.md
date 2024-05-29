# Brief Intro
This is a learning exercise to get acquainted with the sDDF. A basic serial driver has been implemented in pancake, with printf, getchar and scanf.

## Building the cake compiler
Included in this repo is a tar for the cake compiler. Extract this file and run the
following commands:
```
cd cake-x64-64/
make cake
```

To use the newest version of cake compiler with multiple entry points feature, download the newest `cake-sexpr-version` and build the compiler:
```
$ CML_STACK_SIZE=1000 CML_HEAP_SIZE=6000 ./path/to/cake-x64-64/cake --sexp=true --exclude_prelude=true --skip_type_inference=true --emit_empty_ffi=true --reg_alg=0 < ./cake-sexpr-version  > cake_sexp.S
$ gcc ./path/to/cake-x64-64/basis_ffi.c cake_sexp.S -o cake-new
```

## Building the serial driver
```
$ cd serial_system
$ make BUILD_DIR=<path/to/build> MICROKIT_SDK=<path/to/microkit/sdk> CAKE_COMPILER=<path/to/cake/compiler/binary> MICROKIT_BOARD=imx8mm MICROKIT_CONFIG=(release/debug)
```

## Supported Boards

### iMX8MM-EVK

