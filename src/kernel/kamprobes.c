#include "rscfl/kernel/kamprobes.h"

#include <linux/cpu.h>
#include <linux/vmalloc.h>

#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/res_common.h"
#include "rscfl/subsys_list.h"

#define WRAPPER_SIZE 71

#define WORD_SIZE_IN_BYTES 8

#define CALL_WIDTH 5
#define JMP_WIDTH 5
#define MOV_WIDTH 8

struct orig_insn
{
  u8 *loc;
  unsigned char vals[CALL_WIDTH];
};

static struct orig_insn *probe_list;
static unsigned int no_probes = 0;

static char *wrapper_start = NULL;
static char *wrapper_end;

static void add_to_probe_list(u8 *loc)
{
  int i;
  probe_list[no_probes].loc = loc;
  for (i = 0; i < CALL_WIDTH; i++) {
    probe_list[no_probes].vals[i] = loc[i];
  }
  no_probes++;
}

void kamprobes_unregister_all(void)
{
  int i;

  get_online_cpus();
  mutex_lock(KPRIV(text_mutex));
  for (i = 0; i < no_probes; i++) {
    KPRIV(text_poke)(probe_list[i].loc, &probe_list[i].vals, CALL_WIDTH);
  }
  debugk(KERN_NOTICE "Unregistered %d probes\n", no_probes);
  no_probes = 0;
  mutex_unlock(KPRIV(text_mutex));
  put_online_cpus();
}

static inline void emit_rel_address(char **wrapper_end, char *addr)
{
  int32_t *w_end = (int32_t *)*wrapper_end;
  *w_end = (int32_t)(addr - 4 - *wrapper_end);
  (*wrapper_end) += 4;
}

static inline void emit_abs_address(char **wrapper_end, char *addr)
{
  int32_t *w_end = (int32_t *)*wrapper_end;
  // Make a 32 bit pointer.
  *w_end = (int32_t)(addr - (char *)0xffffffff00000000);
  (*wrapper_end) += 4;
}

static inline void emit_ins(char **wrapper_end, char c)
{
  **wrapper_end = c;
  (*wrapper_end)++;
}

static inline void emit_multiple_ins(char **wrapper_end, const char *c,
                                     int num_ins)
{
  memcpy(*wrapper_end, c, num_ins);
  (*wrapper_end) += num_ins;
}

static inline void emit_jump(char **wrapper_end, char *addr)
{
  **wrapper_end = 0xe9;
  (*wrapper_end)++;
  emit_rel_address(wrapper_end, addr);
}

static inline void emit_callq(char **wrapper_end, char *addr)
{
  **wrapper_end = 0xe8;
  (*wrapper_end)++;
  emit_rel_address(wrapper_end, addr);
}

static inline void emit_return_address_to_r11(char **wrapper_end,
                                              char *wrapper_fp)
{
  // mov r11, [rip-addr]
  const char machine_code[] = {0x4c, 0x8b, 0x1d};
  emit_multiple_ins(wrapper_end, machine_code, sizeof(machine_code));
  emit_rel_address(wrapper_end, wrapper_fp - 8);
}

static inline void emit_mov_rsp_r11(char **wrapper_end)
{
  // mov (%rsp), %r11
  const char machine_code[] = {0x4c, 0x8b, 0x1c, 0x24};
  emit_multiple_ins(wrapper_end, machine_code, sizeof(machine_code));
}

static inline void emit_mov_r11_addr(char **wrapper_end, char *addr)
{
  // mov %r11, addr
  const char machine_code[] = {0x4c, 0x89, 0x1d};
  emit_multiple_ins(wrapper_end, machine_code, sizeof(machine_code));
  emit_rel_address(wrapper_end, addr);
}

static inline void emit_mov_addr_rsp(char **wrapper_end, char *addr)
{
  // mov $addr %rsp
  const char machine_code[] = {0x48, 0xc7, 0x04, 0x24};
  emit_multiple_ins(wrapper_end, machine_code, sizeof(machine_code));
  emit_abs_address(wrapper_end, addr);
}

static inline void emit_push_addr(char **wrapper_end, char *addr)
{
  // push ($addr)
  emit_ins(wrapper_end, 0x68);
  emit_abs_address(wrapper_end, addr);
}

static inline void emit_save_registers(char **wrapper_end)
{
  const char insns[] = {0x50,  // rax

                        0x53,  // rbx

                        0x57,  // rdi

                        0x56,  // rsi

                        0x52,  // rdx

                        0x51,  // rcx

                        0x41,  // r8
                        0x50,

                        0x41,  // r9
                        0x51,

                        0x41,  // r10
                        0x52,
  };
  int i;
  for (i = 0; i < sizeof(insns); i++) {
    emit_ins(wrapper_end, insns[i]);
  }
}

static inline void emit_restore_registers(char **wrapper_end)
{
  const char insns[] = {
      0x41,
      0x5a,

      0x41,  // r9
      0x59,

      0x41,  // r8
      0x58,

      0x59,  // rcx

      0x5a,  // rdx

      0x5e,  // rsi

      0x5f,  // rdi

      0x5b,  // rbx

      0x58,  // rax
  };

  int i;
  for (i = 0; i < sizeof(insns); i++) {
    emit_ins(wrapper_end, insns[i]);
  }
}

static inline int is_call_ins(u8 **addr)
{
  return **addr == 0xe8;
}

static inline int is_noop(u8 **addr)
{
  return (**addr == 0x90 || **addr == 0x0f || **addr == 0x1f || **addr == 0x66);
}

int kamprobes_init(int max_probes)
{
  probe_list = kmalloc(sizeof(struct orig_insn) * max_probes, GFP_KERNEL);
  if (probe_list == NULL) {
    return -ENOMEM;
  }

  if (wrapper_start == NULL) {
    wrapper_start = KPRIV(__vmalloc_node_range)(
        WRAPPER_SIZE * max_probes, 1, MODULES_VADDR, MODULES_END,
        GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL_EXEC, NUMA_NO_NODE,
        __builtin_return_address(0));

    wrapper_end = wrapper_start;

    if (wrapper_start == NULL) {
      kfree(probe_list);
      return -ENOMEM;
    }
    debugk("wrapper_start:%p\n", wrapper_start);
  }
  return 0;
}

void kamprobes_free() {
  kfree(probe_list);
  vfree(wrapper_start);
}

int kamprobes_register(u8 **orig_addr, char sys_type, void (*pre_handler)(void),
                       void (*post_handler)(void))
{
  char *wrapper_fp;
  int offset;
  char *target;
  int32_t addr_ptr;
  unsigned char text_poke_isns[CALL_WIDTH];

  int i;

  const char callq_opcode = 0xe8;
  const char jmpq_opcode = 0xe9;

  // Refuse to register probes on any addr which is not a callq or a noop
  if(!is_call_ins(orig_addr) && !is_noop(orig_addr)) {
    printk(KERN_ERR "Failed to set probe at %p", (void *)*orig_addr);
    return -EINVAL;
  }
  // TODO(lc525) return to the code above [--- TESTING ---]
  // if(!is_call_ins(orig_addr) || sys_type == ADDR_KERNEL_SYSCALL) return -EINVAL;

  // If *orig_addr is not a call instruction then we assume it is the start
  // of a sys_ function, so is called through magic pointers. We don't want to
  // rewrite this code, so instead replace the call to __fentry__ with a call
  // to our wrapper. However, we need to save the return address somewhere.
  // We use 8 bytes just before wrapper_fp to do this.
  if (!is_call_ins(orig_addr)) {
    for (i = 0; i < WORD_SIZE_IN_BYTES; i++) {
      emit_ins(&wrapper_end, 0x00);
    }
  }

  // wrapper_fp always points to the start of the current wrapper frame.
  wrapper_fp = wrapper_end;

  // The value of the address pointed to by the stack pointer currently
  // contains the return address of the function we're interposing.
  // If we are storing the return address before the start of the current
  // wrapper frame then we need to take the value of the stack pointer, and
  // put it in a register that we can trash (eg r11), and then move that
  // register to the top of the wrapper frame.
  if (!is_call_ins(orig_addr)) {
    debugk("sys at %p, firstb: %#0x\n", *orig_addr, **orig_addr);
    emit_mov_rsp_r11(&wrapper_end);

    // mov r11 wrapper_fp - 8
    emit_mov_r11_addr(&wrapper_end, wrapper_fp - 8);
  }

  // Find the target of the callq in the original instruction stream.
  // We need this so that after calling the pre handler we can then call
  // the original function.
  offset = ((*orig_addr)[1]) + ((*orig_addr)[2] << 8) +
           ((*orig_addr)[3] << 16) + ((*orig_addr)[4] << 24) + CALL_WIDTH;
  target = (void *)*orig_addr + offset;

  // Preserve arguments passed through registers before calling into the
  // pre-handler. This is to obey the system v abi, whereby the caller has to
  // maintain registers.
  emit_save_registers(&wrapper_end);

  // Call into the pre-handler.
  emit_callq(&wrapper_end, (char *)pre_handler);

  // Restore the register file from what we just pushed onto the stack.
  emit_restore_registers(&wrapper_end);

  // Change the top of the stack so it points at the bottom-half of the wrapper,
  // which is the bit that does the calling of the rtn-handler.
  emit_mov_addr_rsp(&wrapper_end, wrapper_end + JMP_WIDTH + MOV_WIDTH);

  if (is_call_ins(orig_addr)) {
    // Run the original function.
    // If this is a normal function (not a SyS_) then the code we run is the
    // target of the call instruction that we're replacing. We jump into it as
    // we've already pushed a return address onto the stack.
    switch(sys_type){
      case ADDR_CALLQ:
        emit_jump(&wrapper_end, target);
        break;
      case ADDR_KERNEL_SYSCALL:
        emit_jump(&wrapper_end, target+5);
        break;
      case ADDR_INVALID:
      case ADDR_USER_SYSCALL:
        // we shouldn't get syscalls that come directly from userspace
        // as callqs, nor should we probe invalid addresses.
        BUG();
    }

    // Rtn-handling code.

    // Set up the return address of the rtn-handler.
    // This is actually set to be the next instruction in the original
    // instruction stream. This means that control flow goes directly back from
    // the return handler to the original caller, without going back
    // through the wrapper. This is seriously black magic, in that we don't
    // return to where we came from. However, it is efficient.
    emit_push_addr(&wrapper_end, (char *)(*orig_addr + CALL_WIDTH));
  } else {
    // We're adding kamprobes to the function padding, at the top of a syscall.
    // The original code we run is therefore not the target of this memory
    // which would be __fentry__. Rather it is the next instruction in the
    // syscall.
    emit_jump(&wrapper_end, (char *)(*orig_addr + CALL_WIDTH));

    // At the start of our wrapper we mov'd the return pointer to wrapper_fp-8.
    // We now need to restore it.

    // First, move the return address into a register that we can trash (r11).
    emit_return_address_to_r11(&wrapper_end, wrapper_fp);

    // Now push r11, which contains the return address, onto the stack.
    emit_ins(&wrapper_end, 0x41);
    emit_ins(&wrapper_end, 0x53);
  }

  // As we have setup the stack so that [rsp] points to the address that retq
  // should take us to we jump (rather than call) into post-handler.
  // A call would not let us skip the wrapper when we return from the
  // post-handler.

  emit_jump(&wrapper_end, (char *)post_handler);

  // End of setting up the wrapper.

  // Store the original address so that we can remove kamprobes.
  add_to_probe_list(*orig_addr);
  // Ensure we start with a callq opcode, in case of nop-ed insns.

  // Poke the original instruction to point to our wrapper.
  addr_ptr = wrapper_fp - CALL_WIDTH - (char *)*orig_addr;

  if(is_call_ins(orig_addr)) { // callq
    text_poke_isns[0] = callq_opcode;
  } else {                     // SyS_ call
    text_poke_isns[0] = jmpq_opcode;
  }
  memcpy(text_poke_isns + 1, &addr_ptr, CALL_WIDTH - 1);

  get_online_cpus();
  mutex_lock(KPRIV(text_mutex));
  KPRIV(text_poke)(*orig_addr, &text_poke_isns, CALL_WIDTH);
  mutex_unlock(KPRIV(text_mutex));
  put_online_cpus();
  return 0;
}
