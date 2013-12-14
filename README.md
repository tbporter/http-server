http-server
===========

Event driven HTTP server in C



make generates the executable sysstatd

command line options-
-p port
	When given, your web service must run in server mode, accepting HTTP
clients and serving HTTP requests on port ’port.’ Multiple connection must be sup-
ported.
-r relayhost:port
	When given, your web service should connect to host ’re-
layhost’ on port ’port.’. It should accept both fully-qualified host names and IPv4
addresses in dot notation.
-R path
	When given, ‘path’ specifies the root directory of your server, files in it are
reachable under the /files prefix.