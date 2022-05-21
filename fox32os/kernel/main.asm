; fox32os kernel

    org 0x00000800

const FOX32OS_VERSION_MAJOR: 0
const FOX32OS_VERSION_MINOR: 1
const FOX32OS_VERSION_PATCH: 0

const BACKGROUND_COLOR: 0xFF674764
const TEXT_COLOR:       0xFFFFFFFF

    jmp entry

jump_table:
    ; system jump table
    org.pad 0x00000810
    data.32 get_os_version

    ; FXF jump table
    org.pad 0x00000820
    data.32 parse_fxf_binary

    ; task jump table
    org.pad 0x00000830
    data.32 new_task
    data.32 yield_task
    data.32 end_current_task

    ; memory jump table
    org.pad 0x00000840
    data.32 allocate_memory

    ; initialization code
entry:
    ; clear the background
    mov r0, BACKGROUND_COLOR
    call fill_background

draw_startup_text:
    mov r0, startup_str
    mov r1, 16
    mov r2, 464
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    mov r10, FOX32OS_VERSION_MAJOR
    mov r11, FOX32OS_VERSION_MINOR
    mov r12, FOX32OS_VERSION_PATCH
    call draw_format_str_to_background

    ; open startup.cfg
    mov r0, startup_file
    mov r1, 0
    mov r2, startup_file_struct
    call ryfs_open
    cmp r0, 0
    ifz jmp startup_error

    ; load the first 11 bytes of startup.cfg, overwriting the "startup cfg" string
    mov r0, 11
    mov r1, startup_file_struct
    mov r2, startup_file
    call ryfs_read

    ; open the actual startup file
    mov r0, startup_file
    mov r1, 0
    mov r2, startup_file_struct
    call ryfs_open
    cmp r0, 0
    ifz jmp startup_error

    ; read the startup file into memory starting at 0x03000000
    mov r0, startup_file_struct
    mov r1, 0x03000000
    call ryfs_read_whole_file

    ; relocate and execute it as a new task
    mov r0, 0x03000000
    call parse_fxf_binary
    mov r1, r0
    mov r0, 0
    mov r2, rsp
    call new_task

    ; when the startup file yields for the first time, we'll end up back here
    ; jump back to it without adding this "task" (not really a task) into the queue
    ; this does not return
    call end_current_task

startup_error:
    mov r0, BACKGROUND_COLOR
    call fill_background

    mov r0, startup_error_str
    mov r1, 16
    mov r2, 464
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    mov r10, FOX32OS_VERSION_MAJOR
    mov r11, FOX32OS_VERSION_MINOR
    mov r12, FOX32OS_VERSION_PATCH
    call draw_format_str_to_background
    rjmp 0

get_os_version:
    mov r0, FOX32OS_VERSION_MAJOR
    mov r1, FOX32OS_VERSION_MINOR
    mov r2, FOX32OS_VERSION_PATCH
    ret

    #include "allocator.asm"
    #include "fxf/fxf.asm"
    #include "task.asm"

startup_str: data.str "fox32 - OS version %u.%u.%u" data.8 0
startup_error_str: data.str "fox32 - OS version %u.%u.%u - startup.cfg is invalid!" data.8 0

startup_file: data.str "startup cfg"
startup_file_struct: data.32 0 data.32 0

    #include "../../fox32rom/fox32rom.def"

kernel_bottom:
