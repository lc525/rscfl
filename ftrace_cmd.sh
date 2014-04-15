#!/bin/bash
#
# Sample usage:
# $> sudo ./ftrace_cmd -c "ping -c 10 www.google.com" -o ping.tr
#
# This will run the ping process with ftrace enabled for its pid, and output
# the trace in ping.tr
#

DEBUGFS=`mount | grep debugfs | awk '{ print $3; }'`

function tf_write {
  echo $2 > $DEBUGFS/tracing/$1
}

function tf_read {
  cat $DEBUGFS/tracing/$1
}

function print_usage {
  echo "ftrace_cmd [OPTIONS] -c <COMMAND>"
  echo "OPTIONS:"
  echo "  -t <TRACER>: run ftrace with a particular tracer;"
  echo "               by default this is the function tracer"
  echo "  -o <FILE>: output file"
  echo "  -s : ask for confirmation before starting tracing"
}

########## ftrace script

TRACER=function
OUTPUT=trace
COMMAND="ls -l"
STOP=0

while getopts ":hc:t:o:s" opt; do
  case $opt in
    h)
      print_usage
      exit 0
      ;;
    c)
      COMMAND=$OPTARG
      ;;
    t)
      TRACER=$OPTARG
      ;;
    o)
      OUTPUT=$OPTARG
      ;;
    s)
      STOP=1
      ;;
    \?)
      echo "Invalid option -$OPTARG" >&2
      print_usage
      exit 1
      ;;
     :)
       echo "Option -$OPTARG requires an argument"
       print_usage
       exit 1
       ;;
   esac
 done

# you have to be root to be running this
if [ $EUID -ne 0 ]
then
 echo "Please run this as root."
 exit 1
fi
OLD_TRACER=`tf_read current_tracer`

eval "$COMMAND &"
cmd_pid=$!
kill -STOP $cmd_pid

tf_write current_tracer $TRACER
tf_write set_ftrace_pid $cmd_pid

if [ $STOP -eq 1 ]
then
  echo "Tracing ping process with PID=$cmd_pid. Press any key to start..."
  read -n 1 -s
fi

tf_write tracing_on 1
kill -CONT $cmd_pid
wait $cmd_pid

cat $DEBUGFS/tracing/trace > $OUTPUT

tf_write tracing_on 0
echo "Tracing DONE. Output written to $OUTPUT"
tf_write current_tracer $OLD_TRACER

