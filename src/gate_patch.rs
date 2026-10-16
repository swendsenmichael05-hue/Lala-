//! v0.2.7 boundary handling around the retained v0.2.6 policy.
//! No game-state writes or pulse-timing changes.
pub use crate::action_gate::{Decision, Facts};
use crate::action_gate::Gate as LegacyGate;

pub const RIGHT: u64 = 1 << 14;
const VERTICAL_DPAD: u64 = (1 << 13) | (1 << 15);

/// Right plus Up/Down is one macro request only in the enabled, fresh scope.
/// Left, face buttons, triggers and standalone Up/Down keep their meanings.
pub fn trigger_buttons(raw: u64, enabled_scope: bool) -> u64 {
    if enabled_scope && raw & RIGHT != 0 { raw & !VERTICAL_DPAD } else { raw }
}

#[derive(Default)]
pub struct Gate { inner: LegacyGate, actor: u64, nair_chain: bool }
impl Gate {
    pub fn reset_attempt(&mut self) { self.inner.reset_attempt(); }
    pub fn observe(&mut self, mut f: Facts, allow_recovery_cancel: bool) -> Decision {
        if !f.training || f.actor == 0 || self.actor != f.actor {
            self.nair_chain = false;
            self.actor = f.actor;
        }
        if f.nair_air || f.nair_landing { self.nair_chain = true; }
        else if !(self.nair_chain && (f.normal_fall || f.normal_landing)) { self.nair_chain = false; }
        // Situation and status/motion can be sampled on opposite sides of a
        // touchdown update. Retain an identified nair; never inject in this
        // transitional observation, even if an old cancel flag is present.
        if f.grounded && (f.nair_air || (self.nair_chain && f.normal_fall)) {
            f.nair_air = false;
            f.nair_landing = true;
            f.native_cancel = false;
        } else if !f.grounded && f.nair_landing {
            f.nair_landing = false;
            f.nair_air = true;
            f.native_cancel = false;
        }
        let mut d = self.inner.observe(f);
        // Only the caller's EWGF/WGF whitelist may use this branch. It reads
        // existing native permission; it does not enable cancel or skip lag.
        if allow_recovery_cancel && f.training && f.actor != 0 && f.grounded &&
           !f.stopped && f.electric_recovery && f.native_cancel && d.queueable {
            d.actionable = true;
            d.can_continue = true;
        }
        if f.idle { self.inner.reset_attempt(); }
        d
    }
}

/// A repeat started during EWGF recovery must not confirm the OLD electric.
#[derive(Default)]
pub struct ElectricEntry {
    active: bool, left_old: bool, last_frame: f32, confirmed: bool,
}
impl ElectricEntry {
    pub fn start(&mut self, was_electric: bool, frame: f32) {
        *self = Self { active:true, left_old:!was_electric, last_frame:frame, confirmed:false };
    }
    pub fn clear(&mut self) { *self = Self::default(); }
    pub fn observe(&mut self, electric: bool, frame: f32, attack_sent: bool) -> bool {
        if !self.active { return false; }
        if !electric { self.left_old = true; }
        let reentered = self.left_old || frame + 0.5 < self.last_frame;
        if electric && attack_sent && reentered { self.confirmed = true; }
        self.last_frame = frame;
        electric && self.confirmed
    }
}
