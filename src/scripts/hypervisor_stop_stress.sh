#! /bin/bash

for x in $(sudo xl list | grep ubuntu-clone- | sed 's/\s\+/ /g' | cut -d ' ' -f2); do
  sudo xl destroy $x
done

