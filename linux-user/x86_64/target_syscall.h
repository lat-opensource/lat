#ifndef X86_64_TARGET_SYSCALL_H
#define X86_64_TARGET_SYSCALL_H

#define __USER_CS	(0x33)
#define __USER_DS	(0x2B)

struct target_pt_regs {
	abi_ulong r15;
	abi_ulong r14;
	abi_ulong r13;
	abi_ulong r12;
	abi_ulong rbp;
	abi_ulong rbx;
/* arguments: non interrupts/non tracing syscalls only save up to here */
	abi_ulong r11;
	abi_ulong r10;
	abi_ulong r9;
	abi_ulong r8;
	abi_ulong rax;
	abi_ulong rcx;
	abi_ulong rdx;
	abi_ulong rsi;
	abi_ulong rdi;
	abi_ulong orig_rax;
/* end of arguments */
/* cpu exception frame or undefined */
	abi_ulong rip;
	abi_ulong cs;
	abi_ulong eflags;
	abi_ulong rsp;
	abi_ulong ss;
/* top of stack page */
};

/* Maximum number of LDT entries supported. */
#define TARGET_LDT_ENTRIES	8192
/* The size of each LDT entry. */
#define TARGET_LDT_ENTRY_SIZE	8

#define TARGET_GDT_ENTRIES 16
#define TARGET_GDT_ENTRY_TLS_ENTRIES 3
#define TARGET_GDT_ENTRY_TLS_MIN 12
#define TARGET_GDT_ENTRY_TLS_MAX 14

#if 0 // Redefine this
struct target_modify_ldt_ldt_s {
	unsigned int  entry_number;
        abi_ulong     base_addr;
	unsigned int  limit;
	unsigned int  seg_32bit:1;
	unsigned int  contents:2;
	unsigned int  read_exec_only:1;
	unsigned int  limit_in_pages:1;
	unsigned int  seg_not_present:1;
	unsigned int  useable:1;
	unsigned int  lm:1;
};
#else
struct target_modify_ldt_ldt_s {
	unsigned int  entry_number;
        abi_ulong     base_addr;
	unsigned int  limit;
        unsigned int flags;
};
struct target_modify_ldt_ldt_s_32 {
    uint32_t entry_number;
    uint32_t base_addr;
    uint32_t limit;
    uint32_t flags;
};
#endif

struct target_ipc64_perm
{
	int		key;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	cuid;
	uint32_t	cgid;
	unsigned short		mode;
	unsigned short		__pad1;
	unsigned short		seq;
	unsigned short		__pad2;
	abi_ulong		__unused1;
	abi_ulong		__unused2;
};

struct target_msqid64_ds {
	struct target_ipc64_perm msg_perm;
	unsigned int msg_stime;	/* last msgsnd time */
	unsigned int msg_rtime;	/* last msgrcv time */
	unsigned int msg_ctime;	/* last change time */
	abi_ulong  msg_cbytes;	/* current number of bytes on queue */
	abi_ulong  msg_qnum;	/* number of messages in queue */
	abi_ulong  msg_qbytes;	/* max number of bytes on queue */
	unsigned int msg_lspid;	/* pid of last msgsnd */
	unsigned int msg_lrpid;	/* last receive pid */
	abi_ulong  __unused4;
	abi_ulong  __unused5;
};

#define UNAME_MACHINE "x86_64"
#define UNAME_MINIMUM_RELEASE "2.6.32"

#define TARGET_ARCH_SET_GS 0x1001
#define TARGET_ARCH_SET_FS 0x1002
#define TARGET_ARCH_GET_FS 0x1003
#define TARGET_ARCH_GET_GS 0x1004
#define TARGET_MINSIGSTKSZ 2048
#define TARGET_MCL_CURRENT 1
#define TARGET_MCL_FUTURE  2
#define TARGET_MCL_ONFAULT 4
/* vm86 defines */

#define TARGET_BIOSSEG		0x0f000

#define TARGET_CPU_086		0
#define TARGET_CPU_186		1
#define TARGET_CPU_286		2
#define TARGET_CPU_386		3
#define TARGET_CPU_486		4
#define TARGET_CPU_586		5

#define TARGET_VM86_SIGNAL	0	/* return due to signal */
#define TARGET_VM86_UNKNOWN	1	/* unhandled GP fault - IO-instruction or similar */
#define TARGET_VM86_INTx	2	/* int3/int x instruction (ARG = x) */
#define TARGET_VM86_STI	3	/* sti/popf/iret instruction enabled virtual interrupts */

/*
 * Additional return values when invoking new vm86()
 */
#define TARGET_VM86_PICRETURN	4	/* return due to pending PIC request */
#define TARGET_VM86_TRAP	6	/* return due to DOS-debugger request */

/*
 * function codes when invoking new vm86()
 */
#define TARGET_VM86_PLUS_INSTALL_CHECK	0
#define TARGET_VM86_ENTER		1
#define TARGET_VM86_ENTER_NO_BYPASS	2
#define	TARGET_VM86_REQUEST_IRQ	3
#define TARGET_VM86_FREE_IRQ		4
#define TARGET_VM86_GET_IRQ_BITS	5
#define TARGET_VM86_GET_AND_RESET_IRQ	6

/*
 * This is the stack-layout seen by the user space program when we have
 * done a translation of "SAVE_ALL" from vm86 mode. The real kernel layout
 * is 'kernel_vm86_regs' (see below).
 */

struct target_vm86_regs {
/*
 * normal regs, with special meaning for the segment descriptors..
 */
	abi_long ebx;
	abi_long ecx;
	abi_long edx;
	abi_long esi;
	abi_long edi;
	abi_long ebp;
	abi_long eax;
	abi_long __null_ds;
	abi_long __null_es;
	abi_long __null_fs;
	abi_long __null_gs;
	abi_long orig_eax;
	abi_long eip;
	unsigned short cs, __csh;
	abi_long eflags;
	abi_long esp;
	unsigned short ss, __ssh;
/*
 * these are specific to v86 mode:
 */
	unsigned short es, __esh;
	unsigned short ds, __dsh;
	unsigned short fs, __fsh;
	unsigned short gs, __gsh;
};

struct target_revectored_struct {
	abi_ulong __map[8];			/* 256 bits */
};

struct target_vm86_struct {
	struct target_vm86_regs regs;
	abi_ulong flags;
	abi_ulong screen_bitmap;
	abi_ulong cpu_type;
	struct target_revectored_struct int_revectored;
	struct target_revectored_struct int21_revectored;
};

/*
 * flags masks
 */
#define TARGET_VM86_SCREEN_BITMAP	0x0001

struct target_vm86plus_info_struct {
        abi_ulong flags;
#define TARGET_force_return_for_pic (1 << 0)
#define TARGET_vm86dbg_active       (1 << 1)  /* for debugger */
#define TARGET_vm86dbg_TFpendig     (1 << 2)  /* for debugger */
#define TARGET_is_vm86pus           (1 << 31) /* for vm86 internal use */
	unsigned char vm86dbg_intxxtab[32];   /* for debugger */
};

struct target_vm86plus_struct {
	struct target_vm86_regs regs;
	abi_ulong flags;
	abi_ulong screen_bitmap;
	abi_ulong cpu_type;
	struct target_revectored_struct int_revectored;
	struct target_revectored_struct int21_revectored;
	struct target_vm86plus_info_struct vm86plus;
};

#endif /* X86_64_TARGET_SYSCALL_H */
