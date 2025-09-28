# FNMAILER - FidoNet Mailer

In a FidoNet network, the "mailer" is a piece of software that sends and receives packets to/from other nodes.

The libftn mailer is called `fnmailer`.

## Main Functions

- **Communication with other nodes:** The mailer connects to other nodes in the system to send and receive files that contain packets.
- **Routing:** It examines addressing information and determines where messages should go - whether they're destined for local users, need to be forwarded to other nodes, or belong in specific echomail conferences.
- **Duplicate detection:** Tossers typically check for duplicate messages (using unique identifiers) to prevent the same message from appearing multiple times as it propagates through the network.
- **Creating outbound packets:** The tosser also handles the reverse process - gathering outgoing messages and packaging them into compressed bundles to send to other nodes.

## Operation

The basic operation of the mailer is as follows:
    1. Read config file
    2. For each configured network:
       1. Connect to the configured "hub" node.
       2. Package up outbound packets as needed for transmission. (Includes compression if configured.)
       3. Send outbound files.
       4. Receive any new inbound files.
       5. Uncompress or otherwise process inbound files.

The mailer can also be run as a daemon. When run as a daemon, the mailer keeps track of how long it has been since the last time it communicated with a given hub and if it has been more than a certain number of seconds it will initiate a send/receive session as described for basic operation. When it first starts the daemon reads the config file and connects to each configured hub, taking note of when it did so. Then it wakes up once per second and checks if it time to connect to any of the hubs. If so, it connects to that hub and updates the last connected timestamp. The timestamps are only kept in memory, restarting the daemon resets these timestamps and causes an immediate query of all hubs.

## Communication Details

FNMailer uses the binkp protocol to communicate with other nodes. 

The binkp protocol is defined in the following documents:
  - [FTS-1026](docs/ftn/fts-1026.txt)  Binkp/1.0 Protocol specification
  - [FTS-1027](docs/ftn/fts-1027.txt)  Binkp/1.0 optional protocol extension CRAM
  - [FTS-1028](docs/ftn/fts-1028.txt)  Binkp protocol extension Non-reliable Mode
  - [FTS-1029](docs/ftn/fts-1029.txt)  Binkp optional protocol extension Dataframe Compression
  - [FTS-1030](docs/ftn/fts-1030.txt)  Binkp optional protocol extension CRC Checksum
  - [FTS-5005](docs/ftn/fts-5005.txt)  Advanced Binkleyterm Style Outbound flow and control

The binkp client can be found in `include/ftn/binkp.h` and `src/binkp.c`.

The binkp client communicates over TCP/IP. Because TCP/IP is implemented differently on different platforms, libftn also includes a library that wraps the native TCP/IP stack. This library can be found in `include/ftn/net.h` and `src/net.c`. The network library contains functions for opening TCP/IP sockets and sending/receiving data over those sockets. It encapsulates the OS native TCP/IP interfaces and error messages for easier portability.

## Command-Line Options

```bash
fnmailer [options]

Options:
  -c, --config FILE     Configuration file path (required)
  -d, --daemon          Run in continuous (daemon) mode
  -v, --verbose         Enable verbose logging
  -h, --help            Show this help message
      --version         Show version information
```

## Configuration

The mailer is configured through a config file stored in INI format.

The mailer and tosser can use the same ini file.

See [CONFIG.md](CONFIG.md) for details on the configuration file format.

## Signals

When running in daemon mode, `fnmailer` responds to the following signals:

- `SIGHUP`: Reloads the configuration file.
- `SIGTERM`, `SIGINT`: Shuts down gracefully.
- `SIGUSR1`: Dumps processing statistics to the log.
- `SIGUSR2`: Toggles debug logging on or off.