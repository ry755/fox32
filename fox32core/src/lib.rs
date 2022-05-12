#![no_std]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

extern crate alloc;

use core::mem;
use core::ptr;
use core::slice;
use core::str;
use core::fmt;
use alloc::boxed::Box;

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Error {
    Internal = 1,
    Debugger,
    Fault,
    BadOpcode,
    BadCondition,
    BadRegister,
    BadImmediate,
    DivideByZero,
    IoReadFailed,
    IoWriteFailed,
    InterruptsDisabled
}

impl Error {
    fn from_code(code: fox32_err_t) -> Option<Self> {
        if code > fox32_err_t_FOX32_ERR_OK && code <= fox32_err_t_FOX32_ERR_CANTRECOVER {
            Some(unsafe { mem::transmute(code as u32) })
        } else {
            None
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        unsafe {
            let msg_ptr = fox32_strerr(*self as fox32_err_t);
            let msg_slice = slice::from_raw_parts(msg_ptr.cast::<u8>(), libc::strlen(msg_ptr));
            let msg_string = str::from_utf8_unchecked(msg_slice);
            f.write_str(msg_string)
        }
    }
}

pub trait Bus {
    fn io_read(&mut self, port: u32) -> Option<u32>;
    fn io_write(&mut self, port: u32, value: u32) -> Option<()>;
}

pub struct State {
    vm: *mut fox32_vm_t,
    bus: *mut Box<dyn Bus>
}

unsafe extern "C" fn io_read_trampoline(user: *mut libc::c_void, value: *mut u32, port: u32) -> libc::c_int {
    match (*user.cast::<Box<dyn Bus>>()).io_read(port) {
        Some(v) => { *value = v; 0 }
        None => { 1 }
    }
}

unsafe extern "C" fn io_write_trampoline(user: *mut libc::c_void, value: u32, port: u32) -> libc::c_int {
    if (*user.cast::<Box<dyn Bus>>()).io_write(port, value).is_some() { 0 } else { 1 }
}

const LAYOUT_VM: alloc::alloc::Layout = alloc::alloc::Layout::new::<fox32_vm_t>();
const LAYOUT_BUS: alloc::alloc::Layout = alloc::alloc::Layout::new::<Box<dyn Bus>>();

impl State {
    unsafe fn alloc_create(&mut self) {
        let ptr_vm = alloc::alloc::alloc(LAYOUT_VM);
        if ptr_vm.is_null() { alloc::alloc::handle_alloc_error(LAYOUT_VM); }

        let ptr_bus = alloc::alloc::alloc(LAYOUT_BUS);
        if ptr_bus.is_null() { alloc::alloc::handle_alloc_error(LAYOUT_BUS); }

        self.vm = ptr_vm.cast();
        self.bus = ptr_bus.cast();
    }
    unsafe fn alloc_destroy(&mut self) {
        alloc::alloc::dealloc(self.vm.cast(), LAYOUT_VM);
        alloc::alloc::dealloc(self.bus.cast(), LAYOUT_BUS);
    }

    fn init(bus: Box<dyn Bus>) -> Self {
        let mut this = Self { vm: ptr::null_mut(), bus: ptr::null_mut() };
        unsafe {
            this.alloc_create();
            fox32_init(this.vm);
            ptr::write(this.bus, bus);
            (*this.vm).io_user = this.bus.cast();
            (*this.vm).io_read = Some(io_read_trampoline);
            (*this.vm).io_write = Some(io_write_trampoline);
        }
        this
    }
    pub fn new(bus: impl Bus + 'static) -> Self {
        Self::init(Box::new(bus))
    }

    pub fn pointer_instr(&self) -> &mut u32 {
        unsafe { &mut (*self.vm).pointer_instr }
    }
    pub fn pointer_stack(&self) -> &mut u32 {
        unsafe { &mut (*self.vm).pointer_stack }
    }
    pub fn registers(&self) -> &mut [u32; FOX32_REGISTER_COUNT as usize] {
        unsafe { &mut (*self.vm).registers }
    }
    pub fn flag_zero(&self) -> &mut bool {
        unsafe { &mut (*self.vm).flag_zero }
    }
    pub fn flag_carry(&self) -> &mut bool {
        unsafe { &mut (*self.vm).flag_carry }
    }
    pub fn halted(&self) -> &mut bool {
        unsafe { &mut (*self.vm).halted }
    }
    pub fn interrupts_enabled(&self) -> &mut bool {
        unsafe { &mut (*self.vm).interrupts_enabled }
    }
    pub fn interrupts_paused(&self) -> &mut bool {
        unsafe { &mut (*self.vm).interrupts_paused }
    }
    pub fn memory_ram(&self) -> &mut [u8; FOX32_MEMORY_RAM as usize] {
        unsafe { &mut (*self.vm).memory_ram }
    }
    pub fn memory_rom(&self) -> &mut [u8; FOX32_MEMORY_ROM as usize] {
        unsafe { &mut (*self.vm).memory_rom }
    }

    pub fn step(&self) -> Option<Error> {
        Error::from_code(unsafe { fox32_step(self.vm) })
    }
    pub fn resume(&self, count: u32) -> Option<Error> {
        Error::from_code(unsafe { fox32_resume(self.vm, count) })
    }
    pub fn raise(&self, vector: u16) -> Option<Error> {
        Error::from_code(unsafe { fox32_raise(self.vm, vector) })
    }
    pub fn recover(&self, error: Error) -> Option<Error> {
        Error::from_code(unsafe { fox32_recover(self.vm, error as fox32_err_t) })
    }

    pub fn push_byte(&self, value: u8) -> Result<(), Error> {
        Error::from_code(unsafe { fox32_push_byte(self.vm, value) }).map_or(Ok(()), Err)
    }
    pub fn push_half(&self, value: u16) -> Result<(), Error> {
        Error::from_code(unsafe { fox32_push_half(self.vm, value) }).map_or(Ok(()), Err)
    }
    pub fn push_word(&self, value: u32) -> Result<(), Error> {
        Error::from_code(unsafe { fox32_push_word(self.vm, value) }).map_or(Ok(()), Err)
    }
    pub fn pop_byte(&self) -> Result<u8, Error> {
        let mut value = 0u8;
        Error::from_code(unsafe { fox32_pop_byte(self.vm, &mut value) }).map_or(Ok(value), Err)
    }
    pub fn pop_half(&self) -> Result<u16, Error> {
        let mut value = 0u16;
        Error::from_code(unsafe { fox32_pop_half(self.vm, &mut value) }).map_or(Ok(value), Err)
    }
    pub fn pop_word(&self) -> Result<u32, Error> {
        let mut value = 0u32;
        Error::from_code(unsafe { fox32_pop_word(self.vm, &mut value) }).map_or(Ok(value), Err)
    }
}

impl Drop for State {
    fn drop(&mut self) {
        unsafe { self.alloc_destroy() }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::cell::RefCell;
    use alloc::{vec::Vec, rc::Rc};

    static HEWWO_PROGRAM: &'static [u8] = &[
        0x02, 0x97, 0x33, 0x00, 0x00, 0x00, 0x00, 0x02, 0x98, 0x13, 0x00, 0x00,
        0x00, 0x02, 0x88, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x01, 0x02, 0x97,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x9b, 0x00, 0x01, 0x00, 0x91, 0x00,
        0x06, 0x07, 0x00, 0x00, 0x22, 0x88, 0x16, 0x00, 0x00, 0x00, 0x00, 0x9a,
        0x01, 0x00, 0xaa, 0x68, 0x69, 0x20, 0x6c, 0x75, 0x61, 0x20, 0x3a, 0x33,
        0x0d, 0x0a, 0x00
    ];

    #[derive(Clone, Default)]
    struct HewwoBus { vec: Rc<RefCell<Vec<u8>>> }

    impl Bus for HewwoBus {
        fn io_read(&mut self, _port: u32) -> Option<u32> { Some(0) }

        fn io_write(&mut self, port: u32, value: u32) -> Option<()> {
            if port == 0 {
                self.vec.borrow_mut().push(value as u8);
            }
            Some(())
        }
    }

    #[test]
    fn hewwo() {
        let bus = HewwoBus::default();
        let state = State::new(bus.clone());

        *state.pointer_instr() = 0;
        *state.pointer_stack() = 8192;
        *state.halted() = false;

        (&mut state.memory_ram()[..HEWWO_PROGRAM.len()]).copy_from_slice(HEWWO_PROGRAM);

        let error = state.resume(1000);

        assert!(error.is_none());

        let actual: &Vec<u8> = &bus.vec.borrow();
        let expected: &Vec<u8> = &Vec::from("hi lua :3\r\n");
        assert_eq!(actual, expected);
    }
}
