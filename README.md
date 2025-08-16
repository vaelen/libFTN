# libFTN

libFTN is a C library for working with FidoNet (FTN) mail and related protocols.

## Project Goals

- The library is written in ISO/ANSI C ('89) to remain as portable as possible.
- The library should compile with any ISO/ANSI C compiler and not rely on any OS specific code.
- A compatibility layer is provided for non-ANSI and non-POSIX compilers.

## Features

- Functions for parsing FTN nodelists.
- Functions for parsing and creating packed mail objects.
- RFC822 message format conversion library with bidirectional FTN ↔ RFC822 conversion.
- Command-line utilities for converting between FidoNet packets and standard mailbox formats.

## Build Instructions

On UNIX or any UNIX-like operating system (such as Linux or MacOS) you should only need to run `make`.

## Command-Line Utilities

The following utilities are built alongside the library:

### pkt2maildir
Converts FidoNet packet files (.pkt) to maildir format for use with standard email clients.

```bash
./bin/pkt2maildir [options] <maildir_path> <packet_files...>

Options:
  --domain <name>  Domain name for RFC822 addresses (default: fidonet.org)
  --help           Show help message

Example:
  ./bin/pkt2maildir /home/user/Mail/ftn netmail.pkt echomail.pkt
  ./bin/pkt2maildir --domain mynet.org /home/user/Mail/ftn *.pkt
```

### maildir2pkt  
Converts RFC822 messages to FidoNet packet format.

```bash
./bin/maildir2pkt [options] <rfc822_files...>

Options:
  -d <domain>  Domain name for RFC822 addresses (default: fidonet.org)
  -s <dir>     Move processed files to specified 'Sent' directory
  -o <file>    Output packet filename (default: auto-generated)
  -f <addr>    From address (zone:net/node.point format)
  -t <addr>    To address (zone:net/node.point format)
  -h           Show help message

Example:
  ./bin/maildir2pkt -f 1:2/3 -t 1:4/5 -o outbound.pkt message.txt
```

### Other Utilities
- **pktcreate**: Create new FidoNet packets with messages
- **pktview**: Display packet contents in human-readable format
- **pktlist**: List messages in packet files
- **pktbundle**: Bundle multiple packets together
- **nllookup**: Look up nodes in FidoNet nodelists
- **nlview**: Display nodelist information

## Testing

The library includes comprehensive unit tests for all major functionality:

```bash
# Run all tests
make test

# Run specific test suites
./bin/test_rfc822      # RFC822 conversion tests (8/8 tests)
./bin/test_packet      # Packet parsing tests
./bin/test_nodelist    # Nodelist parsing tests
```

All RFC822 conversion functionality includes full roundtrip testing to ensure message fidelity is preserved during FTN ↔ RFC822 conversions.

## Project Structure

- [bin](bin) - Compiled executables will be placed here after compilation.
- [docs](docs) - The FTN technical documentation, originally retrieved from the [FTSC Documents](http://ftsc.org/docs/) website.
- [examples](examples) - Example files.
  - [maildir](examples/maildir) - An example maildir formatted mailbox.
  - [echomail.pkt](examples/echomail.pkt) - An example FidoNet packet containing an echomail message.
  - [netmail.pkt](examples/netmail.pkt) - An example FidoNet packet containing a netmail message.
  - [bundle.pkt](examples/bundle.pkt) - An example FidoNet packet containing multiple messages.
- [include](include) - C header files are placed here.
- [lib](lib) - The compiled library will be placed here after compilation.
- [obj](obj) - Temporary object files are placed here during compilation.
- [src](src) - C source code is placed here.
- [tests](tests) - The C source code and related files required for unit tests are placed here.
- [tmp](tmp) - Used to store temporary files for building or testing.
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
