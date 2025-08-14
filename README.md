# libFTN

libFTN is a C library for working with FidoNet (FTN) mail and related protocols.

## Project Goals

- The library should be written in ISO/ANSI C ('89) to remain as portable as possible.
- The library should compile with any ISO/ANSI C compiler and not rely on any OS specific code.

## Features

- Functions for parsing FTN nodelists.
- Functions for parsing and creating packed mail objects.

## Build Instructions

On UNIX or any UNIX-like operating system (such as Linux or MacOS) you should only need to run `make`.

## Project Structure

- [bin](bin) - Compiled executables will be placed here after compilation.
- [docs](docs) - The FTN technical documentation, originally retrieved from the [FTSC Documents](http://ftsc.org/docs/) website.
- [include](include) - C header files are placed here.
- [lib](lib) - The compiled library will be placed here after compilation.
- [obj](obj) - Temporary object files are placed here during compilation.
- [src](src) - C source code is placed here.
- [tests](tests) - The C source code and related files required for unit tests are placed here.
- [Makefile](Makefile) - Contains the information that `make` needs to properly build the library and executables.
- [README.md](README.md) - This file.
- [TODO.md](TODO.md)  - Should be kept up to date with remaining tasks that need to be completed.

## License

MIT License

Copyright 2025, Andrew C. Young <andrew@vaelen.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
