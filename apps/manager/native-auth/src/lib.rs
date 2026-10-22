//! Small C ABI for the shared Qt Manager's Ed25519 device identity.
//!
//! Private seed persistence belongs to the platform secure-store adapter in
//! the Qt application. This crate only derives the public key and signs bounded
//! protocol messages; it never writes key material to disk.

use ed25519_dalek::{Signature, Signer, SigningKey, VerifyingKey};
use zeroize::Zeroize;

const SEED_BYTES: usize = 32;
const PUBLIC_KEY_BYTES: usize = 32;
const SIGNATURE_BYTES: usize = 64;
const MAX_MESSAGE_BYTES: usize = 65_536;

/// Derive an Ed25519 public key from a 32-byte seed.
///
/// Returns 0 on success and -1 for invalid pointers or lengths.
///
/// # Safety
/// `seed_ptr` must reference 32 readable bytes and `output_ptr` must reference
/// 32 writable bytes for the duration of this call.
#[no_mangle]
pub unsafe extern "C" fn hs_manager_ed25519_public_key(
    seed_ptr: *const u8,
    seed_len: usize,
    output_ptr: *mut u8,
    output_len: usize,
) -> i32 {
    if seed_ptr.is_null()
        || output_ptr.is_null()
        || seed_len != SEED_BYTES
        || output_len != PUBLIC_KEY_BYTES
    {
        return -1;
    }

    let mut seed = [0_u8; SEED_BYTES];
    // SAFETY: pointers and exact lengths were checked above; the caller owns
    // valid readable/writable buffers for this call.
    unsafe {
        seed.copy_from_slice(std::slice::from_raw_parts(seed_ptr, SEED_BYTES));
    }
    let signing_key = SigningKey::from_bytes(&seed);
    seed.zeroize();
    let mut public_key = signing_key.verifying_key().to_bytes();
    // SAFETY: output buffer length was checked above.
    unsafe { std::ptr::copy_nonoverlapping(public_key.as_ptr(), output_ptr, PUBLIC_KEY_BYTES) };
    public_key.zeroize();
    0
}

/// Sign a bounded protocol message with a 32-byte Ed25519 seed.
///
/// Returns 0 on success and -1 for invalid pointers, lengths, or capacity.
///
/// # Safety
/// `seed_ptr` must reference 32 readable bytes, `message_ptr` must reference
/// `message_len` readable bytes, and `signature_ptr` must reference 64 writable
/// bytes for the duration of this call.
#[no_mangle]
pub unsafe extern "C" fn hs_manager_ed25519_sign(
    seed_ptr: *const u8,
    seed_len: usize,
    message_ptr: *const u8,
    message_len: usize,
    signature_ptr: *mut u8,
    signature_len: usize,
) -> i32 {
    if seed_ptr.is_null()
        || message_ptr.is_null()
        || signature_ptr.is_null()
        || seed_len != SEED_BYTES
        || message_len == 0
        || message_len > MAX_MESSAGE_BYTES
        || signature_len != SIGNATURE_BYTES
    {
        return -1;
    }

    let mut seed = [0_u8; SEED_BYTES];
    // SAFETY: pointers and exact capacities were checked above; caller owns
    // valid buffers for the duration of this call.
    unsafe {
        seed.copy_from_slice(std::slice::from_raw_parts(seed_ptr, SEED_BYTES));
    }
    let message = unsafe { std::slice::from_raw_parts(message_ptr, message_len) };
    let signing_key = SigningKey::from_bytes(&seed);
    seed.zeroize();
    let mut signature = signing_key.sign(message).to_bytes();
    // SAFETY: signature output capacity was checked above.
    unsafe { std::ptr::copy_nonoverlapping(signature.as_ptr(), signature_ptr, SIGNATURE_BYTES) };
    signature.zeroize();
    0
}

/// Verify a detached Ed25519 signature over a bounded protocol message.
///
/// Returns 0 for a valid signature, 1 for a well-formed but invalid signature
/// or public key, and -1 for invalid pointers or lengths.
///
/// # Safety
/// `public_key_ptr` must reference 32 readable bytes, `message_ptr` must
/// reference `message_len` readable bytes, and `signature_ptr` must reference
/// 64 readable bytes for the duration of this call.
#[no_mangle]
pub unsafe extern "C" fn hs_manager_ed25519_verify(
    public_key_ptr: *const u8,
    public_key_len: usize,
    message_ptr: *const u8,
    message_len: usize,
    signature_ptr: *const u8,
    signature_len: usize,
) -> i32 {
    if public_key_ptr.is_null()
        || message_ptr.is_null()
        || signature_ptr.is_null()
        || public_key_len != PUBLIC_KEY_BYTES
        || message_len == 0
        || message_len > MAX_MESSAGE_BYTES
        || signature_len != SIGNATURE_BYTES
    {
        return -1;
    }

    let mut public_key_bytes = [0_u8; PUBLIC_KEY_BYTES];
    let mut signature_bytes = [0_u8; SIGNATURE_BYTES];
    // SAFETY: pointers and exact lengths were checked above; the caller owns
    // readable buffers for the duration of this call.
    unsafe {
        public_key_bytes
            .copy_from_slice(std::slice::from_raw_parts(public_key_ptr, PUBLIC_KEY_BYTES));
        signature_bytes.copy_from_slice(std::slice::from_raw_parts(signature_ptr, SIGNATURE_BYTES));
    }
    let message = unsafe { std::slice::from_raw_parts(message_ptr, message_len) };
    let signature = Signature::from_bytes(&signature_bytes);
    signature_bytes.zeroize();
    let verifying_key = match VerifyingKey::from_bytes(&public_key_bytes) {
        Ok(key) => key,
        Err(_) => return 1,
    };
    if verifying_key.verify_strict(message, &signature).is_ok() {
        0
    } else {
        1
    }
}
