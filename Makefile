CC=clang
LD=ld.lld

CFLAGS=--target=x86_64-elf -m64 -I include \
-ffreestanding -fno-builtin \
-fno-stack-protector \
-fno-pic -fno-pie -mno-red-zone \
-fno-asynchronous-unwind-tables \
-fno-unwind-tables

LDFLAGS=-m elf_x86_64 -T linker.ld

BUILD=build
ESP=esp.img
LIMINE=/e/Project/limine
FIRMWARE=E:/Tools/msys2/qemu/share/edk2-x86_64-code.fd

SRC=kernel/kernel.c \
kernel/boot/limine.c \
kernel/arch/x86_64/serial.c \
kernel/arch/x86_64/gdt.c \
kernel/arch/x86_64/idt.c \
kernel/arch/x86_64/pic.c \
kernel/arch/x86_64/paging.c \
kernel/arch/x86_64/timer.c \
kernel/arch/x86_64/syscall.c \
kernel/memory/memmap.c \
kernel/memory/pmm.c \
kernel/memory/heap.c \
kernel/memory/vmm.c \
kernel/sched/task.c \
kernel/sched/wait_queue.c \
kernel/sched/sched.c \
kernel/sys/syscall.c \
kernel/fs/vfs.c \
kernel/fs/fd.c \
kernel/fs/tmpfs.c \
kernel/video/framebuffer.c \
kernel/lib/util.c \
kernel/printk.c \
drivers/vga/vga.c
ASMSRC=kernel/sched/context.S \
kernel/arch/x86_64/gdt_load.S \
kernel/arch/x86_64/isr32.S \
kernel/arch/x86_64/isr80.S \
kernel/arch/x86_64/isr13.S \
kernel/arch/x86_64/isr14.S \
kernel/arch/x86_64/syscall_entry.S \
kernel/arch/x86_64/user.S
OBJ=$(SRC:%.c=$(BUILD)/%.o) $(ASMSRC:%.S=$(BUILD)/%.o)

all: run

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


kernel: $(OBJ)
	$(LD) $(LDFLAGS) -o $(BUILD)/kernel.elf $(OBJ)

esp: kernel
	rm -f $(ESP)
	dd if=/dev/zero of=$(ESP) bs=1M count=64
	mformat -F -i $(ESP) ::
	mmd -i $(ESP) ::/EFI
	mmd -i $(ESP) ::/EFI/BOOT
	mcopy -i $(ESP) $(LIMINE)/BOOTX64.EFI ::/EFI/BOOT/
	mcopy -i $(ESP) $(BUILD)/kernel.elf ::/
	mcopy -i $(ESP) iso/boot/limine.conf ::/EFI/BOOT/

run: esp
	qemu-system-x86_64 \
	-drive if=pflash,format=raw,readonly=on,file=$(FIRMWARE) \
	-drive format=raw,file=$(ESP) \
	-m 512M \
	-serial stdio \
	-no-reboot \
	-no-shutdown

clean:
	rm -rf $(BUILD)
	rm -f $(ESP)
