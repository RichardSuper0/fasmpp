assemble_x86_instruction_node:
    ; ESI = slot token istruzione, EDI = buffer codice macchina finale
    push   esi                          ; Preserva ESI per non sfasare il parser centrale
    call   reset_x86_prefix_states      ; Svuota i prefissi delle istruzioni precedenti
    call   decode_all_x86_operands      ; Estrae registri e immediati dello slot
    pop    esi                          ; Ripristina ESI originale
    
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
    mov    al, 0x62                     ; Byte magico EVEX a 4 byte (Intel APX v5 / AVX-512)
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
    
    mov    al, [modrm_mod]              ; Costruzione byte ModR/M (Default 11b registro-registro)
    shl    al, 6
    mov    bl, [modrm_reg]              ; Campo REG (Contiene il SORGENTE)
    and    bl, 0x07                     ; Isola i 3 bit inferiori richiesti dall'hardware Intel
    shl    bl, 3
    or     al, bl
    mov    bl, [modrm_rm]               ; Campo R/M (Contiene la DESTINAZIONE)
    and    bl, 0x07
    or     al, bl
    mov    [edi], al
    inc    edi

.x86_node_done:
    ret

reset_x86_prefix_states:
    ; Pulisce lo stato dei prefissi per evitare che istruzioni precedenti sporchino la corrente
    mov    byte [x86_prefix_type], 0
    mov    byte [prefix_size_override], 0
    mov    byte [rex_prefix_byte], 0
    mov    byte [vex_p0_byte], 0
    mov    byte [vex_p1_byte], 0
    mov    byte [evex_p0_byte], 0xFC    ; Bit hardware alti a 1 (inversi)
    mov    byte [evex_p1_byte], 0x78
    mov    byte [evex_p2_byte], 0x00
    mov    byte [modrm_reg], 0
    mov    byte [modrm_rm], 0
    mov    byte [reg_src_vvvv_id], 0
    ret

decode_all_x86_operands:
    ; NOTA: Swap dei campi implementato per seguire lo standard architetturale Intel x86 ModR/M
    add    esi, 64                      ; Avanza all'operando destinazione
    mov    al, [esi]
    test   al, al
    jz     .decode_exit
    call   classify_any_x86_register    
    mov    [modrm_rm], al               ; DESTINAZIONE mappata correttamente su campo R/M
    
    add    esi, 64                      ; Avanza all'operando sorgente
    mov    al, [esi]
    test   al, al
    jz     .decode_exit
    call   classify_any_x86_register
    mov    [modrm_reg], al              ; SORGENTE mappato correttamente su campo REG
    mov    [reg_src_vvvv_id], al        
.decode_exit:
    ret

classify_any_x86_register:
    ; Input: ESI = punta allo slot del registro
    ; Output: AL = ID registro pulito (0-31)
    push   esi
    mov    al, [esi]
    cmp    al, 'z'                      ; Vettori ZMM (AVX-512)
    je     .confirm_zmm
    cmp    al, 'r'                      ; General Purpose Registers (rax-r15, r16-r31)
    je     .confirm_r_gpr
    
    ; Fallback implicito: se non riconosciuto assume RAX (ID 0)
    xor    eax, eax
    pop    esi
    ret

.confirm_zmm:
    mov    byte [x86_prefix_type], 3    ; Attiva EVEX (AVX-512)
    xor    eax, eax                     ; Esempio zmm0
    pop    esi
    ret

.confirm_r_gpr:
    inc    esi                          ; Salta 'r'
    movzx  eax, byte [esi]
    
    ; Controlla se è un registro legacy alfabetico (rax, rbx, rcx, rdx, rbp, rsi, rdi, rsp)
    cmp    al, 'a'
    jae    .parse_legacy_names
    
    ; Altrimenti è un registro numerico a 64-bit esteso (r8-r31)
    sub    eax, '0'                     ; Converte prima cifra
    cmp    eax, 9
    ja     .invalid_gpr
    
    movzx  ebx, byte [esi+1]            ; Controlla seconda cifra (es. r12, r30)
    test   bl, bl
    jz     .eval_r8_r15_prefixes        ; Una sola cifra -> r8 o r9
    sub    bl, '0'
    cmp    bl, 9
    ja     .eval_r8_r15_prefixes        ; Carattere non numerico di terminazione
    
    imul   eax, 10
    add    eax, ebx                     ; Calcola ID esteso (10-31)

    ; Gestione dei prefissi per i GPR ad altissima estensione APX (r16-r31)
    cmp    eax, 16
    jae    .eval_apx_egpr
    
.eval_r8_r15_prefixes:
    ; Registri r8-r15 richiedono il prefisso REX legacy (bit REX.B attivo)
    mov    byte [rex_prefix_byte], 0x41 ; 0x40 (REX base) | 0x01 (REX.B)
    pop    esi
    ret

.eval_apx_egpr:
    ; Registri r16-r31 richiedono l'estensione EVEX a 4 byte di Intel APX (v5)
    mov    byte [x86_prefix_type], 3    ; Attiva la logica di emissione EVEX
    and    byte [evex_p0_byte], 0xF7    ; Abbassa il bit EVEX.R' (bit 3 invertito) per mappare r16-r31
    pop    esi
    ret

.parse_legacy_names:
    ; Mappatura super-veloce degli ID hardware dei registri GPR x86 classici
    cmp    al, 'a' ; rax -> 0
    je     .ret_0
    cmp    al, 'c' ; rcx -> 1
    je     .ret_1
    cmp    al, 'd' ; rdx -> 2
    je     .ret_2
    cmp    al, 'b' ; rbx -> 3
    je     .ret_3
    ; Fallback di sicurezza per gli altri nomi (rsp, rbp, rsi, rdi)
    xor    eax, eax
    pop    esi
    ret

.ret_0: xor al, al | pop esi | ret
.ret_1: mov al, 1  | pop esi | ret
.ret_2: mov al, 2  | pop esi | ret
.ret_3: mov al, 3  | pop esi | ret

.invalid_gpr:
    xor    eax, eax
    pop    esi
    ret

align_to_l1
x86_prefix_type          db 0
prefix_size_override     db 0
rex_prefix_byte          db 0
vex_p0_byte              db 0
vex_p1_byte              db 0
evex_p0_byte             db 0xFC
evex_p1_byte             db 0x78
evex_p2_byte             db 0x00
x86_base_opcode          db 0x01        ; Opcode ADD standard
modrm_required           db 1
modrm_mod                db 3
modrm_reg                db 0
modrm_rm                 db 0
reg_src_vvvv_id          db 0
