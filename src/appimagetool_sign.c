#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gio/gio.h>
#include <gcrypt.h>
#include <gpgme.h>

#include "binreloc.h"
#include "appimage/appimage_shared.h"

// used to identify calls to the callback
static const char gpgme_hook[] = "appimagetool gpgme hook";
static const char signature_elf_section[] = ".sha256_sig";
static const char key_elf_section[] = ".sig_key";

char* get_passphrase_from_environment() {
    return getenv("APPIMAGETOOL_SIGN_PASSPHRASE");
}

gpgme_error_t gpgme_passphrase_callback(void* hook, const char* uid_hint, const char* passphrase_info, int prev_was_valid, int fd) {
    (void) passphrase_info;
    (void) prev_was_valid;

    assert(hook == gpgme_hook);

    // passing the passphrase via the environment is at least better than using a CLI parameter
    char* passphrase_from_env = get_passphrase_from_environment();

    if (passphrase_from_env == NULL) {
        fprintf(stderr, "[sign] no passphrase available from environment\n");
        return GPG_ERR_NO_PASSPHRASE;
    }

    fprintf(stderr, "[sign] providing passphrase for key %s\n", uid_hint);

    gpgme_io_writen(fd, passphrase_from_env, strlen(passphrase_from_env));

    // need to write a newline character according to the docs
    // maybe this would have solved the hanging issues we had with gpg(2) as a subprocess when writing to stdin...?
    gpgme_io_writen(fd, "\n", 1);

    return GPG_ERR_NO_ERROR;
}

gpgme_error_t gpgme_status_callback(void* hook, const char* keyword, const char* args) {
    assert(hook == gpgme_hook);

    fprintf(stderr, "[gpgme] %s: %s\n", keyword, args);
    return GPG_ERR_NO_ERROR;
}

// all of these are from gpg based libraries, and are used exclusively in the code below
// whenever there is an error in sign_appimage, we can free() up these resources
static gpgme_ctx_t gpgme_ctx = NULL;
static gpgme_data_t gpgme_appimage_file_data = NULL;
static gpgme_data_t gpgme_sig_data = NULL;
// we support just a single key at the moment
static gpgme_key_t gpgme_key = NULL;
static gpgme_data_t gpgme_key_data = NULL;

// this little helper can be called wherever the resources above must be released
void release_resources() {
    if (gpgme_appimage_file_data != NULL) {
        gpgme_data_release(gpgme_appimage_file_data);
        gpgme_appimage_file_data = NULL;
    }
    if (gpgme_sig_data != NULL) {
        gpgme_data_release(gpgme_sig_data);
        gpgme_sig_data = NULL;
    }
    if (gpgme_ctx != NULL) {
        gpgme_release(gpgme_ctx);
        gpgme_ctx = NULL;
    }
    if (gpgme_key != NULL) {
        gpgme_key_release(gpgme_key);
        gpgme_key = NULL;
    }
    if (gpgme_key_data != NULL) {
        gpgme_data_release(gpgme_key_data);
        gpgme_key_data = NULL;
    }
}

// it's possible to use a single macro to error-check both gpgme and gcrypt, since both originate from the gpg project
// the error types gcry_error_t and gpgme_error_t are both aliases for gpg_error_t
#define gpg_check_call(call_to_function) \
    { \
        gpg_error_t error = (call_to_function); \
        if (error != GPG_ERR_NO_ERROR) { \
            fprintf(stderr, "[sign] %s: call failed: %s\n", #call_to_function, gpgme_strerror(error)); \
            release_resources(); \
            return false; \
        } \
    }

char* calculate_sha256_hex_digest(char* filename) {
    // like gcrypt, gcrypt must be initialized
    {
        static const char gcrypt_minimum_required_version[] = "1.8.1";
        const char* gcrypt_version = gcry_check_version(gcrypt_minimum_required_version);

        if (gcrypt_version == NULL) {
            fprintf(stderr, "[sign] could not initialize gcrypt (>= %s)\n", gcrypt_minimum_required_version);
            release_resources();
            return NULL;
        } else {
            fprintf(stderr, "[sign] found gcrypt version %s\n", gcrypt_version);
        }
    }

    // algo is defined by the spec
    static const int hash_algo = GCRY_MD_SHA256;

    // open file and feed data chunk wise to gcrypt
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "[sign] could not calculate digest: opening file %s failed", filename);
        release_resources();
        return NULL;
    }

    gcry_md_hd_t gcry_md_handle = NULL;

    gpg_check_call(gcry_md_open(&gcry_md_handle, hash_algo, 0));

    // 4k chunks should work well enough
    char read_buffer[4096 * sizeof(char)];

    for (;;) {
        size_t bytes_read = fread(read_buffer, sizeof(char), sizeof(read_buffer), file);

        // if we have an error, we can stop immediately
        if (ferror(file) != 0) {
            fprintf(stderr, "[sign] failed to read chunk for hashing from file %s\n", filename);
            fclose(file);
            gcry_md_close(gcry_md_handle);
            return NULL;
        }

        gcry_md_write(gcry_md_handle, read_buffer, bytes_read);

        // once we read all the data, we can stop
        if (feof(file) != 0) {
            break;
        }
    }

    // done, we can close the file
    // since we do no longer have to worry about the file pointer, we can use gpg_check_call just fine below
    if (fclose(file) != 0) {
        fprintf(stderr, "[sign] failed to close hashed file %s\n", filename);
        gcry_md_close(gcry_md_handle);
        return NULL;
    }

    gpg_check_call(gcry_md_final(gcry_md_handle));

    // conveniently, we can ask gcrypt for the digest size
    unsigned int digest_size = gcry_md_get_algo_dlen(hash_algo);

    // read raw digest (i.e., memory-readable, not hexlified like, e.g., sha256sum would return)
    unsigned char* digest_in_ctx = gcry_md_read(gcry_md_handle, hash_algo);

    if (digest_in_ctx == NULL) {
        fprintf(stderr, "[sign] gcry_md_read failed\n");
        gcry_md_close(gcry_md_handle);
        return NULL;
    }

    // create "human-readable" version for the output
    // the method allocates a suitable string buffer
    // needs to be done before releasing the handle since this destroys digest_in_ctx
    char* out_buffer = appimage_hexlify((const char*) digest_in_ctx, digest_size);

    // release all resources and return the result to the caller
    gcry_md_close(gcry_md_handle);

    return out_buffer;
}

bool add_signers(const char* key_id) {
    // if a key id is specified, we ask gpgme to use this key explicitly, otherwise we use the first key in gpgme's list
    if (key_id != NULL) {
        // we could use the list stuff below, but using gpgme_get_key is a lot easier...
        gpg_check_call(gpgme_get_key(gpgme_ctx, key_id, &gpgme_key, true));
    } else {
        // searching the "first available key" is a little complex in gpgme...
        // since we are just looking for the first one, we don't need a loop at least
        gpg_check_call(gpgme_op_keylist_start(gpgme_ctx, NULL, 0));
        gpg_check_call(gpgme_op_keylist_next(gpgme_ctx, &gpgme_key));
    }

    fprintf(stderr, "[sign] using key with fingerprint %s, issuer name %s\n", gpgme_key->fpr, gpgme_key->uids->name);
    gpg_check_call(gpgme_signers_add(gpgme_ctx, gpgme_key));

    return true;
}

bool embed_data_in_elf_section(const char* filename, const char* elf_section, gpgme_data_t data, bool verbose) {
    // first up: find the ELF section in the AppImage file
    unsigned long key_section_offset = 0;
    unsigned long key_section_length = 0;

    if (verbose) {
        fprintf(stderr, "[sign] embedding data in ELF section %s\n", elf_section);
    }

    bool rv = appimage_get_elf_section_offset_and_length(
        filename,
        elf_section,
        &key_section_offset,
        &key_section_length
    );

    if (!rv || key_section_offset == 0 || key_section_length == 0) {
        fprintf(stderr, "[sign] could not determine offset for signature\n");
        release_resources();
        return false;
    }

    if (verbose) {
        fprintf(stderr, "[sign] key_offset: %lu\n", key_section_offset);
        fprintf(stderr, "[sign] key_length: %lu\n", key_section_length);
    }

    // next, read the exported key's size from the data object
    const off_t data_size = gpgme_data_seek(data, 0, SEEK_END);
    if (data_size < 0) {
        fprintf(stderr, "[sign] failed to detect size of signature\n");
        release_resources();
        return false;
    }

    if (verbose) {
        fprintf(stderr, "[sign] data size: %lu\n", data_size);
    }

    // rewind so we can later read the data
    if (gpgme_data_seek(data, 0, SEEK_SET) < 0) {
        fprintf(stderr, "[sign] failed to rewind data object\n");
        release_resources();
        return false;
    }

    if (data_size > key_section_length) {
        fprintf(stderr, "[sign] cannot embed key in AppImage: size exceeds reserved ELF section size\n");
        release_resources();
        return false;
    }

    // we need to read the signature into a custom buffer
    char data_buffer[data_size];

    size_t total_bytes_read = 0;
    size_t bytes_read = 0;

    for (;;) {
        bytes_read = gpgme_data_read(data, data_buffer, data_size);

        // EOF
        if (bytes_read == 0) {
            break;
        }

        // error
        if (bytes_read < 0) {
            fprintf(stderr, "[sign] failed to read from data object\n");
            release_resources();
            return false;
        }

        total_bytes_read += bytes_read;
    }

    if (total_bytes_read != data_size) {
        fprintf(
            stderr,
            "[sign] failed to read entire data from data object (%lu != %lu)\n",
            total_bytes_read,
            data_size
        );
        release_resources();
        return false;
    }

    FILE* destinationfp = fopen(filename, "r+");

    if (destinationfp == NULL) {
        fprintf(stderr, "[sign] failed to open the destination file for writing\n");
        release_resources();
        return false;
    }

    if (fseek(destinationfp, (long) key_section_offset, SEEK_SET) < 0) {
        fprintf(stderr, "[sign] fseek failed: %s\n", strerror(errno));
        release_resources();
        fclose(destinationfp);
        return false;
    }

    // write at once
    if (fwrite(data_buffer, sizeof(char), data_size, destinationfp) != data_size) {
        fprintf(stderr, "[sign] failed to write signature to AppImage file\n");
        release_resources();
        fclose(destinationfp);
        return false;
    }

    // done -> release file descriptor
    if (fclose(destinationfp) != 0) {
        fprintf(stderr, "[sign] failed to close file descriptor: %s\n", strerror(errno));
        release_resources();
        return false;
    }

    return true;
}

bool sign_appimage(char* appimage_filename, char* key_id, bool verbose) {
    fprintf(stderr, "[sign] signing requested\n");

    // like gcrypt, gpgme must be initialized
    {
        static const char gpgme_minimum_required_version[] = "1.10.0";
        const char* gpgme_version = gpgme_check_version(gpgme_minimum_required_version);

        if (gpgme_version == NULL) {
            fprintf(stderr, "[sign] could not initialize gpgme (>= %s)\n", gpgme_minimum_required_version);
            release_resources();
            return false;
        } else {
            fprintf(stderr, "[sign] found gpgme version %s\n", gpgme_version);
        }
    }

    // as per the spec, an SHA256 hash is signed and the signature is then embedded in the AppImage
    char* hex_digest = calculate_sha256_hex_digest(appimage_filename);
    if (hex_digest == NULL) {
        release_resources();
        return false;
    }

    fprintf(stderr, "[sign] calculated digest: %s\n", hex_digest);

    system("realpath sleep-x86_64.AppImage");

    gpg_check_call(gpgme_new(&gpgme_ctx));

    // we want an ASCII armored signature of plain text (hex string)
    gpgme_set_textmode(gpgme_ctx, true);
    gpgme_set_armor(gpgme_ctx, true);

    // in case the user provides a passphrase in the environment, we have to set the pinentry mode to loopback, like with the CLI
    if (get_passphrase_from_environment() != NULL) {
        gpgme_set_pinentry_mode(gpgme_ctx, GPGME_PINENTRY_MODE_LOOPBACK);
        gpgme_set_passphrase_cb(gpgme_ctx, gpgme_passphrase_callback, (void*) gpgme_hook);
    }

    // implement some fancy logging with log prefixes and stuff
    gpgme_set_status_cb(gpgme_ctx, gpgme_status_callback, (void*) gpgme_hook);

    if (!add_signers(key_id)) {
        release_resources();
        return false;
    }

    assert(gpgme_key != NULL);

    // we don't have to let gpgme copy the data since we can ensure the buffer remains valid the entire time
    gpg_check_call(gpgme_data_new_from_mem(&gpgme_appimage_file_data, hex_digest, strlen(hex_digest), 0));

    // we further need an out buffer to store the signature, we let gpgme handle the allocation
    gpg_check_call(gpgme_data_new(&gpgme_sig_data));

    // now, we can sign the data
    gpg_check_call(gpgme_op_sign(gpgme_ctx, gpgme_appimage_file_data, gpgme_sig_data, GPGME_SIG_MODE_NORMAL));

    gpgme_sign_result_t sign_result = gpgme_op_sign_result(gpgme_ctx);

    if (sign_result == NULL) {
        fprintf(stderr, "[sign] signing failed\n");
        release_resources();
        exit(1);
    }

    // we expect exactly one signature
    assert(sign_result->signatures->next == NULL);

    fprintf(
        stderr,
        "[sign] signed using pubkey algo %s, hash algo %s, key fingerprint %s\n",
        gpgme_pubkey_algo_name(sign_result->signatures->pubkey_algo),
        gpgme_hash_algo_name(sign_result->signatures->hash_algo),
        sign_result->signatures->fpr
    );

    fprintf(stderr, "[sign] embedding signature in AppImage\n");
    if (!embed_data_in_elf_section(appimage_filename, signature_elf_section, gpgme_sig_data, verbose)) {
        fprintf(stderr, "[sign] failed to embed signature in AppImage\n");
        release_resources();
        return false;
    }

    // exporting the key to a gpgme data object is relatively easy
    gpgme_data_new(&gpgme_key_data);
    gpgme_key_t keys_to_export[] = {gpgme_key, NULL};
    gpg_check_call(gpgme_op_export_keys(gpgme_ctx, keys_to_export, 0, gpgme_key_data));

    fprintf(stderr, "[sign] embedding key in AppImage\n");
    if (!embed_data_in_elf_section(appimage_filename, key_elf_section, gpgme_key_data, verbose)) {
        fprintf(stderr, "[sign] failed to embed key in AppImage\n");
        release_resources();
        return false;
    }

    release_resources();
    return true;
}


