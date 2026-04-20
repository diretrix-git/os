# Makefile — Sora OS Kernel
# Target: i686 freestanding ELF32, GRUB Multiboot
# Host:   Windows 11, WSL (Debian/Ubuntu)

# ── Toolchain ────────────────────────────────────────────────────────────────
CC  := i686-linux-gnu-gcc
AS  := nasm
LD  := i686-linux-gnu-ld

# ── Flags ────────────────────────────────────────────────────────────────────
CFLAGS  := -ffreestanding -nostdlib -nostdinc -m32 -O2 -Wall -Wextra -std=gnu99 -Iinclude
ASFLAGS := -f elf32
LDFLAGS := -T linker.ld -nostdlib -m elf_i386

# ── Directories ───────────────────────────────────────────────────────────────
SRC_DIR   := src
BUILD_DIR := build

# ── Sources ───────────────────────────────────────────────────────────────────
C_SRCS   := $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS := $(wildcard $(SRC_DIR)/*.asm)

C_OBJS   := $(patsubst $(SRC_DIR)/%.c,   $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS := $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(ASM_SRCS))
OBJS     := $(ASM_OBJS) $(C_OBJS)

# ── Targets ───────────────────────────────────────────────────────────────────
.PHONY: all run debug clean iso

all: $(BUILD_DIR) kernel.bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

run: kernel.bin
	qemu-system-i386 -kernel kernel.bin -serial stdio

debug: kernel.bin
	qemu-system-i386 -kernel kernel.bin -serial stdio -s -S

iso: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin
	grub-mkrescue -o sora.iso iso

clean:
	rm -rf $(BUILD_DIR) kernel.bin sora.iso
