; menu bar routines

; clear menu bar
; inputs:
; none
; outputs:
; none
clear_menu_bar:
    push r0
    push r1
    push r2
    push r3
    push r4
    push r5
    push r31

    mov r31, 80
    movz.8 r0, ' '
    mov r1, 0
    mov r2, 0
    mov r3, 0xFF000000
    mov r4, 0xFFFFFFFF
    mov r5, 30
clear_menu_bar_loop:
    call draw_font_tile_to_overlay
    add r1, 8
    loop clear_menu_bar_loop

    pop r31
    pop r5
    pop r4
    pop r3
    pop r2
    pop r1
    pop r0
    ret


; draw root menu bar items
; inputs:
; r0: pointer to menu bar root struct
; r1: selected root menu item (or 0xFFFFFFFF for none)
; outputs:
; none
draw_menu_bar_root_items:
    push r0
    push r1
    push r2
    push r3
    push r4
    push r5
    push r6
    push r29
    push r30
    push r31

    movz.8 r31, [r0]              ; load number of root menu items into r31 for looping
    mov r30, r1                   ; r30: number of the selected item
    mov r29, 0                    ; counter of how many menu items drawn so far
    mov r6, r0
    add r6, 5                     ; point to start of root menu items text pointer
    mov r1, 16                    ; X = 16
    mov r2, 0                     ; Y = 0
    mov r5, 30                    ; overlay 30
draw_menu_bar_root_items_loop:
    cmp r30, r29
    ifz mov r3, 0xFFFFFFFF        ; foreground color: white
    ifz mov r4, 0xFF000000        ; background color: black
    ifnz mov r3, 0xFF000000       ; foreground color: black
    ifnz mov r4, 0xFFFFFFFF       ; background color: white

    ; draw colored space before text
    sub r1, 8
    movz.8 r0, ' '
    call draw_font_tile_to_overlay
    add r1, 8

    mov r0, [r6]                  ; get pointer to text
    inc r0                        ; increment past length byte
    call draw_str_to_overlay      ; draw menu item text

    ; draw colored space after text
    movz.8 r0, ' '
    call draw_font_tile_to_overlay

    add r1, 16                    ; add some space next to this menu item
    add r6, 8                     ; increment pointer to text pointer
    inc r29
    loop draw_menu_bar_root_items_loop

    pop r31
    pop r30
    pop r29
    pop r6
    pop r5
    pop r4
    pop r3
    pop r2
    pop r1
    pop r0
    ret

; handle menu bar click
; inputs:
; r0: pointer to menu bar root struct
; r1: X position where the menu bar was clicked
; outputs:
; none
menu_bar_click_event:
    push r0
    push r1
    push r2
    push r3
    push r4
    push r5
    push r28
    push r29
    push r30
    push r31

    ; move the X coordinate to r3
    mov r3, r1
    push r0
    ; the user might've clicked on a root menu item, check to see if they did and what button they clicked
    movz.8 r31, [r0]              ; load number of root menu items into r31 for looping
    mov r30, 0                    ; use r30 as an incrementing counter of menu item lengths calculated so far
    mov r29, 16                   ; use r29 as the starting X coord of the current menu item
    ;mov r28, 0                    ; use r28 as the ending X coord of the current menu item
    mov r4, r0
    add r4, 5                     ; point to start of root menu items text pointer
menu_bar_click_event_loop:
    mov r0, [r4]                  ; get pointer to text
    movz.8 r1, [r0]               ; get length byte
    mul r1, 8                     ; calculate the length in pixels
    mov r28, r1
    add r28, r29                  ; use r28 as the ending X coord of the current menu item
    mov r2, r1
    ; now we need to check if the mouse's X coord is between the values of r29 and r28
    ; if carry flag is set, value is less than
    ; if carry flag is clear, value is greater than or equal to
    mov r1, r3
    ; this is a trick to check if a value is within a certain range
    ; see https://stackoverflow.com/questions/5196527/double-condition-checking-in-assembly for info
    sub r1, r29
    sub r28, r29
    cmp r28, r1
    ifnc jmp menu_bar_click_event_found_item
    inc r30                       ; increment counter of menu item lengths calculated so far
    add r29, r2                   ; add the size in pixels of the current root menu item to the counter
    add r29, 16                   ; add 16 pixels to account for the space between the menu items
    add r4, 8                     ; increment pointer to text pointer
    loop menu_bar_click_event_loop
    ; if we reach this point, then the user didn't click on any root menu items
    ; redraw the root menu items without anything selected
    pop r0
    ;mov r1, 0xFFFFFFFF
    ;call draw_menu_bar_root_items ; close_submenu already calls this
    call close_submenu

    pop r31
    pop r30
    pop r29
    pop r28
    pop r5
    pop r4
    pop r3
    pop r2
    pop r1
    pop r0
    ret
menu_bar_click_event_found_item:
    ; r30 contains the clicked root menu item (starting at 0)
    pop r0
    mov r1, r30
    mov r2, 0xFFFFFFFF
    call draw_menu_bar_root_items
    call draw_submenu_items

    ; push a submenu_update_event_type event to the event stack
    mov r1, r0                    ; event parameter 0: pointer to menu bar root struct
    mov r2, r30                   ; event parameter 1: selected root menu item
    mov r3, 0xFFFFFFFF            ; event parameter 2: hovering submenu item (or 0xFFFFFFFF for none)
    mov r4, 0
    mov r5, 0
    mov r0, submenu_update_event_type
    call push_event

    pop r31
    pop r30
    pop r29
    pop r28
    pop r5
    pop r4
    pop r3
    pop r2
    pop r1
    pop r0
    ret

;menu_items_root:
;    def.8 3                                                     ; number of submenus
;    def.32 menu_items_file_list   def.32 menu_items_file_name   ; pointer to submenu list, pointer to submenu name
;    def.32 menu_items_edit_list   def.32 menu_items_edit_name   ; pointer to submenu list, pointer to submenu name
;    def.32 menu_items_system_list def.32 menu_items_system_name ; pointer to submenu list, pointer to submenu name
;menu_items_file_name:
;    def.8 4 def.str "File" def.8 0x00   ; text length, text, null-terminator
;menu_items_file_list:
;    def.8 2                             ; number of items
;    def.8 6                             ; submenu width (in number of characters)
;    def.8 6 def.str "Test 1" def.8 0x00 ; text length, text, null-terminator
;    def.8 6 def.str "Test 2" def.8 0x00 ; text length, text, null-terminator
;menu_items_edit_name:
;    def.8 4 def.str "Edit" def.8 0x00   ; text length, text, null-terminator
;menu_items_edit_list:
;    def.8 2                             ; number of items
;    def.8 6                             ; submenu width (in number of characters)
;    def.8 6 def.str "Test 3" def.8 0x00 ; text length, text, null-terminator
;    def.8 6 def.str "Test 4" def.8 0x00 ; text length, text, null-terminator
;menu_items_system_name:
;    def.8 6 def.str "System" def.8 0x00 ; text length, text, null-terminator
;menu_items_system_list:
;    def.8 4                             ; number of items
;    def.8 6                             ; submenu width (in number of characters)
;    def.8 6 def.str "Test 5" def.8 0x00 ; text length, text, null-terminator
;    def.8 6 def.str "Test 6" def.8 0x00 ; text length, text, null-terminator
;    def.8 6 def.str "Test 7" def.8 0x00 ; text length, text, null-terminator
;    def.8 6 def.str "Test 8" def.8 0x00 ; text length, text, null-terminator