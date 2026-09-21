; Macro per allineare le tabelle critiche a 64 byte per azzerare i cache-miss L1
macro align_to_l1
{
    align 64
}

; Isola gli ID dei registri (0-31) eliminando i bit alti di spazzatura hardware
macro mask_register_id reg_32
{
    and reg_32, 0x1F
}

; Inietta un campo registro nell'opcode eseguendo la maschera e lo shift in un colpo
macro inject_reg_field opcode_accum, reg_id, shift_bits
{
    push eax
    mov  eax, reg_id
    and  eax, 0x1F                      ; Forza la maschera hardware a 5 bit
    shl  eax, shift_bits
    or   opcode_accum, eax              ; Fonde il campo nell'opcode finale
    pop  eax
}

; Inietta un immediato generico applicando una maschera rigida prima dello shift
macro inject_imm_field opcode_accum, imm_value, mask, shift_bits
{
    push eax
    mov  eax, imm_value
    and  eax, mask                      ; Taglia l'immediato alla misura richiesta
    shl  eax, shift_bits
    or   opcode_accum, eax              ; Fonde nel pacchetto bitwise
    pop  eax
}
