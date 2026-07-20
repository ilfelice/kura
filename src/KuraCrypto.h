/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Encryption layer using AES-256-GCM with PBKDF2 key derivation.
 * Handles encrypting/decrypting the database file contents.
 *
 * File format layout:
 *   [4 bytes]  Magic: "KURA"
 *   [4 bytes]  Version: uint32 (currently 1)
 *   [4 bytes]  PBKDF2 iterations: uint32
 *   [32 bytes] Salt for PBKDF2
 *   [12 bytes] GCM IV/nonce
 *   [16 bytes] GCM authentication tag
 *   [4 bytes]  Ciphertext length: uint32
 *   [N bytes]  Ciphertext (encrypted JSON payload)
 */

#ifndef KURA_CRYPTO_H
#define KURA_CRYPTO_H

#include <SupportDefs.h>
#include <String.h>

// File format constants
static const char kKuraMagic[4] = {'K', 'U', 'R', 'A'};
static const uint32 kKuraVersion = 1;
static const uint32 kDefaultIterations = 200000;
static const int kSaltLength = 32;
static const int kKeyLength = 32;   // AES-256
static const int kIVLength = 12;    // GCM nonce
static const int kTagLength = 16;   // GCM auth tag


class KuraCrypto {
public:
						KuraCrypto();
						~KuraCrypto();

	// Encrypt JSON data with a master password and write to file.
	// Returns B_OK on success, or an error code.
	status_t			EncryptToFile(const char* path,
							const BString& password,
							const BString& jsonData);

	// Decrypt a file and return the JSON data.
	// Returns B_OK on success, B_PERMISSION_DENIED if wrong password,
	// or another error code.
	status_t			DecryptFromFile(const char* path,
							const BString& password,
							BString& jsonData);

	// Get a human-readable error description after a failed operation.
	const char*			ErrorString() const { return fErrorString.String(); }

	// Whether to keep the previous database as a .bak when saving
	void				SetBackupEnabled(bool enabled)
							{ fBackupEnabled = enabled; }

private:
	// Derive a 256-bit key from password + salt using PBKDF2-SHA256.
	status_t			_DeriveKey(const BString& password,
							const uint8* salt, int saltLen,
							uint32 iterations,
							uint8* keyOut, int keyLen);

	// Generate cryptographically secure random bytes.
	status_t			_RandomBytes(uint8* buffer, int length);

	BString				fErrorString;
	bool				fBackupEnabled;
};

#endif // KURA_CRYPTO_H
