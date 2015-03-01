#! /usr/bin/env python2.7

import argparse
import fabric
import fabric.api
import jinja2
import requests
import subprocess
import time

TEMPLATE="""
name = "ubuntu-clone-{{ clone_no }}"
memory = {{ memory }}
vcpus = {{ vcpus }}
disk = ['phy:/dev/rscfl_vg/ubuntu-clone-{{ clone_no }},xvda,w']
bootloader = "pygrub"
vif=[ 'mac=00:16:3f:00:00:{{ padded_clone_no }},bridge=xenbr0' ]
"""

target_vm = "so-22-50"

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
    subprocess.Popen("sudo xl list | grep ubuntu-clone- | sed 's/\s\+/ /g' "
                     "| cut -d ' ' -f2 |"
                     " xargs -d '\n' --no-run-if-empty sudo xl destroy",
                     shell=True)


@fabric.api.task
def run_workload(workload_cmd):
    while True:
        try:
            fabric.api.run("nohup bashc -c '%s' &" % workload_cmd)
            return
        except fabric.exceptions.NetworkError as e:
            pass


@fabric.api.parallel
def run_test_prog(test_prog):
    fabric.api.run("nohup bashc -c '%s'  &" % test_prog)


def run_experiment(no_vms, gold_img, memory, vcpus, workload_cmd, test_prog,
                   sleep_time):
    destroy_existing_doms()
    fabric.api.output["stdout"] = False
    fabric.api.output["running"] = False
    # Initialise program under test (eg lighttpd)
    fabric.api.execute(run_test_prog, test_prog, hosts=[target_vm])

    for x in range(no_vms):
        # Create x VMs
        workload_ip = start_vm(gold_img, memory, vcpus)
        # Make VMs run a workload
        fabric.api.execute(run_workload, workload_cmd, hosts=workload_ip)
        # Start measuring
        payload = {'mark': 'Bvms=%d' % x}
        requests.post('http://so-22-50/mark', payload)

        # Wait
        time.sleep(float(sleep_time))
        # Stop measuring
        payload = {'mark': 'STOP'}
        requests.post('http://so-22-50/mark', payload)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-g', type=int, dest="no_vms",
                        help="Number of concurrent VMs.")
    parser.add_argument('-i', dest="gold_img",
                        help="Location of gold image file.")
    parser.add_argument('-m', dest="memory", default="256",
                        help="Memory for each generator in MB.")
    parser.add_argument('-v', dest="vcpus", default="8",
                        help="VCPUs per generator")
    parser.add_argument('-c', dest="workload_cmd", help="Command to run on "
                        "each of the VMs.")
    parser.add_argument('-p', dest="test_prog", help="Program to run as a test"
                        "eg lighttpd.")
    parser.add_argument('-s', dest="sleep_time", help="Time to let each trial "
                        "run for before increasing laod.")
    args = parser.parse_args()
    run_experiment(args.no_vms, args.gold_img, args.memory, args.vcpus,
                   args.workload_cmd, args.test_prog, args.sleep_time)

if __name__ == "__main__":
    main()
