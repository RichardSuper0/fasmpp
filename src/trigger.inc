evaluate_trigger_intent:
    ; ESI = puntatore alla stringa parametri CLI, EDI = buffer di output in RAM
    mov    [output_base_ptr], edi       ; Salva indirizzo inizio buffer
    mov    [output_current_ptr], edi    ; Inizializza puntatore corrente

    test   esi, esi                     ; Verifica se la riga di comando e NULL
    jz     .cli_error_exit

    mov    al, [esi]                    ; Legge il primo carattere della CLI
    test   al, al                       ; Controlla se stringa vuota
    jz     .cli_error_exit

    ; --- ISOLAMENTO PRIMO ARGOMENTO (SOURCE) ---
    mov    [source_file_path_ptr], esi  ; Blocca l'inizio del path sorgente
    call   scan_and_terminate_arg       ; Cerca lo spazio e tronca con uno zero
    
    ; AL ritorna con il delimitatore trovato: se e 0 significa che la CLI e finita (1 argomento)
    test   al, al
    jz     .handle_single_argument      ; Salta alla troncatura automatica se c'e solo un file

    ; --- ISOLAMENTO SECONDO ARGOMENTO (OUTPUT OPZIONALE) ---
    ; ESI e stato avanzato oltre lo zero dal modulo di scansione ed e pronto
.skip_inter_spaces:
    mov    al, [esi]                    ; Legge i caratteri tra i due argomenti
    cmp    al, 0x20                     ; Salta gli spazi vuoti aggiuntivi
    je     .next_inter_space
    cmp    al, 0x09                     ; Salta le tabulazioni
    jne    .lock_explicit_output
.next_inter_space:
    inc    esi
    jmp    .skip_inter_spaces

.lock_explicit_output:
    test   al, al                       ; Se dopo gli spazi c'e la fine della CLI, era solo un file
    jz     .handle_single_argument

    mov    [output_file_path_ptr], esi  ; Salva il puntatore all'output esplicito (es. main.exe)
    call   scan_and_terminate_arg       ; Isola anche il secondo argomento eliminando spazzatura finale
    jmp    .start_pipeline_execution

.handle_single_argument:
    ; Se l'utente non ha specificato l'output, calcola il nome troncando l'estensione
    call   build_automatic_output_name

.start_pipeline_execution:
    ; --- AVVIO PIPELINE HARDWARE ---
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
    ; Scansiona la stringa in ESI fino a uno spazio, tab o fine stringa.
    ; Inietta uno zero per isolare l'argomento e avanza ESI oltre lo zero.
.scan_loop:
    mov    al, [esi]
    test   al, al
    jz     .found_term_char
    cmp    al, 0x20
    je     .found_term_char
    cmp    al, 0x09
    je     .found_term_char
    inc    esi
    jmp    .scan_loop
.found_term_char:
    push   eax                          ; Salva il carattere delimitatore (0, spazio o tab)
    mov    byte [esi], 0                ; Inietta null-termination in-place
    inc    esi                          ; Avanza oltre lo zero appena scritto
    pop    eax                          ; Ripristina il vecchio delimitatore in AL
    ret

build_automatic_output_name:
    ; Routine speculare: copia la sorgente nel buffer e taglia l'estensione al primo punto
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
    cmp    al, '.'                      ; Cerca il punto dell'estensione (es. "main.asm")
    je     .truncate_at_dot
    inc    edi
    jmp    .scan_dot_loop

.truncate_at_dot:
    mov    byte [edi], 0                ; Taglia via l'estensione: rimane solo "main"
.auto_done:
    pop    esi
    ret

segment readable writeable
output_file_path_buffer rb 256          ; Spazio RAM per la generazione dell'output automatico
