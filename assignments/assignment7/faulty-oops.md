# Faulty Oops Anlysis

For ECEA 5306. Several sections follow.

## Initial Failure Output

Using `/dev/faulty` caused a sigmentation fault due to a `kernel NULL pointer dereference at virtual address 0000000000000000`, evidenced by output to the console:

```txt
root@qemuarm64:~# echo "hello_world" > /dev/faulty
[   22.373088] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
[   22.373137] Mem abort info:
[   22.373142]   ESR = 0x0000000096000044
[   22.373157]   EC = 0x25: DABT (current EL), IL = 32 bits
[   22.373168]   SET = 0, FnV = 0
[   22.373174]   EA = 0, S1PTW = 0
[   22.373182]   FSC = 0x04: level 0 translation fault
[   22.373191] Data abort info:
[   22.373197]   ISV = 0, ISS = 0x00000044, ISS2 = 0x00000000
[   22.373204]   CM = 0, WnR = 1, TnD = 0, TagAccess = 0
[   22.373210]   GCS = 0, Overlay = 0, DirtyBit = 0, Xs = 0
[   22.373271] user pgtable: 4k pages, 48-bit VAs, pgdp=0000000044d05000
[   22.373291] [0000000000000000] pgd=0000000000000000, p4d=0000000000000000
[   22.373439] Internal error: Oops: 0000000096000044 [#1]  SMP
[   22.375589] Modules linked in: scull(O) faulty(O) hello(O)
[   22.376324] CPU: 2 UID: 0 PID: 331 Comm: sh Tainted: G           O        6.18.54-yocto-standard #1 PREEMPT
[   22.376782] Tainted: [O]=OOT_MODULE
[   22.376938] Hardware name: linux,dummy-virt (DT)
[   22.377153] pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[   22.377410] pc : faulty_write+0xc/0x20 [faulty]
[   22.377863] lr : vfs_write+0xd8/0x398
[   22.378125] sp : ffff800082443d40
[   22.378204] x29: ffff800082443db0 x28: ffff00000361b480 x27: 0000000000000000
[   22.378478] x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
[   22.378636] x23: 0000000040000000 x22: ffff800082443df0 x21: 000000000000000c
[   22.378889] x20: 0000aaaada9d7850 x19: ffff0000033926c0 x18: 0000000000000000
[   22.379137] x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
[   22.379402] x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
[   22.379615] x11: 0000000000000000 x10: 0000000000000000 x9 : ffff80008035b188
[   22.379816] x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000001
[   22.379961] x5 : 0000000000000000 x4 : ffff800079706068 x3 : ffff800082443df0
[   22.380615] x2 : 000000000000000c x1 : 0000aaaada9d7850 x0 : 0000000000000000
[   22.380914] Call trace:
[   22.381139]  faulty_write+0xc/0x20 [faulty] (P)
[   22.381434]  ksys_write+0x6c/0x118
[   22.381608]  __arm64_sys_write+0x20/0x40
[   22.381698]  invoke_syscall.constprop.0+0x48/0xf0
[   22.381823]  el0_svc_common.constprop.0+0x40/0xf0
[   22.381932]  do_el0_svc+0x24/0x40
[   22.382064]  el0_svc+0x38/0x118
[   22.382204]  el0t_64_sync_handler+0xa0/0xe8
[   22.382334]  el0t_64_sync+0x198/0x1a0
[   22.382567] Code: ffff8000 aa1e03e9 d503201f d2800000 (b900001f)
[   22.382790] ---[ end trace 0000000000000000 ]---
Segmentation fault

Poky (Yocto Project Reference Distro) 6.0.4 qemuarm64 /dev/ttyAMA0
```

## `syslog`

Largely identical output is also found in `/var/log/messages`.

## `objdump`

`objdump` does not appear in my target file system, but does exist in my Yocto build at

```txt
./bitbake-builds/yocto-assignments-wrynose/build/tmp/work/qemuarm64-poky-linux/linux-yocto/6.18.54+git/recipe-sysroot-native/usr/libexec/aarch64-poky-linux/gcc/aarch64-poky-linux/15.3.0/objdump
```

> (This is not named `aarc64-linux-objdump` as indicated in lecture.)

My build's `faulty.ko` for `core-image-aesd` is found here:

```txt
./bitbake-builds/yocto-assignments-wrynose/build/tmp/work/qemuarm64-poky-linux/core-image-aesd/1.0/rootfs/lib/modules/6.18.54-yocto-standard/updates/faulty.ko
```

### Using `objdump`

The call trace above shows failure at symbol `faulty_write`, and disassembly for that symbol (`--disassemble=faulty_write`) yields approximately 126 lines of assembler output for the various module functions. The `--source` option does not alter the output at the moment.

The output for `faulty_write` is:

```arm
0000000000000008 <faulty_write>:
   8:   d503201f        nop
   c:   d503201f        nop
  10:   d2800000        mov     x0, #0x0                        // #0
  14:   b900001f        str     wzr, [x0]
  18:   d65f03c0        ret
  1c:   d503201f        nop
  20:   d503201f        nop
  24:   d503201f        nop
```

### What Have We Found

The instruction at `10` loads a zero into register `x0`.
The instruction at `14` tries to write the contents of register `wzr` to the address held by register `x0`. `x0` contains a zero, so this is a NULL pointer dereference very shortly after the start of `faulty_write`.

> `wzr` is a special purpose register in ARM that always reads as zero.

Examining source for `faulty_write` shows what we already know, a NULL pointer dereference:

```c
/* faulty.c */

ssize_t faulty_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;
	return 0;
}
```
