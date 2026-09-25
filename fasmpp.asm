format binary
use32
org 0x0

include 'src/macro.inc'

; ============================================================
; DATI GLOBALI
; ============================================================

align 64

source_file_path_ptr dd 0
output_file_path_ptr dd 0
output_base_ptr      dd 0
output_current_ptr   dd 0
core_error_code      dd 0

current_line         dd 1
current_column       dd 0
error_token_ptr      dd 0
error_file_addr      dd 0
error_line           dd 0
error_column         dd 0

active_isa_target        db 0
default_host_isa_context db 1
output_format_type       db 0
location_counter         dd 0
hardware_entry_point     dd 0

text_error_lock          db 0

; ============================================================
; BUFFER STATICI
; ============================================================

align 64

output_file_path_buffer rb 256

include_path_buffer     rb 256
include_file_load_area  rb 256 * 1024

token_scratch_matrix    rb 64 * 16384

; Buffer di output predefinito per l’uso standalone.
; Un loader esterno può sostituirlo impostando EDI prima dell’entry point.
output_blob_buffer      rb 4 * 1024 * 1024

; ============================================================
; CODICE
; ============================================================

include 'src/trigger.inc'
include 'src/lexer.inc'
include 'src/tokenizer.inc'
include 'src/parser.inc'
include 'src/assembler.inc'

include 'src/targets/x86_core.inc'
include 'src/targets/arm_core.inc'
include 'src/targets/riscv_core.inc'
