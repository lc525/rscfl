#!/bin/bash

# Take a squid output file
# Extract the domains from the requests
# Find the domains with more than 100 requests
# Randomise them
# Associate them with a machine in so-22-* range
# Build a hosts file based on these.

sed 's_.*\(http://[^/]*\).*_\1_' $1 | sort | uniq -c |  egrep "[0-9][0-9][0-9] http" | shuf | nl -nrn -v 51 -w9 | sed 's/^ *\([0-9]\)/128.232.22-\1/'  | sed 's/ [0-9]* //' | awk  '{print $1,$3}' | sed 's_http://__'
