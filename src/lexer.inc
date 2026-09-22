execute_lexer_32:
    ; ESI = puntatore sorgente asm, EDX = token scratch pad (slot 64 byte)
    
.lexer_loop:
    mov    al, [esi]                    ; Legge carattere dal file sorgente
    test   al, al                       ; Controlla se siamo a fine testo
    jz     .lexer_done                  ; Esce se trova il terminatore zero

    ; Salto spaziature e caratteri di controllo hardware
    check_delimiter_and_skip al, 0x20, .skip_char
    check_delimiter_and_skip al, 0x09, .skip_char
    check_delimiter_and_skip al, 0x0D, .skip_char
    check_delimiter_and_skip al, 0x0A, .handle_newline

    cmp    al, ';'                      ; Intercetta l'inizio di un commento
    je     .skip_comment

    call   check_for_include_directive  ; Controlla la stringa "include"
    test   al, al                       ; Se AL e 1, l'include e stato processato
    jnz    .lexer_loop                  ; Riavvia il loop sul file incluso

    call   extract_token_to_matrix_slot ; Isola il token e lo scrive nella matrice
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
    cmp    al, 0x0A                     ; Cerca il ritorno a capo (fine riga)
    jne    .skip_comment                ; Continua a saltare finche la riga non finisce
    jmp    .handle_newline

.lexer_done:
    mov    byte [edx], 0xFF             ; Inietta il marcatore EOF per il parser
    ret

check_for_include_directive:
    push   esi                          ; Salva il puntatore di scansione corrente
    mov    eax, [esi]                   ; Carica i primi 4 byte del token
    cmp    eax, 'clni'                  ; Verifica la stringa "incl" (Little Endian)
    jne    .no_include
    
    mov    bl, [esi+4]                  ; Carica il quinto carattere
    cmp    bl, 'u'                      ; Verifica "u"
    jne    .no_include
    mov    bl, [esi+5]                  ; Carica il sesto carattere
    cmp    bl, 'd'                      ; Verifica "d"
    jne    .no_include
    mov    bl, [esi+6]                  ; Carica il settimo carattere
    cmp    bl, 'e'                      ; Verifica "e"
    jne    .no_include

    add    esi, 7                       ; Salta l'intera parola "include"
.find_quote:
    mov    al, [esi]                    ; Legge i caratteri post direttiva
    inc    esi
    test   al, al
    jz     .error_include
    check_delimiter_and_skip al, 0x20, .find_quote ; Salta gli spazi prima dell'apice
    cmp    al, "'"                      ; Controlla se trova l'apice di apertura
    jne    .no_include

    lea    edi, [include_path_buffer]   ; Punta al buffer del percorso
    xor    ecx, ecx                     ; ECX contatore lunghezza a 32-bit
.copy_path:
    mov    al, [esi]                    ; Legge i caratteri dentro gli apici
    inc    esi
    cmp    al, "'"                      ; Controlla se trova l'apice di chiusura
    je     .path_extracted
    test   al, al                       ; Errore se il file si interrompe prima
    jz     .error_include
    
    mov    [edi+ecx], al                ; Scrive il carattere nel buffer
    inc    ecx
    cmp    ecx, 255                     ; Protezione overrun stringa percorso
    jae    .error_include
    jmp    .copy_path

.path_extracted:
    mov    byte [edi+ecx], 0            ; Inietta la Null-termination sul percorso
    
    push   edx                          ; Salva la posizione corrente della matrice
    mov    esi, include_path_buffer     ; Passa il percorso estratto
    lea    edi, [include_file_load_area] ; Buffer di transito per il file incluso
    call   sys_load_include_file_to_ram ; Syscall host per caricare il file incluso
    
    lea    esi, [include_file_load_area] ; Punta al testo del file incluso in RAM
    pop    edx                          ; Ripristina la matrice dei token
    call   execute_lexer_32             ; Lexer ricorsivo sullo stesso flusso EDX
    
    pop    eax                          ; Ripristina lo stack pulito
    mov    al, 1                        ; Imposta flag di successo dell'include
    ret

.no_include:
    pop    esi                          ; Ripristina il puntatore originale
    xor    al, al                       ; Ritorna zero (nessun include trovato)
    ret

.error_include:
    mov    dword [core_error_code], 0x02 ; Errore 2: Errore sintassi include
    pop    esi
    xor    al, al
    ret

extract_token_to_matrix_slot:
    push   edx                          ; Salva l'inizio dello slot corrente
    xor    ecx, ecx                     ; Contatore lunghezza a 32-bit
.tok_loop:
    mov    al, [esi]                    ; Legge byte della parola
    test   al, al                       ; Controlla fine testo
    jz     .tok_done
    
    ; Filtri delimitatori per isolare la stringa
    check_delimiter_and_skip al, 0x20, .tok_done
    check_delimiter_and_skip al, 0x09, .tok_done
    check_delimiter_and_skip al, 0x0A, .tok_done
    check_delimiter_and_skip al, 0x0D, .tok_done
    check_delimiter_and_skip al, ',', .tok_done
    check_delimiter_and_skip al, ';', .tok_done

    mov    [edx], al                    ; Scrive il carattere nello scratch pad
    inc    esi                          ; Avanza nella sorgente text
    inc    edx                          ; Avanza nello slot RAM
    inc    ecx
    cmp    ecx, 63                      ; Protezione per non sfondare lo slot
    jne    .tok_loop
.tok_done:
    mov    byte [edx], 0                ; Null-termination forzata del singolo slot
    pop    edx                          ; Ripristina l'indirizzo base dello slot
    ret
