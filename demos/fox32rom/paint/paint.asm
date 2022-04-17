    org 0x00000800

    call enable_menu_bar
    call clear_menu_bar
    mov r0, menu_items_root
    mov r1, 0xFFFFFFFF
    call draw_menu_bar_root_items

    call clear_canvas

event_loop:
    call get_next_event

    ; was the mouse clicked?
    cmp r0, EVENT_TYPE_MOUSE_CLICK
    ifz mov.8 [is_drawing], 1

    ; was the mouse released?
    cmp r0, EVENT_TYPE_MOUSE_RELEASE
    ifz mov.8 [is_drawing], 0

    ; did the user click the menu bar?
    cmp r0, EVENT_TYPE_MENU_BAR_CLICK
    ifz mov r0, menu_items_root
    ifz call menu_bar_click_event

    ; is the user in a menu?
    cmp r0, EVENT_TYPE_MENU_UPDATE
    ifz call menu_update_event

    ; did the user click a menu item?
    cmp r0, EVENT_TYPE_MENU_CLICK
    ifz call menu_click_event

    cmp.8 [is_drawing], 0
    ifnz call draw_pixel

    jmp event_loop

menu_click_event:
    ; r2 contains the clicked root menu
    ; r3 contains the clicked menu item

    ; system
    cmp r2, 0
    ifz call system_menu_click_event

    ; canvas
    cmp r2, 1
    ifz call canvas_menu_click_event

    ret

system_menu_click_event:
    ; r2 contains the clicked root menu
    ; r3 contains the clicked menu item

    ; shut down
    cmp r3, 0
    ifz icl
    ifz halt

    ret

canvas_menu_click_event:
    ; r2 contains the clicked root menu
    ; r3 contains the clicked menu item

    ; clear
    cmp r3, 0
    ifz call clear_canvas

    ret

draw_pixel:
    call get_mouse_position
    mov r2, 4
    mov r3, 4
    mov r4, 0xFFFFFFFF
    call draw_filled_rectangle_to_background

    ret

clear_canvas:
    mov r0, 0xFF000000
    call fill_background

    ret

menu_items_root:
    data.8 2                                                              ; number of menus
    data.32 menu_items_system_list data.32 menu_items_system_name         ; pointer to menu list, pointer to menu name
    data.32 menu_items_background_list data.32 menu_items_background_name ; pointer to menu list, pointer to menu name
menu_items_system_name:
    data.8 6 data.str "System" data.8 0x00 ; text length, text, null-terminator
menu_items_background_name:
    data.8 6 data.str "Canvas" data.8 0x00 ; text length, text, null-terminator
menu_items_system_list:
    data.8 1                                     ; number of items
    data.8 11                                    ; menu width (usually longest item + 2)
    data.8 9  data.str "Shut Down" data.8 0x00   ; text length, text, null-terminator
menu_items_background_list:
    data.8 1                                     ; number of items
    data.8 7                                     ; menu width (usually longest item + 2)
    data.8 5  data.str "Clear" data.8 0x00       ; text length, text, null-terminator

is_drawing: data.8 0

    #include "../../../fox32rom/fox32rom.def"
