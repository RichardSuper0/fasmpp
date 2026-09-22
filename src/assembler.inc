execute_instruction_assembly_dispatch:
    ; ESI = slot token istruzione, EDI = buffer di output crudo final
    
    mov    al, [active_isa_target]      ; Legge l'architettura bloccata dal parser
    cmp    al, 1
    je     .x86
    cmp    al, 2
    je     .arm
    cmp    al, 3
    je     .riscv
    ret                                 ; Ritorna se non supportata o vuota

.x86:
    call   assemble_x86_instruction_node ; Salta al sotto-motore in targets/x86_core.inc
    mov    [output_current_ptr], edi    ; Aggiorna contatore byte macchina sul buffer
    ret
.arm:
    call   assemble_arm_instruction_node ; Salta al sotto-motore in targets/arm_core.asm
    mov    [output_current_ptr], edi
    ret
.riscv:
    call   assemble_riscv_instruction_node ; Salta al sotto-motore in targets/riscv_core.inc
    mov    [output_current_ptr], edi
    ret
