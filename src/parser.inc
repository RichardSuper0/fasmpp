execute_universal_parser:
    ; ESI = token_scratch_matrix, EDI = buffer codice finale
    
.parser_main_loop:
    mov    al, [esi]
    cmp    al, 0xFF                     ; Verifica fine dei token
    je     .parser_exit

    test   al, al                       ; Salta slot vuoti
    jz     .parser_next

    call   check_format_directive       ; Intercetta direttiva format
    test   al, al
    jnz    .parser_next

    call   check_segment_directive      ; Intercetta direttiva segment
    test   al, al
    jnz    .parser_next

    call   check_entry_directive        ; Intercetta direttiva entry
    test   al, al
    jnz    .parser_next

    call   check_symbol_assignment      ; Risolve lunghezze ed etichette (=)
    test   al, al
    jnz    .parser_next

    mov    bl, [active_isa_target]      ; Autodetect hardware registri
    test   bl, bl
    jnz    .dispatch
    call   probe_register_isa_intent
    mov    [active_isa_target], al

.dispatch:
    call   execute_instruction_assembly_dispatch
.parser_next:
    add    esi, 64                      ; Avanza allo slot da 64 byte in cache line
    jmp    .parser_main_loop
.parser_exit:
    ret

check_format_directive:
    mov    eax, [esi]
    cmp    eax, 'mrof'                  ; Verifica "form"
    jne    .not_format
    mov    bl, [esi+4]
    cmp    bl, 'a'
    jne    .not_format
    mov    bl, [esi+5]
    cmp    bl, 't'
    jne    .not_format

    add    esi, 64                      ; Consuma "format" e passa all'argomento
    mov    eax, [esi]
    cmp    eax, 'enib'                  ; "bina" -> binary flat
    je     .set_bin
    cmp    eax, 'FLE '                  ; "ELF "
    je     .eval_elf
    cmp    eax, '  EP'                  ; "PE  "
    je     .eval_pe

    jmp    .trigger_format_error        ; Formato invalido o sconosciuto (es. ELFWWW)

.set_bin:
    mov    byte [output_format_type], 0 ; Format 0 = binary piatto crudo
    mov    al, 1
    ret
.eval_elf:
    mov    bl, [esi+4]
    cmp    bl, '6'                      ; ELF64
    je     .set_elf64
    mov    byte [output_format_type], 1 ; ELF32 default
    mov    al, 1
    ret
.set_elf64:
    mov    byte [output_format_type], 2
    mov    al, 1
    ret
.eval_pe:
    mov    bl, [esi+4]
    cmp    bl, '6'                      ; PE64
    je     .set_pe64
    mov    byte [output_format_type], 3 ; PE32 default
    mov    al, 1
    ret
.set_pe64:
    mov    byte [output_format_type], 4
    mov    al, 1
    ret
.not_format:
    xor    al, al
    ret

check_segment_directive:
    mov    eax, [esi]
    cmp    eax, 'mges'                  ; Verifica "segm"
    jne    .not_segment
    mov    bl, [esi+4]
    cmp    bl, 'e'
    jne    .not_segment
    mov    bl, [esi+5]
    cmp    bl, 'n'
    jne    .not_segment
    mov    bl, [esi+6]
    cmp    bl, 't'
    jne    .not_segment
    add    esi, 64                      ; Salta ad attributi hardware
    mov    al, 1
    ret
.not_segment:
    xor    al, al
    ret

check_entry_directive:
    mov    eax, [esi]
    cmp    eax, 'rtne'                  ; Verifica "entr"
    jne    .not_entry
    mov    bl, [esi+4]
    cmp    bl, 'y'                      ; Verifica "y"
    jne    .not_entry

    add    esi, 64                      ; Passa allo slot dell'etichetta entry point
    ; [...] Prende l'indirizzo dell'etichetta (es. _start) e aggiorna [hardware_entry_point]
    mov    al, 1
    ret
.not_entry:
    xor    al, al
    ret

check_symbol_assignment:
    mov    eax, [esi+64]                ; Controlla lo slot successivo
    cmp    byte [esi+64], '='           ; Identifica l'operatore costante
    jne    .not_assignment
    
    ; [...] Risolve l'equazione (len = $ - msg) leggendo il [location_counter]
    ; [...] Inserisce il valore risultante nella tabella simboli scratch RAM
    mov    al, 1
    add    esi, 64                      ; Salta oltre l'operatore '=' consumato
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
    mov    ah, [esi+1]
    cmp    ah, 'x'
    je     .confirm_x86
    cmp    ah, 'p'
    je     .confirm_x86
    cmp    ah, 'i'
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
    mov    ah, [esi+1]
    cmp    ah, 'a'
    jne    .confirm_x86
    mov    bl, [esi+2]
    test   bl, bl
    jz     .confirm_riscv               ; "ra" esatto = RISC-V
    jmp    .confirm_x86
.resolve_x:
    mov    al, [default_host_isa_context]
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
    mov    eax, [esi+56]                ; Estrae riga esatta congelata dal lexer
    mov    [error_line], eax
    mov    eax, [esi+60]                ; Estrae colonna esatta congelata dal lexer
    mov    [error_column], eax
    xor    al, al                       ; Forza l'uscita interrompendo il ciclo
    ret
