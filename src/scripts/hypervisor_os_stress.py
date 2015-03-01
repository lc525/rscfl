#! /usr/bin/env python2.7

import argparse
import fabric
import fabric.api
import jinja2
import subprocess

TEMPLATE="""
name = "ubuntu-clone-{{ clone_no }}"
memory = {{ memory }}
vcpus = {{ vcpus }}
disk = ['phy:/dev/rscfl_vg/ubuntu-clone-{{ clone_no }},xvda,w']
bootloader = "pygrub"
vif=[ 'mac=00:16:3f:00:00:{{ padded_clone_no }},bridge=xenbr0' ]
"""

def prepare_config(clone_no, memory, vcpus):
    f = open('/tmp/ubuntu-%d' % clone_no, 'w')
    template = jinja2.Template(TEMPLATE)

    clone_properties = {}
    clone_properties['clone_no'] = clone_no
    clone_properties['padded_clone_no'] = "%02d" % (clone_no + 1)
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


def start_vms(no_vms, gold_img, memory, vcpus):
    for i in xrange(no_vms):
        prepare_config(i, memory, vcpus)
        prepare_disk(i, gold_img)
    for i in xrange(no_vms):
        boot_clone(i)


def destroy_existing_doms():
    subprocess.Popen("sudo xl list | grep ubuntu-clone- | sed 's/\s\+/ /g' "
                     "| cut -d ' ' -f2 |"
                     " xargs -d '\n' --no-run-if-empty sudo xl destroy",
                     shell=True)


@fabric.api.parallel
def run_workload(workload_cmd):
    while True:
        try:
            fabric.api.run(workload_cmd)
            return
        except fabric.exceptions.NetworkError as e:
            pass


def run_experiment(no_vms, gold_img, memory, vcpus, workload_cmd):
    destroy_existing_doms()
    start_vms(no_vms, gold_img, memory, vcpus)
    ips = []
    for x in range(no_vms):
        ips.append("so-22-%d" % (x + 51))
    fabric.api.output["stdout"] = False

    fabric.api.execute(run_workload, workload_cmd, hosts=ips)


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
    args = parser.parse_args()
    run_experiment(args.no_vms, args.gold_img, args.memory, args.vcpus,
                   args.workload_cmd)

if __name__ == "__main__":
    main()
