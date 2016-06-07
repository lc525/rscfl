/*
 * This is a documented example of how to use the rscfl API.
 * It is focused on covering most of the features of the high-level API.
 */

#include <stdio.h>

#include <rscfl/user/res_api.h>

int main(int argc, char** argv) {
  /*
   * Step 1: Initialising the API.
   *
   * This creates a shared memory region between this program and the rscfl
   * kernel module, from where accounting data can be read and where different
   * control options can be set. Call rscfl_init(..) once per thread.
   *
   * After rscfl_init(), applications can use rscfl_get_handle() to get to the
   * rscfl handle of the current thread. If executed before rscfl_init(),
   * rscfl_get_handle() will perform the initialisation with a default
   * configuration (self-monitoring).
   */

  rscfl_handle rhdl_;

  // To monitor another process, do:
  // rscfl_config cfg { .monitored_pid = 2345 };
  // rhdl_ = rscfl_init(cfg)
  //
  // We do self-monitoring here (no config argument needed for rscfl_init):
  rhdl_ = rscfl_init();
  if(rhdl_ == NULL) {
    fprintf(stderr,
            "Unable to talk to rscfl kernel module.\n"
            "Check that:\n"
            "\t - rscfl is loaded\n"
            "\t - you have R permissions on /dev/rscfl-data\n"
            "\t - you have RW permissions on /dev/rscfl-ctrl\n");
    return 1;
  }


  /*
   * High level summary about resourceful collected data:
   *
   * Resourceful collects per-system call resource consumption information.
   * Such information comes back from the rscfl kernel module in 3 connected
   * data structures: struct accounting, subsys_accounting and subsys_idx_set.
   *
   * One measurement implies getting back one struct accounting and
   * a (variable) number of struct subsys_accounting, indexed for simple access
   * by subsys_idx_set.
   *
   * Every time the API reads accounting data for a system call, it will get
   * back an accounting data structure. The pair (rscfl_handle, accounting)
   * uniquely identifies _one_ measurement. However, in itself, struct
   * accounting is just a handle: it contains very little useful data (for
   * example, the rc member variable will show whether any errors were
   * encountered kernel-side for that measurement, and nr_subsystems will
   * let the application know how many kernel subsystems were touched by the
   * measured system call).
   *
   * Besides those, one should not work with other members of
   * struct accounting directly. Instead, the high-level API contains functions
   * that will extract the actual measurements identified by one struct
   * accounting.
   *
   * The data structure containing the actual measurements inside one kernel
   * subsystem (Network, VFS, Block devices, etc) is subsys_accounting. One
   * system call can touch multiple such subsystems during its execution. The
   * set of those per-subsystem measurements is indexed in a subsys_idx_set
   * structure, obtained by calling rscfl_get_subsys(rscfl_handle, accounting).
   *
   * Multiple measurements can be aggregated within the same subsys_idx_set
   * index: this is very much a set union operation: if a new measurement
   * contains data for subsystems which aren't already present in the
   * subsys_idx_set those will be added to the index. For subsystems
   * that are already present in the index,
   */

  /*
   * Step 2 (optional): Define an aggegator
   *
   * This one is large enough to aggregate data across multiple system calls,
   * for 7 subsystems.
   *
   * The application _owns_ the data in the subsys_idx_set pointer that is
   * returned, and will need to free it using free_subsys_idx_set(agg)
   */
  subsys_idx_set* agg = rscfl_get_new_aggregator(7);

  /*
   * Step 3: Accounting for resources consumed by system calls
   */
  int err;
  struct accounting acct;

  // declare interest in accounting the next system call
  err = rscfl_acct_next(rhdl_);
  if(err) fprintf(stderr, "Error accounting for system call [interest]\n");
  int fp = fopen("rscfl_file", "w");

  err = rscfl_read_acct(rhdl_, &acct);
  /*
   * Not doing a rscfl_read_acct after the system call was performed will
   * result in data about the accounting (accounting, subsys_accounting) to be
   * kept on the kernel side. However, the ammount stored on the kernel side
   * is limited (by compile-time selected options in res_common.h).
   *
   * Depending on the frequency of other accounted syscalls that happen before
   * the rscfl_read_acct, the kernel buffers storing the accounting data can get
   * full, and rscfl will start discarding data (errors are written to dmesg).
   */
  if(err) fprintf(stderr, "Error accounting for system call [data read]\n");

  /* when using aggregators:
   *
   * This is a zero-copy operation for subsystems that already have data
   * stored within the aggregator (a simple merge is performed). For new
   * subsystems, memory is allocated and a memcpy from kernel memory is done.
   */
  rscfl_merge_acct_into(rhdl_, &acct, agg);

  /* otherwise (no aggregator), just need subsystem data for this acct:
   *
   * subsys_idx_set* adata = rscfl
   * adata = rscfl_get_subsys(rhdl_, &acct);
   * ...
   * free_subsys_idx_set(adata);
   *
   */

  // same as above for recording the resources consumed by fclose()
  err = rscfl_acct_next(rhdl_);
  if(err) fprintf(stderr, "Error accounting for system call [interest]\n");
  fclose(fp);
  err = rscfl_read_acct(rhdl_, &acct);
  if(err) fprintf(stderr, "Error accounting for system call [data read]\n");
  rscfl_merge_acct_into(rhdl_, &acct, agg);



  /*
   * Step 4: iterating through results:
   */

   // (a) getting results for a particular subsystem
   int set_ix = agg->idx[FILESYSTEMSVFSANDINFRASTRUCTURE];
   struct subsys_accounting sa = agg->set[set_ix];
    // all relevant data within sa:
    //  - sa.cpu.cycles               : cycles spent in subsystem
    //  - sa.cpu.wall_clock_time      : wct spent in subsystem
    //  - sa.sched.cycles_out_local   : cycles spent scheduled-out
    //  - sa.sched.wct_out_local      : wct spent scheduled-out
    //  ...
    //  The ... represent the other members of sa. At the moment we're not
    //  actually recording any of those extra members (except for extra data
    //  when running on top of XEN). This is to limit probe effects and not
    //  a fundamental limitation

   // (b) iterating through all subsystems of a subsys_idx_set:
   for(int i = 0; i < agg->set_size; i++) {
     // use the reverse index ids to find out what subsystem we're looking at
     // in agg->set[i]:
     short subsys_id = subsys->ids[i];
     // the user-readable subsystem name can be obtained by accessing
     // rscfl_subsys_name[subsys_id]
     struct subsys_accounting sa_id = agg->set[i];
     // all relevant data within sa_id (see comment in section 4(a) above)
   }



  /*
   * Final step: Free resources
   */
  free_subsys_idx_set(agg);
}
