//! Pure request policy; reads facts supplied by Smash, never changes game state.
//! Keeping this separate makes aerial/landing and recovery decisions host-testable.
#[derive(Clone, Copy, Default)]
pub struct Facts {
    pub actor: u64,
    pub training: bool,
    pub grounded: bool,
    pub stopped: bool,
    pub idle: bool,
    pub movement: bool,
    pub direct_movement: bool,
    pub electric_recovery: bool,
    pub ewgf: bool,
    pub attack_sent: bool,
    pub nair_air: bool,
    pub nair_landing: bool,
    pub normal_fall: bool,
    pub normal_landing: bool,
    pub native_cancel: bool,
    pub fast_movement: bool,
}
#[derive(Clone, Copy, Default, Debug)]
pub struct Decision {
    pub permitted: bool,
    pub queueable: bool,
    pub can_continue: bool,
    pub actionable: bool,
    pub movement_ready: bool,
    pub brake: bool,
    pub nair_buffer: bool,
    pub landing_cancel_ready: bool,
    pub recovered_to_movement: bool,
}
#[derive(Default)]
pub struct Gate {
    actor: u64,
    nair_chain: bool,
    electric_seen: bool,
}
impl Gate {
    pub fn reset_attempt(&mut self) { self.electric_seen = false; }
    pub fn observe(&mut self, f: Facts) -> Decision {
        if !f.training || f.actor == 0 {
            *self = Self::default();
            return Decision::default();
        }
        if self.actor != f.actor {
            *self = Self { actor: f.actor, ..Self::default() };
        }
        // Follow only this nair into normal fall/autocancel landing. Any other
        // aerial, hitstun, air dodge, ledge state or special breaks the chain.
        if f.nair_air || f.nair_landing {
            self.nair_chain = true;
        } else if !(self.nair_chain && (f.normal_fall || f.normal_landing)) {
            self.nair_chain = false;
        }
        let air_queue = !f.grounded && self.nair_chain && (f.nair_air || f.normal_fall);
        let landing_queue = f.grounded && self.nair_chain &&
            (f.nair_landing || f.normal_landing);
        let queueable = f.idle || f.movement || f.electric_recovery || air_queue || landing_queue;
        let permitted = f.grounded || air_queue;
        if !permitted || !queueable { self.electric_seen = false; }
        // A forward input enters DASH before EWGF: that is NOT proof of attack
        // recovery. Require the real EWGF status after an attack pulse.
        if f.attack_sent && f.ewgf && f.grounded { self.electric_seen = true; }
        let moving_ready = f.fast_movement && f.grounded && !f.stopped &&
            f.movement && f.direct_movement;
        let recovered = self.electric_seen && moving_ready && !f.electric_recovery;
        // Native cancel permission is read, never enabled by this plugin.
        // No guessed motion frame or elapsed-time threshold grants permission.
        let landing_ready = landing_queue && !f.stopped && f.native_cancel;
        let actionable = f.grounded && !f.stopped && (f.idle || landing_ready || recovered);
        Decision {
            permitted,
            queueable,
            can_continue: f.grounded && !f.stopped && queueable &&
                (!landing_queue || landing_ready),
            actionable,
            movement_ready: moving_ready,
            brake: f.grounded && f.movement,
            nair_buffer: air_queue || landing_queue,
            landing_cancel_ready: landing_ready,
            recovered_to_movement: recovered,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn idle() -> Facts { Facts { actor: 7, training:true, grounded:true, idle:true, fast_movement:true, ..Facts::default() } }
    fn air() -> Facts { Facts { grounded:false, idle:false, nair_air:true, ..idle() } }
    fn land() -> Facts { Facts { idle:false, nair_landing:true, ..idle() } }
    fn dash() -> Facts { Facts { idle:false, movement:true, direct_movement:true, ..idle() } }
    fn electric() -> Facts { Facts { idle:false, electric_recovery:true, ewgf:true, attack_sent:true, ..idle() } }
    #[test] fn idle_unchanged() { let d=Gate::default().observe(idle()); assert!(d.actionable&&d.can_continue&&d.permitted); }
    #[test] fn airborne_nair_queues_without_injection() { let d=Gate::default().observe(air()); assert!(d.permitted&&d.queueable&&d.nair_buffer); assert!(!d.can_continue&&!d.actionable&&!d.brake&&!d.movement_ready); }
    #[test] fn landing_nair_is_not_rejected() { let d=Gate::default().observe(land()); assert!(d.queueable&&d.permitted&&d.nair_buffer); assert!(!d.actionable&&!d.can_continue); }
    #[test] fn native_landing_cancel_can_start_before_wait() { let d=Gate::default().observe(Facts {native_cancel:true,..land()}); assert!(d.landing_cancel_ready&&d.can_continue&&d.actionable); }
    #[test] fn hitlag_overrides_native_cancel() { let d=Gate::default().observe(Facts {native_cancel:true,stopped:true,..land()}); assert!(d.queueable); assert!(!d.landing_cancel_ready&&!d.can_continue&&!d.actionable); }
    #[test] fn native_cancel_never_starts_while_airborne() { let d=Gate::default().observe(Facts {native_cancel:true,..air()}); assert!(!d.can_continue&&!d.actionable); }
    #[test] fn nair_fall_landing_idle_chain() { let mut g=Gate::default(); g.observe(air()); let d=g.observe(Facts {nair_air:false,normal_fall:true,..air()}); assert!(d.queueable&&!d.actionable); let d=g.observe(Facts {idle:false,normal_landing:true,..idle()}); assert!(d.nair_buffer&&!d.actionable); assert!(g.observe(idle()).actionable); }
    #[test] fn ordinary_fall_without_nair_stays_out_of_scope() { let d=Gate::default().observe(Facts {nair_air:false,normal_fall:true,..air()}); assert!(!d.permitted&&!d.queueable); }
    #[test] fn ordinary_landing_without_nair_remains_blocked() { let d=Gate::default().observe(Facts {idle:false,normal_landing:true,native_cancel:true,..idle()}); assert!(!d.queueable&&!d.actionable); }
    #[test] fn another_aerial_breaks_nair_chain() { let mut g=Gate::default(); g.observe(air()); g.observe(Facts {nair_air:false,..air()}); let d=g.observe(Facts {nair_air:false,normal_fall:true,..air()}); assert!(!d.queueable&&!d.permitted); }
    #[test] fn damage_interrupts_nair_chain() { let mut g=Gate::default(); g.observe(air()); g.observe(Facts {idle:false,..idle()}); assert!(!g.observe(Facts {idle:false,normal_landing:true,..idle()}).queueable); }
    #[test] fn new_actor_cannot_inherit_air_queue() { let mut g=Gate::default(); g.observe(air()); assert!(!g.observe(Facts {actor:8,nair_air:false,normal_fall:true,..air()}).queueable); }
    #[test] fn exit_training_clears_air_queue() { let mut g=Gate::default(); g.observe(air()); g.observe(Facts {training:false,..air()}); assert!(!g.observe(Facts {nair_air:false,normal_fall:true,..air()}).queueable); }
    #[test] fn initial_dash_is_not_completed_recovery() { let d=Gate::default().observe(dash()); assert!(d.movement_ready&&!d.recovered_to_movement&&!d.actionable); }
    #[test] fn command_forward_dash_does_not_unlock_recovery() { let d=Gate::default().observe(Facts {attack_sent:true,..dash()}); assert!(!d.recovered_to_movement&&!d.actionable); }
    #[test] fn actual_ewgf_exit_to_dash_unlocks() { let mut g=Gate::default(); assert!(!g.observe(electric()).actionable); let d=g.observe(dash()); assert!(d.actionable&&d.recovered_to_movement); }
    #[test] fn actual_ewgf_without_attack_pulse_does_not_unlock() { let mut g=Gate::default(); g.observe(Facts {attack_sent:false,..electric()}); assert!(!g.observe(dash()).recovered_to_movement); }
    #[test] fn turning_still_waits() { let mut g=Gate::default(); g.observe(electric()); let d=g.observe(Facts {direct_movement:false,..dash()}); assert!(d.queueable&&d.brake&&!d.actionable&&!d.recovered_to_movement); }
    #[test] fn fast_movement_false_retains_legacy_recovery_gate() { let mut g=Gate::default(); g.observe(electric()); assert!(!g.observe(Facts {fast_movement:false,..dash()}).actionable); }
    #[test] fn recovery_never_unlocked_during_the_electric() { let mut g=Gate::default(); for _ in 0..100 {let d=g.observe(electric()); assert!(!d.actionable&&!d.recovered_to_movement);} }
    #[test] fn reset_attempt_requires_new_ewgf_observation() { let mut g=Gate::default(); g.observe(electric()); g.reset_attempt(); assert!(!g.observe(dash()).recovered_to_movement); }
    #[test] fn different_actor_cannot_inherit_recovery() { let mut g=Gate::default(); g.observe(electric()); assert!(!g.observe(Facts {actor:8,..dash()}).recovered_to_movement); }
    #[test] fn hitlag_never_unlocks_movement() { let mut g=Gate::default(); g.observe(electric()); assert!(!g.observe(Facts {stopped:true,..dash()}).actionable); }
    #[test] fn unsupported_attack_never_becomes_cancelable() { let d=Gate::default().observe(Facts {idle:false,native_cancel:true,..idle()}); assert!(!d.queueable&&!d.actionable&&!d.can_continue); }
    #[test] fn null_actor_fails_closed() { let d=Gate::default().observe(Facts {actor:0,..idle()}); assert!(!d.permitted&&!d.can_continue); }
    #[test] fn malformed_air_movement_cannot_inject() { let d=Gate::default().observe(Facts {grounded:false,..dash()}); assert!(!d.permitted&&!d.can_continue&&!d.actionable&&!d.movement_ready&&!d.brake); }
}
