execute_universal_parser:
    ; ESI = token_scratch_matrix, EDI = buffer codice macchina finale
    
.parser_main_loop:
    mov    al, [esi]                    ; Legge il primo byte dello slot token
    cmp    al, 0xFF                     ; Controlla se e il marcatore EOF del lexer
    je     .parser_exit_success         ; Esce se il parsing e completato

    test   al, al                       ; Salta gli slot vuoti nella matrice
    jz     .parser_next_slot

    mov    bl, [active_isa_target]      ; Verifica se l'architettura e gia stata rilevata
    test   bl, bl                       ; Se BL non e zero, l'ISA e gia bloccata
    jnz    .dispatch_to_assembler_core

    call   probe_register_isa_intent    ; Esegue l'Autodetect Hardware analizzando il token
    mov    [active_isa_target], al      ; Memorizza l'ISA rilevata (1=x86, 2=ARM, 3=RISC-V)

.dispatch_to_assembler_core:
    call   execute_instruction_assembly_dispatch ; Salta al regista in assembler.inc
    
.parser_next_slot:
    add    esi, 64                      ; Avanza rigido al prossimo slot da 64 byte
    jmp    .parser_main_loop

.parser_exit_success:
    ret

probe_register_isa_intent:
    push   esi                          ; Salva l'inizio dello slot corrente
    mov    al, [esi]                    ; Carica il primo carattere del token stringa

    cmp    al, 'r'                      ; Filtro registri estesi x86 o 'ra' RISC-V
    je     .eval_r_prefix
    cmp    al, 'e'                      ; Filtro registri estesi x86 a 32-bit (eax, ebx...)
    je     .confirm_x86

    mov    ah, [esi+1]                  ; Analisi secondo carattere per suffissi x86 a 16-bit
    cmp    ah, 'x'                      ; Registri ax, bx, cx, dx
    je     .confirm_x86
    cmp    ah, 'p'                      ; Registri sp, bp
    je     .confirm_x86
    cmp    ah, 'i'                      ; Registri si, di
    je     .confirm_x86

    cmp    al, 'w'                      ; Filtro registri a 32-bit ARM A64 (w0-w31)
    je     .confirm_arm

    cmp    al, 'x'                      ; Gestione ambiguita registri 'x' (ARM64 vs RISC-V)
    je     .resolve_x_register

    mov    eax, [esi]                   ; Carica i primi 4 byte per gli alias RISC-V
    cmp    eax, 'orez'                  ; Stringa "zero" in Little Endian
    je     .confirm_riscv
    
    movzx  eax, word [esi]              ; Carica i primi 2 byte per gli alias corti RISC-V
    cmp    ax, 'ps'                     ; Stringa "sp" (Stack Pointer)
    je     .confirm_riscv
    cmp    ax, 'pg'                     ; Stringa "gp" (Global Pointer)
    je     .confirm_riscv

    xor    al, al                       ; Architettura non identificabile da questo token
    pop    esi
    ret

.eval_r_prefix:
    mov    ah, [esi+1]                  ; Controlla il carattere dopo 'r'
    cmp    ah, 'a'                      ; Se trova 'a', potrebbe essere 'ra' di RISC-V
    jne    .confirm_x86                 ; Se non e 'a', sono registri x86 (rax, rbx, r8-r31)
    mov    bl, [esi+2]                  ; Controlla il terzo carattere dello slot
    test   bl, bl                       ; Se e zero, la stringa e esattamente "ra"
    jz     .confirm_riscv               ; "ra" esatto (Return Address) = RISC-V
    jmp    .confirm_x86                 ; Se continua (es. "rax"), allora e x86_64

.resolve_x_register:
    mov    ah, [esi+1]                  ; Legge il carattere dopo la 'x'
    cmp    ah, '0'                      ; Verifica che sia una cifra numerica valida
    jb     .unknown_token
    cmp    ah, '9'
    ja     .unknown_token
    mov    al, [default_host_isa_context] ; Risolve l'ambiguita usando il contesto del blob host
    pop    esi
    ret

.confirm_x86:
    mov    al, 1                        ; ID Architettura 1 = x86
    pop    esi
    ret

.confirm_arm:
    mov    al, 2                        ; ID Architettura 2 = ARM
    pop    esi
    ret

.confirm_riscv:
    mov    al, 3                        ; ID Architettura 3 = RISC-V
    pop    esi
    ret

.unknown_token:
    xor    al, al                       ; Ritorna 0 (Token generico o istruzione)
    pop    esi
    ret
