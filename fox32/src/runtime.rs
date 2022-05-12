// runtime.rs

pub trait Runtime: Send {
    fn halted_get(&mut self) -> bool;
    fn halted_set(&mut self, halted: bool);

    fn interrupts_enabled_get(&mut self) -> bool;
    fn interrupts_enabled_set(&mut self, interrupts_enabled: bool);

    fn raise(&mut self, vector: u16);

    fn step(&mut self);
}

impl Runtime for crate::Cpu {
    fn halted_get(&mut self) -> bool {
        self.halted
    }
    fn halted_set(&mut self, halted: bool) {
        self.halted = halted
    }

    fn interrupts_enabled_get(&mut self) -> bool {
        self.interrupts_enabled
    }
    fn interrupts_enabled_set(&mut self, interrupts_enabled: bool) {
        self.interrupts_enabled = interrupts_enabled
    }

    fn raise(&mut self, vector: u16) {
        self.interrupt(crate::Interrupt::Request(vector as u8));
    }

    fn step(&mut self) {
        self.execute_memory_instruction();
    }
}

impl Runtime for fox32core::State {
    fn halted_get(&mut self) -> bool {
        *self.halted()
    }
    fn halted_set(&mut self, halted: bool) {
        *self.halted() = halted;
    }

    fn interrupts_enabled_get(&mut self) -> bool {
        *self.interrupts_enabled()
    }
    fn interrupts_enabled_set(&mut self, interrupts_enabled: bool) {
        *self.interrupts_enabled() = interrupts_enabled;
    }

    fn raise(&mut self, vector: u16) {
        match fox32core::State::raise(self, vector) {
            Some(fox32core::Error::InterruptsDisabled) | None => {}
            Some(error) => {
                panic!("fox32core failed to raise interrupt {:#06X}: {}", vector, error);
            }
        }
    }

    fn step(&mut self) {
        if let Some(error) = fox32core::State::resume(self, 8192) {
            panic!("fox32core failed to execute next instruction: {}", error);
        }
    }
}
