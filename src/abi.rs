//! The checked documentation uses a const uint32 reference for Npad IDs.
//! Do not change this to a by-value integer.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct PadPrefix {
    pub sampling_number:i64,
    pub buttons:u64,
    pub lx:i32, pub ly:i32, pub rx:i32, pub ry:i32,
    pub attributes:u32,
    pub reserved:u32,
}
#[repr(C)]
pub struct DisplayVersion { pub name:[u8;16] }
extern "C" {
    #[link_name="\u{1}_ZN2nn3hid12GetNpadStateEPNS0_11NpadGcStateERKj"]
    pub fn get_gc_state(out:*mut PadPrefix, id:*const u32);
    #[link_name="\u{1}_ZN2nn2oe17GetDisplayVersionEPNS0_14DisplayVersionE"]
    pub fn get_display_version(out:*mut DisplayVersion);
}
