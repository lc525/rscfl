#!/bin/bash
MODNAME=`lsmod | grep rscfl | awk '{ print $1; }'`

function print_usage {
  echo "$0 [rscfl_new_ko]"
  echo ""
  echo "- rmmods any rscfl_*.ko kernel module (killing applications that use it)"
  echo "- when given a kernel module as a parameter, it inserts it into the"
  echo "  kernel using staprun"
}

while getopts ":h" opt; do
  case $opt in
    h)
      print_usage
      exit 0
      ;;
  esac
done

# if an existing module is already loaded, remove it from the kernel
if [ ! -z "$MODNAME" ]
then
  scripts/rscfl_stop
  if [ $? -eq 0 ]
  then
    echo "Stop rscfl OK, cool off period...(30s)"
  fi
  sleep 15
  #existing processes using rscfl*.ko
  echo "Removing modules with name rscfl*.ko..."
  sudo rmmod $MODNAME
fi

if [ $# -eq 1 ]
then
  #insert new module
  echo "running new module ($1)..."
  sudo insmod $1 &
fi

