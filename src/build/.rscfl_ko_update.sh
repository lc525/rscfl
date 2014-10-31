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

sudo dmesg --clear

if [ ! -z "$MODNAME" ]
then
  #existing processes using rscfl*.ko
  echo "killing processes dealing with rscfl*.ko..."
  PIDS=`ps -ef | grep staprun | grep -v "grep" | awk '{ print $2; }'`
  if [ ! -z "$PIDS" ]
  then
    sudo kill -s SIGINT $PIDS
  fi
fi

if [ $# -eq 1 ]
then
  #insert new module
  echo "running new module ($1) with staprun..."
  sudo staprun $1 &
fi

