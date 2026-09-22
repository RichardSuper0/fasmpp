execute_lexer_32:
    ; ESI = puntatore sorgente asm, EDX = token scratch pad (slot 64 byte)
    
.lexer_loop:
    mov    al, [esi]                    ; Legge carattere dal file sorgente
    test   al, al                       ; Controlla fine testo
    jz     .lexer_done                  ; Esce se trova il terminatore zero

    check_delimiter_and_skip al, 0x20, .skip_char
    check_delimiter_and_skip al, 0x09, .skip_char
    check_delimiter_and_skip al, 0x0D, .skip_char
    check_delimiter_and_skip al, 0x0A, .handle_newline

    cmp    al, ';'                      ; Intercetta inizio di un commento
    je     .skip_comment

    call   check_for_include_directive  ; Controlla parola include
    test   al, al                       ; Se AL e 1, include processato
    jnz    .lexer_loop                  ; Riavvia il loop sul file incluso

    call   extract_token_to_matrix_slot ; Isola il token nella matrice
    add    edx, 64                      ; Avanza rigido al prossimo slot L1
    jmp    .lexer_loop

.skip_char:
    inc    esi                          ; Avanza al carattere successivo
    jmp    .lexer_loop

.handle_newline:
    inc    esi                          ; Consuma il Line Feed
    jmp    .lexer_loop

.skip_comment:
    inc    esi                          ; Salta il carattere ';'
    mov    al, [esi]                    ; Legge il contenuto del commento
    test   al, al                       ; Controllo fine file improvvisa
    jz     .lexer_done
    cmp    al, 0x0A                     ; Cerca il fine riga (Line Feed)
    jne    .skip_comment                ; Continua a saltare nella riga
    jmp    .handle_newline

.lexer_done:
    mov    byte [edx], 0xFF             ; Inietta marcatore EOF per il parser
    ret

check_for_include_directive:
    push   esi                          ; Salva puntatore di scansione
    mov    eax, [esi]                   ; Carica i primi 4 byte del token
    cmp    eax, 'clni'                  ; Verifica stringa "incl" (Little Endian)
    jne    .no_include
    
    mov    bl, [esi+4]                  ; Quinto carattere
    cmp    bl, 'u'                      ; Verifica "u"
    jne    .no_include
    mov    bl, [esi+5]                  ; Sesto carattere
    cmp    bl, 'd'                      ; Verifica "d"
    jne    .no_include
    mov    bl, [esi+6]                  ; Settimo carattere
    cmp    bl, 'e'                      ; Verifica "e"
    jne    .no_include

    add    esi, 7                       ; Salta la parola "include"
.find_quote:
    mov    al, [esi]                    ; Legge caratteri post direttiva
    inc    esi
    test   al, al
    jz     .error_include
    check_delimiter_and_skip al, 0x20, .find_quote ; Salta spazi pre-apice
    cmp    al, "'"                      ; Controlla se trova l'apice
    jne    .no_include

    lea    edi, [include_path_buffer]   ; Buffer percorso
    xor    ecx, ecx                     ; Azzera contatore a 32-bit
.copy_path:
    mov    al, [esi]                    ; Legge caratteri del path
    inc    esi
    cmp    al, "'"                      ; Cerca apice di chiusura
    je     .path_extracted
    test   al, al                       ; Errore se file si interrompe prima
    jz     .error_include
    
    mov    [edi+ecx], al                ; Scrive nel buffer
    inc    ecx
    cmp    ecx, 255                     ; Protezione buffer overrun
    jae    .error_include
    jmp    .copy_path

.path_extracted:
    mov    byte [edi+ecx], 0            ; Inietta Null-termination
    
    push   edx                          ; Salva posizione matrice
    mov    esi, include_path_buffer     ; Passa il percorso estratto
    lea    edi, [include_file_load_area] ; Buffer di transito area RAM
    call   sys_load_include_file_to_ram ; Syscall host caricamento file
    
    lea    esi, [include_file_load_area] ; Punta al testo in RAM
    pop    edx                          ; Ripristina matrice
    call   execute_lexer_32             ; Lexer ricorsivo su stesso EDX
    
    pop    eax                          ; Ripristina stack
    mov    al, 1                        ; Segnala include eseguito
    ret

.no_include:
    pop    esi                          ; Ripristina puntatore originale
    xor    al, al                       ; Ritorna zero (nessun include)
    ret

.error_include:
    mov    dword [core_error_code], 0x02 ; Errore 2: Sintassi include errata
    pop    esi
    xor    al, al
    ret

extract_token_to_matrix_slot:
    push   edx                          ; Salva inizio slot corrente
    xor    ecx, ecx                     ; Contatore lunghezza a 32-bit
.tok_loop:
    mov    al, [esi]                    ; Legge byte della parola
    test   al, al                       ; Controlla fine testo
    jz     .tok_done
    
    check_delimiter_and_skip al, 0x20, .tok_done
    check_delimiter_and_skip al, 0x09, .tok_done
    check_delimiter_and_skip al, 0x0A, .tok_done
    check_delimiter_and_skip al, 0x0D, .tok_done
    check_delimiter_and_skip al, ',', .tok_done
    check_delimiter_and_skip al, ';', .tok_done

    mov    [edx], al                    ; Scrive nello scratch pad dello slot
    inc    esi                          ; Avanza nella sorgente
    inc    edx                          ; Avanza nello slot RAM
    inc    ecx
    cmp    ecx, 63                      ; Protezione overrun dello slot
    jne    .tok_loop
.tok_done:
    mov    byte [edx], 0                ; Null-termination forzata slot
    pop    edx                          ; Ripristina indirizzo base slot
    ret
