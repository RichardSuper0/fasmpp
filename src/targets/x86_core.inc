assemble_x86_instruction_node:
    ; ESI = slot token istruzione, EDI = buffer codice macchina finale
    
    call   decode_all_x86_operands      ; Estrae registri (GPR, ZMM, CR, K) e immediati
    
    ; --- EMISSIONE PREFISSI DI OVERRIDE / COMPATIBILITÀ ---
    mov    al, [prefix_size_override]   ; Prefisso 0x66 per forzare dimensioni
    test   al, al
    jz     .check_vex_evex
    mov    [edi], al
    inc    edi

.check_vex_evex:
    mov    al, [x86_prefix_type]        ; 0=Legacy/REX, 1=VEX2, 2=VEX3, 3=EVEX
    cmp    al, 3
    je     .emit_evex_v5_block
    cmp    al, 2
    je     .emit_vex3_block
    cmp    al, 1
    je     .emit_vex2_block

    ; --- EMISSIONE REX LEGACY (x86_64 v1-v4) ---
    mov    al, [rex_prefix_byte]        ; Prefisso REX standard (W, R, X, B)
    test   al, al
    jz     .emit_base_op
    mov    [edi], al
    inc    edi
    jmp    .emit_base_op

.emit_vex2_block:
    ; Prefisso VEX a 2 byte (AVX/AVX2 - 0xC5)
    mov    al, 0xC5
    mov    [edi], al
    inc    edi
    mov    al, [vex_p0_byte]            ; Contiene R inverso e vvvv
    mov    [edi], al
    inc    edi
    jmp    .emit_base_op

.emit_vex3_block:
    ; Prefisso VEX a 3 byte (AVX/AVX2/FMA - 0xC4)
    mov    al, 0xC4
    mov    [edi], al
    inc    edi
    mov    al, [vex_p0_byte]            ; Contiene R, X, B inversi e mappa m-mmmm
    mov    [edi], al
    inc    edi
    mov    al, [vex_p1_byte]            ; Contiene W, vvvv, L, pp
    mov    [edi], al
    inc    edi
    jmp    .emit_base_op

.emit_evex_v5_block:
    ; Prefisso EVEX a 4 byte (AVX-512 / APX v5 - 0x62)
    mov    al, 0x62
    mov    [edi], al
    inc    edi
    mov    al, [evex_p0_byte]           ; R, X, B, R', 0, 0, m, m
    mov    [edi], al
    inc    edi
    mov    al, [evex_p1_byte]           ; W, v, v, v, v, 1, p, p
    mov    [edi], al
    inc    edi
    mov    al, [evex_p2_byte]           ; z, L', L, b, V', a, a, a
    mov    [edi], al
    inc    edi

.emit_base_op:
    mov    al, [x86_base_opcode]        ; Opcode primario dell'istruzione
    mov    [edi], al
    inc    edi

    ; --- EMISSIONE COMPATTA COMPONENTI MODR/M E SIB ---
    mov    al, [modrm_required]
    test   al, al
    jz     .check_immediate
    
    ; Costruzione ModR/M basata su bit shifting a 32/8 bit
    mov    al, [modrm_mod]
    shl    al, 6
    mov    bl, [modrm_reg]
    and    bl, 0x07                     ; Isola i 3 bit base storici
    shl    bl, 3
    or     al, bl
    mov    bl, [modrm_rm]
    and    bl, 0x07
    or     al, bl
    mov    [edi], al                    ; Scrive byte ModR/M
    inc    edi

.check_immediate:
    ; [... Controllo ed emissione immediati latenti a 8/32/64 bit ...]
    ret

decode_all_x86_operands:
    ; Analizza la stringa per estrarre qualsiasi tipologia di registro
    add    esi, 64
    mov    al, [esi]
    test   al, al
    jz     .decode_exit

    call   classify_any_x86_register    ; Ritorna AL=ID (0-31), AH=Tipo, DL=Estensione
    ; AH Tipo: 1=GPR, 2=ZMM/YMM/XMM, 3=Control/Debug, 4=Mask K, 5=Segment
    ; DL Estensione: 1 se richiede bit superiori (R8-R15, R16-R31, ZMM16-ZMM31)
    
    ; [... Mappatura geometrica dei campi ModR/M, VEX o EVEX a seconda del tipo ...]
.decode_exit:
    ret

classify_any_x86_register:
    ; Analizzatore universale di stringhe registro senza registri a 16-bit
    mov    al, [esi]
    
    cmp    al, 'z'                      ; Controlla vettori ZMM (AVX-512)
    je     .parse_zmm_vector
    cmp    al, 'k'                      ; Controlla registri maschera K
    je     .parse_mask_k
    cmp    al, 'c'                      ; Controlla registri di controllo CR
    je     .parse_special_sys
    cmp    al, 'd'                      ; Controlla registri di debug DR
    je     .parse_special_sys
    
    ; [... Prosegue con lo scanning accelerato di GPR storici ed estensioni APX ...]
    xor    eax, eax
    xor    edx, edx
    ret

.parse_zmm_vector:
    mov    byte [x86_prefix_type], 3    ; Forza l'attivazione immediata di EVEX
    ; [... Estrae ID 0-31 e setta DL=1 se ID >= 16 ...]
    mov    ah, 2                        ; Tipo 2 = Vettoriale ZMM
    ret

.parse_mask_k:
    ; [... Mappa registri k0-k7 per opzioni di masking hardware ...]
    mov    ah, 4                        ; Tipo 4 = Maschera K
    ret

.parse_special_sys:
    ; [... Mappa CR0-CR8 o DR0-DR7 per istruzioni privilegiate di sistema ...]
    mov    ah, 3                        ; Tipo 3 = Sistema/Controllo
    ret

; --- VARIABILI DI STATO UNIVERSALI DEL MOTORINO ---
x86_prefix_type          db 0           ; 0=Legacy, 1=VEX2, 2=VEX3, 3=EVEX
prefix_size_override     db 0           ; Abilita byte 0x66 per override dimensione
rex_prefix_byte          db 0           ; Payload REX per compatibilita 64-bit v1-v4
vex_p0_byte              db 0           ; Payload 1 VEX
vex_p1_byte              db 0           ; Payload 2 VEX
evex_p0_byte             db 0xFC        ; Payload 1 EVEX (default inversione attiva)
evex_p1_byte             db 0x78        ; Payload 2 EVEX
evex_p2_byte             db 0x00        ; Payload 3 EVEX
x86_base_opcode          db 0           ; Opcode fondamentale dell'istruzione corrente
modrm_required           db 1           ; Flag attivazione byte ModR/M
modrm_mod                db 3           ; Default 11b (registro-registro)
modrm_reg                db 0           ; Campo Reg
modrm_rm                 db 0           ; Campo R/M
