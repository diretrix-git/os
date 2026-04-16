; =============================================================================
; Stage 1 Bootloader - MBR (Master Boot Record)
; Size: Exactly 512 bytes
; Loaded by BIOS at physical address 0x00007C00
; =============================================================================

[BITS 16]
[ORG 0x7C00]

; =============================================================================
; Entry point - BIOS jumps here after loading the MBR
; =============================================================================
start:
    ; Set up segment registers
    cli                     ; Disable interrupts during setup
    xor ax, ax              ; AX = 0
    mov ds, ax              ; DS = 0 (data segment)
    mov es, ax              ; ES = 0 (extra segment)
    mov ss, ax              ; SS = 0 (stack segment)
    mov sp, 0x7C00          ; Stack grows down from 0x7C00
    sti                     ; Re-enable interrupts

    ; Save boot drive number (passed by BIOS in DL)
    mov [boot_drive], dl

    ; Print "Stage 1 OK" message
    mov si, msg_stage1_ok
    call print_string

    ; =============================================================================
    ; Load Stage 2 from disk
    ; Stage 2 is located at LBA 1 (sector 2, right after MBR)
    ; We load 74 sectors starting at LBA 1 to cover the full stage2 payload
    ; =============================================================================
    
    ; CHS Calculation for LBA 1:
    ; LBA to CHS formula (for standard 1.44MB floppy format):
    ;   Sector = (LBA % sectors_per_track) + 1
    ;   Head = (LBA / sectors_per_track) % num_heads
    ;   Cylinder = LBA / (sectors_per_track * num_heads)
    ; 
    ; Assuming: 18 sectors/track, 2 heads, 36 total sectors per cylinder
    ; LBA 1: Cylinder=0, Head=0, Sector=2
    
    mov dl, [boot_drive]    ; Boot drive
    mov dh, 0               ; Head 0
    mov ch, 0               ; Cylinder 0
    mov cl, 2               ; Sector 2 (sectors are 1-indexed!)
    mov al, 74              ; Number of sectors to read (full stage2 payload)
    mov ah, 0x02            ; BIOS disk read function
    mov bx, 0x7E00          ; Load address (right after MBR)
    
    int 0x13                ; Call BIOS disk interrupt
    
    ; Check for errors
    jc disk_error           ; If carry flag set, read failed
    
    ; =============================================================================
    ; Jump to Stage 2
    ; Stage 2 is loaded at 0x0000:0x7E00 (real mode address)
    ; =============================================================================
    mov si, msg_loading_stage2
    call print_string
    
    jmp 0x0000:0x7E00       ; Far jump to Stage 2

; =============================================================================
; Error handlers
; =============================================================================
disk_error:
    mov si, msg_disk_error
    call print_string
    jmp $                   ; Infinite loop

; =============================================================================
; Helper function: Print null-terminated string
; Input: SI = offset of string
; Uses: AX, BX
; =============================================================================
print_string:
    push ax
    push bx
.loop:
    lodsb                   ; Load byte at SI into AL, increment SI
    or al, al               ; Check if null terminator
    jz .done
    mov ah, 0x0E            ; BIOS teletype function
    mov bx, 0x0007          ; Page 0, attribute 7 (normal text)
    int 0x10                ; Print character
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; =============================================================================
; Data section
; =============================================================================
boot_drive:         db 0
msg_stage1_ok:      db "Stage 1 OK", 13, 10, 0
msg_loading_stage2: db "Loading Stage 2...", 13, 10, 0
msg_disk_error:     db "Disk read error!", 13, 10, 0

; =============================================================================
; Padding and boot signature
; Must be exactly 512 bytes with 0x55AA at the end
; =============================================================================
times 510-($-$$) db 0   ; Pad to 510 bytes
dw 0xAA55               ; Boot signature (magic number)
