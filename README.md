<div align="center">
  <img src="logo.png" alt="Kyuzen OS Logo" width="200" height="200" />

  # Kyuzen OS

  **A 64-bit Higher-Half Operating System Built from Scratch**

  ![Arch](https://img.shields.io/badge/arch-x86__64-blue)
  ![Compiler](https://img.shields.io/badge/compiler-clang-orange)
  ![Bootloader](https://img.shields.io/badge/bootloader-limine-lightgrey)
  ![Format](https://img.shields.io/badge/executable-ELF-yellow)
  ![License](https://img.shields.io/badge/license-MIT-green)
</div>

Kyuzen OS is a monolithic-style operating system designed to interact at the Bare Metal level featuring advanced memory management, a virtual file system, and User Space application execution.

## ✨ Key Features
- **64-bit Higher-Half Kernel:** Secure mapping at high virtual memory addresses (above `0xFFFFFFFF80000000`).
- **Limine Boot Protocol:** Dual BIOS & UEFI support out-of-the-box.
- **Custom File System (KyuzenFS):** Robust VFS integration on top of an ATA storage device driver.
- **Dynamic ELF Execution:** Capable of running standard `.elf` binary programs (Calculator, Notepad, File Manager, etc.).
- **GUI & Peripherals:** Comprehensive support for PS/2 Mouse, Keyboard, RTC, and a user interface framework (`libgui`).

## 📂 Directory Structure
| Directory | Main Function |
| --- | --- |
| `arch/x86/` | Architecture-specific code (Low-level: GDT, IDT, ISR, Assembly). |
| `kernel/` | Core logic: PMM, VMM (Paging), Heap Allocation, Syscalls, Task Manager. |
| `drivers/` | Hardware interaction: Storage (ATA), Mouse, Keyboard, PCI, Timer, TTY. |
| `fs/` | Virtual File System (VFS) abstraction layer and KyuzenFS implementation. |
| `apps/` & `user_apps/`| User Space directory (`userlib.c` wrapper, `libgui.c`, and application source code). |
| `include/` | Global header files repository (`.h`). |
| `limine/` | Pre-compiled bootloader binaries and installation utilities. |

## 🛠️ Prerequisites
Ensure your build environment has the following dependencies set up:
- **Clang/LLVM** (`clang` compiler and `ld.lld` linker)
- **NASM** (Netwide Assembler)
- **GNU Make**
- **Xorriso** (Hybrid ISO creator)
- **QEMU** (`qemu-system-x86_64`)

## 🚀 Build Guide

### 1. Compile OS & Applications
Compile the main kernel into `myos.bin` format and build user space programs:
```bash
make all
make apps
```

### 2. Build Bootable ISO
Package the kernel, `limine.conf` configuration script, user apps, and logo into an `.iso` image file:
```bash
make boot_image.iso
```

### 3. Simulator Execution (QEMU)
Run the packaged OS in a virtual machine:
```bash
make run
```
> **Info:** *This command will execute QEMU with optimal parameters: `-cpu max -m 512M` and load an additional virtual disk.*

### 4. Clean Compilation Cache
Use this command after you make significant changes to the main structure/code:
```bash
make clean
make clean-apps
```

## 📄 License
This project is licensed and distributed under the **MIT License**. You are free to use, modify, and redistribute this source code.

<br/>
<div align="center">
  <i>Designed using modern Clang toolchain (-mno-red-zone, -mcmodel=kernel) for stable kernel-level performance.</i>
</div>
