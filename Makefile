CC=clang
LD=ld.lld

CFLAGS=--target=x86_64-elf -m64 -I include \
-ffreestanding -fno-builtin \
-fno-stack-protector \
-fno-pic -fno-pie \
-fno-asynchronous-unwind-tables \
-fno-unwind-tables

LDFLAGS=-m elf_x86_64 -T linker.ld

BUILD=build
ESP=esp.img
LIMINE=/e/Project/limine
FIRMWARE=E:/Tools/msys2/qemu/share/edk2-x86_64-code.fd

SRC=kernel/kernel.c kernel/printk.c drivers/vga/vga.c
OBJ=$(SRC:%.c=$(BUILD)/%.o)

all: run

$(BUILD)/%.o: %.c
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
