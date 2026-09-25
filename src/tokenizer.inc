
; ============================================================
; TOKENIZER
; ============================================================

execute_tokenizer_32:
    ; ESI = matrice token lexer
    ; EDX = matrice token normalizzata

.tokenizer_loop:
    cmp dword [core_error_code], 0
    jne .tokenizer_done

    mov al, [esi]

    cmp al, 0xFF
    je  .tokenizer_done

    test al, al
    jz   .skip_empty

    call copy_token_and_metadata

.skip_empty:
    add esi, 64
    add edx, 64
    jmp .tokenizer_loop

.tokenizer_done:
    mov byte [edx], 0xFF
    ret


copy_token_and_metadata:
    push esi
    push edi
    push ecx

    mov edi, edx
    xor ecx, ecx

.copy_loop:
    mov al, [esi]

    test al, al
    jz   .copy_done

    cmp ecx, 51
    jae .copy_done

    mov [edi], al
    inc esi
    inc edi
    inc ecx
    jmp .copy_loop

.copy_done:
    mov byte [edi], 0

    mov eax, [esi-ecx+52]
    mov [edx+52], eax

    mov eax, [esi-ecx+56]
    mov [edx+56], eax

    mov eax, [esi-ecx+60]
    mov [edx+60], eax

    pop ecx
    pop edi
    pop esi
    ret
