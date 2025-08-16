# TODO

## Cleanup Tasks
1. Move all header files except `ftn.h` under an `ftn` folder.

## Scanner / Tosser

I need to create an application that can process incoming packets and send them to the right places.

### Requirements

1. It should use a simple config file in INI format that keeps track of the following information:
    - NODE Section
        - Name
        - Sysop
        - Contact Info
    - USENET Section
        - The location of the system USENET folder.
    - MAIL Section
        - A mapping of usernames to mail directories.
    - NETWORK Sections (One per FTN network)
        - Network name (Fidonet, fsxNet, etc.)
        - Domain name (fidonet.org, fsxnet.nz, etc.)
        - Node Address
        - Parent Node Address
        - Location of the current netlist
        - Location of inbox
        - Location of outbox
        - Location of processed folder

2. It should have two modes of operation: single shot and continuous.
    - Continuous mode should sleep for a configurable number of seconds between polling the inbox and outbox for
        new packes.
    - In either mode, once new packets are discovered, they should be imported into the proper place by calling pkt2mail and/or pkt2news.
