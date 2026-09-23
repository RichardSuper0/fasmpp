execute_universal_parser:
    ; ESI = token_scratch_matrix, EDI = buffer codice finale
    
.parser_main_loop:
    mov    al, [esi]
    cmp    al, 0xFF                     ; Verifica fine dei token (EOF)
    je     .parser_exit

    test   al, al                       ; Salta slot vuoti
    jz     .parser_next

    cmp    dword [core_error_code], 0   
    jnz    .parser_exit

    call   check_format_directive       
    test   al, al
    jnz    .parser_main_loop            

    call   check_segment_directive      
    test   al, al
    jnz    .parser_main_loop            

    call   check_entry_directive        
    test   al, al
    jnz    .parser_main_loop            

    call   check_symbol_assignment      
    test   al, al
    jnz    .parser_main_loop            

    mov    bl, [active_isa_target]      
    test   bl, bl
    jnz    .dispatch
    call   probe_register_isa_intent
    mov    [active_isa_target], al

.dispatch:
    call   execute_instruction_assembly_dispatch
    cmp    dword [core_error_code], 0   
    jnz    .parser_exit

.parser_next:
    add    esi, 64                      
    jmp    .parser_main_loop

.parser_exit:
    ret

check_format_directive:
    mov    eax, [esi]
    cmp    eax, 'mrof'                  
    jne    .not_format
    cmp    word [esi+4], 'at'           
    jne    .not_format

    add    esi, 64                      
    mov    eax, [esi]
    cmp    eax, 'anib'                  
    je     .set_bin
    cmp    eax, 'FLE '                  
    je     .eval_elf
    cmp    eax, '  EP'                  
    je     .eval_pe

    jmp    .trigger_format_error        

.set_bin:
    mov    byte [output_format_type], 0 
    add    esi, 64                      
    mov    al, 1
    ret
.eval_elf:
    cmp    byte [esi+4], '6'            
    je     .set_elf64
    mov    byte [output_format_type], 1 
    add    esi, 64
    mov    al, 1
    ret
.set_elf64:
    mov    byte [output_format_type], 2
    add    esi, 64
    mov    al, 1
    ret
.eval_pe:
    cmp    byte [esi+4], '6'            
    je     .set_pe64
    mov    byte [output_format_type], 3 
    add    esi, 64
    mov    al, 1
    ret
.set_pe64:
    mov    byte [output_format_type], 4
    add    esi, 64
    mov    al, 1
    ret
.not_format:
    xor    al, al
    ret

check_segment_directive:
    mov    eax, [esi]
    cmp    eax, 'mges'                  
    jne    .not_segment
    cmp    dword [esi+4], 'tnes'        
    jne    .not_segment                 
    
    add    esi, 128                     
    mov    al, 1
    ret
.not_segment:
    xor    al, al
    ret

check_entry_directive:
    mov    eax, [esi]
    cmp    eax, 'rtne'                  
    jne    .not_entry
    cmp    byte [esi+4], 'y'            
    jne    .not_entry

    add    esi, 128                     
    mov    al, 1
    ret
.not_entry:
    xor    al, al
    ret

check_symbol_assignment:
    cmp    byte [esi+64], '='           
    jne    .not_assignment
    
    add    esi, 128                     
    mov    al, 1
    ret
.not_assignment:
    xor    al, al
    ret

probe_register_isa_intent:
    push   esi
    mov    al, [esi]
    cmp    al, 'r'
    je     .eval_r
    cmp    al, 'e'
    je     .confirm_x86
    cmp    byte [esi+1], 'x'
    je     .confirm_x86
    cmp    byte [esi+1], 'p'
    je     .confirm_x86
    cmp    byte [esi+1], 'i'
    je     .confirm_x86
    cmp    al, 'w'
    je     .confirm_arm
    cmp    al, 'x'
    je     .resolve_x
    mov    eax, [esi]
    cmp    eax, 'orez'                  
    je     .confirm_riscv
    xor    al, al
    pop    esi
    ret
.eval_r:
    cmp    byte [esi+1], 'a'
    jne    .confirm_x86
    cmp    byte [esi+2], 0
    jz     .confirm_riscv               
    jmp    .confirm_x86
.resolve_x:
    movzx  eax, byte [default_host_isa_context]
    pop    esi
    ret
.confirm_x86:
    mov    al, 1
    pop    esi
    ret
.confirm_arm:
    mov    al, 2
    pop    esi
    ret
.confirm_riscv:
    mov    al, 3
    pop    esi
    ret

.trigger_format_error:
    mov    text_error_lock, 1           ; Segnala blocco diagnostica interno
    mov    dword [core_error_code], 0x06 
    jmp    .freeze_diagnostics

.trigger_illegal_instruction:
    mov    dword [core_error_code], 0x05 

.freeze_diagnostics:
    mov    [error_token_ptr], esi       
    mov    eax, [esi+52]                
    mov    [error_file_addr], eax
    mov    eax, [esi+56]                
    mov    [error_line], eax
    mov    eax, [esi+60]                
    mov    [error_column], eax
    xor    al, al                       
    ret

text_error_lock db 0
