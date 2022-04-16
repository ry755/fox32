#include "fox32.h"

#include <stdio.h>
#include <stdnoreturn.h>
#include <string.h>

typedef fox32_err_t err_t;

static const char *const err_messages[] = {
    "",
    "internal error",
    "breakpoint reached",
    "access out of bounds",
    "invalid opcode",
    "invalid condition",
    "invalid register",
    "write to immediate",
    "division by zero",
    "io read failed",
    "io write failed"
};

static const char *err_tostring(err_t err) {
    if (err > 0 && err <= FOX32_ERR_IOWRITE) {
        return err_messages[err];
    }
    return err_messages[FOX32_ERR_OK];
}

typedef fox32_io_read_t io_read_t;
typedef fox32_io_write_t io_write_t;

static int io_read_default_impl(void *user, uint32_t *value, uint32_t port) {
    return (void) user, (void) value, (int) port;
}
static int io_write_default_impl(void *user, uint32_t value, uint32_t port) {
    if (port == 0) {
        putchar((int) value);
        fflush(stdout);
    }
    return (void) user, (int) port;
}

static io_read_t *const io_read_default = io_read_default_impl;
static io_write_t *const io_write_default = io_write_default_impl;

enum {
    OP_NOP   = 0x00,
    OP_ADD   = 0x01,
    OP_MUL   = 0x02,
    OP_AND   = 0x03,
    OP_SLA   = 0x04,
    OP_SRA   = 0x05,
    OP_BSE   = 0x06,
    OP_CMP   = 0x07,
    OP_JMP   = 0x08,
    OP_RJMP  = 0x09,
    OP_PUSH  = 0x0A,
    OP_IN    = 0x0B,
    OP_ISE   = 0x0C,
    OP_IMUL  = 0x0D,
    OP_HALT  = 0x10,
    OP_INC   = 0x11,
    OP_POW   = 0x12,
    OP_OR    = 0x13,
    OP_SRL   = 0x15,
    OP_BCL   = 0x16,
    OP_MOV   = 0x17,
    OP_CALL  = 0x18,
    OP_RCALL = 0x19,
    OP_POP   = 0x1A,
    OP_OUT   = 0x1B,
    OP_ICL   = 0x1C,
    OP_BRK   = 0x20,
    OP_SUB   = 0x21,
    OP_DIV   = 0x22,
    OP_XOR   = 0x23,
    OP_ROL   = 0x24,
    OP_ROR   = 0x25,
    OP_BTS   = 0x26,
    OP_MOVZ  = 0x27,
    OP_LOOP  = 0x28,
    OP_RLOOP = 0x29,
    OP_RET   = 0x2A,
    OP_IDIV  = 0x2D,
    OP_DEC   = 0x31,
    OP_REM   = 0x32,
    OP_NOT   = 0x33,
    OP_RTA   = 0x39,
    OP_RETI  = 0x3A,
    OP_IREM  = 0x3D
};

enum {
    SZ_BYTE,
    SZ_HALF,
    SZ_WORD
};

#define OP(_size, _opcode) (((uint8_t) (_opcode)) | (((uint8_t) (_size)) << 6))

enum {
    CD_ALWAYS,
    CD_IFZ,
    CD_IFNZ,
    CD_IFC,
    CD_IFNC,
    CD_IFGT,
    CD_IFLTEQ
};

enum {
    TY_REG,
    TY_REGPTR,
    TY_IMM,
    TY_IMMPTR,
    TY_NONE
};

enum {
    EX_DEBUGGER,
    EX_FAULT,
    EX_ILLEGAL,
    EX_DIVZERO,
    EX_BUS
};

static uint8_t ptr_get8(const void *ptr) {
    return *((const uint8_t *) ptr);
}
static uint16_t ptr_get16(const void *ptr) {
    const uint8_t *bytes = ptr;
    return
        (((uint16_t) bytes[0])) |
        (((uint16_t) bytes[1]) << 8);
}
static uint32_t ptr_get32(const void *ptr) {
    const uint8_t *bytes = ptr;
    return
        (((uint32_t) bytes[0])) |
        (((uint32_t) bytes[1]) <<  8) |
        (((uint32_t) bytes[2]) << 16) |
        (((uint32_t) bytes[3]) << 24);
}
static void ptr_set8(void *ptr, uint8_t value) {
    *((uint8_t *) ptr) = value;
}
static void ptr_set16(void *ptr, uint16_t value) {
    uint8_t *bytes = ptr;
    bytes[0] = (uint8_t) (value);
    bytes[1] = (uint8_t) (value >> 8);
}
static void ptr_set32(void *ptr, uint32_t value) {
    uint8_t *bytes = ptr;
    bytes[0] = (uint8_t) (value);
    bytes[1] = (uint8_t) (value >>  8);
    bytes[2] = (uint8_t) (value >> 16);
    bytes[3] = (uint8_t) (value >> 24);
}

#define SIZE8 1
#define SIZE16 2
#define SIZE32 4

typedef fox32_vm_t vm_t;

static void vm_init(vm_t *vm) {
    memset(vm, 0, sizeof(vm_t));
    vm->pointer_instr = FOX32_POINTER_DEFAULT_INSTR;
    vm->pointer_stack = FOX32_POINTER_DEFAULT_STACK;
    vm->halted = true;
    vm->interrupt_enabled = true;
    vm->io_user = NULL;
    vm->io_read = io_read_default;
    vm->io_write = io_write_default;
}

static noreturn void vm_panic(vm_t *vm, err_t err) {
    longjmp(vm->panic_jmp, (vm->panic_err = err, 1));
}
static noreturn void vm_unreachable(vm_t *vm) {
    vm_panic(vm, FOX32_ERR_INTERNAL);
}

static uint32_t vm_io_read(vm_t *vm, uint32_t port) {
    uint32_t value = 0;
    int status = vm->io_read(vm->io_user, &value, port);
    if (status != 0) {
        vm_panic(vm, FOX32_ERR_IOREAD);
    }
    return value;
}
static void vm_io_write(vm_t *vm, uint32_t port, uint32_t value) {
    int status = vm->io_write(vm->io_user, value, port);
    if (status != 0) {
        vm_panic(vm, FOX32_ERR_IOWRITE);
    }
}

static uint8_t vm_flags_get(vm_t *vm) {
    return (((uint8_t) vm->flag_carry) << 1) | ((uint8_t) vm->flag_zero);
}
static void vm_flags_set(vm_t *vm, uint8_t flags) {
    vm->flag_zero = (flags & 1) != 0;
    vm->flag_carry = (flags & 2) != 0;
}

static uint32_t *vm_findlocal(vm_t *vm, uint8_t local) {
    if (local < FOX32_REGISTER_COUNT) {
        return &vm->registers[local];
    }
    if (local == FOX32_REGISTER_COUNT) {
        return &vm->pointer_stack;
    }
    vm_panic(vm, FOX32_ERR_BADREGISTER);
}

static uint8_t *vm_findmemory(vm_t *vm, uint32_t address, uint32_t size) {
    if (
        (address + size < FOX32_MEMORY_RAM) ||
        (address >= FOX32_MEMORY_ROM_START && (address -= FOX32_MEMORY_ROM_START) + size < FOX32_MEMORY_ROM)
    ) {
        return &vm->memory_ram[address];
    }
    vm_panic(vm, FOX32_ERR_FAULT);
}

#define VM_READ_BODY(_ptr_get, _size) \
    return _ptr_get(vm_findmemory(vm, address, _size));

static uint8_t vm_read8(vm_t *vm, uint32_t address) {
    VM_READ_BODY(ptr_get8, SIZE8)
}
static uint16_t vm_read16(vm_t *vm, uint32_t address) {
    VM_READ_BODY(ptr_get16, SIZE16)
}
static uint32_t vm_read32(vm_t *vm, uint32_t address) {
    VM_READ_BODY(ptr_get32, SIZE32)
}

#define VM_WRITE_BODY(_ptr_set, _size) \
    _ptr_set(vm_findmemory(vm, address, _size), value);

static void vm_write8(vm_t *vm, uint32_t address, uint8_t value) {
    VM_WRITE_BODY(ptr_set8, SIZE8)
}
static void vm_write16(vm_t *vm, uint32_t address, uint16_t value) {
    VM_WRITE_BODY(ptr_set16, SIZE16)
}
static void vm_write32(vm_t *vm, uint32_t address, uint32_t value) {
    VM_WRITE_BODY(ptr_set32, SIZE32)
}

#define VM_PUSH_BODY(_vm_write, _size) \
    _vm_write(vm, vm->pointer_stack -= _size, value);

static void vm_push8(vm_t *vm, uint8_t value) {
    VM_PUSH_BODY(vm_write8, SIZE8)
}
static void vm_push16(vm_t *vm, uint16_t value) {
    VM_PUSH_BODY(vm_write16, SIZE16)
}
static void vm_push32(vm_t *vm, uint32_t value) {
    VM_PUSH_BODY(vm_write32, SIZE32)
}

#define VM_POP_BODY(_vm_read, _size)                 \
    uint32_t pointer_stack_prev = vm->pointer_stack; \
    return _vm_read(vm, (vm->pointer_stack += _size, pointer_stack_prev));

static uint8_t vm_pop8(vm_t *vm) {
    VM_POP_BODY(vm_read8, SIZE8)
}
static uint16_t vm_pop16(vm_t *vm) {
    VM_POP_BODY(vm_read16, SIZE16)
}
static uint32_t vm_pop32(vm_t *vm) {
    VM_POP_BODY(vm_read32, SIZE32)
}

#define VM_SOURCE_BODY(_vm_read, _size, _type, _move)                           \
    uint32_t pointer_base = vm->pointer_instr_mut;                              \
    switch (prtype) {                                                           \
        case TY_REG: {                                                          \
            if (_move) vm->pointer_instr_mut += SIZE8;                          \
            return (_type) *vm_findlocal(vm, vm_read8(vm, pointer_base));       \
        };                                                                      \
        case TY_REGPTR: {                                                       \
            if (_move) vm->pointer_instr_mut += SIZE8;                          \
            return _vm_read(vm, *vm_findlocal(vm, vm_read8(vm, pointer_base))); \
        };                                                                      \
        case TY_IMM: {                                                          \
            if (_move) vm->pointer_instr_mut += _size;                          \
            return _vm_read(vm, pointer_base);                                  \
        };                                                                      \
        case TY_IMMPTR: {                                                       \
            if (_move) vm->pointer_instr_mut += SIZE32;                         \
            return _vm_read(vm, vm_read32(vm, pointer_base));                   \
        };                                                                      \
    }                                                                           \
    vm_unreachable(vm);

static uint8_t vm_source8(vm_t *vm, uint8_t prtype) {
    VM_SOURCE_BODY(vm_read8, SIZE8, uint8_t, true)
}
static uint8_t vm_source8_stay(vm_t *vm, uint8_t prtype) {
    VM_SOURCE_BODY(vm_read8, SIZE8, uint8_t, false)
}
static uint16_t vm_source16(vm_t *vm, uint8_t prtype) {
    VM_SOURCE_BODY(vm_read16, SIZE16, uint16_t, true)
}
static uint16_t vm_source16_stay(vm_t *vm, uint8_t prtype) {
    VM_SOURCE_BODY(vm_read16, SIZE16, uint16_t, false)
}
static uint32_t vm_source32(vm_t *vm, uint8_t prtype) {
    VM_SOURCE_BODY(vm_read32, SIZE32, uint32_t, true)
}
static uint32_t vm_source32_stay(vm_t *vm, uint8_t prtype) {
    VM_SOURCE_BODY(vm_read32, SIZE32, uint32_t, false)
}

#define VM_TARGET_BODY(_vm_write, _localvalue)                                   \
    uint32_t pointer_base = vm->pointer_instr_mut;                               \
    switch (prtype) {                                                            \
        case TY_REG: {                                                           \
            vm->pointer_instr_mut += SIZE8;                                      \
            uint8_t local = vm_read8(vm, pointer_base);                          \
            *vm_findlocal(vm, local) = _localvalue;                              \
            return;                                                              \
        };                                                                       \
        case TY_REGPTR: {                                                        \
            vm->pointer_instr_mut += SIZE8;                                      \
            _vm_write(vm, *vm_findlocal(vm, vm_read8(vm, pointer_base)), value); \
            return;                                                              \
        };                                                                       \
        case TY_IMM: {                                                           \
            vm_panic(vm, FOX32_ERR_BADIMMEDIATE);                                \
            return;                                                              \
        };                                                                       \
        case TY_IMMPTR: {                                                        \
            vm->pointer_instr_mut += SIZE32;                                     \
            _vm_write(vm, vm_read32(vm, pointer_base), value);                   \
            return;                                                              \
        };                                                                       \
    };                                                                           \
    vm_unreachable(vm);

static void vm_target8(vm_t *vm, uint8_t prtype, uint8_t value) {
    VM_TARGET_BODY(vm_write8, (*vm_findlocal(vm, local) & 0xFFFFFF00) | (uint32_t) value)
}
static void vm_target8_zero(vm_t *vm, uint8_t prtype, uint8_t value) {
    VM_TARGET_BODY(vm_write8, (uint32_t) value)
}
static void vm_target16(vm_t *vm, uint8_t prtype, uint16_t value) {
    VM_TARGET_BODY(vm_write16, (*vm_findlocal(vm, local) & 0xFFFF0000) | (uint32_t) value)
}
static void vm_target16_zero(vm_t *vm, uint8_t prtype, uint16_t value) {
    VM_TARGET_BODY(vm_write16, (uint32_t) value)
}
static void vm_target32(vm_t *vm, uint8_t prtype, uint32_t value) {
    VM_TARGET_BODY(vm_write32, value)
}

static bool vm_shouldskip(vm_t *vm, uint8_t condition) {
    switch (condition) {
        case CD_ALWAYS: {
            return false;
        };
        case CD_IFZ: {
            return vm->flag_zero == false;
        };
        case CD_IFNZ: {
            return vm->flag_zero == true;
        };
        case CD_IFC: {
            return vm->flag_carry == false;
        };
        case CD_IFNC: {
            return vm->flag_carry == true;
        };
        case CD_IFGT: {
            return (vm->flag_zero == false) && (vm->flag_carry == false);
        };
        case CD_IFLTEQ: {
            return (vm->flag_zero == true) || (vm->flag_carry == true);
        };
    }
    vm_panic(vm, FOX32_ERR_BADCONDITION);
}

static void vm_skipparam(vm_t *vm, uint32_t size, uint8_t prtype) {
    if (prtype < TY_IMM) {
        vm->pointer_instr_mut += SIZE8;
    } else {
        vm->pointer_instr_mut += size;
    }
}

#define CHECKED_ADD(_a, _b, _out) __builtin_add_overflow(_a, _b, _out)
#define CHECKED_SUB(_a, _b, _out) __builtin_sub_overflow(_a, _b, _out)
#define CHECKED_MUL(_a, _b, _out) __builtin_mul_overflow(_a, _b, _out)

#define OPER_DIV(_a, _b) ((_a) / (_b))
#define OPER_REM(_a, _b) ((_a) % (_b))
#define OPER_AND(_a, _b) ((_a) & (_b))
#define OPER_XOR(_a, _b) ((_a) ^ (_b))
#define OPER_OR(_a, _b) ((_a) | (_b))
#define OPER_SHIFT_LEFT(_a, _b) ((_a) << (_b))
#define OPER_SHIFT_RIGHT(_a, _b) ((_a) >> (_b))
#define OPER_BIT_SET(_a, _b) ((_a) | (1 << (_b)))
#define OPER_BIT_CLEAR(_a, _b) ((_a) & ~(1 << (_b)))

#define ROTATE_LEFT(_size, _a, _b) (((_a) << (_b)) | ((_a) >> (((_size) * 8) - (_b))))
#define ROTATE_LEFT8(_a, _b) ROTATE_LEFT(SIZE8, _a, _b)
#define ROTATE_LEFT16(_a, _b) ROTATE_LEFT(SIZE16, _a, _b)
#define ROTATE_LEFT32(_a, _b) ROTATE_LEFT(SIZE32, _a, _b)
#define ROTATE_RIGHT(_size, _a, _b) (((_a) >> (_b)) | ((_a) << (((_size) * 8) - (_b))))
#define ROTATE_RIGHT8(_a, _b) ROTATE_RIGHT(SIZE8, _a, _b)
#define ROTATE_RIGHT16(_a, _b) ROTATE_RIGHT(SIZE16, _a, _b)
#define ROTATE_RIGHT32(_a, _b) ROTATE_RIGHT(SIZE32, _a, _b)

#define SOURCEMAP_IDENTITY(x) (x)
#define SOURCEMAP_RELATIVE(x) (instr_base + (x))

#define VM_PRELUDE_0() {                      \
    if (vm_shouldskip(vm, instr_condition)) { \
        break;                                \
    }                                         \
}
#define VM_PRELUDE_1(_size) {                  \
    if (vm_shouldskip(vm, instr_condition)) {  \
        vm_skipparam(vm, _size, instr_source); \
        break;                                 \
    }                                          \
}
#define VM_PRELUDE_2(_size) {                  \
    if (vm_shouldskip(vm, instr_condition)) {  \
        vm_skipparam(vm, _size, instr_target); \
        vm_skipparam(vm, _size, instr_source); \
        break;                                 \
    }                                          \
}

#define VM_IMPL_JMP(_sourcemap) {                                      \
    VM_PRELUDE_1(SIZE32);                                              \
    vm->pointer_instr_mut = _sourcemap(vm_source32(vm, instr_source)); \
    break;                                                             \
}

#define VM_IMPL_LOOP(_sourcemap) {                                         \
    if (                                                                   \
        !vm_shouldskip(vm, instr_condition) &&                             \
        (vm->registers[FOX32_REGISTER_LOOP] -= 1) != 0                     \
    ) {                                                                    \
        vm->pointer_instr_mut = _sourcemap(vm_source32(vm, instr_source)); \
    } else {                                                               \
        vm_skipparam(vm, SIZE32, instr_source);                            \
    }                                                                      \
    break;                                                                 \
}

#define VM_IMPL_CALL(_sourcemap) {                         \
    VM_PRELUDE_1(SIZE32);                                  \
    uint32_t pointer_call = vm_source32(vm, instr_source); \
    vm_push32(vm, vm->pointer_instr_mut);                  \
    vm->pointer_instr_mut = _sourcemap(pointer_call);      \
    break;                                                 \
}

#define VM_IMPL_POP(_size, _vm_target, _vm_pop) { \
    VM_PRELUDE_1(_size);                          \
    _vm_target(vm, instr_source, _vm_pop(vm));    \
    break;                                        \
}

#define VM_IMPL_PUSH(_size, _vm_source, _vm_push) { \
    VM_PRELUDE_1(_size);                            \
    _vm_push(vm, _vm_source(vm, instr_source));     \
    break;                                          \
}

#define VM_IMPL_MOV(_size, _vm_source, _vm_target) {            \
    VM_PRELUDE_2(_size);                                        \
    _vm_target(vm, instr_target, _vm_source(vm, instr_source)); \
    break;                                                      \
}

#define VM_IMPL_NOT(_size, _type, _vm_source_stay, _vm_target) { \
    VM_PRELUDE_1(_size);                                         \
    _type v = _vm_source_stay(vm, instr_source);                 \
    _type x = ~v;                                                \
    vm->flag_zero = x == 0;                                      \
    _vm_target(vm, instr_source, x);                             \
    break;                                                       \
}

#define VM_IMPL_INC(_size, _type, _vm_source_stay, _vm_target, _oper) { \
    VM_PRELUDE_1(_size);                                                \
    _type v = _vm_source_stay(vm, instr_source);                        \
    _type x;                                                            \
    vm->flag_carry = _oper(v, 1, &x);                                   \
    vm->flag_zero = x == 0;                                             \
    _vm_target(vm, instr_source, x);                                    \
    break;                                                              \
}

#define VM_IMPL_ADD(_size, _type, _type_target, _vm_source, _vm_source_stay, _vm_target, _oper) { \
    VM_PRELUDE_2(_size);                                                                          \
    _type a = (_type) _vm_source(vm, instr_source);                                               \
    _type b = (_type) _vm_source_stay(vm, instr_target);                                          \
    _type x;                                                                                      \
    vm->flag_carry = _oper(b, a, &x);                                                             \
    vm->flag_zero = x == 0;                                                                       \
    _vm_target(vm, instr_target, (_type_target) x);                                               \
    break;                                                                                        \
}

#define VM_IMPL_AND(_size, _type, _type_target, _vm_source, _vm_source_stay, _vm_target, _oper) { \
    VM_PRELUDE_2(_size);                                                                          \
    _type a = (_type) _vm_source(vm, instr_source);                                               \
    _type b = (_type) _vm_source_stay(vm, instr_target);                                          \
    _type x = _oper(b, a);                                                                        \
    vm->flag_zero = x == 0;                                                                       \
    _vm_target(vm, instr_target, (_type_target) x);                                               \
    break;                                                                                        \
}

#define VM_IMPL_CMP(_size, _type, _vm_source) { \
    _type a = _vm_source(vm, instr_source);     \
    _type b = _vm_source(vm, instr_target);     \
    _type x;                                    \
    vm->flag_carry = CHECKED_SUB(b, a, &x);     \
    vm->flag_zero = x == 0;                     \
    break;                                      \
}

#define VM_IMPL_BTS(_size, _type, _vm_source) { \
    _type a = _vm_source(vm, instr_source);     \
    _type b = _vm_source(vm, instr_target);     \
    _type x = b & (1 << a);                     \
    vm->flag_zero = x == 0;                     \
    break;                                      \
}

static void vm_execute(vm_t *vm) {
    uint32_t instr_base = vm->pointer_instr;
    uint16_t instr_raw = vm_read16(vm, instr_base);

    uint8_t instr_opcode    = (instr_raw >> 8);
    uint8_t instr_condition = (instr_raw >> 4) & 7;
    uint8_t instr_target    = (instr_raw >> 2) & 3;
    uint8_t instr_source    = (instr_raw     ) & 3;

    vm->pointer_instr_mut = instr_base + SIZE16;

    switch (instr_opcode) {
        case OP(SZ_BYTE, OP_NOP):
        case OP(SZ_HALF, OP_NOP):
        case OP(SZ_WORD, OP_NOP): {
            break;
        };

        case OP(SZ_BYTE, OP_HALT):
        case OP(SZ_HALF, OP_HALT):
        case OP(SZ_WORD, OP_HALT): {
            VM_PRELUDE_0();
            vm->halted = true;
            break;
        };

        case OP(SZ_BYTE, OP_BRK):
        case OP(SZ_HALF, OP_BRK):
        case OP(SZ_WORD, OP_BRK): {
            VM_PRELUDE_0();
            vm_panic(vm, FOX32_ERR_DEBUGGER);
            break;
        };

        case OP(SZ_WORD, OP_IN): {
            VM_PRELUDE_2(SIZE32);
            vm_target32(vm, instr_target, vm_io_read(vm, vm_source32(vm, instr_source)));
            break;
        };
        case OP(SZ_WORD, OP_OUT): {
            VM_PRELUDE_2(SIZE32);
            uint32_t value = vm_source32(vm, instr_source);
            uint32_t port = vm_source32(vm, instr_target);
            vm_io_write(vm, port, value);
            break;
        };

        case OP(SZ_WORD, OP_RTA): {
            VM_PRELUDE_2(SIZE32);
            vm_target32(vm, instr_target, instr_base + vm_source32(vm, instr_source));
            break;
        };

        case OP(SZ_WORD, OP_RET): {
            VM_PRELUDE_0();
            vm->pointer_instr_mut = vm_pop32(vm);
            break;
        };
        case OP(SZ_WORD, OP_RETI): {
            VM_PRELUDE_0();
            vm->interrupt_enabled = vm->interrupt_paused;
            vm->interrupt_paused = false;
            vm_flags_set(vm, vm_pop8(vm));
            vm->pointer_instr_mut = vm_pop32(vm);
            break;
        };

        case OP(SZ_WORD, OP_ISE): {
            VM_PRELUDE_0();
            vm->interrupt_enabled = true;
            vm->interrupt_paused = false;
            break;
        };
        case OP(SZ_WORD, OP_ICL): {
            VM_PRELUDE_0();
            vm->interrupt_enabled = false;
            vm->interrupt_paused = false;
            break;
        };

        case OP(SZ_WORD, OP_JMP): VM_IMPL_JMP(SOURCEMAP_IDENTITY);
        case OP(SZ_WORD, OP_CALL): VM_IMPL_CALL(SOURCEMAP_IDENTITY);
        case OP(SZ_WORD, OP_LOOP): VM_IMPL_LOOP(SOURCEMAP_IDENTITY);
        case OP(SZ_WORD, OP_RJMP): VM_IMPL_JMP(SOURCEMAP_RELATIVE);
        case OP(SZ_WORD, OP_RCALL): VM_IMPL_CALL(SOURCEMAP_RELATIVE);
        case OP(SZ_WORD, OP_RLOOP): VM_IMPL_LOOP(SOURCEMAP_RELATIVE);

        case OP(SZ_BYTE, OP_POP): VM_IMPL_POP(SIZE8, vm_target8, vm_pop8);
        case OP(SZ_HALF, OP_POP): VM_IMPL_POP(SIZE16, vm_target16, vm_pop16);
        case OP(SZ_WORD, OP_POP): VM_IMPL_POP(SIZE32, vm_target32, vm_pop32);

        case OP(SZ_BYTE, OP_PUSH): VM_IMPL_PUSH(SIZE8, vm_source8, vm_push8);
        case OP(SZ_HALF, OP_PUSH): VM_IMPL_PUSH(SIZE16, vm_source16, vm_push16);
        case OP(SZ_WORD, OP_PUSH): VM_IMPL_PUSH(SIZE32, vm_source32, vm_push32);

        case OP(SZ_BYTE, OP_MOV): VM_IMPL_MOV(SIZE8, vm_source8, vm_target8);
        case OP(SZ_BYTE, OP_MOVZ): VM_IMPL_MOV(SIZE8, vm_source8, vm_target8_zero);
        case OP(SZ_HALF, OP_MOV): VM_IMPL_MOV(SIZE16, vm_source16, vm_target16);
        case OP(SZ_HALF, OP_MOVZ): VM_IMPL_MOV(SIZE16, vm_source16, vm_target16_zero);
        case OP(SZ_WORD, OP_MOV):
        case OP(SZ_WORD, OP_MOVZ): VM_IMPL_MOV(SIZE32, vm_source32, vm_target32);

        case OP(SZ_BYTE, OP_NOT): VM_IMPL_NOT(SIZE8, uint8_t, vm_source8_stay, vm_target8);
        case OP(SZ_HALF, OP_NOT): VM_IMPL_NOT(SIZE16, uint16_t, vm_source16_stay, vm_target16);
        case OP(SZ_WORD, OP_NOT): VM_IMPL_NOT(SIZE32, uint32_t, vm_source32_stay, vm_target32);

        case OP(SZ_BYTE, OP_INC): VM_IMPL_INC(SIZE8, uint8_t, vm_source8_stay, vm_target8, CHECKED_ADD);
        case OP(SZ_HALF, OP_INC): VM_IMPL_INC(SIZE16, uint16_t, vm_source16_stay, vm_target16, CHECKED_ADD);
        case OP(SZ_WORD, OP_INC): VM_IMPL_INC(SIZE32, uint32_t, vm_source32_stay, vm_target32, CHECKED_ADD);
        case OP(SZ_BYTE, OP_DEC): VM_IMPL_INC(SIZE8, uint8_t, vm_source8_stay, vm_target8, CHECKED_SUB);
        case OP(SZ_HALF, OP_DEC): VM_IMPL_INC(SIZE16, uint16_t, vm_source16_stay, vm_target16, CHECKED_SUB);
        case OP(SZ_WORD, OP_DEC): VM_IMPL_INC(SIZE32, uint32_t, vm_source32_stay, vm_target32, CHECKED_SUB);

        case OP(SZ_BYTE, OP_ADD): VM_IMPL_ADD(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, CHECKED_ADD);
        case OP(SZ_HALF, OP_ADD): VM_IMPL_ADD(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, CHECKED_ADD);
        case OP(SZ_WORD, OP_ADD): VM_IMPL_ADD(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, CHECKED_ADD);
        case OP(SZ_BYTE, OP_SUB): VM_IMPL_ADD(SIZE8, uint8_t, uint8_t ,vm_source8, vm_source8_stay, vm_target8, CHECKED_SUB);
        case OP(SZ_HALF, OP_SUB): VM_IMPL_ADD(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, CHECKED_SUB);
        case OP(SZ_WORD, OP_SUB): VM_IMPL_ADD(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, CHECKED_SUB);
        case OP(SZ_BYTE, OP_MUL): VM_IMPL_ADD(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, CHECKED_MUL);
        case OP(SZ_HALF, OP_MUL): VM_IMPL_ADD(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, CHECKED_MUL);
        case OP(SZ_WORD, OP_MUL): VM_IMPL_ADD(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, CHECKED_MUL);
        case OP(SZ_BYTE, OP_IMUL): VM_IMPL_ADD(SIZE8, int8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, CHECKED_MUL);
        case OP(SZ_HALF, OP_IMUL): VM_IMPL_ADD(SIZE16, int16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, CHECKED_MUL);
        case OP(SZ_WORD, OP_IMUL): VM_IMPL_ADD(SIZE32, int32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, CHECKED_MUL);

        case OP(SZ_BYTE, OP_DIV): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_DIV);
        case OP(SZ_HALF, OP_DIV): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_DIV);
        case OP(SZ_WORD, OP_DIV): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_DIV);
        case OP(SZ_BYTE, OP_REM): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_REM);
        case OP(SZ_HALF, OP_REM): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_REM);
        case OP(SZ_WORD, OP_REM): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_REM);
        case OP(SZ_BYTE, OP_IDIV): VM_IMPL_AND(SIZE8, int8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_DIV);
        case OP(SZ_HALF, OP_IDIV): VM_IMPL_AND(SIZE16, int16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_DIV);
        case OP(SZ_WORD, OP_IDIV): VM_IMPL_AND(SIZE32, int32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_DIV);
        case OP(SZ_BYTE, OP_IREM): VM_IMPL_AND(SIZE8, int8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_REM);
        case OP(SZ_HALF, OP_IREM): VM_IMPL_AND(SIZE16, int16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_REM);
        case OP(SZ_WORD, OP_IREM): VM_IMPL_AND(SIZE32, int32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_REM);

        case OP(SZ_BYTE, OP_AND): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_AND);
        case OP(SZ_HALF, OP_AND): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_AND);
        case OP(SZ_WORD, OP_AND): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_AND);
        case OP(SZ_BYTE, OP_XOR): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_XOR);
        case OP(SZ_HALF, OP_XOR): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_XOR);
        case OP(SZ_WORD, OP_XOR): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_XOR);
        case OP(SZ_BYTE, OP_OR): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_OR);
        case OP(SZ_HALF, OP_OR): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_OR);
        case OP(SZ_WORD, OP_OR): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_OR);

        case OP(SZ_BYTE, OP_SLA): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_SHIFT_LEFT);
        case OP(SZ_HALF, OP_SLA): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_SHIFT_LEFT);
        case OP(SZ_WORD, OP_SLA): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_SHIFT_LEFT);
        case OP(SZ_BYTE, OP_SRL): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_SHIFT_RIGHT);
        case OP(SZ_HALF, OP_SRL): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_SHIFT_RIGHT);
        case OP(SZ_WORD, OP_SRL): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_SHIFT_RIGHT);
        case OP(SZ_BYTE, OP_SRA): VM_IMPL_AND(SIZE8, int8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_SHIFT_RIGHT);
        case OP(SZ_HALF, OP_SRA): VM_IMPL_AND(SIZE16, int16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_SHIFT_RIGHT);
        case OP(SZ_WORD, OP_SRA): VM_IMPL_AND(SIZE32, int32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_SHIFT_RIGHT);

        case OP(SZ_BYTE, OP_ROL): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, ROTATE_LEFT8);
        case OP(SZ_HALF, OP_ROL): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, ROTATE_LEFT16);
        case OP(SZ_WORD, OP_ROL): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, ROTATE_LEFT32);
        case OP(SZ_BYTE, OP_ROR): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, ROTATE_RIGHT8);
        case OP(SZ_HALF, OP_ROR): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, ROTATE_RIGHT16);
        case OP(SZ_WORD, OP_ROR): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, ROTATE_RIGHT32);

        case OP(SZ_BYTE, OP_BSE): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_BIT_SET);
        case OP(SZ_HALF, OP_BSE): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_BIT_SET);
        case OP(SZ_WORD, OP_BSE): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_BIT_SET);
        case OP(SZ_BYTE, OP_BCL): VM_IMPL_AND(SIZE8, uint8_t, uint8_t, vm_source8, vm_source8_stay, vm_target8, OPER_BIT_CLEAR);
        case OP(SZ_HALF, OP_BCL): VM_IMPL_AND(SIZE16, uint16_t, uint16_t, vm_source16, vm_source16_stay, vm_target16, OPER_BIT_CLEAR);
        case OP(SZ_WORD, OP_BCL): VM_IMPL_AND(SIZE32, uint32_t, uint32_t, vm_source32, vm_source32_stay, vm_target32, OPER_BIT_CLEAR);

        case OP(SZ_BYTE, OP_CMP): VM_IMPL_CMP(SIZE8, uint8_t, vm_source8);
        case OP(SZ_HALF, OP_CMP): VM_IMPL_CMP(SIZE16, uint16_t, vm_source16);
        case OP(SZ_WORD, OP_CMP): VM_IMPL_CMP(SIZE32, uint32_t, vm_source32);

        case OP(SZ_BYTE, OP_BTS): VM_IMPL_BTS(SIZE8, uint8_t, vm_source8);
        case OP(SZ_HALF, OP_BTS): VM_IMPL_BTS(SIZE16, uint16_t, vm_source16);
        case OP(SZ_WORD, OP_BTS): VM_IMPL_BTS(SIZE32, uint32_t, vm_source32);

        default:
            vm_panic(vm, FOX32_ERR_BADOPCODE);
    }

    vm->pointer_instr = vm->pointer_instr_mut;
}

static err_t vm_step(vm_t *vm) {
    if (setjmp(vm->panic_jmp) != 0) {
        return vm->halted = true, vm->panic_err;
    }
    vm_execute(vm);
    return FOX32_ERR_OK;
}
static err_t vm_resume(vm_t *vm) {
    if (setjmp(vm->panic_jmp) != 0) {
        return vm->halted = true, vm->panic_err;
    }
    while (!vm->halted) {
        vm_execute(vm);
    }
    return FOX32_ERR_OK;
}

static bool vm_raise(vm_t *vm, uint16_t vector) {
    vm->halted = true;

    if (vm->interrupt_paused || !vm->interrupt_enabled) {
        return false;
    }

    vm->interrupt_paused = true;

    if (setjmp(vm->panic_jmp) != 0) {
        return false;
    }

    uint32_t pointer_handler = vm_read32(vm, SIZE32 * (uint32_t) vector);

    vm_push32(vm, vm->pointer_instr);
    vm_push8(vm, vm_flags_get(vm));

    vm->pointer_instr = pointer_handler;

    return true;
}

static bool vm_recover(vm_t *vm, err_t err) {
    switch (err) {
        case FOX32_ERR_DEBUGGER:
            return vm_raise(vm, EX_DEBUGGER);
        case FOX32_ERR_FAULT:
            return vm_raise(vm, EX_FAULT);
        case FOX32_ERR_BADOPCODE:
        case FOX32_ERR_BADCONDITION:
        case FOX32_ERR_BADREGISTER:
        case FOX32_ERR_BADIMMEDIATE:
            return vm_raise(vm, EX_ILLEGAL);
        case FOX32_ERR_DIVZERO:
            return vm_raise(vm, EX_DIVZERO);
        case FOX32_ERR_IOREAD:
        case FOX32_ERR_IOWRITE:
            return vm_raise(vm, EX_BUS);
        default:
            return false;
    }
}

const char *fox32_strerr(fox32_err_t err) {
    return err_tostring(err);
}
void fox32_init(fox32_vm_t *vm) {
    vm_init(vm);
}
fox32_err_t fox32_step(fox32_vm_t *vm) {
    return vm_step(vm);
}
fox32_err_t fox32_resume(fox32_vm_t *vm) {
    return vm_resume(vm);
}
bool fox32_raise(fox32_vm_t *vm, uint16_t vector) {
    return vm_raise(vm, vector);
}
bool fox32_recover(fox32_vm_t *vm, fox32_err_t err) {
    return vm_recover(vm, err);
}

static vm_t vm;

static const uint8_t lua_test[] = {
  0x02, 0x97, 0x33, 0x00, 0x00, 0x00, 0x00, 0x02, 0x98, 0x13, 0x00, 0x00,
  0x00, 0x02, 0x88, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x01, 0x02, 0x97,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x9b, 0x00, 0x01, 0x00, 0x91, 0x00,
  0x06, 0x07, 0x00, 0x00, 0x22, 0x88, 0x16, 0x00, 0x00, 0x00, 0x00, 0x9a,
  0x01, 0x00, 0xaa, 0x68, 0x69, 0x20, 0x6c, 0x75, 0x61, 0x20, 0x3a, 0x33,
  0x0d, 0x0a, 0x00
};

int main(int argc, char **argv) {
    vm_init(&vm);

    vm.halted = false;

    vm.pointer_instr = 0;
    vm.pointer_stack = 8192;

    memcpy(vm.memory_ram, lua_test, sizeof(lua_test));

    err_t err = vm_resume(&vm);

    puts(err_tostring(err));

    return 0;
}