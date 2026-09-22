evaluate_trigger_intent:
    ; ESI = stringa parametri CLI, EDI = buffer di output in RAM
    mov    [output_base_ptr], edi       ; Salva indirizzo inizio buffer
    mov    [output_current_ptr], edi    ; Inizializza puntatore corrente

    test   esi, esi                     ; Verifica se CLI e NULL
    jz     .cli_error_exit

    mov    al, [esi]                    ; Legge primo carattere
    test   al, al                       ; Controlla se vuoto
    jz     .cli_error_exit

    mov    [source_file_path_ptr], esi  ; Salva puntatore sorgente
    call   scan_and_terminate_arg       ; Isola il primo argomento
    
    test   al, al                       ; Se AL e 0, c'e solo 1 argomento
    jz     .handle_single_argument

.skip_inter_spaces:
    mov    al, [esi]                    ; Legge spazi tra argomenti
    check_delimiter_and_skip al, 0x20, .next_inter_space
    check_delimiter_and_skip al, 0x09, .next_inter_space
    jmp    .lock_explicit_output
.next_inter_space:
    inc    esi
    jmp    .skip_inter_spaces

.lock_explicit_output:
    test   al, al                       ; Se fine stringa, era un solo argomento
    jz     .handle_single_argument

    mov    [output_file_path_ptr], esi  ; Salva puntatore output esplicito
    call   scan_and_terminate_arg       ; Isola il secondo argomento
    jmp    .start_pipeline

.handle_single_argument:
    call   build_automatic_output_name  ; Tronca estensione e crea nome piatto

.start_pipeline:
    mov    esi, [source_file_path_ptr]  ; Passa sorgente al lexer
    lea    edx, [token_scratch_matrix]
    call   execute_lexer_32

    lea    esi, [token_scratch_matrix]  ; Passa matrice al tokenizer/parser
    call   execute_tokenizer_32
    lea    esi, [token_scratch_matrix]
    call   execute_universal_parser
    ret

.cli_error_exit:
    mov    dword [core_error_code], 0x01 ; Errore CLI
    ret

scan_and_terminate_arg:
.scan_loop:
    mov    al, [esi]
    test   al, al
    jz     .found_term
    check_delimiter_and_skip al, 0x20, .found_term
    check_delimiter_and_skip al, 0x09, .found_term
    inc    esi
    jmp    .scan_loop
.found_term:
    push   eax
    mov    byte [esi], 0                ; Null-termination in-place
    inc    esi
    pop    eax
    ret

build_automatic_output_name:
    push   esi
    mov    esi, [source_file_path_ptr]
    lea    edi, [output_file_path_buffer]
    mov    [output_file_path_ptr], edi
.copy_auto:
    mov    al, [esi]
    mov    [edi], al
    test   al, al
    jz     .find_dot
    inc    esi
    inc    edi
    jmp    .copy_auto
.find_dot:
    mov    edi, [output_file_path_ptr]
.scan_dot:
    mov    al, [edi]
    test   al, al
    jz     .auto_done
    check_delimiter_and_skip al, '.', .truncate
    inc    edi
    jmp    .scan_dot
.truncate:
    mov    byte [edi], 0                ; Taglia estensione
.auto_done:
    pop    esi
    ret

output_file_path_buffer rb 256
