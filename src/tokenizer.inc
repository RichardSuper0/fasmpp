execute_tokenizer_32:
    ; ESI = matrice grezza lexer, EDX = token_scratch_matrix allineata
    
.tokenizer_loop:
    mov    al, [esi]                    ; Legge carattere corrente dello slot
    test   al, al                       ; Verifica fine del testo
    jz     .tokenizer_done
    cmp    al, 0xFF                     ; Controllo marcatore EOF del lexer
    je     .tokenizer_done

    call   copy_token_to_64byte_slot    ; Taglia rigido a 64 byte per la cache
    add    edx, 64                      ; Sposta in avanti lo slot L1
    add    esi, 64                      ; Avanza nella sorgente grezza
    jmp    .tokenizer_loop

.tokenizer_done:
    mov    byte [edx], 0xFF             ; Inietta marcatore EOF per il parser
    ret

copy_token_to_64byte_slot:
    push   edx                          ; Salva inizio slot corrente
    xor    ecx, ecx                     ; Azzera contatore a 32-bit

.copy_loop:
    mov    al, [esi]                    ; Carica byte dal lexer
    mov    [edx], al                    ; Scrive byte nello scratch pad
    test   al, al                       ; Controlla se stringa terminata
    jz     .token_finished
    
    inc    esi                          ; Avanza sorgente
    inc    edx                          ; Avanza destinazione
    inc    ecx                          ; Incrementa lunghezza token
    cmp    ecx, 63                      ; Evita di sfondare il limite dello slot
    jne    .copy_loop

.token_finished:
    mov    byte [edx], 0                ; Null-termination forzata locale
    pop    edx                          ; Ripristina puntatore base slot
    ret
