OOPS Message Analysis

    Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000

The above line tells us about the bug. A null pointer was being dereferenced. 
	
	Mem abort info:
    
    ESR = 0x96000045
    
    EC = 0x25: DABT (current EL), IL = 32 bits
    
    SET = 0, FnV = 0
    
    EA = 0, S1PTW = 0
    
    FSC = 0x05: level 1 translation fault


    Data abort info:
    
    ISV = 0, ISS = 0x00000045
    
    CM = 0, WnR = 1
    
    user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042680000
    
    [0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
    
    Internal error: Oops: 96000045 [#1] SMP
    
    Modules linked in: hello(O) faulty(O) scull(O)
   The above line lists the kernel modules which were linked in. 
   The below line shows the CPU core on which it was run, the process ID and the kernel Tainted State. 
   G means a proprietary module was loaded, O means that an externally built out of tree module was loaded. 
    
    CPU: 0 PID: 164 Comm: sh Tainted: G O 5.15.18 #1
    
    Hardware name: linux,dummy-virt (DT)
    
    pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
    
    pc : faulty_write+0x14/0x20 [faulty]
 We can see that the program counter is at the function **faulty_write at 0x14 byte** when the error happened. 
 We can also see that the faulty write function is of total **0x20 bytes** of instructions.
Below we can see the contents of the link register, stack pointer and other registers when the error happened. 
    
    lr : vfs_write+0xa8/0x2b0
    
    sp : ffffffc008c93d80
    
    x29: ffffffc008c93d80 x28: ffffff80026aa640 x27: 0000000000000000
    
    x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
    
    x23: 0000000040000000 x22: 000000000000000c x21: 000000557a2a2760
    
    x20: 000000557a2a2760 x19: ffffff8002649d00 x18: 0000000000000000
    
    x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
    
    x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
    
    x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
    
    x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
    
    x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008c93df0
    
    x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000

Below we can see the call trace, the call originated from the function **faulty_write** from the module **faulty**. Then we can see as the execution passes through library functions and system calls. 

    Call trace:
    
    faulty_write+0x14/0x20 [faulty]
    
    ksys_write+0x68/0x100
    
    __arm64_sys_write+0x20/0x30
    
    invoke_syscall+0x54/0x130
    
    el0_svc_common.constprop.0+0x44/0xf0
    
    do_el0_svc+0x40/0xa0
    
    el0_svc+0x20/0x60
    
    el0t_64_sync_handler+0xe8/0xf0
    
    el0t_64_sync+0x1a0/0x1a4
    
    Code: d2800001 d2800000 d503233f d50323bf (b900003f)
    
    ---[ end trace 57ee244b7cf5e0f0 ]---

