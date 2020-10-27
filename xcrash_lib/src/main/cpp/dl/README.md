# xCrash DL

## Problem

All the time, Android's DL series functions have some compatibility and ability issues:

### `dl_iterate_phdr()`

1. On arm32 arch, only available when Android >= 5.0.
2. It does not hold the linker's global lock in Android 5.x and 4.x, this may cause a crash during iterating.
3. In some Android 4.x and 5.x devices, it returns basename instead of full pathname.
4. In some Android 4.x and 5.x devices, it returns package name for `app_process` instead of `/system/bin/app_process32` or `/system/bin/app_process64`.
5. linker/linker64 is only included since Android 8.1. (AOSP has included linker/linker64 since 8.0, but a large number of devices from other manufacturers have included linker/linker64 since Android 8.1)

### `dlopen()` & `dlsym()`

1. Since Android 7.0, `dlopen()` and `dlsym()` cannot operate system libraries. (Although in most cases, we don’t really need to load the dynamic library from the disk, but just need to get the address of a function to call it.)
2. `dlsym()` can only obtain symbols in `.dynsym`, but we sometimes need to obtain internal symbols in `.symtab` and "`.symtab` in `.gnu_debugdata`".
3. `dlsym()` does not distinguish between functions and objects when searching for symbols. In ELF files with a lot of symbols, this will reduce search efficiency, especially for `.symtab`.

## Solution

xCrash DL tries to make up the above problems. Enjoy the code ~

In addition, the source code of this module (in the current directory) does not depend on any other source code of xCrash, so you can easily use them in your own projects.

## Usage

### `xc_dl_iterate()`

```c
#define XC_DL_DEFAULT     0x00
#define XC_DL_WITH_LINKER 0x01

typedef int (*xc_dl_iterate_cb_t)(struct dl_phdr_info *info, size_t size, void *arg);
int xc_dl_iterate(xc_dl_iterate_cb_t cb, void *cb_arg, int flags);
```

Similar to `dl_iterate_phdr()`.

Difference from the original `dl_iterate_phdr()`, xCrach DL's `xc_dl_iterate()` has an additional "flags" parameter. If you confirm that you need to iterate to the linker/linker64, pass `XC_DL_WITH_LINKER` to the "flags" parameter, otherwise pass `XC_DL_DEFAULT`.

The reason for this is that in Android < 8.1, we need to find the linker/linker64 from `/proc/self/maps`, which has additional overhead.

### `xc_dl_open()` and `xc_dl_close()`

```c
#define XC_DL_DYNSYM 0x01
#define XC_DL_SYMTAB 0x02
#define XC_DL_ALL    (XC_DL_DYNSYM | XC_DL_SYMTAB)

typedef struct xc_dl xc_dl_t;

xc_dl_t *xc_dl_open(const char *pathname, int flags);
void xc_dl_close(xc_dl_t **self);
```

`xc_dl_close()` and `dlclose()` have the same usage.

`xc_dl_open()` and `dlopen()` are similar. But you need to specify whether you need to "open" `.dynsym` (`XC_DL_DYNSYM`), `.symtab` (`XC_DL_SYMTAB`), or both (`XC_DL_ALL`) through the "flags" parameter.

You can “open” ELF by basename or full pathname. However, Android has used the namespace mechanism since 8.0. If you use basename, you need to make sure that no duplicate ELF is loaded into the current process. `xc_dl_open()` will only return the first matching ELF.

This is part of the `/proc/self/maps` of a certain process on Android 10:

```
......
756fc2c000-756fc7c000 r--p 00000000 fd:03 2985  /system/lib64/vndk-sp-29/libc++.so
756fc7c000-756fcee000 --xp 00050000 fd:03 2985  /system/lib64/vndk-sp-29/libc++.so
756fcee000-756fcef000 rw-p 000c2000 fd:03 2985  /system/lib64/vndk-sp-29/libc++.so
756fcef000-756fcf7000 r--p 000c3000 fd:03 2985  /system/lib64/vndk-sp-29/libc++.so
......
7571fdd000-757202d000 r--p 00000000 07:38 20    /apex/com.android.conscrypt/lib64/libc++.so
757202d000-757209f000 --xp 00050000 07:38 20    /apex/com.android.conscrypt/lib64/libc++.so
757209f000-75720a0000 rw-p 000c2000 07:38 20    /apex/com.android.conscrypt/lib64/libc++.so
75720a0000-75720a8000 r--p 000c3000 07:38 20    /apex/com.android.conscrypt/lib64/libc++.so
......
760b9df000-760ba2f000 r--p 00000000 fd:03 2441  /system/lib64/libc++.so
760ba2f000-760baa1000 --xp 00050000 fd:03 2441  /system/lib64/libc++.so
760baa1000-760baa2000 rw-p 000c2000 fd:03 2441  /system/lib64/libc++.so
760baa2000-760baaa000 r--p 000c3000 fd:03 2441  /system/lib64/libc++.so
......
756fb2f000-756fb31000 r--p 00000000 fd:03 2983  /system/lib64/vndk-sp-29/libbinderthreadstate.so
756fb31000-756fb33000 --xp 00002000 fd:03 2983  /system/lib64/vndk-sp-29/libbinderthreadstate.so
756fb33000-756fb34000 rw-p 00004000 fd:03 2983  /system/lib64/vndk-sp-29/libbinderthreadstate.so
756fb34000-756fb35000 r--p 00005000 fd:03 2983  /system/lib64/vndk-sp-29/libbinderthreadstate.so
......
76090f9000-76090fb000 r--p 00000000 fd:03 2424  /system/lib64/libbinderthreadstate.so
76090fb000-76090fd000 --xp 00002000 fd:03 2424  /system/lib64/libbinderthreadstate.so
76090fd000-76090fe000 rw-p 00004000 fd:03 2424  /system/lib64/libbinderthreadstate.so
76090fe000-76090ff000 r--p 00005000 fd:03 2424  /system/lib64/libbinderthreadstate.so
......
```

### `xc_dl_dynsym_func()` and `xc_dl_dynsym_object()`

```c
void *xc_dl_dynsym_func(xc_dl_t *self, const char *sym_name);
void *xc_dl_dynsym_object(xc_dl_t *self, const char *sym_name);
```

Used to find functions and objects in `.dynsym`.

### `xc_dl_symtab_func()` and `xc_dl_symtab_object()`

```c
void *xc_dl_symtab_func(xc_dl_t *self, const char *sym_name);
void *xc_dl_symtab_object(xc_dl_t *self, const char *sym_name);
```

Used to find functions and objects in `.symtab` and "`.symtab` in `.gnu_debugdata`".

## History

xCrash 2.x contains a very rudimentary module "xc_dl" for searching system library symbols, which has many problems in performance and compatibility. xCrash uses it to search a few symbols from libart, libc and libc++.

Later, some other projects began to use the "xc_dl" module alone, including in some performance-sensitive usage scenarios. At this time, we began to realize that we need to rewrite this module, and we need a better implementation.
