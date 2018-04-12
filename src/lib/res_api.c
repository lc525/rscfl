/**** Notice
 * res_api.c: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#include "rscfl/user/res_api.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/types.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/cJSON.h"


// macro function definitions
#define max(a,b)                                \
  ({ typeof (a) _a = (a);                       \
    typeof (b) _b = (b);                        \
    _a > _b ? _a : _b; })

#define min(a,b)                                \
  ({ typeof (a) _a = (a);                       \
    typeof (b) _b = (b);                        \
    _a > _b ? _b : _a; })

DEFINE_REDUCE_FUNCTION(rint, ru64)
DEFINE_REDUCE_FUNCTION(wc, struct timespec)

#define EQUAL(A, B) strncmp(A, B, strlen(B)) == 0
#define EXTRACT_METRIC(metric_name, metric_value, modifier)                  \
  snprintf(metric, METRIC_BUFFER_SIZE,                                       \
           "%s,measurement_id=%llu,subsystem=%s value=" modifier " %llu\n",  \
           metric_name, timestamp, subsystem_name, metric_value, timestamp); \
  strncat(subsystem_metrics, metric, METRIC_BUFFER_SIZE);                    \
  memset(metric, 0, METRIC_BUFFER_SIZE);

// constant definitions
#define METRIC_BUFFER_SIZE             256
#define SUBSYSTEM_METRICS_BUFFER_SIZE  4*METRIC_BUFFER_SIZE   // should be (number of metrics in subsys_accounting)*METRIC_BUFFER_SIZE
#define MEASUREMENTS_BUFFER_SIZE       65536

/*
 * Static function declarations
 */

static int open_file(char *db_name, char *app_name, char *extension);
static int open_influxDB_file(char *app_name);
static int open_mongoDB_file(char *app_name);
static CURL *connect_to_influxDB(void);
static bool connect_to_mongoDB(char *app_name, mongo_handle_t *mongo);
static int store_measurements(rscfl_handle rhdl, char *measurements);
static int store_extra_data(rscfl_handle rhdl, char *json);
static char *escape_spaces(const char *string);
static size_t data_read_callback(void *contents, size_t size, size_t nmemb, void *userp);
static void *influxDB_sender(void *param);
static void *mongoDB_sender(void *param);
static void influxDB_cleanup(rscfl_handle rhdl);
static void mongoDB_cleanup(rscfl_handle rhdl);
static char *build_advanced_query(rscfl_handle rhdl, char *measurement_name,
                                  char *function, char *subsystem_name,
                                  int latest_n,
                                  unsigned long long time_since_us,
                                  unsigned long long time_until_us,
                                  timestamp_array_t *timestamps);

// private struct definitions
struct String{
  char *ptr;
  size_t length;
};

struct influxDB_data{
  subsys_idx_set *data;
  unsigned long long timestamp;
  void (*function)(rscfl_handle, void *);
  void *params;
};

// define subsystem name array for user-space includes of subsys_list.h
const char *rscfl_subsys_name[NUM_SUBSYSTEMS] = {
    SUBSYS_TABLE(SUBSYS_AS_STR_ARRAY)
};

__thread rscfl_handle handle = NULL;

#ifdef RSCFL_BENCH
syscall_interest_t dummy_interest;
#endif

rscfl_handle rscfl_init_api(rscfl_version_t rscfl_ver, rscfl_config* config, char *app_name, bool need_extra_data)
{
  struct stat sb;
  void *ctrl, *buf;
  int fd_data, fd_ctrl;
  struct accounting acct;

  // library was compiled with RSCFL_VERSION, API called from rscfl_ver
  // emit warning if the APIs have different major versions
  if(RSCFL_VERSION.major != rscfl_ver.major) {
    fprintf(stderr, "rscfl: API major version mismatch: "
                    "%d (header) vs %d (library)\n",
                    rscfl_ver.major, RSCFL_VERSION.major);
    #ifdef RSCFL_ERR_VER_MISMATCH
      fprintf(stderr, "rscfl: initialisation aborted\n");
      return NULL;
    #endif
    // if ERROR_ON_VERSION_MISMATCH is not defined, we'll still try to
    // initialize rscfl
  }

  fd_data = open("/dev/" RSCFL_DATA_DRIVER, O_RDWR | O_DSYNC);
  fd_ctrl = open("/dev/" RSCFL_CTRL_DRIVER, O_RDWR | O_DSYNC);
  rscfl_handle rhdl = (rscfl_handle)calloc(1, sizeof(*rhdl));
  if (!rhdl) {
    fprintf(stderr, "Unable to allocate memory for rscfl handle\n");
    return NULL;
  }

  /* Import data from any existing files to the databases */
  // system("/home/branislavuhrin/RMF/source/InfluxDB_import");
  // system("/home/branislavuhrin/RMF/source/MongoDB_import");
  int err;
  rhdl->influx.curl = NULL;
  rhdl->influx.fd = -1;
  rhdl->influx.pipe_read = -1;
  rhdl->influx.pipe_write = -1;

  rhdl->mongo.connected = false;
  rhdl->mongo.fd = -1;
  rhdl->mongo.pipe_read = -1;
  rhdl->mongo.pipe_write = -1;

  if (app_name != NULL && strlen(app_name) < 32 && strncpy(rhdl->app_name, app_name, sizeof(rhdl->app_name)) != NULL){
    // app_name is ok, we can try to connect to InfluxDB
    rhdl->influx.curl = connect_to_influxDB();
    if (rhdl->influx.curl == NULL){
      // we coudln't connect to the database, let's try to open a file for writing the data instead
      printf("Couldn't connect to influxDB, opening a file for writing\n");
      rhdl->influx.fd = open_influxDB_file(app_name);
    }
    if (rhdl->influx.curl == NULL && rhdl->influx.fd == -1){
      // if we couldn't connect or open a file then print an error and move on.
      memset(rhdl->app_name, 0, 32);
      fprintf(stderr, "Couldn't connect to influxDB or open a file for writing. Persistent storage is not supported.\n");
    } else {
      // influxDB storage has been initialised, start influxDB_sender thread. First, open a pipe
      int influx_pipe[2];
      err = pipe(influx_pipe);
      if (err == 0){
        // the pipe was successfully opened, store the fd's in rhdl and start the thread
        rhdl->influx.pipe_read = influx_pipe[0];
        rhdl->influx.pipe_write = influx_pipe[1];
        pthread_t influx_thread;
        err = pthread_create(&influx_thread, NULL, influxDB_sender, rhdl);
        if (err == 0){
          // the thread was successfully created, store that in rhdl
          rhdl->influx.sender_thread = influx_thread;
          rhdl->influx.thread_alive = true;
        } else {
          // thread failed to start, close pipe.
          close(rhdl->influx.pipe_read);
          rhdl->influx.pipe_read = -1;
          close(rhdl->influx.pipe_write);
          rhdl->influx.pipe_write = -1;
        }
      }
      if (err){
        // pipe failed to open or thread failed to start, cleanup
        memset(rhdl->app_name, 0, 32);
        rhdl->influx.thread_alive = false;
        influxDB_cleanup(rhdl);
        fprintf(stderr, "InfluxDB thread or pipe failed to initialise. Persistent storage is not supported.\n");
      } else if (need_extra_data){
        // InfluxDB is fully initialised, we can start initialising mongoDB
        rhdl->mongo.connected = connect_to_mongoDB(app_name, &rhdl->mongo);
        if (rhdl->mongo.connected == false){
          // we coudln't connect to the database, let's try to open a file for writing the data instead
          printf("Couldn't connect to mongoDB, opening a file for writing\n");
          rhdl->mongo.fd = open_mongoDB_file(app_name);
        }
        if (rhdl->mongo.connected == false && rhdl->mongo.fd == -1){
          // if we couldn't connect or open a file then print an error and move on.
          fprintf(stderr, "Couldn't connect to mongoDB or open a file for writing. Storing extra data is not supported.\n");
        } else {
          // mongoDB storage has been initialised, start mongoDB_sender thread. First, open a pipe
          int mongo_pipe[2];
          err = pipe(mongo_pipe);
          if (err == 0){
            // the pipe was successfully opened, store the fd's in rhdl and start the thread
            rhdl->mongo.pipe_read = mongo_pipe[0];
            rhdl->mongo.pipe_write = mongo_pipe[1];
            pthread_t mongo_thread;
            err = pthread_create(&mongo_thread, NULL, mongoDB_sender, rhdl);
            if (err == 0){
              // the thread was successfully created, store that in rhdl
              rhdl->mongo.sender_thread = mongo_thread;
              rhdl->mongo.thread_alive = true;
            } else {
              // thread failed to start, close pipe.
              close(rhdl->mongo.pipe_read);
              rhdl->mongo.pipe_read = -1;
              close(rhdl->mongo.pipe_write);
              rhdl->mongo.pipe_write = -1;
            }
          }
          if (err){
            // pipe failed to open or thread failed to start, cleanup
            rhdl->mongo.thread_alive = false;
            mongoDB_cleanup(rhdl);
            fprintf(stderr, "MongoDB thread or pipe failed to initialise. Storing extra data is not supported.\n");
          }
        }
      }
    }
  } else {
    // if app_name is too long or can't be copied.
    memset(rhdl->app_name, 0, 32);
    printf("Persistent storage is disabled.\n");
  }

  if ((fd_data == -1) || (fd_ctrl == -1)) {
    fprintf(stderr, "rscfl:Unable to access data or ctrl devices\n");
    goto error;
  }
  rhdl->fd_ctrl = fd_ctrl;

  if(config != NULL) {
    ioctl(rhdl->fd_ctrl, RSCFL_CONFIG_CMD, config);
  }

  // mmap memory to store our struct accountings, and struct subsys_accountings

  // note: this (data) mmap needs to happen _before_ the ctrl mmap because the
  // rscfl_data character device also does the initialisation of per-cpu
  // variables later used by rscfl_ctrl.
  buf = mmap(NULL, MMAP_BUF_SIZE, PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_POPULATE, fd_data, 0);
  if (buf == MAP_FAILED) {
    fprintf(stderr,
	    "rscfl: Unable to mmap shared memory with kernel module for data.\n");
    goto error;
  }
  rhdl->buf = buf;

  // mmap memory to store our interests.
  ctrl = mmap(NULL, MMAP_CTL_SIZE, PROT_READ | PROT_WRITE,
              MAP_SHARED | MAP_POPULATE, fd_ctrl, 0);
  if (ctrl == MAP_FAILED) {
    fprintf(stderr,
	    "rscfl: Unable to mmap shared memory for storing interests\n");
    goto error;
  }

  // Initialise pointer so that we write our interests into the mmap-ed region
  // of memory, so resourceful can read them.
  rhdl->ctrl = ctrl;

  // Check data layout version
  if (rhdl->ctrl->version != rscfl_ver.data_layout) {
    fprintf(stderr,
            "rscfl: Version mismatch between rscfl API and kernel data layouts: %d (API) vs %d (.ko)\n",
            rscfl_ver.data_layout, rhdl->ctrl->version);
    goto error;
  }

  rhdl->lst_syscall_id = RSCFL_SYSCALL_ID_OFFSET;
  handle = rhdl;
  return rhdl;

error:
  if (rhdl != NULL) {
    if (rhdl->buf != NULL) {
      munmap(rhdl->buf, MMAP_BUF_SIZE);
    }
    if (rhdl->ctrl != NULL) {
      munmap(rhdl->ctrl, MMAP_BUF_SIZE);
    }
    free(rhdl);
  }
  return NULL;
}

void rscfl_persistent_storage_cleanup(rscfl_handle rhdl)
{
  influxDB_cleanup(rhdl);
  mongoDB_cleanup(rhdl);
}

rscfl_handle rscfl_get_handle_api(rscfl_config *cfg, char *app_name, bool need_extra_data)
{
  if (handle == NULL) {
    handle = rscfl_init(cfg, app_name, need_extra_data);
  }
  return handle;
}

int rscfl_get_token(rscfl_handle rhdl, rscfl_token **token)
{
  _Bool consume_ctrl = 0;
  rscfl_token_list *token_list_hd, *start;
  if ((rhdl == NULL) || (token == NULL)) {
    return -EINVAL;
  }
  // First see if there are any available tokens in the free list
  if(rhdl->free_token_list != NULL) {
    token_list_hd = rhdl->free_token_list;
    *token = token_list_hd->token;
    (*token)->first_acct = 1;
    (*token)->in_use = 1;
    (*token)->data_read = 0;
    rhdl->free_token_list = token_list_hd->next;
    free(token_list_hd);
    //printf("Get token %d\n", (*token)->id);
    return 0;
  } else if(rhdl->ctrl->num_avail_token_ids > 0){
    // consume tokens registered by the kernel with the rscfl_ctrl device
    consume_ctrl = 1;
  } else {
    // explicitly request from the rscfl kernel module some more tokens
    int rc;
    rc = ioctl(rhdl->fd_ctrl, RSCFL_NEW_TOKENS_CMD);
    if(rc != 0)
      return -EAGAIN;
    else
      consume_ctrl = 1;
  }

  // the kernel has placed available token ids in rhdl->ctrl->avail_token_ids
  // effectively, the kernel promisses not to use those ids for any other
  // resource accounting activities.
  //
  // create user-space tokens for those kernel-side ids and add them to a free
  // token list
  //
  // skip adding the first of the available kernel tokens to the free list;
  // instead, return it as the out-argument of this function (**token)
  if(consume_ctrl && rhdl->ctrl->num_avail_token_ids > 0) {
    int i;
    for(i = 0; i<rhdl->ctrl->num_avail_token_ids; i++) {
      rscfl_token *new_token = (rscfl_token *)malloc(sizeof(rscfl_token));
      new_token->id = rhdl->ctrl->avail_token_ids[i];
      new_token->first_acct = 1;
      rhdl->ctrl->avail_token_ids[i] = DEFAULT_TOKEN;
      if(i == 0) {
        *token = new_token;
        (*token)->in_use = 1;
        (*token)->data_read = 0;
        //printf("Get token %d\n", (*token)->id);
      } else {
        new_token->in_use = 0;
        rscfl_token_list *new_free_token = (rscfl_token_list *)malloc(sizeof(rscfl_token_list));
        new_free_token->token = new_token;
        new_free_token->next = rhdl->free_token_list;
        rhdl->free_token_list = new_free_token;
      }
    }
    rhdl->ctrl->num_avail_token_ids = 0;
  } else {
    return -EAGAIN;
  }

  return 0;
}

int rscfl_switch_token_api(rscfl_handle rhdl, rscfl_token *token_to, token_switch_flags flags){
  unsigned short new_id;
  volatile syscall_interest_t *interest;
  //rscfl_debug dbg;
  if (rhdl == NULL) {
    return -EINVAL;
  }
  if(token_to == NULL) {
    if((flags & SW_TK_NULL) != 0)
      new_id = NULL_TOKEN;
    else
      new_id = DEFAULT_TOKEN;
  } else {
    new_id = token_to->id;
  }
  interest = &rhdl->ctrl->interest;

  // no syscalls were executed for the previous token
  if(interest->first_measurement && rhdl->current_token != NULL) {
    rhdl->current_token->first_acct = 1;
  }

  // on setting first_measurement, the kernel side will clear any allocated
  // memory for the given token, but will not free subsystem data.
  // therefore, if rscfl_get_subsys was not called, you will leak subsystem
  // memory
  if((flags & SW_TK_RESET) != 0)
    interest->first_measurement = 1;
  else if (token_to != NULL) {
    interest->first_measurement = token_to->first_acct;
    token_to->first_acct = 0;
  }

  if(new_id == DEFAULT_TOKEN)
    interest->first_measurement = 0;

  if(interest->token_id != new_id){
    interest->token_id = new_id;
    rhdl->current_token = token_to;
    /*
     *printf("token switch from: %d to %d\n", interest->token_id, new_id);
     *msync(rhdl->ctrl, PAGE_SIZE, MS_SYNC);
     *strncpy(dbg.msg, "TKSW", 5);
     *dbg.new_token_id = new_id;
     *ioctl(rhdl->fd_ctrl, RSCFL_DEBUG_CMD, &dbg);
     *rhdl->ctrl->interest.token_swapped = 1;
     */
    return 0;
  } else {
    return 1; // no switch necessary, token_to already active
  }
}

int rscfl_free_token(rscfl_handle rhdl, rscfl_token *token)
{
  rscfl_debug dbg;
  printf("Free for token %d, in_read: %d\n", token->id, token->in_use);
  if(token->in_use) {
    rscfl_token_list *new_hd;
    if ((rhdl == NULL) || (token == NULL)) {
      return -EINVAL;
    }
    if (!token->data_read) {
      struct accounting tmp_acct;
      if(rscfl_read_acct(rhdl, &tmp_acct, token) == 0) {
        rscfl_subsys_free(rhdl, &tmp_acct);
      }
    }
    new_hd = (rscfl_token_list *)malloc(sizeof(rscfl_token_list));
    if (new_hd == NULL) {
      return -ENOMEM;
    }
    new_hd->next = rhdl->free_token_list;
    new_hd->token = token;
    token->first_acct = 1;
    token->in_use = 0;
    rhdl->free_token_list = new_hd;

    strncpy(dbg.msg, "FREE", 5);
    dbg.new_token_id = token->id;
    ioctl(rhdl->fd_ctrl, RSCFL_DEBUG_CMD, &dbg);
    return 0;
  }
  return 0;
}

int rscfl_acct_api(rscfl_handle rhdl, rscfl_token *token, interest_flags fl)
{
  volatile syscall_interest_t *to_acct;
  //int old_token_id;
  rscfl_debug dbg;
  _Bool rst;
  if (rhdl == NULL) {
    return -EINVAL;
  }
#ifdef RSCFL_BENCH
  if(likely((fl & ACCT_KNOP) == 0))
    to_acct = &rhdl->ctrl->interest;
  else
    to_acct = &dummy_interest;
#else
  to_acct = &rhdl->ctrl->interest;
#endif
  //old_token_id = to_acct->token_id;

  // Test if this should reset current persistent flags and make tokens behave as if
  // it's the first measurement. Maintain __ACCT_ERR if this has been set
  // kernel-side
  rst = ((fl & TK_RESET_FL) != 0 || (fl & ACCT_START) != 0);
  if(rst)
    to_acct->flags = (to_acct->flags & __ACCT_ERR) | (fl & __ACCT_FLAG_IS_PERSISTENT);
  else
    to_acct->flags |= (fl & __ACCT_FLAG_IS_PERSISTENT);

  // stop on kernel-side error
  if((to_acct->flags & __ACCT_ERR) != 0) {
    to_acct->syscall_id = 0;
    return -ENODATA;
  }

  // if passed TK_STOP_FL, switch to the null TOKEN. TK_STOP_FL only makes sense
  // in combination with start/stop and kernel-side aggregation.
  if((fl & TK_STOP_FL) != 0) {
    to_acct->token_id = NULL_TOKEN;
    to_acct->first_measurement = rst;
    to_acct->syscall_id = ID_RSCFL_IGNORE;
    return 0;
  }

  // deal with tokens
  if (token != NULL) {
    to_acct->token_id = token->id;
    token->first_acct = 0;
  } else {
    to_acct->token_id = DEFAULT_TOKEN;
  }
  if((fl & ACCT_NEXT_FL) != 0) {
    to_acct->first_measurement = 1;
  } else {
    to_acct->first_measurement = rst;
  }

  if((to_acct->flags & ACCT_STOP) != 0) {
    to_acct->token_id = NULL_TOKEN; // need this to signal the scheduler
                                    // interposition not to record further data
    to_acct->syscall_id = 0;
    to_acct->flags = ACCT_DEFAULT;
    return 0;
  } else if((to_acct->flags & ACCT_START) != 0) {
    to_acct->syscall_id = ID_RSCFL_IGNORE;
  } else {
    to_acct->syscall_id = ++rhdl->lst_syscall_id;
  }
  rhdl->current_token = token;

  strncpy(dbg.msg, "ACCT", 5);
  dbg.new_token_id = token->id;
  ioctl(rhdl->fd_ctrl, RSCFL_DEBUG_CMD, &dbg);
  return 0;
}

int rscfl_read_acct_api(rscfl_handle rhdl, struct accounting *acct, rscfl_token *token)
{
  int i = 0;
  unsigned short tk_id;
  rscfl_token_list *start;
  rscfl_debug dbg;
  if (rhdl == NULL || (rhdl->ctrl->interest.flags & __ACCT_ERR) != 0) {
    return -EINVAL;
  }

  if(token == NULL) {
    tk_id = rhdl->ctrl->interest.token_id;
  }
  else {
    tk_id = token->id;
    token->data_read = 1;
  }

  //printf("Read for token %d\n", token->id);
  struct accounting *shared_acct = (struct accounting *)rhdl->buf;
  if (shared_acct != NULL) {
    while (i < STRUCT_ACCT_NUM) {
      /*
       *printf("** acct use:%d, syscall:%lu, tk_id:%d, subsys_nr:%d\n",
       *       shared_acct->in_use, shared_acct->syscall_id, shared_acct->token_id, shared_acct->nr_subsystems);
       */
      if (shared_acct->in_use == 1) {
        if ((shared_acct->syscall_id == rhdl->lst_syscall_id) ||
            ((shared_acct->syscall_id == ID_RSCFL_IGNORE) && (shared_acct->token_id == tk_id))) {
          memcpy(acct, shared_acct, sizeof(struct accounting));
          shared_acct->in_use = 0;
          strncpy(dbg.msg, "READ", 5);
          dbg.new_token_id = tk_id;
          ioctl(rhdl->fd_ctrl, RSCFL_DEBUG_CMD, &dbg);
          return shared_acct->rc;
        } else {
          shared_acct++;
          i++;
        }
      } else {
        shared_acct++;
        i++;
      }
    }
  } else {
    return -EINVAL;
  }

#ifndef NDEBUG
  // We have failed in finding the correct kernel-side struct accounting
  // loop again for debug purposes:
  shared_acct = (struct accounting *)rhdl->buf;
  i = 0;
  printf("Was looking for token: %d\n", tk_id);

  strncpy(dbg.msg, "RERR", 5);
  dbg.new_token_id = tk_id;
  ioctl(rhdl->fd_ctrl, RSCFL_DEBUG_CMD, &dbg);
  while (i < STRUCT_ACCT_NUM) {
    printf("acct use:%d, syscall:%lu, tk_id:%d, subsys_nr:%d\n",
        shared_acct->in_use, shared_acct->syscall_id, shared_acct->token_id, shared_acct->nr_subsystems);
    shared_acct++;
    i++;
  }
  printf("Free token list:");
  start = rhdl->free_token_list;
  while(start != NULL) {
    printf("%d, ", start->token->id);
    start = start->next;
  }
  printf("\n");
#endif
  return -EINVAL;
}

unsigned long long get_timestamp(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * (unsigned long long)1000000 + tv.tv_usec;
}

int rscfl_store_data_api(rscfl_handle rhdl, subsys_idx_set *data,
                         unsigned long long timestamp,
                         void (*user_fn)(rscfl_handle, void *),
                         void *user_params)
{
  if (data == NULL || rhdl == NULL){
    fprintf(stderr, "Can't store data since data pointer or rscfl_handle is null.\n");
    if (data != NULL){
      free_subsys_idx_set(data);
    }
    return -1;
  }

  if (timestamp == 0) {
    timestamp = get_timestamp();
  }

  if (rhdl->influx.pipe_write == -1){
    fprintf(stderr, "Measurement with timestamp %llu not stored because "
                    "persistent storage is disabled.\n", timestamp);
    free_subsys_idx_set(data);
    return -1;
  }

  if (rhdl->influx.thread_alive) {
    struct influxDB_data buffer;
    int bytes, sent, total;
    buffer.data = data;
    buffer.timestamp = timestamp;
    buffer.function = user_fn;
    buffer.params = user_params;
    total = sizeof(struct influxDB_data);
    sent = 0;
    do
    {
      bytes = write(rhdl->influx.pipe_write, &buffer + sent, total - sent);
      if (bytes < 0){
        perror("ERROR writing measurements to pipe. Disabling persistent storage.");
        free_subsys_idx_set(data);
        rscfl_persistent_storage_cleanup(rhdl);
        return -1;
      }
      sent += bytes;
    } while (sent < total);
  } else {
    fprintf(stderr, "InfluxDB sender thread has exited due to an error. "
                    "Measurement with timestamp %llu and any subsequent "
                    "measurements will not be stored. It is also possible that "
                    "some measurements that were in a queue waiting to get sent "
                    "also weren't stored. Disabling persistent storage.\n", timestamp);
    free_subsys_idx_set(data);
    rscfl_persistent_storage_cleanup(rhdl);
  }
  return 0;
}

int rscfl_read_and_store_data_api(rscfl_handle rhdl, char *info_json,
                                  rscfl_token *token,
                                  void (*user_fn)(rscfl_handle, void *),
                                  void *user_params)
{
  struct accounting acct;
  int err;
  err = rscfl_read_acct(rhdl, &acct, token);
  if(err){
    fprintf(stderr, "Error accounting for system call [data read]\n");
  } else {
    subsys_idx_set* data = rscfl_get_subsys(rhdl, &acct);
    if (info_json != NULL){
      err = rscfl_store_data_with_extra_info(rhdl, data, info_json, user_fn,
                                             user_params);
    } else {
      err = rscfl_store_data(rhdl, data, user_fn, user_params);
    }
    if(err) fprintf(stderr, "Error accounting for system call [data store into DB]\n");
  }
  return err;
}

int rscfl_store_data_with_extra_info_api(rscfl_handle rhdl,
                                         subsys_idx_set *data, char *info_json,
                                         unsigned long long timestamp,
                                         void (*user_fn)(rscfl_handle, void *),
                                         void *user_params)
{
  int bytes, sent, total;
  if (timestamp == 0){
    timestamp = get_timestamp();
  }

  if (rscfl_store_data(rhdl, data, timestamp, user_fn, user_params)) {
    fprintf(stderr, "Extra data for timestamp %llu not stored because storage "
                    "of the corresponding measurement failed.\n", timestamp);
    return -1;
  }

  if (rhdl->mongo.pipe_write == -1){
    fprintf(stderr, "Extra data for timestamp %llu not stored because storage "
                    "of extra data is disabled.\n", timestamp);
    return -1;
  }

  if (rhdl->mongo.thread_alive){
    char *extra_data_unformatted = "{\"timestamp\":%llu,\"data\":%s}";
    char *extra_data = (char *)malloc(strlen(info_json) + 64);
    snprintf(extra_data, strlen(info_json) + 64, extra_data_unformatted, timestamp, info_json);

    total = sizeof(char *);
    sent = 0;
    do
    {
      bytes = write(rhdl->mongo.pipe_write, &extra_data + sent, total - sent);
      if (bytes < 0){
        perror("ERROR writing extra data to pipe. Disabling storage of extra data.");
        free(extra_data);
        mongoDB_cleanup(rhdl);
        return -1;
      }
      sent += bytes;
    } while (sent < total);
  } else {
    fprintf(stderr, "MongoDB sender thread has exited due to an error. "
                    "Extra data with timestamp %llu and any subsequent "
                    "data will not be stored. It is also possible that "
                    "some data that was in a queue waiting to get sent "
                    "also wasn't stored. Disabling storage of extra data.\n", timestamp);
    mongoDB_cleanup(rhdl);
  }

  return 0;
}

char *rscfl_query_measurements(rscfl_handle rhdl, char *query)
{
  if (rhdl == NULL || query == NULL){
    fprintf(stderr, "Can't query data since rscfl handle pointer or query pointer is NULL.\n");
    return NULL;
  }
  if (rhdl->influx.curl != NULL){
    int rv;
    char url[64];
    char message[strlen(query) + 3]; // +3 for the 'q=' and the null terminator
    long response_code;
    struct String str;

    snprintf(url, 64, "http://localhost:8086/query?db=%s&epoch=u", rhdl->app_name);

    /* Initialise the return string */
    str.ptr = malloc(1);  /* will be grown as needed by the realloc in the data_read_callback function */
    if (str.ptr == NULL) {
      fprintf(stderr, "Can't query measurements because malloc failed to allocate a buffer for incoming data.\n");
      return NULL;
    }
    str.length = 0;       /* no data at this point */

    if (curl_easy_setopt(rhdl->influx.curl, CURLOPT_URL, url) != CURLE_OK){
      fprintf(stderr, "Insufficient heap space, can't initialise URL to query measurements.\n");
      free(str.ptr);
      return NULL;
    }
    snprintf(message, strlen(query) + 3, "q=%s", query);

    curl_easy_setopt(rhdl->influx.curl, CURLOPT_POSTFIELDS, message);
    curl_easy_setopt(rhdl->influx.curl, CURLOPT_WRITEFUNCTION, data_read_callback);
    curl_easy_setopt(rhdl->influx.curl, CURLOPT_WRITEDATA, (void *)&str);

    if (rv = curl_easy_perform(rhdl->influx.curl))
    {
      fprintf(stderr, "Can't query measurement - libcurl error (%d): %s\n", rv, curl_easy_strerror(rv));
      free(str.ptr);
      return NULL;
    }

    rv = curl_easy_getinfo(rhdl->influx.curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (rv == CURLE_OK && response_code != 200 && response_code != 204)
    {
      fprintf(stderr, "Couldn't query measurements, HTTP response code from InfluxDB: %ld.\n", response_code);
      free(str.ptr);
      return NULL;
    }
    /* make it so that responses from following requests go to stdout again */
    curl_easy_setopt(rhdl->influx.curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(rhdl->influx.curl, CURLOPT_WRITEDATA, stdout);
    return str.ptr;
  } else {
    fprintf(stderr, "Database not queried because persistent storage is disabled.\n");
    return NULL;
  }
}

mongoc_cursor_t *rscfl_query_extra_data(rscfl_handle rhdl, char *query, char *options)
{
  if (rhdl == NULL || query == NULL){
    fprintf(stderr, "Can't query extra data since rscfl handle pointer or query pointer is NULL.\n");
    return NULL;
  }
  if (rhdl->mongo.connected){
    bson_t *filter;
    bson_t *opts;
    mongoc_cursor_t *cursor;
    bson_error_t error;
    filter = bson_new_from_json((const uint8_t *)query, -1, &error);
    if (filter == NULL){
      fprintf(stderr, "Couldn't parse query. Error: %s\n", error.message);
      return NULL;
    }
    if (options != NULL){
      opts = bson_new_from_json((const uint8_t *)options, -1, &error);
      if (opts == NULL){
        bson_destroy(filter);
        fprintf(stderr, "Couldn't parse query options. Error: %s\n", error.message);
        return NULL;
      }
    } else {
      opts = NULL;
    }
    cursor = mongoc_collection_find_with_opts(rhdl->mongo.collection, filter, opts, NULL);
    bson_destroy(filter);
    if (opts != NULL) bson_destroy(opts);
    return cursor;
  } else {
    fprintf(stderr, "Extra data can't be queried either because persistent storage is "
                    "disabled completely or because need_extra_data was false "
                    "when calling rscfl_init.\n");
    return NULL;
  }
}

query_result_t *rscfl_advanced_query_with_function_api(rscfl_handle rhdl,
                                                       char *measurement_name,
                                                       char *function,
                                                       char *subsystem_name,
                                                       char *extra_data,
                                                       int latest_n,
                                                       unsigned long long time_since_us,
                                                       unsigned long long time_until_us)
{
  if (rhdl == NULL || measurement_name == NULL || function == NULL){
    fprintf(stderr, "Can't perform special query with function, some parameters are missing.\n");
    return NULL;
  }

  char *response;
  if (extra_data != NULL){
    timestamp_array_t array = rscfl_get_timestamps(rhdl, extra_data);
    if (array.length == 0) return NULL;
    response = build_advanced_query(rhdl, measurement_name, function, subsystem_name,
                                    latest_n, time_since_us, time_until_us, &array);
  } else {
    response = build_advanced_query(rhdl, measurement_name, function, subsystem_name,
                                    latest_n, time_since_us, time_until_us, NULL);
  }

  cJSON *response_json;
  if (response == NULL){
    return NULL;
  } else {
    response_json = cJSON_Parse(response);
    free(response);
  }

  cJSON *results_array = cJSON_GetObjectItem(response_json, "results");
  cJSON *result_json = cJSON_GetArrayItem(results_array, 0);
  cJSON *error = cJSON_GetObjectItem(result_json, "error");
  if (error != NULL){
    if (cJSON_IsString(error) && (error->valuestring != NULL)){
      fprintf(stderr,
              "Error occured when trying to submit advanced query.\nError: %s\n",
              error->valuestring);
    }
    cJSON_Delete(response_json);
    return NULL;
  }
  cJSON *series_array = cJSON_GetObjectItem(result_json, "series");
  cJSON *series_json = cJSON_GetArrayItem(series_array, 0);
  cJSON *values_2D_array = cJSON_GetObjectItem(series_json, "values");
  cJSON *values_array = cJSON_GetArrayItem(values_2D_array, 0);

  query_result_t *result = (query_result_t *) malloc(sizeof(query_result_t));
  result->value = 0;
  result->timestamp = 0;
  result->subsystem_name = NULL;

  cJSON *value = cJSON_GetArrayItem(values_array, 1);
  if (cJSON_IsNumber(value))
  {
    result->value = value->valuedouble;
  } else {
    goto error;
  }
  if (EQUAL(function, MIN) || EQUAL(function, MAX)){
    cJSON *timestamp = cJSON_GetArrayItem(values_array, 0);
    if (cJSON_IsNumber(timestamp))
    {
      result->timestamp = (unsigned long long) timestamp->valuedouble;
    } else {
      goto error;
    }
    if (subsystem_name == NULL){
      cJSON *subsystem_name = cJSON_GetArrayItem(values_array, 2);
      if (cJSON_IsString(subsystem_name) && (subsystem_name->valuestring != NULL)){
        result->subsystem_name = malloc(sizeof(char) * (strlen(subsystem_name->valuestring) + 1));
        strncpy(result->subsystem_name, subsystem_name->valuestring, strlen(subsystem_name->valuestring) + 1);
      } else {
        goto error;
      }
    }
  }
  cJSON_Delete(response_json);
  return result;

error:
  cJSON_Delete(response_json);
  rscfl_free_query_result(result);
  return NULL;
}

char *rscfl_advanced_query_api(rscfl_handle rhdl, char *measurement_name,
                               char *subsystem_name,
                               char *extra_data,
                               int latest_n,
                               unsigned long long time_since_us,
                               unsigned long long time_until_us)
{
  if (rhdl == NULL || measurement_name == NULL) {
    fprintf(stderr,
            "Can't perform special query because rscfl handle or "
            "measurement_name is null.\n");
    return NULL;
  }

  char *response;
  if (extra_data != NULL){
    timestamp_array_t array = rscfl_get_timestamps(rhdl, extra_data);
    if (array.length == 0) return NULL;
    response = build_advanced_query(rhdl, measurement_name, NULL, subsystem_name,
                                    latest_n, time_since_us, time_until_us, &array);
  } else {
    response = build_advanced_query(rhdl, measurement_name, NULL, subsystem_name,
                                    latest_n, time_since_us, time_until_us, NULL);
  }

  cJSON *response_json;
  if (response == NULL) {
    return NULL;
  } else {
    response_json = cJSON_Parse(response);
    free(response);
  }

  cJSON *results_array = cJSON_GetObjectItem(response_json, "results");
  cJSON *result_json = cJSON_GetArrayItem(results_array, 0);
  cJSON *error = cJSON_GetObjectItem(result_json, "error");
  if (error != NULL) {
    if (cJSON_IsString(error) && (error->valuestring != NULL)) {
      fprintf(stderr,
              "Error occured when trying to submit advanced query.\nError: %s\n",
              error->valuestring);
    }
    cJSON_Delete(response_json);
    return NULL;
  }
  cJSON *series_array = cJSON_GetObjectItem(result_json, "series");
  cJSON *series_json = cJSON_GetArrayItem(series_array, 0);
  char *series_string = cJSON_Print(series_json);

  cJSON_Delete(response_json);
  return series_string;
}

void rscfl_free_query_result(query_result_t *result)
{
  if (result != NULL){
    if (result->subsystem_name != NULL)
    {
      free(result->subsystem_name);
    }
    free(result);
  }
  return;
}

char *rscfl_get_extra_data(rscfl_handle rhdl, unsigned long long timestamp)
{
  char query[64];
  snprintf(query, 64, "{\"timestamp\":%llu}", timestamp);
  mongoc_cursor_t *cursor = rscfl_query_extra_data(rhdl, query, "{\"projection\":{\"data\":1,\"_id\":0}}");
  char *response = NULL;
  char *extra_data = NULL;
  if (cursor != NULL){
    if (rscfl_get_next_json(cursor, &response)) {
      cJSON *response_json = cJSON_Parse(response);
      cJSON *data = cJSON_GetObjectItem(response_json, "data");
      extra_data = cJSON_PrintUnformatted(data);

      if (rscfl_get_next_json(cursor, &response))
      {
        fprintf(stderr, "More than one result found for timestamp %llu, but only one will be returned.", timestamp);
      }
    }
    mongoc_cursor_destroy(cursor);
  }
  return extra_data;
}

timestamp_array_t rscfl_get_timestamps(rscfl_handle rhdl, char *extra_data)
{
  char query[strlen(extra_data) + 10];
  timestamp_array_t array;
  array.ptr = NULL;
  array.length = 0;
  int array_size = 64; // initially we'll create an array that can hold 64 numbers

  snprintf(query, sizeof(query), "{\"data\":%s}", extra_data);
  mongoc_cursor_t *cursor = rscfl_query_extra_data(rhdl, query, "{\"projection\":{\"timestamp\":1,\"_id\":0}}");

  if (cursor != NULL){
    char *response;
    cJSON *response_json;
    cJSON *timestamp;
    array.ptr = (unsigned long long *)malloc(array_size * sizeof(unsigned long long));
    while (rscfl_get_next_json(cursor, &response)){
      response_json = cJSON_Parse(response);
      timestamp = cJSON_GetObjectItem(response_json, "timestamp");
      if (cJSON_IsNumber(timestamp)) {
        if (array.length == array_size){
          // if the array is full, double it in size
          array_size *= 2;
          array.ptr = (unsigned long long *)realloc(array.ptr, array_size * sizeof(unsigned long long));
        }
        array.ptr[array.length] = (unsigned long long)timestamp->valuedouble;
        array.length++;
      }
      rscfl_free_json(response);
    }
    mongoc_cursor_destroy(cursor);
  } else {
    return array;
  }
  // trim the allocated memory
  array.ptr = (unsigned long long *)realloc(array.ptr, array.length * sizeof(unsigned long long));
  return array;
}

bool rscfl_get_next_json(mongoc_cursor_t *cursor, char **string)
{
  if (cursor == NULL){
    return false;
  }
  const bson_t *doc;
  bson_error_t error;
  char *str;
  if (mongoc_cursor_next(cursor, &doc)){
    str = bson_as_relaxed_extended_json(doc, NULL);
    *string = str;
    return true;
  } else {
    if (mongoc_cursor_error(cursor, &error)){
      fprintf (stderr, "An error occurred: %s\n", error.message);
    }
    return false;
  }
}

subsys_idx_set* rscfl_get_subsys(rscfl_handle rhdl, struct accounting *acct)
{
  int curr_set_ix = 0, i;
  subsys_idx_set *ret_subsys_idx;

  if (acct == NULL) return NULL;

  ret_subsys_idx = malloc(sizeof(subsys_idx_set));
  if (!ret_subsys_idx) return NULL;

  ret_subsys_idx->set_size = acct->nr_subsystems;
  ret_subsys_idx->max_set_size = acct->nr_subsystems;
  ret_subsys_idx->set =
      malloc(sizeof(struct subsys_accounting) * acct->nr_subsystems);
  if (!ret_subsys_idx->set) {
    free(ret_subsys_idx);
    return NULL;
  }
  ret_subsys_idx->ids = malloc(sizeof(short) * acct->nr_subsystems);
  if (!ret_subsys_idx->ids) {
    free(ret_subsys_idx->set);
    free(ret_subsys_idx);
    return NULL;
  }

  for (i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *subsys = rscfl_get_subsys_by_id(rhdl, acct, i);
    if (subsys != NULL) {
      ret_subsys_idx->idx[i] = curr_set_ix;
      memcpy(&ret_subsys_idx->set[curr_set_ix], subsys,
             sizeof(struct subsys_accounting));
      ret_subsys_idx->ids[curr_set_ix] = i;
      subsys->in_use = 0;
      curr_set_ix++;
    } else {
      ret_subsys_idx->idx[i] = -1;
    }
  }

  return ret_subsys_idx;
}

subsys_idx_set* rscfl_get_new_aggregator(unsigned short no_subsystems)
{
  subsys_idx_set *ret_subsys_idx;
  if (no_subsystems > NUM_SUBSYSTEMS) no_subsystems = NUM_SUBSYSTEMS;

  ret_subsys_idx = malloc(sizeof(subsys_idx_set));
  if (!ret_subsys_idx) return NULL;

  ret_subsys_idx->set_size = 0;
  ret_subsys_idx->max_set_size = no_subsystems;
  ret_subsys_idx->set = calloc(no_subsystems, sizeof(struct subsys_accounting));
  if (!ret_subsys_idx->set) {
    free(ret_subsys_idx);
    return NULL;
  }
  memset(ret_subsys_idx->idx, -1, sizeof(short) * NUM_SUBSYSTEMS);

  ret_subsys_idx->ids = malloc(sizeof(short) * no_subsystems);
  if (!ret_subsys_idx->ids) {
    free(ret_subsys_idx->set);
    free(ret_subsys_idx);
    return NULL;
  }

  return ret_subsys_idx;
}

int rscfl_merge_idx_set_into(subsys_idx_set *current, subsys_idx_set *aggregator_into) {
  int agg_set_ix, i, rc = 0;

  agg_set_ix = aggregator_into->set_size;

  for(i=0; i<current->set_size; i++) {
    // fold set[i] into aggregator_info
    // index of current subsystem ix = current->ids[i]
    int ix = current->ids[i];
    if(aggregator_into->idx[ix] == -1) {
      // subsys ix not in aggregator_into, add if sufficient space
      if (agg_set_ix < aggregator_into->max_set_size) {
        aggregator_into->idx[ix] = agg_set_ix;
        aggregator_into->set[agg_set_ix] = current->set[i];
        aggregator_into->ids[agg_set_ix] = ix;
        agg_set_ix++;
        aggregator_into->set_size++;
      } else {
        // not enough space in aggregator_into, set error but continue
        // (the values that could be aggregated remain correct)
        rc++;
      }
    } else {
      // subsys ix exists, merge values
      rscfl_subsys_merge(&aggregator_into->set[aggregator_into->idx[ix]],
                         &current->set[i]);
    }
  }
  return rc;
}

int rscfl_merge_acct_into(rscfl_handle rhdl, struct accounting *acct_from,
                          subsys_idx_set *aggregator_into)
{
  int curr_set_ix, i, rc = 0;
  if (!acct_from || !aggregator_into) return -EINVAL;

  curr_set_ix = aggregator_into->set_size;

  for (i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *new_subsys =
        rscfl_get_subsys_by_id(rhdl, acct_from, i);
    if (new_subsys != NULL) {
      if (aggregator_into->idx[i] == -1) {
        // new_subsys i not in aggregator_into, add if sufficient space
        if (curr_set_ix < aggregator_into->max_set_size) {
          aggregator_into->idx[i] = curr_set_ix;
          memcpy(&aggregator_into->set[curr_set_ix], new_subsys,
                 sizeof(struct subsys_accounting));
          aggregator_into->ids[curr_set_ix] = i;
          new_subsys->in_use = 0;
          curr_set_ix++;
          aggregator_into->set_size++;
        } else {
          // not enough space in aggregator_into, set error but continue
          // (the values that could be aggregated remain correct)
          rc++;
        }
      } else {
        // subsys i exists, merge values
        rscfl_subsys_merge(&aggregator_into->set[aggregator_into->idx[i]],
                           new_subsys);
        new_subsys->in_use = 0;
      }
    }
  }
  return rc;
}

int rscfl_getreset_probe_exits(rscfl_handle rhdl) {
  int exits;
  rscfl_acct_layout_t *rscfl_data = (rscfl_acct_layout_t *)rhdl->buf;
  exits = rscfl_data->subsys_exits;
  rscfl_data->subsys_exits = 0;
  return exits;
}

void free_subsys_idx_set(subsys_idx_set *subsys_set)
{
  if (subsys_set != NULL) {
    free(subsys_set->set);
    free(subsys_set->ids);
  }
  free(subsys_set);
}

inline void rscfl_subsys_merge(struct subsys_accounting *e,
                               const struct subsys_accounting *c) {
  e->subsys_entries              += c->subsys_entries;
  e->subsys_exits                += c->subsys_exits;

  e->cpu.cycles                  += c->cpu.cycles;
  e->cpu.branch_mispredictions   += c->cpu.branch_mispredictions;
  e->cpu.instructions            += c->cpu.instructions;

  rscfl_timespec_add(&e->cpu.wall_clock_time, &c->cpu.wall_clock_time);

  e->mem.alloc                   += c->mem.alloc;
  e->mem.freed                   += c->mem.freed;
  e->mem.page_faults             += c->mem.page_faults;
  e->mem.align_faults            += c->mem.align_faults;

  rscfl_timespec_add(&e->sched.wct_out_local, &c->sched.wct_out_local);
  rscfl_timespec_add(&e->sched.xen_sched_wct, &c->sched.xen_sched_wct);

  e->sched.run_delay               += c->sched.run_delay;
  e->sched.xen_schedules           += c->sched.xen_schedules;
  e->sched.xen_sched_cycles        += c->sched.xen_sched_cycles;
  e->sched.xen_blocks              += c->sched.xen_blocks;
  e->sched.xen_yields              += c->sched.xen_yields;
  e->sched.xen_evtchn_pending_size += c->sched.xen_evtchn_pending_size;
  e->sched.xen_credits_min = min(e->sched.xen_credits_min,
                                 c->sched.xen_credits_min);
  e->sched.xen_credits_max = max(e->sched.xen_credits_max,
                                 c->sched.xen_credits_max);
}

struct subsys_accounting* rscfl_get_subsys_by_id(rscfl_handle rhdl,
                                                 struct accounting *acct,
                                                 rscfl_subsys subsys_id)
{
  if (!acct || acct->acct_subsys[subsys_id] == -1) {
    return NULL;
  }
  rscfl_acct_layout_t *rscfl_data = (rscfl_acct_layout_t *)rhdl->buf;
  return &rscfl_data->subsyses[acct->acct_subsys[subsys_id]];
}

void rscfl_subsys_free(rscfl_handle rhdl, struct accounting *acct)
{
  int i;
  if (rhdl == NULL || acct == NULL) return;

  for (i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *subsys = rscfl_get_subsys_by_id(rhdl, acct, i);
    if (subsys != NULL) subsys->in_use = 0;
  }
}

// Shadow kernels.
#if SHDW_ENABLED != 0
int rscfl_spawn_shdw(rscfl_handle rhdl, shdw_hdl *hdl)
{
  int rc;
  rscfl_ioctl_t ioctl_arg = {0};
  ioctl_arg.shdw_operation = SPAWN_ONLY;
  rc = ioctl(rhdl->fd_ctrl, RSCFL_SHDW_CMD, &ioctl_arg);
  *hdl = ioctl_arg.new_shdw_id;
  return rc;
}

int rscfl_spawn_shdw_for_pid(rscfl_handle rhdl)
{
  rscfl_ioctl_t ioctl_arg = {0};
  ioctl_arg.shdw_operation = SPAWN_SWAP_ON_SCHED;
  ioctl_arg.num_shdw_pages = -1;
  return ioctl(rhdl->fd_ctrl, RSCFL_SHDW_CMD, &ioctl_arg);
}

int rscfl_use_shdw_pages(rscfl_handle rhdl, int use_shdw, int shdw_pages)
{
  rscfl_ioctl_t ioctl_arg = {0};
  ioctl_arg.shdw_operation = SWAP;
  ioctl_arg.swap_to_shdw = use_shdw;
  ioctl_arg.num_shdw_pages = shdw_pages;
  return ioctl(rhdl->fd_ctrl, RSCFL_SHDW_CMD, &ioctl_arg);
}
#endif /* SHDW_ENABLED */

/*
 * Static functions
 */

static int open_file(char *db_name, char *app_name, char *extension)
{
  char filepath[128];

  sprintf(filepath, "/home/bu214/%s_%s.%s", db_name, app_name, extension);
  int outfd = open(filepath, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (outfd < 0){
    perror("ERROR opening file");
    return -1;
  }
  return outfd;
}

static int open_influxDB_file(char *app_name)
{
  return open_file("InfluxDB", app_name, "txt");
}

static CURL *connect_to_influxDB(void)
{
  int rv;
  rv = curl_global_init(CURL_GLOBAL_ALL);
  if (rv) return NULL;

  CURL *curl = curl_easy_init();
  if (curl != NULL)
  {
    if (rv = curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8086")) {goto error;}
    if (rv = curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L)) {goto error;}
    if (rv = curl_easy_perform(curl)) {goto error;}
    if (rv = curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 0L)) {goto error;}
    if (rv = curl_easy_setopt(curl, CURLOPT_POST, 1L)) {goto error;}
  }
  return curl;
error:
  fprintf(stderr, "Can't initialise connection to InfluxDB - libcurl error (%d): %s\n", rv, curl_easy_strerror(rv));
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return NULL;
}

static int open_mongoDB_file(char *app_name)
{
  return open_file("MongoDB", app_name, "json");
}

static bool connect_to_mongoDB(char *app_name, mongo_handle_t *mongo)
{
  const char *uri_str = "mongodb://localhost:27017/?appname=rscfl";
  mongoc_client_t *client;
  mongoc_database_t *database;
  mongoc_collection_t *collection;
  bson_t *command, reply;
  bson_error_t error;
  bool retval;

  mongoc_init();

  /* create a new mongoDB client instance */
  client = mongoc_client_new(uri_str);

  /* Try to ping the 'admin' database to see if mongoDB is running */
  command = BCON_NEW("ping", BCON_INT32(1));
  retval = mongoc_client_command_simple(client, "admin", command, NULL, &reply, &error);
  if (!retval) {
    /* if it's not running then return */
    fprintf(stderr, "%s\n", error.message);
    mongoc_client_destroy(client);
    mongoc_cleanup();
    return false;
  }

  /* Get a handle on the correct database and collection */
  database = mongoc_client_get_database(client, app_name);
  collection = mongoc_client_get_collection(client, app_name, "main_collection");

  mongo->client = client;
  mongo->collection = collection;
  mongo->database = database;
  return true;
}

static int store_measurements(rscfl_handle rhdl, char *measurements)
{
  if (rhdl == NULL || measurements == NULL){
    fprintf(stderr, "Measurement not stored because rscfl handle pointer or data pointer is NULL.\n");
    return -1;
  }

  if (rhdl->influx.fd == -1 && rhdl->influx.curl == NULL){
    fprintf(stderr, "Measurement not stored because persistent storage is disabled.\n");
    return -1;
  }

  if (rhdl->influx.curl != NULL){
    int rv;
    char url[128];
    long response_code;

    snprintf(url, 128, "http://localhost:8086/write?db=%s&precision=u", rhdl->app_name);

    if (curl_easy_setopt(rhdl->influx.curl, CURLOPT_URL, url) != CURLE_OK){
      fprintf(stderr, "Insufficient heap space, can't initialise URL to send measurements.\n");
      goto error;
    }
    curl_easy_setopt(rhdl->influx.curl, CURLOPT_POSTFIELDS, measurements);

    if (rv = curl_easy_perform(rhdl->influx.curl))
    {
      fprintf(stderr, "Can't send measurement - libcurl error (%d): %s\n", rv, curl_easy_strerror(rv));
      goto error;
    }

    rv = curl_easy_getinfo(rhdl->influx.curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (rv == CURLE_OK && response_code == 404)
    {
      /* create database */
      if (curl_easy_setopt(rhdl->influx.curl, CURLOPT_URL, "http://localhost:8086/query") != CURLE_OK){
        fprintf(stderr, "Insufficient heap space, can't initialise URL to send measurements.\n");
        goto error;
      }
      char message[64];
      snprintf(message, 64, "q=CREATE DATABASE \"%s\"", rhdl->app_name);
      curl_easy_setopt(rhdl->influx.curl, CURLOPT_POSTFIELDS, message);
      if (rv = curl_easy_perform(rhdl->influx.curl))
      {
        fprintf(stderr, "Can't create new database - libcurl error (%d): %s\n", rv, curl_easy_strerror(rv));
        goto error;
      }

      rv = curl_easy_getinfo(rhdl->influx.curl, CURLINFO_RESPONSE_CODE, &response_code);
      if (rv == CURLE_OK && response_code != 200 && response_code != 204)
      {
        fprintf(stderr, "Couldn't create DB, HTTP response code from InfluxDB: %ld.\n", response_code);
        goto error;
      }
      /* resend message */
      if (curl_easy_setopt(rhdl->influx.curl, CURLOPT_URL, url) != CURLE_OK){
        fprintf(stderr, "Insufficient heap space, can't initialise URL to send measurements.\n");
        goto error;
      }
      curl_easy_setopt(rhdl->influx.curl, CURLOPT_POSTFIELDS, measurements);

      if (rv = curl_easy_perform(rhdl->influx.curl))
      {
        fprintf(stderr, "Can't send measurement - libcurl error (%d): %s\n", rv, curl_easy_strerror(rv));
        goto error;
      }
    }

    if (rv == CURLE_OK && response_code != 200 && response_code != 204)
    {
      fprintf(stderr, "Couldn't store measurement, HTTP response code from InfluxDB: %ld.\n", response_code);
      goto error;
    }

    // we have successfully sent the data to DB, so we can now return
    return 0;

error:
    fprintf(stderr, "Sending measurements to the database failed. "
                    "Closing the connection and opening a file\n");
    curl_easy_cleanup(rhdl->influx.curl);
    curl_global_cleanup();
    rhdl->influx.curl = NULL;
    rhdl->influx.fd = open_influxDB_file(rhdl->app_name);
    if (rhdl->influx.fd == -1){
      fprintf(stderr, "Can't open a file to store measurements.\n");
      return -1;
    }
  }

  if (rhdl->influx.fd != -1){
    int bytes, written, total;
    total = strlen(measurements);
    written = 0;
    do
    {
      bytes = write(rhdl->influx.fd, measurements + written, total - written);
      if (bytes < 0){
        perror("ERROR writing message to file");
        return -1;
      }
      if (bytes == 0)
        break;
      written += bytes;
    } while (written < total);
  }

  // we have successfully sent the data to a file
  return 0;
}

static int store_extra_data(rscfl_handle rhdl, char *json)
{
  if (rhdl == NULL || json == NULL){
    fprintf(stderr, "Can't store extra data since rscfl handle pointer or data pointer is NULL.\n");
    return -1;
  }

  if (rhdl->mongo.fd == -1 && !rhdl->mongo.connected) {
    fprintf(stderr, "Extra data not stored either because persistent storage is "
                    "disabled completely or because need_extra_data was false "
                    "when calling rscfl_init.\n");
    return -1;
  }

  if (rhdl->mongo.connected){
    bson_t *insert;
    bson_error_t error;
    insert = bson_new_from_json((const uint8_t *)json, -1, &error);

    if (insert == NULL){
      fprintf(stderr, "%s\n", error.message);
      goto error;
    }

    if (!mongoc_collection_insert(rhdl->mongo.collection, MONGOC_INSERT_NONE, insert, NULL, &error)){
      fprintf(stderr, "%s\n", error.message);
      goto error;
    }
    bson_destroy(insert);
    return 0; // we have successfully sent the data to DB, so we can now return

error:
    fprintf(stderr, "Sending measurements to the database failed. "
                    "Closing the connection and opening a file\n");
    mongoc_collection_destroy(rhdl->mongo.collection);
    mongoc_database_destroy(rhdl->mongo.database);
    mongoc_client_destroy(rhdl->mongo.client);
    mongoc_cleanup();
    rhdl->mongo.connected = false;
    rhdl->mongo.fd = open_mongoDB_file(rhdl->app_name);
    if (rhdl->mongo.fd == -1){
      fprintf(stderr, "Can't open a file to store extra data.\n");
      return -1;
    }
  }

  if (rhdl->mongo.fd != -1){
    int bytes, written, total;
    total = strlen(json);
    written = 0;
    do
    {
      bytes = write(rhdl->mongo.fd, json + written, total - written);
      if (bytes < 0){
        perror("ERROR writing message to file");
        return -1;
      }
      if (bytes == 0)
        break;
      written += bytes;
    } while (written < total);
  }

  // we have successfully sent the data to a file
  return 0;
}

static char *escape_spaces(const char *string)
{
  if (string == NULL)
    return NULL;
  char *new_string = calloc(2 * (strlen(string) + 1), sizeof(char));
  char *new_string_iterator = new_string;
  for (; *string; ++string, ++new_string_iterator)
  {
    if (*string == ' '){
      *new_string_iterator = '\\';
      ++new_string_iterator;
      *new_string_iterator = ' ';
    } else {
      *new_string_iterator = *string;
    }
  }
  return new_string;
}

static size_t data_read_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct String *str = (struct String *)userp;

  str->ptr = realloc(str->ptr, str->length + realsize + 1);
  if(str->ptr == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(str->ptr[str->length]), contents, realsize);
  str->length += realsize;
  str->ptr[str->length] = 0;

  return realsize;
}

static void *influxDB_sender(void *param)
{
  rscfl_handle rhdl = (rscfl_handle) param;

  struct influxDB_data buffer;
  unsigned long long timestamp;
  subsys_idx_set *data;
  void (*user_function)(rscfl_handle, void *);
  void *user_params;
  int bytes, total = sizeof(struct influxDB_data), received = 0;
  while((bytes = read(rhdl->influx.pipe_read, &buffer + received, total - received)) > 0) {
    received += bytes;
    if (received == total){
      unsigned long long start = get_timestamp();
      received = 0;
      data = buffer.data;
      timestamp = buffer.timestamp;
      user_function = buffer.function;
      user_params = buffer.params;

      char metric[METRIC_BUFFER_SIZE] = {0};
      char subsystem_metrics[SUBSYSTEM_METRICS_BUFFER_SIZE] = {0};
      char measurements[MEASUREMENTS_BUFFER_SIZE] = {0};
      int measurements_remaining_length = MEASUREMENTS_BUFFER_SIZE;

      int i, err;
      long long wct;
      for(i = 0; i < data->set_size; i++){
        short subsys_id = data->ids[i];
        char *subsystem_name = escape_spaces(rscfl_subsys_name[subsys_id]);

        struct subsys_accounting sa_id = data->set[i];

        EXTRACT_METRIC("cpu.cycles", sa_id.cpu.cycles, "%llu")

        wct = (long long)sa_id.cpu.wall_clock_time.tv_sec*1000000000 + (long long)sa_id.cpu.wall_clock_time.tv_nsec;
        // printf("s: %lld, ns: %lld, ttl: %lld\n",(long long)sa_id.cpu.wall_clock_time.tv_sec, (long long)sa_id.cpu.wall_clock_time.tv_nsec, wct);
        EXTRACT_METRIC("cpu.wall_clock_time", wct, "%lld")

        EXTRACT_METRIC("sched.cycles_out_local", sa_id.sched.cycles_out_local, "%llu")

        wct = (long long)sa_id.sched.wct_out_local.tv_sec*1000000000 + (long long)sa_id.sched.wct_out_local.tv_nsec;
        // printf("s: %lld, ns: %lld, ttl: %lld\n",(long long)sa_id.sched.wct_out_local.tv_sec, (long long)sa_id.sched.wct_out_local.tv_nsec, wct);
        EXTRACT_METRIC("sched.wct_out_local", wct, "%lld")

        // printf("subsystem_metrics:%s\n",subsystem_metrics);

        if(strlen(subsystem_metrics) > measurements_remaining_length){
          err = store_measurements(rhdl, measurements);
          if (err){
            rhdl->influx.thread_alive = false;
            fprintf(stderr,
                    "Failed to store measurements, enqueuing of more data has "
                    "been disabled. Attempting to store and free remaining "
                    "resources in the pipe.\n");
            free_subsys_idx_set(data);
          }
          // printf("sent:%s\n",measurements);
          memset(measurements, 0, MEASUREMENTS_BUFFER_SIZE);
          measurements_remaining_length = MEASUREMENTS_BUFFER_SIZE;
        }

        strncat(measurements, subsystem_metrics, measurements_remaining_length);
        measurements_remaining_length -= strlen(subsystem_metrics);
        memset(subsystem_metrics, 0, SUBSYSTEM_METRICS_BUFFER_SIZE);
        free(subsystem_name);
      }
      free_subsys_idx_set(data);
      err = store_measurements(rhdl, measurements);
      if (err){
        rhdl->influx.thread_alive = false;
        fprintf(stderr,
                "Failed to store measurements, enqueuing of more data has "
                "been disabled. Attempting to store and free remaining "
                "resources in the pipe.\n");
      }
      // printf("sent:%s\n",measurements);
      if (user_function != NULL){
        user_function(rhdl, user_params);
      }
      unsigned long long end = get_timestamp();
      printf("influx loop time %llu us\n", end - start);
    }
  }
  if (bytes == 0){
    printf("Finished reading from pipe, clean exit from influxDB sender thread.\n");
  } else {
    perror("Error occured when trying to read from pipe on influxDB sender thread. The thread will now exit.");
  }

exit:
  rhdl->influx.thread_alive = false;
  return NULL;
}

static void *mongoDB_sender(void *param)
{
  rscfl_handle rhdl = (rscfl_handle) param;
  char *str;
  int err, bytes, total = sizeof(char *), received = 0;
  while((bytes = read(rhdl->mongo.pipe_read, &str + received, total - received)) > 0) {
    received += bytes;
    if (received == total){
      unsigned long long start = get_timestamp();
      received = 0;
      err = store_extra_data(rhdl, str);
      free(str);
      if (err){
        rhdl->mongo.thread_alive = false;
        fprintf(stderr,
                "Failed to store extra data, enqueuing of more data has "
                "been disabled. Attempting to store and free remaining "
                "resources in the pipe.\n");
      }
      unsigned long long end = get_timestamp();
      printf("mongo loop time %llu us\n", end - start);
    }
  }
  if (bytes == 0){
    printf("Finished reading from pipe, clean exit from mongoDB sender thread.\n");
  } else if (err){
    fprintf(stderr, "Failed to store extra data, mongoDB sender thread will now exit.\n");
  } else {
    perror("Error occured when trying to read from pipe on mongoDB sender thread. The thread will now exit.");
  }
  rhdl->mongo.thread_alive = false;
  return NULL;
}

static void influxDB_cleanup(rscfl_handle rhdl)
{
  if (rhdl->influx.pipe_write != -1){
    close(rhdl->influx.pipe_write);
    rhdl->influx.pipe_write = -1;
    if(pthread_join(rhdl->influx.sender_thread, NULL)) {
      fprintf(stderr, "Error joining thread\n");
    }
    close(rhdl->influx.pipe_read);
    rhdl->influx.pipe_read = -1;
  }
  if (rhdl->influx.curl != NULL){
    curl_easy_cleanup(rhdl->influx.curl);
    curl_global_cleanup();
    rhdl->influx.curl = NULL;
  }
  if (rhdl->influx.fd != -1){
    close(rhdl->influx.fd);
    rhdl->influx.fd = -1;
  }
}

static void mongoDB_cleanup(rscfl_handle rhdl)
{
  if (rhdl->mongo.pipe_write != -1){
    close(rhdl->mongo.pipe_write);
    rhdl->mongo.pipe_write = -1;
    if(pthread_join(rhdl->mongo.sender_thread, NULL)) {
      fprintf(stderr, "Error joining thread\n");
    }
    close(rhdl->mongo.pipe_read);
    rhdl->mongo.pipe_read = -1;
  }
  if (rhdl->mongo.connected){
    mongoc_collection_destroy(rhdl->mongo.collection);
    mongoc_database_destroy(rhdl->mongo.database);
    mongoc_client_destroy(rhdl->mongo.client);
    mongoc_cleanup();
    rhdl->mongo.connected = false;
  }
  if (rhdl->mongo.fd != -1){
    close(rhdl->mongo.fd);
    rhdl->mongo.fd = -1;
  }
}

static char *build_advanced_query(rscfl_handle rhdl, char *measurement_name,
                                  char *function, char *subsystem_name,
                                  int latest_n,
                                  unsigned long long time_since_us,
                                  unsigned long long time_until_us,
                                  timestamp_array_t *timestamps)
{
  char select_clause[32] = {0};
  if (function == NULL){
    snprintf(select_clause, 32, "\"value\"");
  } else {
    snprintf(select_clause, 32, "%s(\"value\")", function);
  }
  if (subsystem_name == NULL && (function == NULL || EQUAL(function, MIN) || EQUAL(function, MAX))) {
    strncat(select_clause, ",\"subsystem\"", 32 - strlen(select_clause));
  }
  // printf("\n\nselect_clause: %s\n", select_clause);

  char *subsystem_constraint = "";
  if (subsystem_name != NULL) {
    subsystem_constraint = (char *)malloc((strlen(subsystem_name) + 32)*sizeof(char));
    snprintf(subsystem_constraint, strlen(subsystem_name) + 32,
             "\"subsystem\" = \'%s\'", subsystem_name);
  }
  // printf("subsystem_constraint: %s\n", subsystem_constraint);

  char time_constraint[128] = {0};
  unsigned long long time_since_ns = time_since_us * 1000;
  unsigned long long time_until_ns = time_until_us * 1000;
  if (time_since_ns == 0 && time_until_ns == 0){
    time_constraint[0] = '\0';  // empty string
  } else if (time_since_ns != 0 && time_until_ns != 0){
    if (time_since_ns == time_until_ns){
      snprintf(time_constraint, 128, "\"time\" = %llu", time_since_ns);
    } else {
      snprintf(time_constraint, 128, "\"time\" >= %llu AND \"time\" <= %llu",
               time_since_ns, time_until_ns);
    }
  } else if (time_since_ns != 0){
    snprintf(time_constraint, 128, "\"time\" >= %llu", time_since_ns);
  } else {
    // time_until_ns != 0
    snprintf(time_constraint, 128, "\"time\" <= %llu", time_until_ns);
  }
  // printf("time_constraint: %s\n", time_constraint);

  char *measurement_ids = "";
  if (timestamps != NULL && timestamps->ptr != NULL){
    measurement_ids = (char *)malloc(timestamps->length * 64 * sizeof(char));
    snprintf(measurement_ids, 64, "\"measurement_id\" = \'%llu\'", *timestamps->ptr);
    int i;
    for (i = 1; i < timestamps->length; i++){
      snprintf(measurement_ids + strlen(measurement_ids), 64,
               " OR \"measurement_id\" = \'%llu\'", timestamps->ptr[i]); // append to the end
    }
  }
  // printf("measurement_ids: %s\n", measurement_ids);

  char *where_clause = "";
  if (measurement_ids[0] != '\0' || time_constraint[0] != '\0' || subsystem_constraint[0] != '\0'){
    int length_of_constraints = strlen(subsystem_constraint) +
                                strlen(time_constraint) +
                                strlen(measurement_ids) + 16;
    where_clause = (char *)malloc(length_of_constraints * sizeof(char));
    snprintf(where_clause, length_of_constraints, " WHERE");

    if (subsystem_constraint[0] != '\0'){
      snprintf(where_clause + strlen(where_clause), length_of_constraints - strlen(where_clause),
               " %s", subsystem_constraint);
    }

    if (time_constraint[0] != '\0'){
      if (strlen(where_clause) > 6){
        // if there alreasy is a condition, add an AND
        snprintf(where_clause + strlen(where_clause), length_of_constraints - strlen(where_clause),
                 " AND %s", time_constraint);
      } else {
        snprintf(where_clause + strlen(where_clause), length_of_constraints - strlen(where_clause),
                 " %s", time_constraint);
      }
    }

    if (measurement_ids[0] != '\0'){
      if (strlen(where_clause) > 6){
        // if there already is a condition, add an AND
        snprintf(where_clause + strlen(where_clause), length_of_constraints - strlen(where_clause),
                 " AND %s", measurement_ids);
      } else {
        snprintf(where_clause + strlen(where_clause), length_of_constraints - strlen(where_clause),
                 " %s", measurement_ids);
      }
    }
  }
  // printf("where_clause: %s\n", where_clause);
  char order_clause[64] = {0};
  if (latest_n != 0){
    snprintf(order_clause, 64, " ORDER BY time DESC LIMIT %d", latest_n);
  }
  // printf("order_clause: %s\n", order_clause);

  char *empty_query;
  int query_length = strlen(where_clause) + strlen(measurement_name) +
                     strlen(select_clause) + strlen(order_clause) + 128;
  char query[query_length];

  if (function != NULL && latest_n != 0) {
    empty_query =
        "SELECT %s FROM (SELECT \"value\",\"subsystem\" FROM \"%s\"%s%s) ORDER "
        "BY time DESC";
  } else {
    empty_query = "SELECT %s FROM \"%s\"%s%s";
  }
  snprintf(query, query_length, empty_query, select_clause, measurement_name,
           where_clause, order_clause);
  printf("query: %s\n", query);
  char *response = rscfl_query_measurements(rhdl, query);

  if (subsystem_constraint[0] != '\0') free(subsystem_constraint);
  if (measurement_ids[0] != '\0') free(measurement_ids);
  if (where_clause[0] != '\0') free(where_clause);

  return response;
}
