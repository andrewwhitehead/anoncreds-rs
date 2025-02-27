#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>


#define MAX_ATTRIBUTES_COUNT 125

enum ErrorCode
#ifdef __cplusplus
  : size_t
#endif // __cplusplus
 {
  Success = 0,
  Input = 1,
  IOError = 2,
  InvalidState = 3,
  Unexpected = 4,
  CredentialRevoked = 5,
  InvalidUserRevocId = 6,
  ProofRejected = 7,
  RevocationRegistryFull = 8,
};
#ifndef __cplusplus
typedef size_t ErrorCode;
#endif // __cplusplus

/**
 * ByteBuffer is a struct that represents an array of bytes to be sent over the FFI boundaries.
 * There are several cases when you might want to use this, but the primary one for us
 * is for returning protobuf-encoded data to Swift and Java. The type is currently rather
 * limited (implementing almost no functionality), however in the future it may be
 * more expanded.
 *
 * ## Caveats
 *
 * Note that the order of the fields is `len` (an i64) then `data` (a `*mut u8`), getting
 * this wrong on the other side of the FFI will cause memory corruption and crashes.
 * `i64` is used for the length instead of `u64` and `usize` because JNA has interop
 * issues with both these types.
 *
 * ### `Drop` is not implemented
 *
 * ByteBuffer does not implement Drop. This is intentional. Memory passed into it will
 * be leaked if it is not explicitly destroyed by calling [`ByteBuffer::destroy`], or
 * [`ByteBuffer::destroy_into_vec`]. This is for two reasons:
 *
 * 1. In the future, we may allow it to be used for data that is not managed by
 *    the Rust allocator\*, and `ByteBuffer` assuming it's okay to automatically
 *    deallocate this data with the Rust allocator.
 *
 * 2. Automatically running destructors in unsafe code is a
 *    [frequent footgun](https://without.boats/blog/two-memory-bugs-from-ringbahn/)
 *    (among many similar issues across many crates).
 *
 * Note that calling `destroy` manually is often not needed, as usually you should
 * be passing these to the function defined by [`define_bytebuffer_destructor!`] from
 * the other side of the FFI.
 *
 * Because this type is essentially *only* useful in unsafe or FFI code (and because
 * the most common usage pattern does not require manually managing the memory), it
 * does not implement `Drop`.
 *
 * \* Note: in the case of multiple Rust shared libraries loaded at the same time,
 * there may be multiple instances of "the Rust allocator" (one per shared library),
 * in which case we're referring to whichever instance is active for the code using
 * the `ByteBuffer`. Note that this doesn't occur on all platforms or build
 * configurations, but treating allocators in different shared libraries as fully
 * independent is always safe.
 *
 * ## Layout/fields
 *
 * This struct's field are not `pub` (mostly so that we can soundly implement `Send`, but also so
 * that we can verify rust users are constructing them appropriately), the fields, their types, and
 * their order are *very much* a part of the public API of this type. Consumers on the other side
 * of the FFI will need to know its layout.
 *
 * If this were a C struct, it would look like
 *
 * ```c,no_run
 * struct ByteBuffer {
 *     // Note: This should never be negative, but values above
 *     // INT64_MAX / i64::MAX are not allowed.
 *     int64_t len;
 *     // Note: nullable!
 *     uint8_t *data;
 * };
 * ```
 *
 * In rust, there are two fields, in this order: `len: i64`, and `data: *mut u8`.
 *
 * For clarity, the fact that the data pointer is nullable means that `Option<ByteBuffer>` is not
 * the same size as ByteBuffer, and additionally is not FFI-safe (the latter point is not
 * currently guaranteed anyway as of the time of writing this comment).
 *
 * ### Description of fields
 *
 * `data` is a pointer to an array of `len` bytes. Note that data can be a null pointer and therefore
 * should be checked.
 *
 * The bytes array is allocated on the heap and must be freed on it as well. Critically, if there
 * are multiple rust shared libraries using being used in the same application, it *must be freed
 * on the same heap that allocated it*, or you will corrupt both heaps.
 *
 * Typically, this object is managed on the other side of the FFI (on the "FFI consumer"), which
 * means you must expose a function to release the resources of `data` which can be done easily
 * using the [`define_bytebuffer_destructor!`] macro provided by this crate.
 */
typedef struct ByteBuffer {
  int64_t len;
  uint8_t *data;
} ByteBuffer;

/**
 * `FfiStr<'a>` is a safe (`#[repr(transparent)]`) wrapper around a
 * nul-terminated `*const c_char` (e.g. a C string). Conceptually, it is
 * similar to [`std::ffi::CStr`], except that it may be used in the signatures
 * of extern "C" functions.
 *
 * Functions accepting strings should use this instead of accepting a C string
 * directly. This allows us to write those functions using safe code without
 * allowing safe Rust to cause memory unsafety.
 *
 * A single function for constructing these from Rust ([`FfiStr::from_raw`])
 * has been provided. Most of the time, this should not be necessary, and users
 * should accept `FfiStr` in the parameter list directly.
 *
 * ## Caveats
 *
 * An effort has been made to make this struct hard to misuse, however it is
 * still possible, if the `'static` lifetime is manually specified in the
 * struct. E.g.
 *
 * ```rust,no_run
 * # use ffi_support::FfiStr;
 * // NEVER DO THIS
 * #[no_mangle]
 * extern "C" fn never_do_this(s: FfiStr<'static>) {
 *     // save `s` somewhere, and access it after this
 *     // function returns.
 * }
 * ```
 *
 * Instead, one of the following patterns should be used:
 *
 * ```
 * # use ffi_support::FfiStr;
 * #[no_mangle]
 * extern "C" fn valid_use_1(s: FfiStr<'_>) {
 *     // Use of `s` after this function returns is impossible
 * }
 * // Alternative:
 * #[no_mangle]
 * extern "C" fn valid_use_2(s: FfiStr) {
 *     // Use of `s` after this function returns is impossible
 * }
 * ```
 */
typedef const char *FfiStr;

typedef struct FfiList_FfiStr {
  size_t count;
  const FfiStr *data;
} FfiList_FfiStr;

typedef struct FfiList_FfiStr FfiStrList;

typedef struct FfiList_i64 {
  size_t count;
  const int64_t *data;
} FfiList_i64;

typedef struct FfiCredRevInfo {
  ObjectHandle reg_def;
  ObjectHandle reg_def_private;
  ObjectHandle registry;
  int64_t reg_idx;
  struct FfiList_i64 reg_used;
  FfiStr tails_path;
} FfiCredRevInfo;

typedef struct FfiCredentialEntry {
  ObjectHandle credential;
  int64_t timestamp;
  ObjectHandle rev_state;
} FfiCredentialEntry;

typedef struct FfiList_FfiCredentialEntry {
  size_t count;
  const struct FfiCredentialEntry *data;
} FfiList_FfiCredentialEntry;

typedef struct FfiCredentialProve {
  int64_t entry_idx;
  FfiStr referent;
  int8_t is_predicate;
  int8_t reveal;
} FfiCredentialProve;

typedef struct FfiList_FfiCredentialProve {
  size_t count;
  const struct FfiCredentialProve *data;
} FfiList_FfiCredentialProve;

typedef struct FfiList_ObjectHandle {
  size_t count;
  const ObjectHandle *data;
} FfiList_ObjectHandle;

typedef struct FfiRevocationEntry {
  int64_t def_entry_idx;
  ObjectHandle entry;
  int64_t timestamp;
} FfiRevocationEntry;

typedef struct FfiList_FfiRevocationEntry {
  size_t count;
  const struct FfiRevocationEntry *data;
} FfiList_FfiRevocationEntry;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void anoncreds_buffer_free(struct ByteBuffer buffer);

ErrorCode anoncreds_create_credential(ObjectHandle cred_def,
                                      ObjectHandle cred_def_private,
                                      ObjectHandle cred_offer,
                                      ObjectHandle cred_request,
                                      FfiStrList attr_names,
                                      FfiStrList attr_raw_values,
                                      FfiStrList attr_enc_values,
                                      FfiStr rev_reg_id,
                                      const struct FfiCredRevInfo *revocation,
                                      ObjectHandle *cred_p,
                                      ObjectHandle *rev_reg_p,
                                      ObjectHandle *rev_delta_p);

ErrorCode anoncreds_create_credential_definition(FfiStr schema_id,
                                                 ObjectHandle schema,
                                                 FfiStr tag,
                                                 FfiStr issuer_id,
                                                 FfiStr signature_type,
                                                 int8_t support_revocation,
                                                 ObjectHandle *cred_def_p,
                                                 ObjectHandle *cred_def_pvt_p,
                                                 ObjectHandle *key_proof_p);

ErrorCode anoncreds_create_credential_offer(FfiStr schema_id,
                                            FfiStr cred_def_id,
                                            ObjectHandle key_proof,
                                            ObjectHandle *cred_offer_p);

ErrorCode anoncreds_create_credential_request(FfiStr prover_did,
                                              ObjectHandle cred_def,
                                              ObjectHandle master_secret,
                                              FfiStr master_secret_id,
                                              ObjectHandle cred_offer,
                                              ObjectHandle *cred_req_p,
                                              ObjectHandle *cred_req_meta_p);

ErrorCode anoncreds_create_master_secret(ObjectHandle *master_secret_p);

ErrorCode anoncreds_create_or_update_revocation_state(ObjectHandle rev_reg_def,
                                                      ObjectHandle rev_reg_list,
                                                      int64_t rev_reg_index,
                                                      FfiStr tails_path,
                                                      ObjectHandle rev_state,
                                                      ObjectHandle old_rev_reg_list,
                                                      ObjectHandle *rev_state_p);

ErrorCode anoncreds_create_presentation(ObjectHandle pres_req,
                                        struct FfiList_FfiCredentialEntry credentials,
                                        struct FfiList_FfiCredentialProve credentials_prove,
                                        FfiStrList self_attest_names,
                                        FfiStrList self_attest_values,
                                        ObjectHandle master_secret,
                                        struct FfiList_ObjectHandle schemas,
                                        FfiStrList schema_ids,
                                        struct FfiList_ObjectHandle cred_defs,
                                        FfiStrList cred_def_ids,
                                        ObjectHandle *presentation_p);

ErrorCode anoncreds_create_revocation_registry(ObjectHandle cred_def,
                                               FfiStr cred_def_id,
                                               FfiStr tag,
                                               FfiStr rev_reg_type,
                                               FfiStr issuance_type,
                                               int64_t max_cred_num,
                                               FfiStr tails_dir_path,
                                               ObjectHandle *reg_def_p,
                                               ObjectHandle *reg_def_private_p,
                                               ObjectHandle *reg_entry_p,
                                               ObjectHandle *reg_init_delta_p);

ErrorCode anoncreds_create_schema(FfiStr schema_name,
                                  FfiStr schema_version,
                                  FfiStr issuer_id,
                                  FfiStrList attr_names,
                                  ObjectHandle *result_p);

ErrorCode anoncreds_credential_get_attribute(ObjectHandle handle,
                                             FfiStr name,
                                             const char **result_p);

ErrorCode anoncreds_encode_credential_attributes(FfiStrList attr_raw_values, const char **result_p);

ErrorCode anoncreds_generate_nonce(const char **nonce_p);

ErrorCode anoncreds_get_current_error(const char **error_json_p);

ErrorCode anoncreds_merge_revocation_registry_deltas(ObjectHandle rev_reg_delta_1,
                                                     ObjectHandle rev_reg_delta_2,
                                                     ObjectHandle *rev_reg_delta_p);

void anoncreds_object_free(ObjectHandle handle);

ErrorCode anoncreds_object_get_json(ObjectHandle handle, struct ByteBuffer *result_p);

ErrorCode anoncreds_object_get_type_name(ObjectHandle handle, const char **result_p);

ErrorCode anoncreds_process_credential(ObjectHandle cred,
                                       ObjectHandle cred_req_metadata,
                                       ObjectHandle master_secret,
                                       ObjectHandle cred_def,
                                       ObjectHandle rev_reg_def,
                                       ObjectHandle *cred_p);

ErrorCode anoncreds_revocation_registry_definition_get_attribute(ObjectHandle handle,
                                                                 FfiStr name,
                                                                 const char **result_p);

ErrorCode anoncreds_revoke_credential(ObjectHandle rev_reg_def,
                                      ObjectHandle rev_reg,
                                      int64_t cred_rev_idx,
                                      FfiStr tails_path,
                                      ObjectHandle *rev_reg_p,
                                      ObjectHandle *rev_reg_delta_p);

ErrorCode anoncreds_set_default_logger(void);

ErrorCode anoncreds_update_revocation_registry(ObjectHandle rev_reg_def,
                                               ObjectHandle rev_reg,
                                               struct FfiList_i64 issued,
                                               struct FfiList_i64 revoked,
                                               FfiStr tails_path,
                                               ObjectHandle *rev_reg_p,
                                               ObjectHandle *rev_reg_delta_p);

ErrorCode anoncreds_verify_presentation(ObjectHandle presentation,
                                        ObjectHandle pres_req,
                                        struct FfiList_ObjectHandle schemas,
                                        FfiStrList schema_ids,
                                        struct FfiList_ObjectHandle cred_defs,
                                        FfiStrList cred_def_ids,
                                        struct FfiList_ObjectHandle rev_reg_defs,
                                        FfiStrList rev_reg_def_ids,
                                        struct FfiList_FfiRevocationEntry rev_reg_entries,
                                        int8_t *result_p);

char *anoncreds_version(void);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
