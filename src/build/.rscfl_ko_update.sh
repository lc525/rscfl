#!/bin/bash

function print_usage {
  echo "$0 <rscfl_new_ko>"
  echo ""
  echo "removes any running rscfl_*.ko (killing any applications that"
  echo "use it), and inserts the new module into the kernel"
}

if [ $# -lt 1 ]
then
  print_usage
  exit 1
fi
MODNAME=`lsmod | grep rscfl | awk '{ print $1; }'`

if [ ! -z "$MODNAME" ]
then
  #existing processes using rscfl*.ko
  echo "killing processes dealing with rscfl*.ko..."
  PIDS=`ps -ef | grep rscfl_*.ko | grep -v "grep\|rscfl_ko_update" | awk '{ print $2; }'`
  if [ ! -z "$PIDS" ]
  then
    sudo kill -9 $PIDS
  fi

  #rmmod module
  echo "removing old module ($MODNAME)..."
  sudo rmmod $MODNAME
fi

#insert new module
echo "running new module ($1) with staprun..."
sudo staprun $1 &
