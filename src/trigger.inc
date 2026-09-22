evaluate_trigger_intent:
    ; ESI = stringa parametri CLI, EDI = buffer di output in RAM
    mov    [output_base_ptr], edi       ; Salva indirizzo inizio buffer
    mov    [output_current_ptr], edi    ; Inizializza puntatore corrente

    test   esi, esi                     ; Verifica se la CLI e NULL
    jz     .cli_error_exit

    mov    al, [esi]                    ; Legge il primo carattere della CLI
    test   al, al                       ; Controlla se stringa vuota
    jz     .cli_error_exit

    ; --- ISOLAMENTO SORGENTE ---
    mov    [source_file_path_ptr], esi  ; Blocca l'inizio del path sorgente
    call   scan_and_terminate_arg       ; Cerca lo spazio e tronca con uno zero
    
    test   al, al                       ; Se AL e 0, la CLI ha 1 solo argomento
    jz     .handle_single_argument

    ; --- ISOLAMENTO OUTPUT OPZIONALE ---
.skip_inter_spaces:
    mov    al, [esi]                    ; Legge i caratteri di spazio intermedio
    check_delimiter_and_skip al, 0x20, .next_inter_space
    check_delimiter_and_skip al, 0x09, .next_inter_space
    jmp    .lock_explicit_output
.next_inter_space:
    inc    esi
    jmp    .skip_inter_spaces

.lock_explicit_output:
    test   al, al                       ; Se e fine stringa, era un solo argomento
    jz     .handle_single_argument

    mov    [output_file_path_ptr], esi  ; Salva il puntatore all'output esplicito
    call   scan_and_terminate_arg       ; Isola e ripulisce la coda
    jmp    .start_pipeline_execution

.handle_single_argument:
    call   build_automatic_output_name  ; Esegue la troncatura automatica dell'estensione

.start_pipeline_execution:
    ; --- AVVIO PIPELINE ---
    mov    esi, [source_file_path_ptr]  ; Carica il percorso del sorgente
    lea    edx, [token_scratch_matrix]  ; Punta alla matrice scratch per i token
    call   execute_lexer_32             ; Avvia il lexer a 32-bit protetti

    lea    esi, [token_scratch_matrix]  ; Passa la matrice degli slot al parser
    call   execute_universal_parser     ; Avvia l'autodetect e l'emissione dei bit
    ret

.cli_error_exit:
    mov    dword [core_error_code], 0x01 ; Errore 1: Manca il file di input
    ret

scan_and_terminate_arg:
.scan_loop:
    mov    al, [esi]
    test   al, al
    jz     .found_term_char
    check_delimiter_and_skip al, 0x20, .found_term_char
    check_delimiter_and_skip al, 0x09, .found_term_char
    inc    esi
    jmp    .scan_loop
.found_term_char:
    push   eax                          ; Salva il carattere delimitatore
    mov    byte [esi], 0                ; Inietta null-termination in-place
    inc    esi                          ; Avanza oltre lo zero appena scritto
    pop    eax                          ; Ripristina il delimitatore in AL
    ret

build_automatic_output_name:
    push   esi
    mov    esi, [source_file_path_ptr]
    lea    edi, [output_file_path_buffer]
    mov    [output_file_path_ptr], edi

.copy_auto_loop:
    mov    al, [esi]
    mov    [edi], al
    test   al, al
    jz     .find_dot_to_truncate
    inc    esi
    inc    edi
    jmp    .copy_auto_loop

.find_dot_to_truncate:
    mov    edi, [output_file_path_ptr]
.scan_dot_loop:
    mov    al, [edi]
    test   al, al
    jz     .auto_done
    check_delimiter_and_skip al, '.', .truncate_at_dot
    inc    edi
    jmp    .scan_dot_loop

.truncate_at_dot:
    mov    byte [edi], 0                ; Taglia via l'estensione (.asm)
.auto_done:
    pop    esi
    ret
