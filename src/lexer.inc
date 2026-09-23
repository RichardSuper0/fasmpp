execute_lexer_32:
    ; ESI = sorgente asm, EDX = token scratch pad (slot 64 byte)
    
.lexer_loop:
    cmp    dword [core_error_code], 0   ; Controlla se è già scattato un errore
    jnz    .lexer_done                  ; Se diverso da 0 stacca tutto e abortisce

    mov    al, [esi]                    ; Legge carattere dal file sorgente
    test   al, al                       ; Controlla fine testo
    jz     .lexer_done                  ; Esce al terminatore zero

    ; --- CONTROLLO DELIMITATORI E AVANZAMENTO ---
    cmp    al, 0x20                     ; Spazio
    je     .skip_char
    cmp    al, 0x09                     ; Tab
    je     .skip_char
    cmp    al, 0x0D                     ; Carriage Return
    je     .skip_char
    cmp    al, 0x0A                     ; Line Feed (New Line)
    je     .handle_newline

    cmp    al, ';'                      ; Intercetta commento
    je     .skip_comment

    ; Se non è un delimitatore, controlliamo se è una direttiva include
    call   check_for_include_directive  
    test   al, al
    jnz    .lexer_loop                  ; Se include processato, ESI è già aggiornato

    ; Altrimenti è un token normale: lo estraiamo
    call   extract_token_to_matrix_slot ; Isola il token e avanza ESI
    add    edx, 64                      ; Avanza allo slot successivo L1
    jmp    .lexer_loop

.skip_char:
    inc    esi                          ; Avanza nel sorgente
    inc    dword [current_column]       ; Incrementa colonna per ogni byte letto
    jmp    .lexer_loop

.handle_newline:
    inc    dword [current_line]         ; Incrementa contatore righe
    mov    dword [current_column], 0    ; Resetta contatore colonne
    inc    esi                          ; Consuma Line Feed
    jmp    .lexer_loop

.skip_comment:
    inc    esi                          ; Salta ';'
.comment_loop:
    mov    al, [esi]
    test   al, al
    jz     .lexer_done
    cmp    al, 0x0A                     ; Cerca fine riga (Line Feed)
    je     .handle_newline
    inc    esi
    jmp    .comment_loop

.lexer_done:
    mov    byte [edx], 0xFF             ; Marcatore EOF per l'affettatrice
    ret

check_for_include_directive:
    ; Verifica la stringa "include" senza distruggere ESI in caso di fallimento
    push   esi
    mov    eax, [esi]                   ; Carica primi 4 byte
    cmp    eax, 'incl'                  ; Nota: in Little Endian "incl" è 'lcni' invertito,
    jne    .no_include                  ; ma qui usiamo il confronto diretto corretto.
    
    cmp    word [esi+4], 'ud'
    jne    .no_include
    cmp    byte [esi+6], 'e'
    jne    .no_include

    add    esi, 7                       ; Salta "include"
.find_quote:
    mov    al, [esi]
    inc    esi
    test   al, al
    jz     .error_include
    cmp    al, 0x20
    je     .find_quote                  ; Salta spazi pre-apice
    cmp    al, 0x09
    je     .find_quote                  ; Salta tab pre-apice
    cmp    al, "'"                      ; Controlla se c'è l'apice di apertura
    jne    .no_include

    lea    edi, [include_path_buffer]   ; Buffer del percorso file
    xor    ecx, ecx                     ; Azzera contatore lunghezza
.copy_path:
    mov    al, [esi]
    inc    esi
    cmp    al, "'"                      ; Cerca l'apice di chiusura
    je     .path_extracted
    test   al, al
    jz     .error_include
    
    mov    [edi+ecx], al                ; Scrive nel buffer del percorso
    inc    ecx
    cmp    ecx, 255                     ; Protezione buffer overflow
    jae    .error_include
    jmp    .copy_path

.path_extracted:
    mov    byte [edi+ecx], 0            ; Termina la stringa del percorso
    
    ; --- SALVATAGGIO STATO PER RICORSIONE ---
    push   edx                          ; Salva lo slot corrente della matrice dei token
    push   esi                          ; SALVA IL PUNTATORE DEL FILE CORRENTE (Risolve il bug!)
    
    mov    esi, include_path_buffer
    lea    edi, [include_file_load_area]
    call   sys_load_include_file_to_ram ; Richiama la syscall dell'host
    
    lea    esi, [include_file_load_area]
    call   execute_lexer_32             ; Chiamata ricorsiva sul file incluso
    
    pop    esi                          ; RIPRISTINA IL FILE PADRE (Risolve il bug!)
    pop    edx                          ; Ripristina lo slot della matrice
    
    add    esp, 4                       ; Pulisce il push esi iniziale di check_for_include_directive
    mov    al, 1                        ; Segnala che abbiamo processato un include
    ret

.no_include:
    pop    esi
    xor    al, al                       ; Ritorna 0 (non era un include)
    ret

.error_include:
    mov    dword [core_error_code], 0x02 ; Errore 2: Sintassi include errata
    pop    esi
    xor    al, al
    ret

extract_token_to_matrix_slot:
    push   edx
    push   edi
    
    ; Salva i metadati di debug negli ultimi 12 byte dello slot da 64 byte
    mov    eax, esi                     
    mov    [edx+52], eax                ; RAM offset del token
    mov    eax, [current_line]
    mov    [edx+56], eax                ; Numero riga
    mov    eax, [current_column]
    mov    [edx+60], eax                ; Numero colonna

    mov    edi, edx                     ; Destinazione per la scrittura dei caratteri
    xor    ecx, ecx                     ; Contatore lunghezza token
.tok_loop:
    mov    al, [esi]
    test   al, al
    jz     .tok_done
    
    ; Se incontra un delimitatore, interrompe l'estrazione del token corrente
    cmp    al, 0x20
    je     .tok_done
    cmp    al, 0x09
    je     .tok_done
    cmp    al, 0x0A
    je     .tok_done
    cmp    al, 0x0D
    je     .tok_done
    cmp    al, ','
    je     .tok_separator
    cmp    al, ';'
    je     .tok_done

    mov    [edi], al                    ; Scrive il carattere nello slot
    inc    esi
    inc    dword [current_column]       
    inc    edi
    inc    ecx
    cmp    ecx, 51                      ; Lascia spazio per i 12 byte di metadati finali
    jne    .tok_loop
    jmp    .tok_done

.tok_separator:
    ; Gestione speciale per la virgola: la consumiamo come delimitatore ma interrompiamo il token
    inc    esi
    inc    dword [current_column]

.tok_done:
    mov    byte [edi], 0                ; Stringa del token terminata da zero
    pop    edi
    pop    edx
    ret
