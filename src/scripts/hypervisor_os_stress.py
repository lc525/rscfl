#! /usr/bin/env python2.7

import argparse
import fabric
import fabric.api
import jinja2
import requests
import subprocess
import time
import sys

TEMPLATE="""
name = "ubuntu-clone-{{ clone_no }}"
memory = {{ memory }}
vcpus = {{ vcpus }}
disk = ['phy:/dev/rscfl_vg/ubuntu-clone-{{ clone_no }},xvda,w']
bootloader = "pygrub"
vif=[ 'mac=00:16:3f:00:00:{{ padded_clone_no }},bridge=xenbr0' ]
"""

target_vm = "so-22-50.dtg.cl.cam.ac.uk"

current_no_vms = 0

def prepare_config(clone_no, memory, vcpus):
    f = open('/tmp/ubuntu-%d' % clone_no, 'w')
    template = jinja2.Template(TEMPLATE)

    clone_properties = {}
    clone_properties['clone_no'] = clone_no
    clone_properties['padded_clone_no'] = "%02d" % (clone_no)
    clone_properties['memory'] = memory
    clone_properties['vcpus'] = vcpus

    f.write(template.render(clone_properties))
    f.close()


def prepare_disk(clone_no, gold_img):
    # Remove old disk.
    subprocess.call(['sudo', 'lvremove', '-f',
                     '/dev/rscfl_vg/ubuntu-clone-%d' % clone_no])
    # Create the new disk.
    subprocess.call(['sudo', 'lvcreate', '-L1G', '-s', '-n', 'ubuntu-clone-%d' %
                     clone_no, gold_img])


def boot_clone(clone_no):
    subprocess.call(['sudo', 'xl', 'create', '/tmp/ubuntu-%d' % clone_no])


def start_vm(gold_img, memory, vcpus):
    global current_no_vms
    current_no_vms += 1
    prepare_config(current_no_vms, memory, vcpus)
    prepare_disk(current_no_vms, gold_img)
    boot_clone(current_no_vms)
    return "128.232.22.%d" % (current_no_vms + 50)

def destroy_existing_doms():
    subprocess.Popen("for x in $(sudo xl list | grep ubuntu-clone- |"
                     " sed 's/\s\+/ /g'| "
                     " cut -d ' ' -f2 ); do sudo xl destroy $x; done",
                     shell=True)


@fabric.api.task
def run_workload(workload_cmd):
    while True:
        try:
            fabric.api.run("set -m; %s &" % workload_cmd, pty=True)
            return
        except fabric.exceptions.NetworkError as e:
            pass


@fabric.api.task
def run_test_prog(test_prog):
    return fabric.api.run("set -m; %s &" % test_prog, pty=True)


def run_experiment(no_vms, gold_img, memory, vcpus, workload_cmd, workload_cmd_freq,
                   test_prog, sleep_time, mark, from_vms):
    destroy_existing_doms()
    fabric.api.output["stdout"] = False
    fabric.api.output["running"] = False
    # Initialise program under test (eg lighttpd)
    res = fabric.api.execute(run_test_prog, test_prog, hosts=[target_vm])
    if res[target_vm] == None:
        print("Error executing test_prog")
        return

    cmd_ix = 0
    for x in range(1, no_vms + 1):
        # Create x VMs
        workload_ip = start_vm(gold_img, memory, vcpus)
        # Make VMs run a workload
        fabric.api.execute(run_workload, workload_cmd[cmd_ix], hosts=workload_ip)
        workload_cmd_freq[cmd_ix] = workload_cmd_freq[cmd_ix] - 1

        if x < from_vms:
            continue
        if x == from_vms:
            sys.stdout.write("Experiment starts in 5 sec....")
            sys.stdout.flush()
            time.sleep(5.0)

        # Start measuring
        payload = {'mark': 'vms_%d_%s' % (x, mark[cmd_ix])}
        requests.post('http://so-22-50/mark', payload)
        if x == from_vms:
            print("started!\n")

        if x <= no_vms:
          # Wait
          time.sleep(float(sleep_time))
          # Stop measuring
          payload = {'mark': 'STOP'}
          requests.post('http://so-22-50/mark', payload)

        if workload_cmd_freq[cmd_ix] == 0:
         cmd_ix = cmd_ix + 1


def main():
    fabric.api.env.use_ssh_config = True
    parser = argparse.ArgumentParser()
    parser.add_argument('-g', type=int, dest="no_vms",
                        help="Number of concurrent VMs.")
    parser.add_argument('-w', type=int, dest="from_vms", default="1",
                        help="Start with this many vms")
    parser.add_argument('-i', dest="gold_img",
                        help="Location of gold image file.")
    parser.add_argument('-m', dest="memory", default="256",
                        help="Memory for each generator in MB.")
    parser.add_argument('-v', dest="vcpus", default="8",
                        help="VCPUs per generator")
    parser.add_argument('-c', nargs='+', dest="workload_cmd",
                        help="Commands to run on each of the VMs. Use multiple"
                        "commands in \"\" in combination with -f and -e")
    parser.add_argument('-f', type=int, nargs='+', dest="workload_cmd_freq",
                        help="For each of the commands in -c, the number of vms"
                        " on which that command should run")
    parser.add_argument('-p', dest="test_prog", help="Program to run as a test"
                        "eg lighttpd.")
    parser.add_argument('-s', dest="sleep_time", help="Time to let each trial "
                        "run for before increasing load.")
    parser.add_argument('-e', nargs='+', dest="mark",
                        help="Extra metadata for experiment, one value per"
                        " command in -c")
    args = parser.parse_args()

    if args.workload_cmd_freq == None:
	args.workload_cmd_freq = [args.no_vms]

    run_experiment(args.no_vms, args.gold_img, args.memory, args.vcpus,
                   args.workload_cmd, args.workload_cmd_freq, args.test_prog,
                   args.sleep_time, args.mark, args.from_vms)

if __name__ == "__main__":
    main()
