# MyOS Makefile
# Builds a two-stage bootloader and kernel for x86 32-bit

SHELL := C:/msys64/usr/bin/sh.exe

# Toolchain
CC = gcc.exe
LD = ld.exe
NASM = /c/msys64/ucrt64/bin/nasm.exe
OBJCOPY = objcopy.exe
QEMU = C:/msys64/ucrt64/bin/qemu-system-i386.exe

# Directories
BOOT_DIR = boot
KERNEL_DIR = kernel
BUILD_DIR = build

# Output files
OS_IMAGE = myos.img

# Compiler flags
CFLAGS = -ffreestanding -fno-stack-protector -nostdlib -nodefaultlibs \
         -O2 -Wall -Wextra -I$(KERNEL_DIR)
LDFLAGS = -T linker.ld -nostdlib

# NASM flags
NASM_FLAGS = -f bin

# Source files
STAGE1_SRC = $(BOOT_DIR)/stage1.asm
STAGE2_ENTRY_SRC = $(BOOT_DIR)/stage2_entry.asm
STAGE2_SRC = $(BOOT_DIR)/stage2.c

KERNEL_SRCS = $(wildcard $(KERNEL_DIR)/*.c)
KERNEL_ASM_SRCS = $(KERNEL_DIR)/isr.asm $(KERNEL_DIR)/switch.asm

# Object files
STAGE2_ENTRY_OBJ = $(BUILD_DIR)/stage2_entry.o
STAGE2_OBJ = $(BUILD_DIR)/stage2.o
KERNEL_OBJS = $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS))
KERNEL_ASM_OBJS = $(patsubst $(KERNEL_DIR)/%.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRCS))

# All object files for kernel
ALL_KERNEL_OBJS = $(KERNEL_OBJS) $(KERNEL_ASM_OBJS)

.PHONY: all clean run debug

all: $(BUILD_DIR) $(OS_IMAGE)

$(BUILD_DIR):
	python -c "from pathlib import Path; Path(r'$(BUILD_DIR)').mkdir(parents=True, exist_ok=True)"

# =============================================================================
# Stage 1 Bootloader (MBR)
# =============================================================================
$(BUILD_DIR)/stage1.bin: $(STAGE1_SRC) | $(BUILD_DIR)
	$(NASM) $(NASM_FLAGS) $< -o $@

# =============================================================================
# Stage 2 Bootloader
# =============================================================================
$(BUILD_DIR)/stage2_entry.bin: $(STAGE2_ENTRY_SRC) | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

$(BUILD_DIR)/stage2.o: $(STAGE2_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -m32 -c $< -o $@ -fno-pic -fno-pie

STAGE2_LD = stage2.ld

$(BUILD_DIR)/stage2.elf: $(BUILD_DIR)/stage2.o $(STAGE2_LD)
	$(LD) -m i386pe -Ttext=0x8000 -T $(STAGE2_LD) -o $@ $<

$(BUILD_DIR)/stage2_c.bin: $(BUILD_DIR)/stage2.elf
	$(OBJCOPY) --change-section-vma .text=-0x8000 --change-section-vma .rdata=-0x8000 \
		--change-section-vma .rodata=-0x8000 --change-section-vma .data=-0x8000 -O binary $< $@

$(BUILD_DIR)/stage2.bin: $(BUILD_DIR)/stage2_entry.bin $(BUILD_DIR)/stage2_c.bin
	python -c "from pathlib import Path; out=Path(r'$(BUILD_DIR)/stage2.bin'); out.write_bytes(Path(r'$(BUILD_DIR)/stage2_entry.bin').read_bytes() + Path(r'$(BUILD_DIR)/stage2_c.bin').read_bytes())"

# =============================================================================
# Kernel
# =============================================================================
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(CC) $(CFLAGS) -m32 -c $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.asm
	$(NASM) -f win32 $< -o $@

$(BUILD_DIR)/isr.o: $(KERNEL_DIR)/isr.asm
	$(NASM) -f win32 $< -o $@

$(BUILD_DIR)/switch.o: $(KERNEL_DIR)/switch.asm
	$(NASM) -f win32 $< -o $@

$(BUILD_DIR)/kernel.bin: $(ALL_KERNEL_OBJS)
	$(LD) -m i386pe -Ttext=0x100000 -T linker.ld -o $(BUILD_DIR)/kernel.elf $^
	$(OBJCOPY) -O binary $(BUILD_DIR)/kernel.elf $@

# =============================================================================
# Create disk image
# =============================================================================
ifeq ($(OS),Windows_NT)
$(OS_IMAGE): $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin
	python -c "from pathlib import Path; img=Path(r'$(OS_IMAGE)'); img.write_bytes(b'\x00'*10485760)"
	python -c "from pathlib import Path; img=Path(r'$(OS_IMAGE)'); out=img.open('r+b'); out.seek(0); out.write(Path(r'$(BUILD_DIR)/stage1.bin').read_bytes()); out.close()"
	python -c "from pathlib import Path; img=Path(r'$(OS_IMAGE)'); out=img.open('r+b'); out.seek(512); out.write(Path(r'$(BUILD_DIR)/stage2.bin').read_bytes()); out.close()"
	python -c "from pathlib import Path; img=Path(r'$(OS_IMAGE)'); out=img.open('r+b'); out.seek(5632); out.write(Path(r'$(BUILD_DIR)/kernel.bin').read_bytes()); out.close()"
	@echo "OS image created: $(OS_IMAGE)"
else
$(OS_IMAGE): $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin
	# Create a 10MB disk image
	dd if=/dev/zero of=$(OS_IMAGE) bs=1M count=10 2>/dev/null
	
	# Write stage 1 (MBR) at sector 0
	dd if=$(BUILD_DIR)/stage1.bin of=$(OS_IMAGE) conv=notrunc bs=512 seek=0 2>/dev/null
	
	# Write stage 2 at sector 1 (LBA 1)
	dd if=$(BUILD_DIR)/stage2.bin of=$(OS_IMAGE) conv=notrunc bs=512 seek=1 2>/dev/null
	
	# Write kernel at sector 11 (LBA 11)
	dd if=$(BUILD_DIR)/kernel.bin of=$(OS_IMAGE) conv=notrunc bs=512 seek=11 2>/dev/null
	
	@echo "OS image created: $(OS_IMAGE)"
endif

# =============================================================================
# Run in QEMU
# =============================================================================
run: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE) -m 32M -serial stdio -monitor none -nographic -device i8042

debug: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE) -m 32M -serial stdio -monitor none -nographic -device i8042 \
		-d int,cpu_reset -no-reboot -no-shutdown

clean:
	python -c "import os,shutil; p=r'$(BUILD_DIR)'; shutil.rmtree(p) if os.path.isdir(p) else None; p=r'$(OS_IMAGE)'; os.remove(p) if os.path.isfile(p) else None"
