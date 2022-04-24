; debug monitor shell

monitor_shell_start:
    mov.8 [MODIFIER_BITMAP], 0
    call monitor_shell_clear_buffer
    mov r0, monitor_shell_prompt
    call print_string_to_monitor

monitor_shell_event_loop:
    call get_next_event

    ; was a key pressed?
    cmp r0, EVENT_TYPE_KEY_DOWN
    ifz call monitor_shell_key_down_event

    ; was a key released?
    cmp r0, EVENT_TYPE_KEY_UP
    ifz call monitor_shell_key_up_event

    jmp monitor_shell_event_loop

monitor_shell_key_down_event:
    mov r0, r1

    cmp.8 r0, LSHIFT
    ifz jmp shift_pressed
    cmp.8 r0, RSHIFT
    ifz jmp shift_pressed
    cmp.8 r0, CAPS
    ifz jmp caps_pressed

    ; first, check if enter, delete, or backspace was pressed
    cmp.8 r0, 0x1C ; enter
    ifz jmp monitor_shell_key_down_event_enter
    cmp.8 r0, 0x6F ; delete
    ifz jmp monitor_shell_key_down_event_backspace
    cmp.8 r0, 0x0E ; backspace
    ifz jmp monitor_shell_key_down_event_backspace

    ; otherwise, add it to the text buffer and print it to the screen
    call scancode_to_ascii
    call print_character_to_monitor
    call monitor_shell_push_character
    call redraw_monitor_console_line
    ret
monitor_shell_key_down_event_enter:
    mov r0, 10 ; line feed
    call print_character_to_monitor
    call monitor_shell_parse_line
    call monitor_shell_clear_buffer
    mov r0, monitor_shell_prompt
    call print_string_to_monitor
    ret
monitor_shell_key_down_event_backspace:
    ; delete the last character from the screen, and pop the last character from the buffer
    mov r0, 8 ; backspace character
    call print_character_to_monitor
    mov r0, ' ' ; space character
    call print_character_to_monitor
    mov r0, 8 ; backspace character
    call print_character_to_monitor
    call monitor_shell_delete_character
    call redraw_monitor_console_line
    ret

monitor_shell_key_up_event:
    mov r0, r1

    cmp.8 r0, LSHIFT
    ifz jmp shift_released
    cmp.8 r0, RSHIFT
    ifz jmp shift_released

    ret

monitor_shell_parse_line:
    ; if the line is empty, just return
    cmp.8 [MONITOR_SHELL_TEXT_BUF_BOTTOM], 0
    ifz ret

    ; separate the command from the arguments
    ; store the pointer to the arguments
    mov r0, MONITOR_SHELL_TEXT_BUF_BOTTOM
    mov r1, ' '
    call monitor_shell_tokenize
    mov [MONTIOR_SHELL_ARGS_PTR], r0

    mov r0, MONITOR_SHELL_TEXT_BUF_BOTTOM
    mov r1, test_string
    call compare_string
    ifz mov r0, test_working
    ifz call print_string_to_monitor

    ret
test_string: data.str "test" data.8 0
test_working: data.str "it's working!!!" data.8 10 data.8 0

; return tokens separated by the specified character
; returns the next token in the list
; inputs:
; r0: pointer to null-terminated string
; r1: separator character
; outputs:
; r0: pointer to next token or zero if none
monitor_shell_tokenize:
    cmp.8 [r0], r1
    ifz jmp monitor_shell_tokenize_found_token

    cmp.8 [r0], 0
    ifz mov r0, 0
    ifz ret

    inc r0
    jmp monitor_shell_tokenize
monitor_shell_tokenize_found_token:
    mov.8 [r0], 0
    inc r0
    ret

; push a character to the text buffer
; inputs:
; r0: character
; outputs:
; none
monitor_shell_push_character:
    push r1

    mov r1, [MONITOR_SHELL_TEXT_BUF_PTR]
    cmp r1, MONITOR_SHELL_TEXT_BUF_TOP
    ifgteq jmp monitor_shell_push_character_end
    mov.8 [r1], r0
    inc [MONITOR_SHELL_TEXT_BUF_PTR]
monitor_shell_push_character_end:
    pop r1
    ret

; pop a character from the text buffer and zero it
; inputs:
; none
; outputs:
; r0: character
monitor_shell_delete_character:
    push r1

    mov r1, [MONITOR_SHELL_TEXT_BUF_PTR]
    cmp r1, MONITOR_SHELL_TEXT_BUF_BOTTOM
    iflteq jmp monitor_shell_pop_character_end
    dec [MONITOR_SHELL_TEXT_BUF_PTR]
    movz.8 r0, [r1]
    mov.8 [r1], 0
monitor_shell_pop_character_end:
    pop r1
    ret

; mark the text buffer as empty
; inputs:
; none
; outputs:
; none
monitor_shell_clear_buffer:
    push r0

    ; set the text buffer poinrer to the start of the text buffer
    mov [MONITOR_SHELL_TEXT_BUF_PTR], MONITOR_SHELL_TEXT_BUF_BOTTOM

    ; set the first character as null
    mov r0, [MONITOR_SHELL_TEXT_BUF_PTR]
    mov.8 [r0], 0

    pop r0
    ret

const MONITOR_SHELL_TEXT_BUF_TOP:    0x03ED4000
const MONITOR_SHELL_TEXT_BUF_BOTTOM: 0x03ED3FE0 ; 32 characters
const MONITOR_SHELL_TEXT_BUF_PTR:    0x03ED3FDC ; 4 bytes - pointer to the current input character
const MONTIOR_SHELL_ARGS_PTR:        0x03ED36C5 ; 4 bytes - pointer to the beginning of the command arguments

monitor_shell_prompt: data.str "> " data.8 0
