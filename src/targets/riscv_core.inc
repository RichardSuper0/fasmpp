assemble_riscv_instruction_node:
    ; ESI = slot token istruzione, EDI = buffer output codice macchina
    push   esi                          ; Preserva ESI per non sfasare il parser centrale
    call   decode_riscv_operands        ; Isola i campi dei registri e immediati
    pop    esi                          ; Ripristina ESI originale
    
    mov    al, [riscv_fmt_type]         ; Rileva formato geometrico (1=R, 2=I, 3=S, 4=B)
    cmp    al, 2
    je     .emit_i_type
    
    ; --- FORMATO R-TYPE (Istruzioni registro-registro) ---
    mov    eax, [riscv_base_opcode]     ; Carica l'opcode base (7 bit inferiori)
    movzx  ebx, byte [riscv_rd]
    inject_reg_field eax, ebx, 7        ; Inietta campo RD (bit 7-11)
    movzx  ebx, byte [riscv_funct3]
    inject_imm_field eax, ebx, 0x07, 12 ; Inietta campo funct3 (bit 12-14)
    movzx  ebx, byte [riscv_rs1]
    inject_reg_field eax, ebx, 15       ; Inietta campo RS1 (bit 15-19)
    movzx  ebx, byte [riscv_rs2]
    inject_reg_field eax, ebx, 20       ; Inietta campo RS2 (bit 20-24)
    movzx  ebx, byte [riscv_funct7]
    inject_imm_field eax, ebx, 0x7F, 25 ; Inietta campo funct7 (bit 25-31)
    jmp    .write_riscv_word

.emit_i_type:
    ; --- FORMATO I-TYPE (Istruzioni registro-immediato) ---
    mov    eax, [riscv_base_opcode]
    movzx  ebx, byte [riscv_rd]
    inject_reg_field eax, ebx, 7
    movzx  ebx, byte [riscv_funct3]
    inject_imm_field eax, ebx, 0x07, 12
    movzx  ebx, byte [riscv_rs1]
    inject_reg_field eax, ebx, 15
    mov    ebx, [riscv_imm]             ; Carica interamente l'immediato a 32-bit dword
    inject_imm_field eax, ebx, 0xFFF, 20 ; Impacchetta nei bit alti 20-31

.write_riscv_word:
    mov    [edi], eax                   ; Sputa la Word fissa a 32-bit sul buffer
    add    edi, 4                       ; Avanza rigido di 4 byte sul buffer macchina
    ret

decode_riscv_operands:
    ; Spostamento progressivo a slot da 64 byte
    add    esi, 64                      ; Legge registro destinazione (RD)
    call   extract_riscv_reg_id
    mov    [riscv_rd], al
    
    add    esi, 64                      ; Legge registro sorgente 1 (RS1)
    call   extract_riscv_reg_id
    mov    [riscv_rs1], al
    
    add    esi, 64                      ; Legge terzo operando (RS2 o Immediato)
    mov    al, [esi]
    cmp    al, 'x'                      ; Se inizia con 'x' è un registro, altrimenti immediato
    je     .operand_is_reg
    
    ; --- GESTIONE IMMEDIATO NUMERICO REALE ---
    mov    byte [riscv_fmt_type], 2     ; Commuta la geometria su I-TYPE
    call   parse_riscv_immediate        ; Estrae numericamente la stringa intera
    mov    [riscv_imm], eax             ; Scrive l'intero valore a 32-bit (risolve dword clear)
    ret

.operand_is_reg:
    mov    byte [riscv_fmt_type], 1     ; Commuta la geometria su R-TYPE
    call   extract_riscv_reg_id
    mov    [riscv_rs2], al
    ret

extract_riscv_reg_id:
    push   esi
    inc    esi                          ; Salta la 'x'
    movzx  eax, byte [esi]
    sub    eax, '0'                     ; Prima cifra
    cmp    eax, 9
    ja     .invalid_reg
    
    movzx  ebx, byte [esi+1]            ; Controlla se c'è una seconda cifra
    test   bl, bl
    jz     .done_reg
    sub    bl, '0'
    cmp    bl, 9
    ja     .done_reg
    
    imul   eax, 10
    add    eax, ebx                     ; (Cifra1 * 10) + Cifra2

.done_reg:
    mask_reg_id eax                     ; Forza la maschera a 5 bit (0-31)
    pop    esi
    ret
.invalid_reg:
    xor    eax, eax
    pop    esi
    ret

parse_riscv_immediate:
    ; Converte una stringa ASCII numerica puntata da ESI in un intero in EAX
    push   esi
    xor    eax, eax                     ; Accumulatore risultato
    xor    ecx, ecx                     ; Registro di sbrigo per i caratteri

.imm_loop:
    movzx  ecx, byte [esi]
    test   cl, cl
    jz     .imm_done
    sub    cl, '0'
    cmp    cl, 9
    ja     .imm_done                    ; Si ferma se incontra caratteri non numerici
    
    imul   eax, 10
    add    eax, ecx
    inc    esi
    jmp    .imm_loop

.imm_done:
    pop    esi
    ret

align_to_l1
riscv_fmt_type      db 1                ; 1=R-Type, 2=I-Type
riscv_base_opcode   dd 0x33             ; Opcode base intero standard RISC-V OP (ADD)
riscv_rd            db 0
riscv_rs1           db 0
riscv_rs2           db 0
riscv_funct3        db 0                ; Da mappare dinamicamente (es. 0x0 per ADD)
riscv_funct7        db 0                ; Da mappare dinamicamente (es. 0x00 per ADD)
riscv_imm           dd 0
