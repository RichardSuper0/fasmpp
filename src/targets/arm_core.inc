assemble_arm_instruction_node:
    ; ESI = slot token istruzione, EDI = buffer output codice macchina
    push   esi                          ; Preserva ESI per non sfasare il parser centrale
    call   decode_arm_operands          ; Analizza gli slot operandi per riempire rd, rn, rm
    pop    esi                          ; Ripristina ESI originale
    
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
    ; Estrazione dinamica operandi: istruzione -> rd -> rn -> rm (distanti 64 byte l'uno dall'altro)
    add    esi, 64                      
    call   parse_single_arm_register
    mov    [arm_rd], al

    add    esi, 64                      
    call   parse_single_arm_register
    mov    [arm_rn], al

    add    esi, 64                      
    call   parse_single_arm_register
    mov    [arm_rm], al
    ret

parse_single_arm_register:
    push   esi
    mov    al, [esi]
    
    cmp    al, 'w'                      ; Registro 'w' indica ARM64
    je     .set_arm64
    cmp    al, 'x'                      ; Registro 'x' indica ARM64
    je     .set_arm64
    
    mov    byte [arm_mode_width], 32
    jmp    .parse_digits

.set_arm64:
    mov    byte [arm_mode_width], 64

.parse_digits:
    inc    esi                          ; Salta il prefisso ('w', 'x' o 'r')
    movzx  eax, byte [esi]
    sub    eax, '0'                     ; Converte il primo carattere
    cmp    eax, 9
    ja     .invalid_reg                 
    
    movzx  ebx, byte [esi+1]
    test   bl, bl
    jz     .done_parsing                
    
    sub    bl, '0'
    cmp    bl, 9
    ja     .done_parsing                
    
    imul   eax, 10
    add    eax, ebx

.done_parsing:
    mask_reg_id eax                     
    pop    esi
    ret

.invalid_reg:
    xor    eax, eax                     
    pop    esi
    ret

align_to_l1
arm_mode_width     db 64                
arm32_base_opcode  dd 0x00800000        
arm64_base_opcode  dd 0x8B000000        
arm_cond_field     db 0x0E              
arm_rd             db 0
arm_rn             db 0
arm_rm             db 0
