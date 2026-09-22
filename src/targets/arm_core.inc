assemble_arm_instruction_node:
    ; ESI = slot token istruzione, EDI = buffer output codice macchina
    call   decode_arm_operands          ; Analizza lo slot per capire lo stato ARM32/ARM64
    
    mov    al, [arm_mode_width]         ; Rileva larghezza (32 = ARM32, 64 = ARM64 nativo)
    cmp    al, 64
    je     .emit_arm64_word
    
    ; --- GENERAZIONE ARM32 LEGACY (Word a 32-bit fissi con esecuzione condizionale) ---
    mov    eax, [arm32_base_opcode]
    movzx  ebx, byte [arm_rd]
    inject_reg_field eax, ebx, 12       ; ARM32 posiziona RD nei bit 12-15
    movzx  ebx, byte [arm_rn]
    inject_reg_field eax, ebx, 16       ; Posiziona RN nei bit 16-19
    movzx  ebx, byte [arm_rm]
    inject_reg_field eax, ebx, 0        ; Posiziona RM nei bit 0-3
    movzx  ebx, byte [arm_cond_field]   ; Condizione hardware (es. 1110b per ALWAYS)
    inject_imm_field eax, ebx, 0x0F, 28 ; Impacchetta nei bit 28-31
    jmp    .write_arm_word

.emit_arm64_word:
    ; --- GENERAZIONE ARM64 / ARMv9 (A64 standard geometrico) ---
    mov    eax, [arm64_base_opcode]     ; Carica base hardware (es. 0x8B000000 per ADD intero)
    movzx  ebx, byte [arm_rd]
    inject_reg_field eax, ebx, 0        ; ARM64 posiziona RD nei bit 0-4
    movzx  ebx, byte [arm_rn]
    inject_reg_field eax, ebx, 5        ; Posiziona RN nei bit 5-9
    movzx  ebx, byte [arm_rm]
    inject_reg_field eax, ebx, 16       ; Posiziona RM nei bit 16-20

.write_arm_word:
    mov    [edi], eax                   ; Sputa la Word generata sul buffer
    add    edi, 4                       ; Avanza di 4 byte nel codice macchina
    ret

decode_arm_operands:
    add    esi, 64                      ; Passa all'operando di destinazione
    mov    al, [esi]
    cmp    al, 'w'                      ; Registro 'w' o 'x' indica ARM64 nativo A64
    je     .set_arm64_context
    cmp    al, 'x'
    je     .set_arm64_context
    
    ; Altrimenti e un registro 'r' classico (r0-r15) di ARM32
    mov    byte [arm_mode_width], 32
    mov    al, [esi+1]
    sub    al, '0'
    mask_reg_id eax
    mov    [arm_rd], al
    ret

.set_arm64_context:
    mov    byte [arm_mode_width], 64
    mov    al, [esi+1]                  ; Estrae l'ID del registro (0-31)
    sub    al, '0'
    mask_reg_id eax
    mov    [arm_rd], al
    ret

arm_mode_width     db 64                ; Fallback contesto 64-bit nativo (ARMv8/v9)
arm32_base_opcode  dd 0x00800000        ; Base opcode ADD intero ARM32
arm64_base_opcode  dd 0x8B000000        ; Base opcode ADD intero A64
arm_cond_field     db 0x0E              ; Condizione 14 (AL = Always execution)
arm_rd             db 0
arm_rn             db 0
arm_rm             db 0
