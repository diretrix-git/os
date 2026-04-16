; =============================================================================
; Stage 2 Entry Point - Assembly
; Handles transition from 16-bit real mode to 32-bit protected mode
; =============================================================================

[BITS 16]
[ORG 0x7E00]

global _start
extern _stage2_main

_start:
    ; Set up segments and stack in 16-bit mode
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack below at 0x7C00

    ; Enable A20 line
    mov si, msg_stage2_entry_ok
    call print_string
    call enable_a20
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    ; Switch to protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to flush pipeline and load CS with 32-bit code segment
    jmp 0x08:protected_mode_start

[BITS 32]
protected_mode_start:
    ; Now in 32-bit protected mode
    ; Set up segment registers with data segment (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack at 0x90000
    mov esp, 0x90000

    ; Protected-mode debug output to serial port COM1
    mov dx, 0x3F8
    mov al, 'P'
    out dx, al
    mov al, 'M'
    out dx, al
    mov al, 'O'
    out dx, al
    mov al, 'K'
    out dx, al
    mov al, 0x0D
    out dx, al
    mov al, 0x0A
    out dx, al

    ; Call C function (now safe because we're in protected mode)
    ; The C code is loaded right after this bootloader
    ; At address 0x8000 (stage2.bin loaded at sector 2, 4 sectors = 2048 bytes after 0x7E00)
    mov eax, 0x8000
    call eax

    
    ; Should never return, but halt if it does
halt_loop:
    hlt
    jmp halt_loop

; =============================================================================
; Enable A20 line
; =============================================================================
[BITS 16]
enable_a20:
    ; Use port 0x92 to enable A20 in a simpler, more reliable way for QEMU
    in al, 0x92
    or al, 0x02
    out 0x92, al
    ret

; =============================================================================
; Helper function: Print null-terminated string in real mode
; Uses: AX, BX, SI
; =============================================================================
print_string:
    push ax
    push bx
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; =============================================================================
; GDT (Global Descriptor Table)
; =============================================================================
align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF

gdt_descriptor:
    dw gdt_descriptor - gdt_start - 1
    dd gdt_start

msg_stage2_entry_ok: db "Stage 2 entry OK", 13, 10, 0

; Pad stage2 entry to a full 512-byte sector so stage2 C code begins at 0x8000.
; Stage 1 loads stage2.bin starting at 0x7E00, and the C payload must start
; at 0x8000 to match the protected-mode call.
    times 512-(($-$$)) db 0