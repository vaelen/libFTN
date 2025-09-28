# Configuration

The mailer and tosser are configured through a config file stored in INI format.

Config file section names and keys should be case insensitive.

## Configuration Sections

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
- `binkp.address`: The address and port of the hub's binkp server. Default port is 24554.
- `binkp.password`: The password to use when connecting to the binkp server.
- `binkp.inbound`: The path to the binkp inbound folder for this network.
- `binkp.outbound`: The path to the binkp outbound folder for this network.
- `binkp.processed`: The path to the directory where processed binkp files are moved.
- `binkp.bad`: The path to the directiry where malformed binkp files are moved.

## Example Config File

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
binkp.address = binkp.fidonet.org
binkp.password = password
binkp.inbound = /var/spool/ftn/fidonet/binkp/inbound
binkp.outbound = /var/spool/ftn/fidonet/binkp/outbound
binkp.processed = /var/spool/ftn/fidonet/binkp/processed
binkp.bad = /var/spool/ftn/fidonet/binkp/bad

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
binkp.address = agency.bbs.nz:24554
binkp.password = password
binkp.inbound = /var/spool/ftn/fsxnet/binkp/inbound
binkp.outbound = /var/spool/ftn/fsxnet/binkp/outbound
binkp.processed = /var/spool/ftn/fsxnet/binkp/processed
binkp.bad = /var/spool/ftn/fsxnet/binkp/bad
```