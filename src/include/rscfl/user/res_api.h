/* Resourceful user-facing API
 * Usage example (for brevity, we skip the code checking for errors after each
 * API call):
 *
 * First, call rscfl_init(). a separate call is needed for each application
 * thread - it allocates two shared memory regions between user and kernel
 * space: one that holds resource accounting data and a second for storing
 * measurement requests from the application:
 *
 * rhdl = rscfl_init();
 *
 * The rscfl_handle you get back will be needed in all subsequent API calls on
 * that thread.
 *
 * Now, declare interest in measuring the resource consumption of the next
 * syscall by calling rscfl_acct_next:
 *
 * int err = rscfl_acct_next(rhdl);
 * int fd = open("/../file_path", O_CREAT); // syscall being measured
 * ...
 *
 * You can then call rscfl_acct_read, and obtain the struct accounting object
 * that holds an index of all the resource consumption data for your syscall:
 *
 * struct accounting acct_hdl;
 * err = rscfl_read_acct(rhdl, &acct_hdl);
 *
 * The pair (rhdl, acct_hdl) uniquely identifies this particular measurement.
 * To get access to the measured resource consumption data, call one of the
 * high-level API functions. As a simple example, being interested only in this
 * particular call (no aggregation), we can call rscfl_get_subsys:
 *
 * subsys_idx_set *open_subsys_measurements;
 * open_subsys_measurements = rscfl_get_subsys(rhdl, &acct_hdl);
 *
 * The returned structure contains an index of all the kernel subsystems
 * touched during the open call (the idx data member).
 *
 * If idx[i] = -1 then subsystem i was not touched.
 * On the other hand, if idx[i] = val (val!=-1) then you can read the
 * corresponding measured data from set[val] (look at the definition of
 * struct subsys_accounting in costs.h to see what data is available)
 *
 * TODO(lc525): write documentation for the aggregation functions and the
 * advanced API
 *
 */
#ifndef _RES_API_H_
#define _RES_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/subsys_list.h"

#define MAX_PAYLOAD 1024 /* maximum payload size*/
#define SUBSYS_AS_STR_ARRAY(a, b, c) [a] = c,

/*
 * RSCFL API data structures
 */

/* array containing the user-friendly names of each subsystem */
extern const char *rscfl_subsys_name[NUM_SUBSYSTEMS];

typedef enum {
  SW_TK_DEFAULT  = EBIT(0),
  SW_TK_RESET    = EBIT(1),
  SW_TK_NULL     = EBIT(2),
} token_switch_flags;

/*
 * Resourceful tokens allow us to track the resources consumed by a request
 * when it is not in the kernel. This can be in userspace or in the hypervisor.
 *
 * Each request should get a token at the start, and use that token
 * throughout the entire request.
 */
struct rscfl_token {
  unsigned short id;
  _Bool first_acct;
  _Bool data_read;
  _Bool in_use; //prevents double-free situations
};
typedef struct rscfl_token rscfl_token;


struct rscfl_token_list {
  rscfl_token *token;
  struct rscfl_token_list *next;
};
typedef struct rscfl_token_list rscfl_token_list;


/*
 * rscfl_handle_t* (typedef-ed to rscfl_handle) represents the user-space
 * descriptor for interacting with the kernel module. This is obtained
 * per-thread by calling rscfl_init() and has to then be passed to all other API
 * functions.
 */
struct rscfl_handle_t {
  char *buf;
  unsigned long lst_syscall_id;
  rscfl_ctrl_layout_t *ctrl;
  /*
   * Rscfl generates a pool of tokens that can be used by userspace without
   * performing a mode switch into the kernel.
   * This pool is replenished whenever there is a system call that finds a
   * reduction in the number of free tokens.
   */
  //rscfl_token *fresh_tokens[NUM_READY_TOKENS];
  rscfl_token_list *free_token_list;
  rscfl_token *current_token;
  //int ready_token_sp;
  int fd_ctrl;
};
typedef struct rscfl_handle_t *rscfl_handle;

/*
 * subsys_idx_set holds subsystem accounting data in user space, indexed by
 * subsystem id.
 *
 * idx is the index: idx[i] holds the offset in the set array where the data for
 *                   subsystem i is held. idx[i] == -1 if there is no data for
 *                   subsystem i
 *
 * set is an array: each element holds data for one subsystem. this is typically
 *                  accessed through set[idx[i]] when idx[i] != -1
 *
 * ids: ids[j] is the id of the subsystem data stored in set[j]
 *
 * app_data:        a void pointer that applications can use to add their own
 *                  data structures containing measurements. app_data is owned
 *                  by the application, so it's the application's responsibility
 *                  to free the memory for this member.
 * set_size:        the number of subsystems currently stored in the set array
 * max_set_size:    the maximum number of subsystems that can be stored in the
 *                  set array. this is the allocated size of set.
 */
struct subsys_idx_set {
  short idx[NUM_SUBSYSTEMS];
  struct subsys_accounting *set;
  short *ids;
  void *app_data;
  short set_size;
  short max_set_size;
};
typedef struct subsys_idx_set subsys_idx_set;


/****************************
 *
 * Basic API
 *
 ****************************/

/*
 * -- common functionality --
 */

/**
 * \brief initialises the resourceful API. call once on every app thread.
 *
 * rscfl_init is a macro so that we can do automatic API version checking, with
 * default arguments. Depending on the number of arguments it receives,
 * rscfl_init transforms into one of the rscfl_init_X functions (where X is the
 * number of arguments).
 *
 * In each case, the actual function being called is rscfl_init_api(...).
 */
#define rscfl_init(...) CONCAT(rscfl_init_, VARGS_NR(__VA_ARGS__))(__VA_ARGS__)
#define rscfl_init_0() rscfl_init_api(RSCFL_VERSION, NULL)
/*
 * Do not call directly. use the rscfl_init(...) macro instead.
 *
 * \param api_ver the version of the API. automatically set by the rscfl_init
 *                macro to RSCFL_VERSION
 * \param cfg     rscfl configuration. use the same configuration for all
 */
#define rscfl_init_1(cfg) rscfl_init_api(RSCFL_VERSION, cfg)
/**
 * Initialises rscfl for the calling thread.
 *
 * Note:
 *  Do not call directly. Use the rscfl_init(...) macro instead. The macro will
 *  automatically call this function and it will pass the correct api_ver
 *  parameter.
 *
 * Communication with the rscfl kernel module is established and kernel-side
 * resources (buffers for holding resource accounting for the thread) are
 * allocated.
 *
 * Parameters:
 *   ver (rscfl_version_t): The rscfl version that matches the current headers
 *    (res_api.h). This is checked against the version baked in the user-space
 *    library (librscfl) that the application will be linked with, and the
 *    version of the rscfl kernel module currently executing.
 *    When users call the rscfl_init macro
 *    instead of this function, the parameter is automatically set correctly
 *    to RSCFL_VERSION (so that API and data compatibility checks can take
 *    place)
 *
 *   cfg (rscfl_config*):  User-owned variable pointing to rscfl configuration
 *    options. This configuration can no longer be changed after initialisation.
 *
 * Returns:
 *   rscfl_handle : an opaque rscfl handle
 *
 * References:
 *   rscfl_init
 *
 * See Also:
 *  * RSCFL_ERR_VERSION_MISMATCH (in config.h).
 *  * The ERR_ON_VERSION_MISMATCH cmake build option.
 */
rscfl_handle rscfl_init_api(rscfl_version_t ver, rscfl_config* config);

#define rscfl_get_handle(...) CONCAT(rscfl_get_handle_, VARGS_NR(__VA_ARGS__))(__VA_ARGS__)
#define rscfl_get_handle_0() rscfl_get_handle_api(NULL)
#define rscfl_get_handle_1(cfg) rscfl_get_handle_api(cfg)

/**
 * Returns the rscfl handle for the current thread. If rscfl was not
 * initialised on the thread, it will perform the initialisation with config
 * cfg. If rscfl is already initialised, new configurations **WILL NOT** be
 * applied.
 *
 * :param cfg:    rscfl configuration. use the same configuration as in the
 *                first call to this function at al times (on the fly config
 *                change not supported yet)
 * :type cfg:     rscfl_config*
 * :param token:  an inexistent token
 */
rscfl_handle rscfl_get_handle_api(rscfl_config *cfg);

/*
 * If successful returns 0, and sets the value of *token to be a new token.
 *
 * Tokens are used to aggregate the resources used by an entire request,
 * in particular to allow a request to find out how resource has been consumed
 * by the system whilst it has been scheduled out by the scheduler or
 * hypervisor.
 */
int rscfl_get_token(rscfl_handle rhdl, rscfl_token **token);

#define rscfl_switch_token(...) CONCAT(rscfl_switch_token_, VARGS_NR(__VA_ARGS__))(__VA_ARGS__)
#define rscfl_switch_token_2(handle, token) rscfl_switch_token_api(handle, token, SW_TK_DEFAULT)
#define rscfl_switch_token_3(handle, token, fl) rscfl_switch_token_api(handle, token, fl)
int rscfl_switch_token_api(rscfl_handle rhdl, rscfl_token *token_to, token_switch_flags fl);

/*
 * Returns an int as we put the token on a reuse list. Allocation of
 * memory to put the element on the list may fail.
 */
int rscfl_free_token(rscfl_handle, rscfl_token *);

/*
 *
 */
#define rscfl_acct(...) CONCAT(rscfl_acct_, VARGS_NR(__VA_ARGS__))(__VA_ARGS__)
#define rscfl_acct_1(handle) rscfl_acct_api(handle, NULL, ACCT_DEFAULT)
#define rscfl_acct_2(handle, token) rscfl_acct_api(handle, token, ACCT_DEFAULT)
#define rscfl_acct_3(handle, token, fl) rscfl_acct_api(handle, token, fl)
int rscfl_acct_api(rscfl_handle, rscfl_token *token, interest_flags fl);

#define rscfl_read_acct(...) CONCAT(rscfl_read_acct_, VARGS_NR(__VA_ARGS__))(__VA_ARGS__)
#define rscfl_read_acct_2(handle, acct) rscfl_read_acct_api(handle, acct, NULL)
#define rscfl_read_acct_3(handle, acct, token) rscfl_read_acct_api(handle, acct, token)
int rscfl_read_acct_api(rscfl_handle handle, struct accounting *acct, rscfl_token *token);

/*
 * -- high level API functions --
 */

/*!
 * \brief reads the per-subsystem resource accounting that was measured for acct
 *
 * It returns a subsys_idx_set structure, containing both the set of subsystems
 * that were active for the measured system call(s), and an subsystem_id-based
 * index for fast querying.
 *
 * The resource accounting data is copied to userspace, freeing the
 * corresponding kernel resources.
 *
 * The returned subsys_idx_set pointer is owned by the calling application, and
 * it will have to be freed using free_subsys_idx_set(...).
 *
 * \param rhdl the resourceful handle for the thread where measurement is done
 *             (typically the current thread)
 * \param [in] acct a pointer to the accounting data structure obtained from
 *                  calling rscfl_acct_read
 */
subsys_idx_set* rscfl_get_subsys(rscfl_handle rhdl, struct accounting *acct);

/*!
 * \brief returns an empty subsys_idx_set capable of holding resource accounting
 *        data for no_subsystems.
 *
 * This can be used when aggregating data across multiple acct_next calls, by
 * passing the resulting subsys_idx_set to rscfl_merge_acct_into.
 *
 * The returned subsys_idx_set pointer is owned by the calling application, and
 * it will have to be freed using free_subsys_idx_set(...).
 *
 * \param no_subsystems the maximum number of subsystems which the aggregator
 *                      will be able to hold. Calling this with NUM_SUBSYSTEMS
 *                      means the aggregator can hold everything (safest option
 *                      but it also allocates quite a bit more memory)
 */
subsys_idx_set* rscfl_get_new_aggregator(unsigned short no_subsystems);


/*!
 * \brief merge one subsys_idx_set into another (efficiently)
 */
int rscfl_merge_idx_set_into(subsys_idx_set *current, subsys_idx_set *aggregator_into);


 /*!
 * \brief rscfl_merge_acct_into allows fast aggregation of subsys accounting
 *        data in user space
 *
 *
 * \param rhdl the resourceful handle for the thread where measurement is done
 *             (typically the current thread)
 * \param [in] acct_from the acct data structure from where we want to take the
 *                       per-subsystem information
 * \param [in] aggregator_into the subsys_idx_set aggregator to which we'll add
 *                             the data measured for acct_from.
 *
 * If acct_from has touched subsystems not present in subsys_idx_set before,
 * those will be added to the aggregator (a set union of the subsystems is
 * performed).
 *
 * If the aggregator is not large enough to hold the union, the
 * number of subsystems that couldn't be added is returned. 0 is returned on
 * normal exit.
 *
 * This function frees the kernel-side resources allocated for the subsystems
 * that we have aggregated. If aggregator_into already contains data for
 * a particular subsystem, no extra copies of the new subsystem data are done
 * in user-space.
 */
int rscfl_merge_acct_into(rscfl_handle rhdl, struct accounting *acct_from,
                          subsys_idx_set *aggregator_into);

/*!
 * \brief get the number of probes for which accounting took place and resets
 *        the number to 0
 */
int rscfl_getreset_probe_exits(rscfl_handle rhdl);

/*!
 * \brief free_subsys_idx_set: free memory once the user space is done using the
 *                             subsystem data
 */
void free_subsys_idx_set(subsys_idx_set *subsys_set);


/*
 * -- low level API functions --
 *
 *
 * only use those functions directly if you know what you're doing
 *
 * for example, using rscfl_get_subsys_by_id does not free the kernel-side
 * resources allocated for storing per-subsystem measurement data. You would
 * need to explicitly set subsys->in_use to 0 or call rscfl_subsys_free
 * afterwards.
 *
 * failing to use the proper calling protocol of those functions might lead to
 * leaking kernel memory and system instability
 *
 */

/*!
 * \brief merge the data measured in two kernel subsystems
 *
 * \param [in,out] e the subsys_accounting in which data will be aggregated
 *                   (the existing subsys_accounting)
 * \param [in]     c the current subsys_accounting. The measurement data for
 *                   this subsystem will be added to the exising
 *                   subsys_accounting
 */
void rscfl_subsys_merge(struct subsys_accounting *existing_subsys,
                        const struct subsys_accounting *new_subsys);

/*!
 * \brief gets the measurements done for acct in a particular kernel subsystem
 *
 * \param rhdl the resourceful handle for the thread where measurement is done
 *             (typically the current thread)
 * \param [in] acct a pointer to the accounting data structure obtained from
 *                  calling rscfl_acct_read
 * \param subsys_id the id of the subsystem (one of the values in the
 *                  rscfl_subsys enum)
 *
 * returns NULL if the measured code path did not touch subsystem subsys_id
 */
struct subsys_accounting* rscfl_get_subsys_by_id(rscfl_handle rhdl,
                                                 struct accounting *acct,
                                                 rscfl_subsys subsys_id);

/*!
 * \brief marks the kernel-side memory used for subsystem accounting storage as
 *        free
 *
 *  The memory for all subsystems touched during measurements done for acct is
 *  marked as available.
 */
void rscfl_subsys_free(rscfl_handle rhdl, struct accounting *acct);


/****************************
 *
 * Advanced API
 *
 ****************************/


#define DEFINE_SELECT_FCT_PTR(pname, rtype) \
  typedef rtype* (*subsys_select_##pname)(struct subsys_accounting*,           \
                                         rscfl_subsys)

#define DEFINE_COMBINE_FCT_PTR(pname, rtype) \
  typedef void (*subsys_combine_##pname)(rtype*, const rtype*)

#define SELECT_FCT_PTR(pname) subsys_select_##pname
#define COMBINE_FCT_PTR(pname) subsys_combine_##pname

#define DECLARE_REDUCE_FUNCTION(pname, rtype)                                  \
int rscfl_subsys_reduce_##pname(rscfl_handle rhdl, struct accounting *acct,    \
                                  int free_subsys,                             \
                                  rtype *accum,                                \
                                  SELECT_FCT_PTR(pname) select,                \
                                  COMBINE_FCT_PTR(pname) combine)              \


#define DEFINE_REDUCE_FUNCTION(pname, rtype)                                   \
int rscfl_subsys_reduce_##pname(rscfl_handle rhdl, struct accounting *acct,    \
                                  int free_subsys,                             \
                                  rtype *accum,                                \
                                  SELECT_FCT_PTR(pname) select,                \
                                  COMBINE_FCT_PTR(pname) combine)              \
{                                                                              \
  int i;                                                                       \
  if(acct == NULL) return -EINVAL;                                             \
                                                                               \
  for(i = 0; i < NUM_SUBSYSTEMS; ++i) {                                        \
    struct subsys_accounting *subsys =                                         \
      rscfl_get_subsys_by_id(rhdl, acct, (rscfl_subsys)i);                     \
    rtype* current;                                                            \
    if(subsys != NULL) {                                                       \
      current = select(subsys, (rscfl_subsys)i);                               \
      combine(accum, current);                                                 \
      if(free_subsys) subsys->in_use = 0;                                      \
    }                                                                          \
  }                                                                            \
                                                                               \
  return 0;                                                                    \
}

#define REDUCE_SUBSYS(pname, rhdl, acct, free_subsys, accum, select, combine)  \
  rscfl_subsys_reduce_##pname(rhdl, acct, free_subsys, accum, select, combine)


DEFINE_SELECT_FCT_PTR(rint, ru64);
DEFINE_COMBINE_FCT_PTR(rint, ru64);
DECLARE_REDUCE_FUNCTION(rint, ru64);

DEFINE_SELECT_FCT_PTR(wc, struct timespec);
DEFINE_COMBINE_FCT_PTR(wc, struct timespec);
DECLARE_REDUCE_FUNCTION(wc, struct timespec);


// Shadow kernels.
int rscfl_spawn_shdw(rscfl_handle, shdw_hdl *);

int rscfl_use_shdw_pages(rscfl_handle, int, int);

#ifdef __cplusplus
}
#endif
#endif
