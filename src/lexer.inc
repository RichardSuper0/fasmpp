; ============================================================
; LEXER
; ============================================================

execute_lexer_32:
    ; ESI = sorgente ASCII terminata da zero
    ; EDX = matrice token di destinazione

.lexer_loop:
    cmp dword [core_error_code], 0
    jne .lexer_done

    mov al, [esi]

    test al, al
    jz   .lexer_done

    cmp al, 0x20
    je  .skip_char

    cmp al, 0x09
    je  .skip_char

    cmp al, 0x0D
    je  .skip_char

    cmp al, 0x0A
    je  .newline

    cmp al, ';'
    je  .comment

    call check_for_include_directive
    test al, al
    jnz .lexer_loop

    call extract_token_to_matrix_slot
    add edx, 64
    jmp .lexer_loop

.skip_char:
    inc esi
    inc dword [current_column]
    jmp .lexer_loop

.newline:
    inc esi
    inc dword [current_line]
    mov dword [current_column], 0
    jmp .lexer_loop

.comment:
    inc esi

.comment_loop:
    mov al, [esi]

    test al, al
    jz   .lexer_done

    cmp al, 0x0A
    je  .newline

    inc esi
    jmp .comment_loop

.lexer_done:
    mov byte [edx], 0xFF
    ret


; ============================================================
; INCLUDE
; ============================================================

check_for_include_directive:
    push esi

    cmp byte [esi+0], 'i'
    jne .not_include
    cmp byte [esi+1], 'n'
    jne .not_include
    cmp byte [esi+2], 'c'
    jne .not_include
    cmp byte [esi+3], 'l'
    jne .not_include
    cmp byte [esi+4], 'u'
    jne .not_include
    cmp byte [esi+5], 'd'
    jne .not_include
    cmp byte [esi+6], 'e'
    jne .not_include

    add esi, 7

.skip_spaces:
    mov al, [esi]

    cmp al, 0x20
    je  .skip_one

    cmp al, 0x09
    je  .skip_one

    cmp al, "'"
    je  .start_path

    cmp al, '"'
    je  .start_path

    jmp .include_error

.skip_one:
    inc esi
    jmp .skip_spaces

.start_path:
    mov ah, al
    inc esi

    lea edi, [include_path_buffer]
    xor ecx, ecx

.copy_path:
    mov al, [esi]

    test al, al
    jz   .include_error

    cmp al, ah
    je  .path_done

    cmp ecx, 255
    jae .include_error

    mov [edi+ecx], al
    inc ecx
    inc esi
    jmp .copy_path

.path_done:
    mov byte [edi+ecx], 0
    inc esi

    ; Il loader host deve riempire include_file_load_area.
    push esi
    push edx

    mov esi, include_path_buffer
    lea edi, [include_file_load_area]
    call sys_load_include_file_to_ram

    cmp dword [core_error_code], 0
    jne .include_restore_error

    lea esi, [include_file_load_area]
    call execute_lexer_32

    pop edx
    pop esi

    mov al, 1
    ret

.include_restore_error:
    pop edx
    pop esi
    xor al, al
    ret

.include_error:
    mov dword [core_error_code], 0x02
    pop esi
    xor al, al
    ret

.not_include:
    pop esi
    xor al, al
    ret


; ============================================================
; ESTRAZIONE TOKEN
; ============================================================

extract_token_to_matrix_slot:
    push edi
    push ecx

    mov [edx+52], esi
    mov eax, [current_line]
    mov [edx+56], eax
    mov eax, [current_column]
    mov [edx+60], eax

    mov edi, edx
    xor ecx, ecx

.token_loop:
    mov al, [esi]

    test al, al
    jz   .token_done

    cmp al, 0x20
    je  .token_done

    cmp al, 0x09
    je  .token_done

    cmp al, 0x0A
    je  .token_done

    cmp al, 0x0D
    je  .token_done

    cmp al, ','
    je  .separator

    cmp al, ';'
    je  .token_done

    cmp al, '='
    je  .token_done

    cmp al, '['
    je  .token_done

    cmp al, ']'
    je  .token_done

    cmp ecx, 51
    jae .token_done

    mov [edi], al
    inc edi
    inc esi
    inc ecx
    inc dword [current_column]
    jmp .token_loop

.separator:
    inc esi
    inc dword [current_column]

.token_done:
    mov byte [edi], 0

    pop ecx
    pop edi
    ret
