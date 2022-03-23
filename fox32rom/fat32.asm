; FAT32 routines
; see fatgen103.pdf for details

; get BPB data and fill the BPB struct
fat32_get_bpb_data:
    mov r0, 0x80002000       ; command to set the location of the buffer
    mov r1, 0x80003000       ; command to read a sector from disk 0 into the buffer

    out r0, sector_buffer    ; set the memory buffer location
    out r1, 0                ; read the first sector into memory

    mov r0, sector_buffer
    add r0, 11
    mov.16 [bytes_per_sector], [r0]
    add r0, 2
    mov.8 [sectors_per_cluster], [r0]
    inc r0
    mov.16 [reserved_sector_count], [r0]
    add r0, 2
    mov.8 [number_of_fats], r0
    add r0, 20
    mov [each_fat_size_in_sectors], [r0]
    add r0, 8
    mov [root_directory_cluster], [r0]

    mov r0, bytes_per_sector_string
    mov r1, 16
    mov r2, 32
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    movz.16 r10, [bytes_per_sector]
    call draw_format_str_to_background

    mov r0, sectors_per_cluster_string
    mov r1, 16
    add r2, 16
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    movz.8 r10, [sectors_per_cluster]
    call draw_format_str_to_background

    mov r0, reserved_sector_count_string
    mov r1, 16
    add r2, 16
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    movz.16 r10, [reserved_sector_count]
    call draw_format_str_to_background

    mov r0, number_of_fats_string
    mov r1, 16
    add r2, 16
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    movz.8 r10, [number_of_fats]
    call draw_format_str_to_background

    mov r0, each_fat_size_in_sectors_string
    mov r1, 16
    add r2, 16
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    mov r10, [each_fat_size_in_sectors]
    call draw_format_str_to_background

    mov r0, root_directory_cluster_string
    mov r1, 16
    add r2, 16
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    mov r10, [root_directory_cluster]
    call draw_format_str_to_background

    mov r0, [each_fat_size_in_sectors]
    movz.8 r1, [number_of_fats]
    mul r0, r1
    add.16 r0, [reserved_sector_count]
    mov.16 [first_data_sector], r0

    mov r0, first_data_sector_string
    mov r1, 16
    add r2, 16
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    movz.16 r10, [first_data_sector]
    call draw_format_str_to_background

    mov r0, [root_directory_cluster]
    call fat32_get_first_sector_of_cluster
    mov r10, r0

    mov r0, root_directory_sector_string
    mov r1, 16
    add r2, 16
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    call draw_format_str_to_background

    mov r0, [root_directory_cluster]
    call fat32_get_fat_entry_for_cluster
    mov r10, r0
    mov r11, r1

    mov r0, root_directory_fat_string
    mov r1, 16
    add r2, 16
    mov r3, TEXT_COLOR
    mov r4, 0x00000000
    call draw_format_str_to_background



    mov r0, 0x80002000       ; command to set the location of the buffer
    mov r1, 0x80003000       ; command to read a sector from disk 0 into the buffer

    ; read the root directory sector into sector_buffer
    ; set the memory buffer location
    out r0, sector_buffer

    ; find the sector number from the cluster number
    mov r0, [root_directory_cluster]
    call fat32_get_first_sector_of_cluster

    ; read the sector into memory
    out r1, r0

    mov r0, sector_buffer
    add r0, 32

    mov r1, 16
    add r2, 16
    mov r3, TEXT_COLOR
    mov r4, 0xFF000000
    call draw_str_to_background

    ret

; calculate FirstSectorofCluster = ((N – 2) * BPB_SecPerClus) + FirstDataSector
; inputs:
; r0: cluster number
; outputs:
; r0: sector number
fat32_get_first_sector_of_cluster:
    push r1

    sub r0, 2
    movz.8 r1, [sectors_per_cluster]
    mul r0, r1
    movz.16 r1, [first_data_sector]
    add r0, r1

    pop r1
    ret

; find the FAT entry for the specified cluster number
; FATOffset = N * 4
; ThisFATSecNum = BPB_ResvdSecCnt + (FATOffset / BPB_BytsPerSec)
; ThisFATEntOffset = REM(FATOffset / BPB_BytsPerSec)
; see the FAT32 spec for details on the outputs
; inputs:
; r0: cluster number
; outputs:
; r0: ThisFATSecNum
; r1: ThisFATEntOffset
fat32_get_fat_entry_for_cluster:
    push r10
    push r11
    push r12

    mul r0, 4

    mov r10, r0
    movz.16 r11, [bytes_per_sector]
    div r10, r11
    movz.16 r11, [reserved_sector_count]
    add r10, r11

    mov r11, r0
    movz.16 r12, [bytes_per_sector]
    rem r11, r12

    mov r0, r10
    mov r1, r11

    pop r12
    pop r11
    pop r10
    ret

; data

const first_data_sector:        0x01FFF6F0 ; 2 bytes

const sector_buffer:            0x01FFF6F2 ; 512 bytes

; BPB struct:
const bytes_per_sector:         0x01FFF8F2 ; BPB_BytsPerSec, 2 bytes, offset byte 11
const sectors_per_cluster:      0x01FFF8F4 ; BPB_SecPerClus, 1 byte,  offset byte 13
const reserved_sector_count:    0x01FFF8F5 ; BPB_RsvdSecCnt, 2 bytes  offset byte 14
const number_of_fats:           0x01FFF8F7 ; BPB_NumFATS,    1 byte   offset byte 16
const each_fat_size_in_sectors: 0x01FFF8F8 ; BPB_FATSz32,    4 bytes  offset byte 36
const root_directory_cluster:   0x01FFF8FC ; BPB_RootClus,   4 bytes  offset byte 44

bytes_per_sector_string:         data.str "bytes per sector: %u"         data.8 0x00
sectors_per_cluster_string:      data.str "sectors per cluster: %u"      data.8 0x00
reserved_sector_count_string:    data.str "reserved sector count: %u"    data.8 0x00
number_of_fats_string:           data.str "number of fats: %u"           data.8 0x00
each_fat_size_in_sectors_string: data.str "each fat size in sectors: %u" data.8 0x00
root_directory_cluster_string:   data.str "root directory cluster: %u"   data.8 0x00
first_data_sector_string:        data.str "first data sector: %u"        data.8 0x00
root_directory_sector_string:    data.str "root directory sector: %u"    data.8 0x00
root_directory_fat_string:       data.str "root directory ThisFATSecNum: %u, ThisFATEntOffset: %u" data.8 0x00