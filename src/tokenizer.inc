execute_tokenizer_32:
    ; ESI = matrice grezza lexer, EDX = token_scratch_matrix allineata L1
    
.tokenizer_loop:
    cmp    dword [core_error_code], 0   ; Controlla se e gia scattato un errore
    jnz    .tokenizer_done              ; Se c'e un errore si ferma e bom

    mov    al, [esi]
    test   al, al
    jz     .tokenizer_done
    cmp    al, 0xFF                     ; EOF segnalato dal lexer
    je     .tokenizer_done

    call   copy_token_and_metadata      ; Copia stringa e metadati geometrici
    add    edx, 64                      ; Prossimo slot Cache L1
    add    esi, 64                      ; Prossimo slot sorgente
    jmp    .tokenizer_loop

.tokenizer_done:
    mov    byte [edx], 0xFF             ; Marcatore fine per il parser
    ret

copy_token_and_metadata:
    push   edx
    xor    ecx, ecx                     ; Contatore lunghezza token

.copy_loop:
    mov    al, [esi]
    mov    [edx], al                    ; Copia stringa token
    test   al, al
    jz     .metadata_flush
    inc    esi
    inc    edx
    inc    ecx
    cmp    ecx, 51                      ; Protezione coda metadati
    jne    .copy_loop

.metadata_flush:
    pop    edx                          ; Ripristina indirizzo base slot
    push   edx
    
    ; Sposta in blocco i 12 byte di metadati geometrici calcolati dal lexer
    mov    eax, [esi+52]                ; Legge indirizzo RAM del file
    mov    [edx+52], eax
    mov    eax, [esi+56]                ; Legge riga
    mov    [edx+56], eax
    mov    eax, [esi+60]                ; Legge colonna
    mov    [edx+60], eax

    pop    edx
    ret
