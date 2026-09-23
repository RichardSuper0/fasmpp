execute_universal_parser:
    ; ESI = token_scratch_matrix, EDI = buffer codice finale
    
.parser_main_loop:
    mov    al, [esi]
    cmp    al, 0xFF                     ; Verifica fine dei token (EOF)
    je     .parser_exit

    test   al, al                       ; Salta slot vuoti
    jz     .parser_next

    cmp    dword [core_error_code], 0   ; Se è scattato un errore in una sotto-funzione, abortisce
    jnz    .parser_exit

    call   check_format_directive       ; Intercetta direttiva format
    test   al, al
    jnz    .parser_main_loop            ; Se processato, ESI è già avanzato correttamente, non saltare a .parser_next!

    call   check_segment_directive      ; Intercetta direttiva segment
    test   al, al
    jnz    .parser_main_loop            ; Come sopra

    call   check_entry_directive        ; Intercetta direttiva entry
    test   al, al
    jnz    .parser_main_loop            ; Come sopra

    call   check_symbol_assignment      ; Risolve lunghezze ed etichette (=)
    test   al, al
    jnz    .parser_main_loop            ; Come sopra

    mov    bl, [active_isa_target]      ; Autodetect hardware registri
    test   bl, bl
    jnz    .dispatch
    call   probe_register_isa_intent
    mov    [active_isa_target], al

.dispatch:
    call   execute_instruction_assembly_dispatch
    cmp    dword [core_error_code], 0   ; Verifica errore post-dispatch
    jnz    .parser_exit

.parser_next:
    add    esi, 64                      ; Avanza allo slot da 64 byte in cache line
    jmp    .parser_main_loop

.parser_exit:
    ret

check_format_directive:
    mov    eax, [esi]
    cmp    eax, 'mrof'                  ; Verifica "form" ('form' in little endian)
    jne    .not_format
    cmp    word [esi+4], 'at'           ; Verifica "at"
    jne    .not_format

    add    esi, 64                      ; Avanza ESI per puntare all'argomento del format
    mov    eax, [esi]
    cmp    eax, 'anib'                  ; "bina" ('bina' in little-endian = 0x616E6962) per binary
    je     .set_bin
    cmp    eax, 'FLE '                  ; "ELF " (Little endian: 0x20464C45)
    je     .eval_elf
    cmp    eax, '  EP'                  ; "PE  "
    je     .eval_pe

    jmp    .trigger_format_error        ; Formato invalido o sconosciuto

.set_bin:
    mov    byte [output_format_type], 0 ; Format 0 = binary piatto crudo
    add    esi, 64                      ; Consuma l'argomento (es. "binary")
    mov    al, 1
    ret
.eval_elf:
    cmp    byte [esi+4], '6'            ; ELF64
    je     .set_elf64
    mov    byte [output_format_type], 1 ; ELF32 default
    add    esi, 64
    mov    al, 1
    ret
.set_elf64:
    mov    byte [output_format_type], 2
    add    esi, 64
    mov    al, 1
    ret
.eval_pe:
    cmp    byte [esi+4], '6'            ; PE64
    je     .set_pe64
    mov    byte [output_format_type], 3 ; PE32 default
    add    esi, 64
    mov    al, 1
    ret
.set_pe64:
    mov    byte [output_format_type], 4
    add    esi, 64
    mov    al, 1
    ret
.not_format:
    xor    al, al
    ret

check_segment_directive:
    mov    eax, [esi]
    cmp    eax, 'mges'                  ; Verifica "segm"
    jne    .not_segment
    cmp    dword [esi+4], 'tnes'        ; Verifica "ent" ('tnes' invertito)
    jne    .not_segment                 ; (Se il token è più corto controlla solo i caratteri necessari)
    
    add    esi, 128                     ; Salta sia "segment" che l'argomento hardware successivo
    mov    al, 1
    ret
.not_segment:
    xor    al, al
    ret

check_entry_directive:
    mov    eax, [esi]
    cmp    eax, 'rtne'                  ; Verifica "entr"
    jne    .not_entry
    cmp    byte [esi+4], 'y'            ; Verifica "y"
    jne    .not_entry

    add    esi, 128                     ; Salta sia "entry" che l'etichetta target (es. _start)
    ; [...] Gestione interna per aggiornare [hardware_entry_point]
    mov    al, 1
    ret
.not_entry:
    xor    al, al
    ret

check_symbol_assignment:
    cmp    byte [esi+64], '='           ; Identifica l'operatore costante nello slot successivo
    jne    .not_assignment
    
    ; [...] Risolve l'equazione leggendo il [location_counter]
    add    esi, 128                     ; Salta sia il Simbolo che l'operatore '=' consumato
    mov    al, 1
    ret
.not_assignment:
    xor    al, al
    ret

probe_register_isa_intent:
    push   esi
    mov    al, [esi]
    cmp    al, 'r'
    je     .eval_r
    cmp    al, 'e'
    je     .confirm_x86
    cmp    byte [esi+1], 'x'
    je     .confirm_x86
    cmp    byte [esi+1], 'p'
    je     .confirm_x86
    cmp    byte [esi+1], 'i'
    je     .confirm_x86
    cmp    al, 'w'
    je     .confirm_arm
    cmp    al, 'x'
    je     .resolve_x
    mov    eax, [esi]
    cmp    eax, 'orez'                  ; "zero" RISC-V
    je     .confirm_riscv
    xor    al, al
    pop    esi
    ret
.eval_r:
    cmp    byte [esi+1], 'a'
    jne    .confirm_x86
    cmp    byte [esi+2], 0
    jz     .confirm_riscv               ; "ra" esatto = RISC-V
    jmp    .confirm_x86
.resolve_x:
    movzx  eax, byte [default_host_isa_context]
    pop    esi
    ret
.confirm_x86:
    mov    al, 1
    pop    esi
    ret
.confirm_arm:
    mov    al, 2
    pop    esi
    ret
.confirm_riscv:
    mov    al, 3
    pop    esi
    ret

; --- STRUTTURA DI SCATOLA NERA E CONGELAMENTO ERRORI ---
.trigger_format_error:
    mov    dword [core_error_code], 0x06 ; Codice errore 0x06 (Invalid Format)
    jmp    .freeze_diagnostics

.trigger_illegal_instruction:
    mov    dword [core_error_code], 0x05 ; Codice errore 0x05 (Illegal Instruction)

.freeze_diagnostics:
    mov    [error_token_ptr], esi       ; Salva indirizzo inizio stringa fallita
    mov    eax, [esi+52]                ; Estrae l'offset RAM del file originale
    mov    [error_file_addr], eax
    mov    eax, [esi+56]                ; Estrae riga esatta
    mov    [error_line], eax
    mov    eax, [esi+60]                ; Estrae colonna esatta
    mov    [error_column], eax
    xor    al, al                       ; Restituisce 0 per rompere i controlli
    ret
