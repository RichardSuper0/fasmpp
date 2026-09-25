; ============================================================
; PARSER UNIVERSALE
; ============================================================

execute_universal_parser:
    ; ESI = matrice token
    ; EDI = buffer output

.parser_loop:
    cmp dword [core_error_code], 0
    jne .parser_exit

    mov al, [esi]

    cmp al, 0xFF
    je  .parser_exit

    test al, al
    jz   .next_token

    call check_format_directive
    test al, al
    jnz .parser_loop

    call check_segment_directive
    test al, al
    jnz .parser_loop

    call check_entry_directive
    test al, al
    jnz .parser_loop

    call check_symbol_assignment
    test al, al
    jnz .parser_loop

    cmp byte [active_isa_target], 0
    jne .dispatch

    call probe_register_isa_intent
    test al, al
    jz   .unknown_isa

    mov [active_isa_target], al

.dispatch:
    call execute_instruction_assembly_dispatch
    jmp .next_token

.unknown_isa:
    mov dword [core_error_code], 0x05
    mov [error_token_ptr], esi
    jmp .parser_exit

.next_token:
    add esi, 64
    jmp .parser_loop

.parser_exit:
    ret


; ============================================================
; FORMAT
; ============================================================

check_format_directive:
    cmp byte [esi+0], 'f'
    jne .no

    cmp byte [esi+1], 'o'
    jne .no

    cmp byte [esi+2], 'r'
    jne .no

    cmp byte [esi+3], 'm'
    jne .no

    cmp byte [esi+4], 'a'
    jne .no

    cmp byte [esi+5], 't'
    jne .no

    add esi, 64

    cmp byte [esi+0], 'b'
    jne .not_bin

    cmp byte [esi+1], 'i'
    jne .not_bin

    cmp byte [esi+2], 'n'
    jne .not_bin

    mov byte [output_format_type], 0
    add esi, 64
    mov al, 1
    ret

.not_bin:
    mov dword [core_error_code], 0x06
    xor al, al
    ret

.no:
    xor al, al
    ret


; ============================================================
; SEGMENT
; ============================================================

check_segment_directive:
    cmp byte [esi+0], 's'
    jne .no

    cmp byte [esi+1], 'e'
    jne .no

    cmp byte [esi+2], 'g'
    jne .no

    cmp byte [esi+3], 'm'
    jne .no

    cmp byte [esi+4], 'e'
    jne .no

    cmp byte [esi+5], 'n'
    jne .no

    cmp byte [esi+6], 't'
    jne .no

    add esi, 64
    mov al, 1
    ret

.no:
    xor al, al
    ret


; ============================================================
; ENTRY
; ============================================================

check_entry_directive:
    cmp byte [esi+0], 'e'
    jne .no

    cmp byte [esi+1], 'n'
    jne .no

    cmp byte [esi+2], 't'
    jne .no

    cmp byte [esi+3], 'r'
    jne .no

    cmp byte [esi+4], 'y'
    jne .no

    mov eax, [location_counter]
    mov [hardware_entry_point], eax

    add esi, 64
    mov al, 1
    ret

.no:
    xor al, al
    ret


; ============================================================
; ASSEGNAZIONE SIMBOLICA
; ============================================================

check_symbol_assignment:
    ; Riconosce un token terminante con '='.
    ; La gestione completa della tabella simboli verrà aggiunta
    ; quando sarà definito il formato della symbol table.

    cmp byte [esi+0], 0
    je  .no

    mov edi, esi
    xor ecx, ecx

.scan:
    mov al, [edi]

    test al, al
    jz   .no

    cmp al, '='
    je  .found

    inc edi
    inc ecx
    cmp ecx, 51
    jb  .scan

.no:
    xor al, al
    ret

.found:
    mov byte [esi], 0
    add esi, 64
    mov al, 1
    ret


; ============================================================
; RILEVAMENTO ISA
; ============================================================

probe_register_isa_intent:
    mov al, [esi]

    cmp al, 'r'
    je  .check_riscv_or_x86

    cmp al, 'x'
    je  .arm64_or_riscv

    cmp al, 'w'
    je  .arm64

    cmp al, 'e'
    je  .x86

    cmp al, 'a'
    je  .arm

    xor al, al
    ret

.check_riscv_or_x86:
    cmp byte [esi+1], 'a'
    je  .x86

    cmp byte [esi+1], 'b'
    je  .x86

    cmp byte [esi+1], 's'
    je  .x86

    cmp byte [esi+1], 'd'
    je  .x86

    mov al, 3
    ret

.arm64_or_riscv:
    mov al, 2
    ret

.arm64:
    mov al, 2
    ret

.arm:
    mov al, 2
    ret

.x86:
    mov al, 1
    ret


; ============================================================
; DIAGNOSTICA
; ============================================================

freeze_parser_diagnostics:
    mov [error_token_ptr], esi

    mov eax, [esi+52]
    mov [error_file_addr], eax

    mov eax, [esi+56]
    mov [error_line], eax

    mov eax, [esi+60]
    mov [error_column], eax

    ret
