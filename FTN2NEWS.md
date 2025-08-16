# Support for USENET (RFC1036)

The libFTN application suite supports converting FidoNet Echomail messages to and from USENET messages.
USENET messages are an extension of standard Internet Mail messages (as defined in [RFC822](docs/rfc822.txt).
The details of the USENET format can be found in [RFC1036](docs/rfc1036.txt). For the purposes of this library,
only section 2 of RFC1036 applies.

## Converting FidoNet Echomail Messages to USENET Articles

- The `pkt2news` application reads a FidoNet packet and writes out any Echomail messages found in the
  packet as RFC1036 formatted USENET articles. RFC1036 is an extension of RFC822 with additional headers
  for supporting USENET news articles.
- Attributes and control fields from Echomail messages which do not map cleanly to RFC1036 are still
  stored as headers in the generated USENET articles as X- style headers to maintain round-trip compatibility.
- [FSC-0070](docs/fsc-0070.002) contains additional information on how to convert messages from Echomail 
  format to USENET format, including information on mapping headers and converting message IDs. 
- When writing out a USENET article, the newsgroup of the article will be set to `NETWORK.AREA` where `NETWORK` is the
  lower case name of the FTN network ("fidonet" by default, but this can be overridden with a command line parmeter) 
  and `AREA` is the lowercase area name which has been extracted from the message.
- The `pkt2news` application writes the USENET formated messages into a directory structure which looks like this:
  `USENET_ROOT/NETWORK/AREA/ARTICLE_NUM`.
    - `USERNET_ROOT` is provided to the application at run time as a command line parameter.
    - `NETWORK` defaults to `fidonet` but it can be overriden through an optional command line parameter.
    - `AREA` is the name of the EchoMail area converted to lowercase.
    - `ARTICLE_NUM` is the unique message number of this article within the folder. It starts at 0 and increments each
      time a new article is created in the area.
    - A file named `active` at the root of `USENET_ROOT` contains a list of all newsgroups, one per line, in the form 
      `newsgroup high low perm` where `high` is the largest article number in the folder and `low` is the lowest 
      article number in the folder. The value of `perm` is `y` for groups that users can post to and `n` for groups
      that are read only. If a row already exists for a given newsgroup, `pkt2news` will not alter the value of
      `perm`. If `pkt2news` is adding a new line to the `active` file for a given newsgroup, then it will always set
      the `perm` field to `y`. For example: `fidonet.announce 1 12 y`.

## Coverting USENET Articles to FidoNet Echomail Messages

- The `msg2pkt` command will generate an Echomail message packet when given a USENET article as input.
- This works the same way as it does for Netmail.
