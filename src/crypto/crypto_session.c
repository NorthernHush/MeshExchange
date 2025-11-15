#include "../../include/protocol.h"
#include "crypto_session.h"
#include <string.h>
#include <stdlib.h>
#include <sodium.h>

// Initialize crypto session
int crypto_session_init(crypto_session_t *session) {
    if (!session) return -1;

    memset(session, 0, sizeof(crypto_session_t));
    if (sodium_init() < 0) {
        return -1; // libsodium initialization failed
    }
    return crypto_session_generate_keys(session);
}

// Generate ECDH keypair
int crypto_session_generate_keys(crypto_session_t *session) {
    if (!session) return -1;

    // Generate private key
    randombytes_buf(session->private_key, ECDH_PRIVATE_KEY_LEN);

    // Derive public key: public = private * G
    crypto_scalarmult_base(session->public_key, session->private_key);

    return 0;
}

// Compute shared secret from peer's public key
int crypto_session_compute_shared_secret(crypto_session_t *session) {
    if (!session || !session->peer_public_key) return -1;

    // shared_secret = private * peer_public
    if (crypto_scalarmult(session->shared_secret, session->private_key,
                         session->peer_public_key) != 0) {
        return -1; // Invalid public key
    }

    session->ecdh_completed = 1;
    return 0;
}

// Derive session key from shared secret using HKDF
int crypto_session_derive_session_key(crypto_session_t *session) {
    if (!session || !session->ecdh_completed) return -1;

    // Use BLAKE2b as HKDF-like function
    crypto_generichash(session->session_key, SESSION_KEY_LEN,
                      session->shared_secret, crypto_scalarmult_BYTES,
                      NULL, 0);

    session->session_established = 1;
    return 0;
}

// Encrypt metadata using XChaCha20-Poly1305
int crypto_session_encrypt_metadata(crypto_session_t *session,
                                   const char *filename, long long filesize,
                                   const char *recipient, EncryptedMetadata *encrypted) {
    if (!session || !session->session_established || !encrypted) return -1;

    // Generate random nonce
    randombytes_buf(encrypted->nonce, XCHACHA20_NONCE_LEN);

    unsigned long long ciphertext_len;

    // Encrypt filename
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            encrypted->encrypted_filename, &ciphertext_len,
            (const unsigned char *)filename, strlen(filename),
            NULL, 0, // no additional data
            NULL, // no secret nonce
            encrypted->nonce, session->session_key) != 0) {
        return -1;
    }
    memcpy(encrypted->filename_auth_tag, encrypted->encrypted_filename + ciphertext_len - 16, 16);

    // Encrypt filesize
    uint8_t size_buf[sizeof(long long)];
    memcpy(size_buf, &filesize, sizeof(long long));
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            encrypted->encrypted_size, &ciphertext_len,
            size_buf, sizeof(long long),
            NULL, 0, NULL, encrypted->nonce, session->session_key) != 0) {
        return -1;
    }
    memcpy(encrypted->size_auth_tag, encrypted->encrypted_size + ciphertext_len - 16, 16);

    // Encrypt recipient if provided
    if (recipient && strlen(recipient) > 0) {
        if (crypto_aead_xchacha20poly1305_ietf_encrypt(
                encrypted->encrypted_recipient, &ciphertext_len,
                (const unsigned char *)recipient, strlen(recipient),
                NULL, 0, NULL, encrypted->nonce, session->session_key) != 0) {
            return -1;
        }
        memcpy(encrypted->recipient_auth_tag, encrypted->encrypted_recipient + ciphertext_len - 16, 16);
    }

    return 0;
}

// Decrypt metadata using XChaCha20-Poly1305
int crypto_session_decrypt_metadata(crypto_session_t *session,
                                   const EncryptedMetadata *encrypted,
                                   char *filename, long long *filesize, char *recipient) {
    if (!session || !session->session_established || !encrypted) return -1;

    unsigned long long plaintext_len;

    // Decrypt filename
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            (unsigned char *)filename, &plaintext_len,
            NULL, // no secret nonce
            encrypted->encrypted_filename, ENCRYPTED_METADATA_MAX_LEN,
            NULL, 0, // no additional data
            encrypted->nonce, session->session_key) != 0) {
        return -1; // Authentication failed
    }
    filename[plaintext_len] = '\0';

    // Decrypt filesize
    uint8_t size_buf[sizeof(long long)];
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            size_buf, &plaintext_len,
            NULL,
            encrypted->encrypted_size, sizeof(long long) + 16,
            NULL, 0, encrypted->nonce, session->session_key) != 0) {
        return -1;
    }
    memcpy(filesize, size_buf, sizeof(long long));

    // Decrypt recipient if present
    if (recipient && encrypted->encrypted_recipient[0]) {
        if (crypto_aead_xchacha20poly1305_ietf_decrypt(
                (unsigned char *)recipient, &plaintext_len,
                NULL,
                encrypted->encrypted_recipient, FINGERPRINT_LEN + 16,
                NULL, 0, encrypted->nonce, session->session_key) != 0) {
            return -1;
        }
        recipient[plaintext_len] = '\0';
    } else if (recipient) {
        recipient[0] = '\0';
    }

    return 0;
}

// Clean up session
void crypto_session_cleanup(crypto_session_t *session) {
    if (session) {
        sodium_memzero(session, sizeof(crypto_session_t));
    }
}
