I need to create an application that can process incoming packets and 
send them to the right places. Let's call it `pktscan`.

### Requirements

1. It should use a simple config file in INI format that keeps track 
   of the following information:
    - SYSTEM Section
		- Path to pkt2mail
		- Path to pkt2news
		- Path to msg2pkt
    - NODE Section
        - Name
        - Sysop
        - Contact Info
		- List of FTN networks
    - USENET Section
        - The location of the system USENET folder.
    - MAIL Section
        - The location of the user's mail dd
    - NETWORK Sections (One per FTN network)
        - Network name (Fidonet, fsxNet, etc.)
        - Domain name (fidonet.org, fsxnet.nz, etc.)
        - Node Address
        - Hub Node Address
        - Location of the current netlist
        - Location of inbox
        - Location of outbox
        - Location of processed folder

2. Config file section names and keys should be case insensitive.

3. It should have two modes of operation: single shot and continuous.
    - Single shot mode is the default mode. It processes all files in
	  the inbox and then exits.
    - Continuous mode should works the same as single shot mode 
	  except that it doesn't exit and instead sleeps for a 
	  configurable number of seconds before processing the inbox
	  again.
    - In either mode, once new packets are discovered in the inbox, 
	  they should be imported into the proper place by calling both 
	  `pkt2mail` and `pkt2news`. The `pktscan` application should not
	  directly read or manipulate packets. It should rely on other
	  programs to do that.


### Example Config File

```ini
[system]
pkt2mail = /usr/local/bin/pkt2mail
pkt2news = /usr/local/bin/pkt2news
msg2pkt  = /usr/local/bin/msg2pkt

[node]
name = 68K Mac Club
; The first network listed is the default one
networks = fsxnet,fidonet
sysop = vaelen
sysop_name = Andrew Young
email = sysop@m68k.club
www = http://m68k.club
telnet = bbs.m68k.club

[news]
path = /var/spool/news

[mail]
path = /var/spool/mail/%USER%
sent = .Sent

; There should be one section for each of the networks listed in [node].

[fsxnet]
name = fsxNet
domain = fsxnet.nz
address = 21:1/141
hub = 21:1/100
inbox = /var/spool/ftn/fsxnet/inbox
outbox = /var/spool/ftn/fsxnet/outbox
processed = /var/spool/ftn/fsxnet/processed

[fidonet]
name = Fidonet
domain = fidonet.org
address = 1:1/102.1
hub = 1:1/100
inbox = /var/spool/ftn/fidonet/inbox
outbox = /var/spool/ftn/fidonet/outbox
processed = /var/spool/ftn/fidonet/processed
```
