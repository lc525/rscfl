# **rscfl**

### Status

We are working on improvements for making rscfl modular and simple to build on your system.
This would also allow you to use the fast probing mechanism (kamprobes) discussed in
our :login; article (https://www.usenix.org/publications/login/fall2017/carata)
independently of rscfl. Those interested may track https://github.com/lc525/kamprobes

We expect those changes to be pushed publicly by the end of October 2017.


### 1. Building from source

The build process for Resourceful involves generating a list of kernel probe
points. This is automated as long as you have access to a linux kernel source
tree and a vmlinux image with debug symbols (the default on Ubuntu systems).

Resourceful doesn't require you to _run_ a customized kernel, but the static
analysis step performed at build time for determining the probe points locations
requires access to the kernel source tree of the running kernel (having at least
the equivalent version of the vanilla kernel sources is highly recommended).

Before starting the build process, please set the following environment
variables:

* `RSCFL_LINUX_ROOT` - A linux source directory for subsystem identification
* `RSCFL_LINUX_VMLINUX` - The path towards a vmlinux that was built with `CONFIG_DEBUG_INFO=y`
* `RSCFL_LINUX_BUILD` - The linux directory in which vmlinux was originally built

The trickiest bit is setting the `RSCFL_LINUX_BUILD` variable. This is the path
which was used to build the vmlinux pointed to by `RSCFL_LINUX_VMLINUX`. If you're
working with a distribution's vmlinux, this can be difficult to find.

We'll automate the process, but until then you'll have to:
  - `objdump -d vmlinux` (CTRL-C after a short while to stop the printing)
  - pick a function boundary address or a callq addres from the dump
  - pass that address to addr2line and note the prefix before what would be
    the root of the source tree
  - set the `RSCFL_LINUX_BUILD` environment variable to that prefix

After setting the variables, enter the build directory and do (this will
be an out-of-source build). Please see ./.build -h for a full set of options.

```
  $ ./.build -cp
  $ sudo make install
```

The first build will take quite a long while (~5min) because of our one-time
static analysis step. This will be re-run only when a new kernel version is
detected by the build system.

Now, you can insert the kernel module and run some basic tests:

```
  $ modprobe rscfl_sys.ko
  $ make check
```

Stopping Resourceful is a two-step process:

```
  $ rscfl_stop
  $ modprobe -r rscfl_sys.ko
```
Some kernel `BUG_ON` messages might appear in dmesg, but they should be benign.

If needed you can also remove Resourceful from your system by running the
following command in the build directory. Please make sure you have stopped
Resourceful before this.

```
  $ sudo make uninstall
```
