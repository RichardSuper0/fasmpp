assemble_x86_instruction_node:
    ; ESI = slot token istruzione, EDI = buffer codice macchina finale
    call   decode_all_x86_operands      ; Estrae registri e immediati dello slot
    
    mov    al, [prefix_size_override]   ; Override dimensione (0x66) per x86-16
    test   al, al
    jz     .check_vex_evex
    mov    [edi], al
    inc    edi

.check_vex_evex:
    mov    al, [x86_prefix_type]        ; 0=Legacy, 1=VEX2, 2=VEX3, 3=EVEX
    cmp    al, 3
    je     .emit_evex_v5
    cmp    al, 2
    je     .emit_vex3
    cmp    al, 1
    je     .emit_vex2

    mov    al, [rex_prefix_byte]        ; REX legacy per x86_64v1-v4
    test   al, al
    jz     .emit_base_op
    mov    [edi], al
    inc    edi
    jmp    .emit_base_op

.emit_vex2:
    mov    al, 0xC5                     ; Byte magico VEX a 2 byte (AVX)
    mov    [edi], al
    inc    edi
    mov    al, [vex_p0_byte]
    mov    [edi], al
    inc    edi
    jmp    .emit_base_op

.emit_vex3:
    mov    al, 0xC4                     ; Byte magico VEX a 3 byte (AVX2/FMA)
    mov    [edi], al
    inc    edi
    mov    al, [vex_p0_byte]
    mov    [edi], al
    inc    edi
    mov    al, [vex_p1_byte]
    mov    [edi], al
    inc    edi
    jmp    .emit_base_op

.emit_evex_v5:
    mov    al, 0x62                     ; Byte magico EVEX a 4 byte (v5 APX)
    mov    [edi], al
    inc    edi
    mov    al, [evex_p0_byte]           ; payload P0 (R, X, B, R' inversi)
    mov    [edi], al
    inc    edi
    mov    al, [evex_p1_byte]           ; payload P1 (W, vvvv sorgente)
    mov    [edi], al
    inc    edi
    mov    al, [evex_p2_byte]           ; payload P2 (z, L'L, b=APX NF, aaa)
    mov    [edi], al
    inc    edi

.emit_base_op:
    mov    al, [x86_base_opcode]        ; Scrive l'opcode primario dell'istruzione
    mov    [edi], al
    inc    edi

    mov    al, [modrm_required]         ; Controllo ed emissione ModR/M
    test   al, al
    jz     .x86_node_done
    
    mov    al, [modrm_mod]              ; Costruzione byte ModR/M
    shl    al, 6
    mov    bl, [modrm_reg]
    and    bl, 0x07
    shl    bl, 3
    or     al, bl
    mov    bl, [modrm_rm]
    and    bl, 0x07
    or     al, bl
    mov    [edi], al
    inc    edi

.x86_node_done:
    ret

decode_all_x86_operands:
    add    esi, 64                      ; Avanza all'operando destinazione
    mov    al, [esi]
    test   al, al
    jz     .decode_exit
    call   classify_any_x86_register    ; Identifica tipo registro ed estensioni
    mov    [modrm_reg], al
    
    add    esi, 64                      ; Avanza all'operando sorgente
    mov    al, [esi]
    test   al, al
    jz     .decode_exit
    call   classify_any_x86_register
    mov    [modrm_rm], al
    mov    [reg_src_vvvv_id], al        ; Mappa provvisoriamente anche per EVEX vvvv
.decode_exit:
    ret

classify_any_x86_register:
    mov    al, [esi]
    cmp    al, 'z'                      ; Controllo vettori ZMM (AVX-512)
    je     .confirm_zmm
    cmp    al, 'r'                      ; Controllo GPR estesi r8-r31 o legacy
    je     .confirm_r_gpr
    xor    al, al                       ; Default RAX / ID 0
    ret
.confirm_zmm:
    mov    byte [x86_prefix_type], 3    ; Forza EVEX a 4 byte per AVX-512
    xor    al, al
    ret
.confirm_r_gpr:
    mov    bl, [esi+1]
    cmp    bl, '1'                      ; Controllo se numerico esteso
    jae    .eval_apx_egpr
    xor    al, al
    ret
.eval_apx_egpr:
    mov    cl, [esi+2]                  ; Legge seconda cifra per R16-R31
    test   cl, cl
    jz     .legacy_r8_r15
    mov    byte [x86_prefix_type], 3    ; Forza EVEX per estensione APX v5 EGPR
    mov    al, 16                       ; Esempio base ID registro APX
    ret
.legacy_r8_r15:
    mov    byte [rex_prefix_byte], 0x41 ; Forza REX.B per r8-r15
    mov    al, 8
    ret

x86_prefix_type          db 0           ; 0=Legacy, 1=VEX2, 2=VEX3, 3=EVEX
prefix_size_override     db 0           ; 0x66 attivo se 1
rex_prefix_byte          db 0           ; REX byte
vex_p0_byte              db 0
vex_p1_byte              db 0
evex_p0_byte             db 0xFC        ; Default bit inversi a 1
evex_p1_byte             db 0x78
evex_p2_byte             db 0x00
x86_base_opcode          db 0x01        ; Esempio opcode base ADD
modrm_required           db 1
modrm_mod                db 3           ; Default registro-registro (11b)
modrm_reg                db 0
modrm_rm                 db 0
reg_src_vvvv_id          db 0
