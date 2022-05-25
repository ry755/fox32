; use ffmpeg to convert an audio file for playback:
; ffmpeg -i input.mp3 -f s16le -ac 1 -ar 22050 audio.raw

; fox32's audio output works by swapping between two buffers
; interrupt 0xFE (vector 0x000003F8) is fired when it's time to swap buffers
; when that interrupt fires, it calls `refill_buffer` which fills up the buffer

    org 0x00000800

    ; set the interrupt vector for interrupt 0xFE
    mov [0x000003F8], refill_buffer

    ; enable audio playback
    mov r0, 0x80000600
    out r0, 1

    ; hang here
    rjmp 0

refill_buffer:
    not.8 [current_buffer]
    cmp.8 [current_buffer], 0
    ifnz jmp refill_buffer_1

refill_buffer_0:
    mov r31, 8192 ; 32768 bytes = 8192 words
    mov r0, [audio_pointer]
    mov r1, 0x0212C000 ; buffer 0 address
refill_buffer_loop:
    mov [r1], [r0]
    add r0, 4
    add r1, 4
    loop refill_buffer_loop
    mov [audio_pointer], r0
    reti

refill_buffer_1:
    mov r31, 8192 ; 32768 bytes = 8192 words
    mov r0, [audio_pointer]
    mov r1, 0x02134000 ; buffer 1 address
    jmp refill_buffer_loop

current_buffer: data.8 0
audio_pointer: data.32 audio_buffer
audio_buffer:
    #include_bin "audio.raw"
