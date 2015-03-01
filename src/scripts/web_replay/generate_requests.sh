#!/bin/bash

# When given a squid proxy log as $1 this builds a file of the form
# timestamp http://domain/filesize

 cat $1 | sed 's_^[!0-9\.]* \([0-9]*\) .*\(http://[^/]*\).*$_\2/\1 _'
