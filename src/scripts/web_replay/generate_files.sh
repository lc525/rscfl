#!/bin/bash

# Generate files of every size found in the squid trace from $1

for x in $(cat $1 | cut -d ' ' -f2 | sort -n | uniq | sed ':a;N;$!ba;s/\n/ /g' );
do
    sudo dd if=/dev/urandom of=/$2/$x.data bs=$x count=1
done
