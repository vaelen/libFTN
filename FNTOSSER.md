# FTNTOSS - FidoNet Tosser

In a FidoNet network, the "tosser" is a piece of software that processes incoming packets and distributes messages to their proper destinations.

The libftn tosser is called `fntosser`.

## Main Functions

- **Unpacking and sorting:** The tosser receives compressed mail packets (bundles of messages) from other FidoNet nodes and "tosses" them into the appropriate message areas (echoes, netmail, etc.) on the local system.
- **Routing:** It examines addressing information and determines where messages should go - whether they're destined for local users, need to be forwarded to other nodes, or belong in specific echomail conferences.
- **Duplicate detection:** Tossers typically check for duplicate messages (using unique identifiers) to prevent the same message from appearing multiple times as it propagates through the network.
- **Creating outbound packets:** The tosser also handles the reverse process - gathering outgoing messages and packaging them into compressed bundles to send to other nodes.

## Operation

The tosser has two modes of operation: single shot and continuous.
    - Single shot mode is the default mode. It processes all files in
	  the inbox and then exits.
    - Continuous mode should works the same as single shot mode 
	  except that it doesn't exit and instead sleeps for a 
	  configurable number of seconds before processing the inbox
	  again.

## Command-Line Options

```bash
./bin/fntosser [options]

Options:
  -c, --config FILE     Configuration file path (required)
  -d, --daemon          Run in continuous (daemon) mode
  -s, --sleep SECONDS   Sleep interval for daemon mode (default: 60)
  -v, --verbose         Enable verbose logging
  -h, --help            Show this help message
      --version         Show version information
```

## Signals

When running in daemon mode, `fntosser` responds to the following signals:

- `SIGHUP`: Reloads the configuration file.
- `SIGTERM`, `SIGINT`: Shuts down gracefully.
- `SIGUSR1`: Dumps processing statistics to the log.
- `SIGUSR2`: Toggles debug logging on or off.