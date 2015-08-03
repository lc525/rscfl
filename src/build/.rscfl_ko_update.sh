#!/bin/bash

function print_usage {
  echo "$0 [rscfl_new_ko]"
  echo ""
  echo "- rmmods any rscfl_*.ko kernel module (killing applications that use it)"
  echo "- when given a kernel module as a parameter, it inserts it into the"
  echo "  kernel using staprun"
}

MODNAME=`lsmod | grep rscfl | awk '{ print $1; }'`

while getopts ":h" opt; do
  case $opt in
    h)
      print_usage
      exit 0
      ;;
  esac
done

if [ ! -z "$MODNAME" ]
then
  scripts/rscfl_stop
  if [ $? -eq 0 ]
  then
    echo "Stop rscfl OK, cool off period...(30s)"
  fi
  sleep 30
  #existing processes using rscfl*.ko
  echo "Removing modules with name rscfl*.ko..."
  lsmod | grep rscfl | sed 's/ .*$//' | xargs sudo rmmod
fi

if [ $# -eq 1 ]
then
  sudo dmesg --clear
  #insert new module
  echo "running new module ($1)..."
  sudo insmod $1 &
fi

