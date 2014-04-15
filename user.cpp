 void on_cost_done(call_cost* cost, void* ctx){
  //read costs and take actions (aggregate, build histograms, etc)
 }

 int main(int argc, char** argv){
   init_resource_acct(); // called on each thread - does some memory allocation
                         // and calls an ioctl setting up resource accounting.
   int filter = SYS | PROC | NET_SOCK; // defines what resources we're
                                       // interested in, default: ALL (include
                                       // all resources)
   call_cost *cost_o, *cost_w;
   acct_next(&cost_o, filter);         // declares interest in measuring the
                                       // resource consumption of the next syscall
   int fd = open("/../file_path", O_CREAT); // measured syscall (cost_o)

   char buf[BUF_SIZE];
   int sz = read(fd, buf, BUF_SIZE);        // syscall not measured

   acct_next(&cost_w, filter);
   // register callback for when the async part of the cost is fully computed
   cost_callback_async(cost_w, on_cost_done, 0);
   int res = write(fd, &buf, BUF_SIZE);     // measured syscall (cost_w)

   // if the write is asynchronous, cost_w will keep being updated by the
   // kernel for a while

   // do whatever you want with the call_cost data. you can read the sync
   // component as soon as the syscall is done. You should not touch the async
   // component until the kernel has set the async_done flag to true.
   // you can either wait with
   //    wait_async_cost(call_cost*);
   // or register a callback function using
   //    cost_callback_async(call_cost*, callback, ctx); (as above)

   close_resource_acct(); // de-allocate resource accounting structures
 }
