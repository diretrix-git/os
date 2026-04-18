; =============================================================================
; Context Switch Function - Fixed Version
; =============================================================================

[BITS 32]
section .text

global _switch_context
_switch_context:
    ; Parameters at entry:
    ; [esp+4]  = old_ctx
    ; [esp+8]  = new_ctx
    
    push ebp
    push ebx
    push esi
    push edi
    
    ; Stack layout after pushes:
    ; [esp+0]  = edi
    ; [esp+4]  = esi
    ; [esp+8]  = ebx
    ; [esp+12] = ebp (saved)
    ; [esp+16] = return addr
    ; [esp+20] = old_ctx (param 1)
    ; [esp+24] = new_ctx (param 2)
    
    ; === SAVE CURRENT CONTEXT ===
    mov ebp, [esp+20]         ; old_ctx pointer
    
    mov [ebp+0], eax
    mov [ebp+4], ebx
    mov [ebp+8], ecx
    mov [ebp+12], edx
    mov [ebp+16], esi
    
    mov edi, [esp+0]          ; load edi from stack
    mov [ebp+20], edi         ; save to context
    
    mov edi, [esp+12]         ; load saved ebp from stack
    mov [ebp+24], edi         ; save to context
    
    ; === LOAD FROM NEW CONTEXT ===
    mov ebp, [esp+24]         ; new_ctx pointer
    
    mov eax, [ebp+0]
    mov ebx, [ebp+4]
    mov ecx, [ebp+8]
    mov edx, [ebp+12]
    mov esi, [ebp+16]
    mov edi, [ebp+20]
    
    ; Cache new esp and ds BEFORE overwriting ebp
    mov ecx, [ebp+28]         ; new esp
    mov edx, [ebp+44]         ; new ds
    
    ; Restore ebp from new context
    mov ebp, [ebp+24]
    
    ; Load segment register
    mov ds, edx
    
    ; NOW switch stack
    mov esp, ecx
    
    ; Pop saved registers from new stack
    pop ebp
    pop edi
    pop esi
    pop ebx
    
    ; Return to new eip
    ret
