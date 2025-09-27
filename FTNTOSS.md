# FTNTOSS - FidoNet Tosser

In a FidoNet network, the "tosser" is a piece of software that processes incoming packets and distributes messages to their proper destinations.

The libftn tosser is called `ftntoss`. 

## Main Functions

- **Unpacking and sorting:** The tosser receives compressed mail packets (bundles of messages) from other FidoNet nodes and "tosses" them into the appropriate message areas (echoes, netmail, etc.) on the local system.
- **Routing:** It examines addressing information and determines where messages should go - whether they're destined for local users, need to be forwarded to other nodes, or belong in specific echomail conferences.
- **Duplicate detection:** Tossers typically check for duplicate messages (using unique identifiers) to prevent the same message from appearing multiple times as it propagates through the network.
- **Creating outbound packets:** The tosser also handles the reverse process - gathering outgoing messages and packaging them into compressed bundles to send to other nodes.

## Operation ##

The tosser has two modes of operation: single shot and continuous.
    - Single shot mode is the default mode. It processes all files in
	  the inbox and then exits.
    - Continuous mode should works the same as single shot mode 
	  except that it doesn't exit and instead sleeps for a 
	  configurable number of seconds before processing the inbox
	  again.

## Configuration

The tosser can be configured with command line parameters or through a config file. Config files are stored in INI format.

The config file has the following sections:
    - NODE Section
        - Name
        - Sysop
        - Contact Info
		- List of FTN networks
    - NEWS Section
        - The location of the NEWS folder. 
          - Supports the following replacements:
            - %USER% - User name
            - %NETWORK% - Network name
          - The news folder is expected to be in the standard "spool" format with the first level being the network name and the second level being the name of the Echomail area.
    - MAIL Section
        - The location of the user's mail directory.
          - Supports the following replacements:
            - %USER% - User name
            - %NETWORK% - Network name
        - The location of the mail outbox folder
    - NETWORK Sections (One per FTN network)
        - Network name (Fidonet, fsxNet, etc.)
        - Domain name (fidonet.org, fsxnet.nz, etc.)
        - Node Address
        - Hub Node Address
        - Location of the current netlist
        - Location of inbox
        - Location of outbox
        - Location of processed folder
        - Location of the "bad" folder for placing packets that could not be processed.

Config file section names and keys should be case insensitive.

### Example Config File

```ini
[node]
name = 68K Mac Club
; The first network listed is the default network
networks = fsxnet,fidonet
sysop = vaelen
sysop_name = Andrew Young
email = sysop@m68k.club
www = http://m68k.club
telnet = bbs.m68k.club

[news]
path = /var/spool/news

[mail]
inbox = /var/spool/mail/%USER%
outbox = /var/spool/mail/%USER%/.Outbox
sent = /var/spool/mail/%USER%/.Sent

[fsxnet]
name = fsxNet
domain = fsxnet.nz
address = 21:1/141
hub = 21:1/100
inbox = /var/spool/ftn/fsxnet/inbox
outbox = /var/spool/ftn/fsxnet/outbox
processed = /var/spool/ftn/fsxnet/processed
bad = /var/spool/ftn/fsxnet/bad

[fidonet]
name = Fidonet
domain = fidonet.org
address = 1:1/102.1
hub = 1:1/100
inbox = /var/spool/ftn/fidonet/inbox
outbox = /var/spool/ftn/fsxnet/outbox
processed = /var/spool/ftn/fsxnet/processed
bad = /var/spool/ftn/fidonet/bad

```
