#include "help.h"

const char *HELP =
"Usage: btsend [-a address] [-p port] [-l log_level] <file>\n"
"       btrecv [-a address] [-p port] [-l log_level] <file>\n"
"Options:\n"
"  -a address    Send to or listen at the specified address\n"
"                Default: 127.0.0.1\n"
"  -p port       Specify the port number, default 6666\n"
"  -l log_level  Set logging level (verbosity), valid levels are\n"
"                debug, info, warn (default), error, critical\n"
"  <file>        Give one file as input source or output destination\n"
;

const char *VERSION = 
"iBug backTCP "
"1.0";
