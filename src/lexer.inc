execute_lexer_32:
    ; ESI = sorgente asm, EDX = token scratch pad (slot 64 byte)
    
.lexer_loop:
    cmp    dword [core_error_code], 0   ; Controlla se e gia scattato un errore
    jnz    .lexer_done                  ; Se diverso da 0 stacca tutto e abortisce

    mov    al, [esi]                    ; Legge carattere dal file sorgente
    test   al, al                       ; Controlla fine testo
    jz     .lexer_done                  ; Esce al terminatore zero

    inc    dword [current_column]       ; Incrementa colonna per ogni byte letto

    check_delimiter_and_skip al, 0x20, .skip_char
    check_delimiter_and_skip al, 0x09, .skip_char
    check_delimiter_and_skip al, 0x0D, .skip_char
    check_delimiter_and_skip al, 0x0A, .handle_newline

    cmp    al, ';'                      ; Intercetta commento
    je     .skip_comment

    call   check_for_include_directive  ; Gestione include ricorsivi
    test   al, al
    jnz    .lexer_loop                  ; Se include processato, riavvia ciclo

    call   extract_token_to_matrix_slot ; Isola il token
    add    edx, 64                      ; Avanza allo slot successivo L1
    jmp    .lexer_loop

.skip_char:
    inc    esi                          ; Avanza sorgente
    jmp    .lexer_loop

.handle_newline:
    inc    dword [current_line]         ; Incrementa contatore righe
    mov    dword [current_column], 0    ; Resetta contatore colonne
    inc    esi                          ; Consuma Line Feed
    jmp    .lexer_loop

.skip_comment:
    inc    esi                          ; Salta ';'
    mov    al, [esi]
    test   al, al
    jz     .lexer_done
    cmp    al, 0x0A                     ; Cerca fine riga
    jne    .skip_comment
    jmp    .handle_newline

.lexer_done:
    mov    byte [edx], 0xFF             ; Marcatore EOF per l'affettatrice
    ret

check_for_include_directive:
    push   esi
    mov    eax, [esi]                   ; Carica primi 4 byte del token
    cmp    eax, 'clni'                  ; Verifica "incl"
    jne    .no_include
    
    mov    bl, [esi+4]
    cmp    bl, 'u'                      ; Verifica "u"
    jne    .no_include
    mov    bl, [esi+5]
    cmp    bl, 'd'                      ; Verifica "d"
    jne    .no_include
    mov    bl, [esi+6]
    cmp    bl, 'e'                      ; Verifica "e"
    jne    .no_include

    add    esi, 7                       ; Salta "include"
.find_quote:
    mov    al, [esi]
    inc    esi
    test   al, al
    jz     .error_include
    check_delimiter_and_skip al, 0x20, .find_quote ; Salta spazi pre-apice
    cmp    al, "'"                      ; Controlla apice
    jne    .no_include

    lea    edi, [include_path_buffer]   ; Buffer percorso
    xor    ecx, ecx                     ; Azzera contatore a 32-bit
.copy_path:
    mov    al, [esi]
    inc    esi
    cmp    al, "'"                      ; Cerca apice chiusura
    je     .path_extracted
    test   al, al
    jz     .error_include
    
    mov    [edi+ecx], al                ; Scrive nel buffer
    inc    ecx
    cmp    ecx, 255                     ; Protezione overrun
    jae    .error_include
    jmp    .copy_path

.path_extracted:
    mov    byte [edi+ecx], 0            ; Termina stringa path
    
    push   edx                          ; Salva matrice
    mov    esi, include_path_buffer
    lea    edi, [include_file_load_area]
    call   sys_load_include_file_to_ram ; Syscall host caricamento
    
    lea    esi, [include_file_load_area]
    pop    edx
    call   execute_lexer_32             ; Lexer ricorsivo
    
    pop    eax
    mov    al, 1                        ; Segnala include eseguito
    ret

.no_include:
    pop    esi
    xor    al, al
    ret

.error_include:
    mov    dword [core_error_code], 0x02 ; Errore 2: Sintassi include errata
    pop    esi
    xor    al, al
    ret

extract_token_to_matrix_slot:
    push   edx
    
    ; Timbro metadati geometrici negli ultimi 12 byte dello slot
    mov    eax, esi                     ; Indirizzo RAM corrente del file
    mov    [edx+52], eax
    mov    eax, [current_line]
    mov    [edx+56], eax                ; Salva riga
    mov    eax, [current_column]
    mov    [edx+60], eax                ; Salva colonna

    xor    ecx, ecx                     ; Contatore lunghezza a 32-bit
.tok_loop:
    mov    al, [esi]
    test   al, al
    jz     .tok_done
    
    check_delimiter_and_skip al, 0x20, .tok_done
    check_delimiter_and_skip al, 0x09, .tok_done
    check_delimiter_and_skip al, 0x0A, .tok_done
    check_delimiter_and_skip al, 0x0D, .tok_done
    check_delimiter_and_skip al, ',', .tok_done
    check_delimiter_and_skip al, ';', .tok_done

    mov    [edx], al                    ; Scrive nello scratch pad
    inc    esi
    inc    dword [current_column]       ; Avanza contatore colonne nella copia
    inc    edx
    inc    ecx
    cmp    ecx, 51                      ; Protezione metadati in coda
    jne    .tok_loop
.tok_done:
    mov    byte [edx], 0                ; Termina token localmente
    pop    edx
    ret
