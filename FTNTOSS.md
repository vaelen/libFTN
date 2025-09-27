# FTNTOSS - FidoNet Tosser

In a FidoNet network, the "tosser" is a piece of software that processes incoming packets and distributes messages to their proper destinations.

The libftn tosser is called `ftntoss`.

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
./bin/ftntoss [options]

Options:
  -c, --config FILE     Configuration file path (required)
  -d, --daemon          Run in continuous (daemon) mode
  -s, --sleep SECONDS   Sleep interval for daemon mode (default: 60)
  -v, --verbose         Enable verbose logging
  -h, --help            Show this help message
      --version         Show version information
```

## Configuration

The tosser can be configured with command line parameters or through a config file. Config files are stored in INI format.

Config file section names and keys should be case insensitive.

The config file has the following sections:

### [node]
This section contains general information about the FTN node.

- `name`: The name of the node.
- `sysop`: The name of the system operator.
- `sysop_name`: The full name of the system operator.
- `email`: The email address of the system operator.
- `www`: The URL of a website for the node.
- `telnet`: The telnet address of a BBS for the node.
- `networks`: A comma-separated list of the FTN networks the node is a member of.

### [news]
This section configures the USENET news spool.

- `path`: The root path of the news spool. It supports the following replacements:
    - `%USER%`: User name
    - `%NETWORK%`: Network name

### [mail]
This section configures local mail delivery.

- `inbox`: The path to the user's mail inbox. It supports the following replacements:
    - `%USER%`: User name
- `outbox`: The path to the user's mail outbox.
- `sent`: The path to the user's sent mail folder.

### [logging]
This section configures the logging options.

- `level`: The log level. Can be `debug`, `info`, `warning`, `error`, or `critical`. Default is `info`.
- `use_syslog`: If `yes`, log to syslog in daemon mode. Default is `no`.
- `log_file`: The path to the log file when not using syslog. Default is standard output.
- `ident`: The identifier to use for syslog messages. Default is `ftntoss`.

### [daemon]
This section configures the daemon mode.

- `pid_file`: The path to the PID file. Default is `/var/run/ftntoss.pid`.
- `sleep_interval`: The sleep interval in seconds between processing cycles. Default is `60`.

### Network Sections
One section for each FTN network listed in the `networks` key of the `[node]` section.

- `name`: The display name of the network (e.g., `Fidonet`, `fsxNet`).
- `domain`: The domain name for the network (e.g., `fidonet.org`, `fsxnet.nz`).
- `address`: The FTN address of the node on this network.
- `hub`: The FTN address of the hub for this network.
- `inbox`: The path to the inbox directory for this network.
- `outbox`: The path to the outbox directory for this network.
- `processed`: The path to the directory where processed packets are moved.
- `bad`: The path to the directory where malformed packets are moved.
- `duplicate_db`: The path to the duplicate message database for this network.

### Example Config File

```ini
[node]
name = Example Node
networks = fidonet,fsxnet
sysop = John Doe
sysop_name = John Doe
email = sysop@example.org
www = http://example.org
telnet = bbs.example.org

[news]
path = /var/spool/news

[mail]
inbox = /var/mail/%USER%
outbox = /var/mail/%USER%/.Outbox
sent = /var/mail/%USER%/.Sent

[logging]
level = info
use_syslog = yes
ident = ftntoss

[daemon]
pid_file = /var/run/ftntoss.pid
sleep_interval = 60

[fidonet]
name = Fidonet
domain = fidonet.org
address = 1:2/3.4
hub = 1:2/0
inbox = /var/spool/ftn/fidonet/inbox
outbox = /var/spool/ftn/fidonet/outbox
processed = /var/spool/ftn/fidonet/processed
bad = /var/spool/ftn/fidonet/bad
duplicate_db = /var/spool/ftn/fidonet/dupes.db

[fsxnet]
name = fsxNet
domain = fsxnet.nz
address = 21:1/100.0
hub = 21:1/0
inbox = /var/spool/ftn/fsxnet/inbox
outbox = /var/spool/ftn/fsxnet/outbox
processed = /var/spool/ftn/fsxnet/processed
bad = /var/spool/ftn/fsxnet/bad
duplicate_db = /var/spool/ftn/fsxnet/dupes.db
```

## Signals

When running in daemon mode, `ftntoss` responds to the following signals:

- `SIGHUP`: Reloads the configuration file.
- `SIGTERM`, `SIGINT`: Shuts down gracefully.
- `SIGUSR1`: Dumps processing statistics to the log.
- `SIGUSR2`: Toggles debug logging on or off.