; ============================================================
; MACRO E ROUTINE COMUNI
; ============================================================

macro align_to_l1
{
    align 64
}

macro mask_reg_id reg
{
    and reg, 0x1F
}

; opcode_accum deve essere un registro o una memoria.
; ECX viene usato come temporaneo e viene preservato.
macro inject_reg_field opcode_accum, reg_id, shift_bits
{
    push ecx

    mov  ecx, reg_id
    and  ecx, 0x1F

    if shift_bits > 0
        shl ecx, shift_bits
    end if

    or   opcode_accum, ecx

    pop  ecx
}

macro inject_imm_field opcode_accum, imm_value, mask, shift_bits
{
    push ecx

    mov  ecx, imm_value
    and  ecx, mask

    if shift_bits > 0
        shl ecx, shift_bits
    end if

    or   opcode_accum, ecx

    pop  ecx
}

macro check_delimiter_and_skip char_reg, delimiter, target_label
{
    cmp char_reg, delimiter
    je  target_label
}

macro byte_swap_32 reg
{
    bswap reg
}

macro compare_token_4bytes token_ptr, dword_literal, match_label
{
    mov eax, dword [token_ptr]
    cmp eax, dword_literal
    je  match_label
}

; ============================================================
; LOADER INCLUDE DI FALLBACK
; ============================================================
;
; Questa routine mantiene compatibilità con:
;
;     call sys_load_include_file_to_ram
;
; Parametri previsti:
;     ESI = percorso del file
;     EDI = area RAM destinazione
;
; Un blob flat non possiede automaticamente accesso al filesystem.
; La routine segnala quindi errore 0x02 senza creare simboli mancanti.
;
; In futuro potrà essere sostituita da una syscall o da un loader
; fornito dal programma host.

sys_load_include_file_to_ram:
    mov dword [core_error_code], 0x02
    xor eax, eax
    ret
