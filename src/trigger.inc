evaluate_trigger_intent:
    ; ESI = stringa parametri CLI cruda, EDI = buffer di output finale
    mov    [output_base_ptr], edi       ; Salva indirizzo inizio buffer
    mov    [output_current_ptr], edi    ; Inizializza puntatore corrente

    test   esi, esi                     ; Verifica se CLI e vuota
    jz     .cli_error_exit

.cli_scan_loop:
    mov    al, [esi]                    ; Legge carattere CLI
    test   al, al                       ; Controlla fine stringa
    jz     .validate_extracted_paths

    cmp    al, '-'                      ; Intercetta inizio flag
    jne    .next_cli_char

    mov    bl, [esi+1]                  ; Isola il carattere flag
    cmp    bl, 's'                      ; Controlla flag sorgente
    je     .parse_source_arg

    cmp    bl, 'o'                      ; Controlla flag output
    je     .parse_output_arg

.next_cli_char:
    inc    esi                          ; Avanza nella CLI
    jmp    .cli_scan_loop

.parse_source_arg:
    add    esi, 2                       ; Salta "-s"
.strip_spaces_source:
    mov    al, [esi]                    ; Legge path
    cmp    al, 0x20                     ; Salta spazi vuoti
    je     .inc_source_space
    cmp    al, 0x09                     ; Salta tabulazioni
    jne    .lock_source_pointer
.inc_source_space:
    inc    esi
    jmp    .strip_spaces_source
.lock_source_pointer:
    mov    [source_file_path_ptr], esi  ; Blocca inizio stringa sorgente
    call   isolate_path_string          ; Esegue null-termination in-place
    jmp    .cli_scan_loop

.parse_output_arg:
    add    esi, 2                       ; Salta "-o"
.strip_spaces_output:
    mov    al, [esi]
    cmp    al, 0x20                     ; Salta spazi vuoti
    je     .inc_output_space
    cmp    al, 0x09                     ; Salta tabulazioni
    jne    .lock_output_pointer
.inc_output_space:
    inc    esi
    jmp    .strip_spaces_output
.lock_output_pointer:
    mov    [output_file_path_ptr], esi  ; Blocca inizio stringa output
    call   isolate_path_string          ; Esegue null-termination in-place
    jmp    .cli_scan_loop

.validate_extracted_paths:
    cmp    dword [source_file_path_ptr], 0 ; Verifica presenza path sorgente
    je     .cli_error_exit
    cmp    dword [output_file_path_ptr], 0 ; Verifica presenza path output
    je     .cli_error_exit

    mov    esi, [source_file_path_ptr]  ; Carica path sorgente per il lexer
    lea    edx, [token_scratch_matrix]  ; Punta alla matrice scratch
    call   execute_lexer_32             ; Avvia il lexer a 32-bit protetti

    lea    esi, [token_scratch_matrix]  ; Passa i token estratti al parser
    call   execute_universal_parser     ; Avvia autodetect e traduzione bitwise
    ret

.cli_error_exit:
    mov    dword [core_error_code], 0x01 ; Errore 1: CLI invalida o incompleta
    ret

isolate_path_string:
    push   esi                          ; Salva puntatore scansione path
.iso_loop:
    mov    al, [esi]                    ; Legge carattere path
    test   al, al                       ; Fine stringa naturale
    jz     .iso_done
    cmp    al, 0x20                     ; Spazio interrompe il path
    je     .iso_null
    cmp    al, 0x09                     ; Tab interrompe il path
    je     .iso_null
    inc    esi
    jmp    .iso_loop
.iso_null:
    mov    byte [esi], 0                ; Inietta null-termination in-place
.iso_done:
    pop    esi                          ; Ripristina puntatore
    ret
