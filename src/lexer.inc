; ============================================================
; GESTIONE ARGOMENTI E AVVIO PIPELINE
; ============================================================

evaluate_trigger_intent:
    ; ESI = stringa parametri CLI
    ; EDI = buffer output

    test esi, esi
    jz   .cli_error_exit

    test byte [esi], 0
    jz   .cli_error_exit

    mov [output_base_ptr], edi
    mov [output_current_ptr], edi

    mov [source_file_path_ptr], esi

    call scan_and_terminate_arg

    ; AL contiene il delimitatore trovato:
    ; 0    = fine stringa
    ; 20h  = spazio
    ; 09h  = tab
    test al, al
    jz   .handle_single_argument

.skip_inter_spaces:
    mov al, [esi]

    cmp al, 0x20
    je  .consume_inter_space

    cmp al, 0x09
    je  .consume_inter_space

    test al, al
    jz   .handle_single_argument

    jmp .lock_explicit_output

.consume_inter_space:
    inc esi
    jmp .skip_inter_spaces

.lock_explicit_output:
    mov [output_file_path_ptr], esi
    call scan_and_terminate_arg
    jmp .start_pipeline

.handle_single_argument:
    call build_automatic_output_name

.start_pipeline:
    ; Il lexer usa EDX come destinazione corrente.
    lea edx, [token_scratch_matrix]

    ; Mantiene il buffer di output originale.
    mov edi, [output_base_ptr]

    mov esi, [source_file_path_ptr]
    call execute_lexer_32

    cmp dword [core_error_code], 0
    jnz .pipeline_done

    ; Il tokenizer riceve la matrice lexer come sorgente
    ; e la stessa matrice come destinazione normalizzata.
    lea esi, [token_scratch_matrix]
    lea edx, [token_scratch_matrix]
    call execute_tokenizer_32

    cmp dword [core_error_code], 0
    jnz .pipeline_done

    lea esi, [token_scratch_matrix]
    mov edi, [output_base_ptr]
    call execute_universal_parser

.pipeline_done:
    ret

.cli_error_exit:
    mov dword [core_error_code], 0x01
    ret


; ============================================================
; SCANSIONE ARGOMENTO
; ============================================================

scan_and_terminate_arg:
    ; ESI = posizione corrente
    ; AL  = carattere che ha terminato la scansione
    ;
    ; La funzione modifica la stringa inserendo uno zero.

.scan_loop:
    mov al, [esi]

    test al, al
    jz   .found_term

    cmp al, 0x20
    je  .found_term

    cmp al, 0x09
    je  .found_term

    inc esi
    jmp .scan_loop

.found_term:
    push eax

    mov byte [esi], 0

    ; Se il carattere era già lo zero finale, non è necessario
    ; avanzare oltre la stringa.
    pop eax
    test al, al
    jz   .done

    inc esi

.done:
    ret


; ============================================================
; GENERAZIONE AUTOMATICA DEL NOME OUTPUT
; ============================================================

build_automatic_output_name:
    push esi
    push edi
    push ecx

    mov esi, [source_file_path_ptr]
    lea edi, [output_file_path_buffer]

    mov [output_file_path_ptr], edi

.copy_auto:
    mov al, [esi]
    mov [edi], al

    inc esi
    inc edi

    test al, al
    jnz  .copy_auto

    ; EDI punta appena dopo lo zero finale.
    mov ecx, edi
    mov edi, [output_file_path_ptr]

.scan_backwards:
    cmp ecx, edi
    je  .auto_done

    dec ecx

    mov al, [ecx]

    cmp al, '/'
    je  .auto_done

    cmp al, '\'
    je  .auto_done

    cmp al, '.'
    jne .scan_backwards

    mov byte [ecx], 0

.auto_done:
    pop ecx
    pop edi
    pop esi
    ret
