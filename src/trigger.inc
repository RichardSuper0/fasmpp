evaluate_trigger_intent:
    ; ESI = stringa parametri CLI, EDI = buffer di output in RAM
    mov    [output_base_ptr], edi       ; Salva indirizzo inizio buffer
    mov    [output_current_ptr], edi    ; Inizializza puntatore corrente

    test   esi, esi                     ; Verifica se CLI è NULL
    jz     near .cli_error_exit

    mov    al, [esi]                    ; Legge primo carattere
    test   al, al                       ; Controlla se vuoto
    jz     near .cli_error_exit

    mov    [source_file_path_ptr], esi  ; Salva l'inizio del primo argomento (sorgente)
    call   scan_and_terminate_arg       ; Isola il primo argomento. Al ritorno ESI punta DOPO lo 0 inserito
    
    ; Determina se c'era un secondo argomento analizzando dove si è fermata la scansione
    test   al, al                       
    jz     near .handle_single_argument ; Se la stringa CLI era già finita (AL=0), c'è solo un argomento

.skip_inter_spaces:
    mov    al, [esi]                    ; Legge gli spazi vuoti tra il primo e il secondo argomento
    cmp    al, 0x20
    je     .next_inter_space
    cmp    al, 0x09
    je     .next_inter_space
    jmp    .lock_explicit_output
.next_inter_space:
    inc    esi
    jmp    .skip_inter_spaces

.lock_explicit_output:
    test   al, al                       ; Se siamo a fine stringa, non c'è un secondo argomento reale
    jz     near .handle_single_argument

    mov    [output_file_path_ptr], esi  ; Salva il puntatore esatto all'inizio del secondo argomento
    call   scan_and_terminate_arg       ; Isola e pulisce il secondo argomento
    jmp    .start_pipeline

.handle_single_argument:
    call   build_automatic_output_name  ; Genera automaticamente il nome di output piatto

.start_pipeline:
    mov    esi, [source_file_path_ptr]  ; Passa il percorso sorgente al lexer
    lea    edx, [token_scratch_matrix]
    call   execute_lexer_32

    lea    esi, [token_scratch_matrix]  ; Passa la matrice al tokenizer
    call   execute_tokenizer_32
    
    lea    esi, [token_scratch_matrix]  ; Avvia l'analisi sintattica e la generazione nel parser
    call   execute_universal_parser
    ret

.cli_error_exit:
    mov    dword [core_error_code], 0x01 ; Codice Errore CLI
    ret

scan_and_terminate_arg:
    ; Scansiona la stringa CLI. Termina al primo spazio/tab inserendo uno 0.
    ; Restituisce in AL il carattere che ha interrotto la scansione (0, spazio o tab).
.scan_loop:
    mov    al, [esi]
    test   al, al
    jz     .found_term
    cmp    al, 0x20
    je     .found_term
    cmp    al, 0x09
    je     .found_term
    inc    esi
    jmp    .scan_loop
.found_term:
    push   eax                          ; Salva il carattere di stop (0 o spazio)
    mov    byte [esi], 0                ; Taglia e chiude la stringa localmente (Null-termination in-place)
    inc    esi                          ; Sposta ESI sul carattere successivo per i controlli futuri
    pop    eax                          ; Ripristina il carattere di stop in AL per il chiamante
    ret

build_automatic_output_name:
    push   esi
    push   edi
    mov    esi, [source_file_path_ptr]
    lea    edi, [output_file_path_buffer]
    mov    [output_file_path_ptr], edi  ; Imposta la variabile globale sul nostro buffer statico

.copy_auto:
    mov    al, [esi]
    mov    [edi], al
    test   al, al
    jz     near .find_last_dot          ; Copia finita, passiamo alla ricerca dell'estensione
    inc    esi
    inc    edi
    jmp    .copy_auto

.find_last_dot:
    ; EDI adesso punta allo zero terminale del buffer copiato. Torniamo indietro (scansione retrograda)
    ; per trovare l'ULTIMO punto, evitando di rompere i percorsi con cartelle dot-named tipo "../dir/"
    mov    ecx, edi                     ; Salva la fine per calcolare i limiti
    mov    edi, [output_file_path_ptr]  ; Ripartiamo dall'inizio del buffer per sicurezza di calcolo dei limiti
    
.scan_backwards:
    dec    ecx
    cmp    ecx, edi                     ; Siamo tornati all'inizio del file senza trovare punti?
    je     .auto_done
    
    mov    al, [ecx]
    cmp    al, '/'                      ; Se incontriamo un separatore di cartella prima del punto, non c'è estensione
    je     .auto_done
    cmp    al, '\'                      ; Supporto opzionale per percorsi in stile Windows
    je     .auto_done
    
    cmp    al, '.'                      ; Trovato l'ultimo punto reale del file!
    jne    .scan_backwards

.truncate:
    mov    byte [ecx], 0                ; Tronca l'estensione (es. "file.asm" diventa "file")

.auto_done:
    pop    edi
    pop    esi
    ret
