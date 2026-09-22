; Allinea tabelle o funzioni a 64 byte per azzerare i cache-miss L1
macro align_to_l1
{
    align 64
}

; Isola gli ID dei registri (0-31) per le architetture ARM e RISC-V
macro mask_reg_id reg_32
{
    and reg_32, 0x1F                    ; Maschera hardware a 5 bit
}

; Inietta un campo registro in un opcode eseguendo maschera e shift
macro inject_reg_field opcode_accum, reg_id, shift_bits
{
    push eax
    mov  eax, reg_id
    and  eax, 0x1F                      ; Forza la maschera a 5 bit
    shl  eax, shift_bits
    or   opcode_accum, eax              ; Fonde il campo nell'opcode
    pop  eax
}

; Inietta un immediato applicando una maschera rigida prima dello shift
macro inject_imm_field opcode_accum, imm_value, mask, shift_bits
{
    push eax
    mov  eax, imm_value
    and  eax, mask                      ; Taglia l'immediato a misura
    shl  eax, shift_bits
    or   opcode_accum, eax              ; Fonde nel pacchetto opcode
    pop  eax
}

; Rileva i delimitatori ASCII per interrompere al volo la scansione delle stringhe
macro check_delimiter_and_skip char_reg, delimiter, target_label
{
    cmp char_reg, delimiter
    je  target_label                    ; Salta all'etichetta se c'e corrispondenza
}

; Inverte l'ordine dei byte per architetture Big Endian o conversioni di rete
macro byte_swap_32 reg_32
{
    bswap reg_32                        ; Inversione hardware istantanea
}

; Macro per verificare se una stringa nello slot da 64 byte combacia con una Dword fissa
macro compare_token_4bytes token_ptr, dword_literal, match_label
{
    mov eax, [token_ptr]
    cmp eax, dword_literal
    je  match_label                     ; Salta se i primi 4 caratteri corrispondono
}
