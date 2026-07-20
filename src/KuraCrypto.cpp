/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Implementation of AES-256-GCM encryption/decryption with PBKDF2.
 */

#include "KuraCrypto.h"

#include <ByteOrder.h>
#include <Entry.h>
#include <File.h>
#include <Path.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>


KuraCrypto::KuraCrypto()
	:
	fErrorString(""),
	fBackupEnabled(true)
{
}


KuraCrypto::~KuraCrypto()
{
}


status_t
KuraCrypto::EncryptToFile(const char* path, const BString& password,
	const BString& jsonData)
{
	if (password.Length() == 0) {
		fErrorString = "Password cannot be empty";
		return B_BAD_VALUE;
	}

	// Generate random salt and IV
	uint8 salt[kSaltLength];
	uint8 iv[kIVLength];
	if (_RandomBytes(salt, kSaltLength) != B_OK
		|| _RandomBytes(iv, kIVLength) != B_OK) {
		fErrorString = "Failed to generate random bytes";
		return B_ERROR;
	}

	// Derive encryption key from password
	uint8 key[kKeyLength];
	status_t result = _DeriveKey(password, salt, kSaltLength,
		kDefaultIterations, key, kKeyLength);
	if (result != B_OK) {
		fErrorString = "Key derivation failed";
		return result;
	}

	// Encrypt with AES-256-GCM
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		fErrorString = "Failed to create cipher context";
		memset(key, 0, kKeyLength);
		return B_NO_MEMORY;
	}

	int ok = 1;
	ok = ok && EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
	ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIVLength,
		NULL);
	ok = ok && EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv);

	// Clear key from memory as soon as it's loaded into the context
	memset(key, 0, kKeyLength);

	if (!ok) {
		fErrorString = "Encryption initialization failed";
		EVP_CIPHER_CTX_free(ctx);
		return B_ERROR;
	}

	int dataLen = jsonData.Length();
	int cipherLen = dataLen + EVP_MAX_BLOCK_LENGTH;
	uint8* ciphertext = new(std::nothrow) uint8[cipherLen];
	if (ciphertext == NULL) {
		fErrorString = "Out of memory";
		EVP_CIPHER_CTX_free(ctx);
		return B_NO_MEMORY;
	}

	int outLen = 0;
	int totalLen = 0;
	ok = EVP_EncryptUpdate(ctx, ciphertext, &outLen,
		(const uint8*)jsonData.String(), dataLen);
	totalLen = outLen;

	if (ok)
		ok = EVP_EncryptFinal_ex(ctx, ciphertext + totalLen, &outLen);
	totalLen += outLen;

	uint8 tag[kTagLength];
	if (ok)
		ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLength, tag);

	EVP_CIPHER_CTX_free(ctx);

	if (!ok) {
		fErrorString = "Encryption failed";
		delete[] ciphertext;
		return B_ERROR;
	}

	// Write to a temporary file first, then atomically move it into
	// place. A crash or full disk mid-write must never destroy the
	// existing database.
	BString tmpPath(path);
	tmpPath << ".tmp";

	BFile file(tmpPath.String(),
		B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	result = file.InitCheck();
	if (result != B_OK) {
		fErrorString = "Failed to create file";
		delete[] ciphertext;
		return result;
	}

	// Integers are stored little-endian on disk (explicit, so the
	// format is well-defined; a no-op on x86).
	uint32 versionLE = B_HOST_TO_LENDIAN_INT32(kKuraVersion);
	uint32 iterationsLE = B_HOST_TO_LENDIAN_INT32(kDefaultIterations);
	uint32 ctLenLE = B_HOST_TO_LENDIAN_INT32((uint32)totalLen);

	bool writeOk = true;
	writeOk = writeOk && file.Write(kKuraMagic, 4) == 4;
	writeOk = writeOk && file.Write(&versionLE, 4) == 4;
	writeOk = writeOk && file.Write(&iterationsLE, 4) == 4;
	writeOk = writeOk && file.Write(salt, kSaltLength) == kSaltLength;
	writeOk = writeOk && file.Write(iv, kIVLength) == kIVLength;
	writeOk = writeOk && file.Write(tag, kTagLength) == kTagLength;
	writeOk = writeOk && file.Write(&ctLenLE, 4) == 4;
	writeOk = writeOk
		&& file.Write(ciphertext, totalLen) == (ssize_t)totalLen;

	memset(ciphertext, 0, totalLen);
	delete[] ciphertext;

	file.Sync();
	file.Unset();

	if (!writeOk) {
		fErrorString = "Failed to write file (disk full?)";
		BEntry(tmpPath.String()).Remove();
		return B_ERROR;
	}

	// Keep the previous database as a .bak (when enabled), then
	// move the new file into place.
	BEntry existing(path);
	if (fBackupEnabled && existing.Exists()) {
		BString bakPath(path);
		bakPath << ".bak";
		existing.Rename(bakPath.String(), true);
	}

	BEntry tmpEntry(tmpPath.String());
	result = tmpEntry.Rename(path, true);
	if (result != B_OK) {
		fErrorString = "Failed to move database into place";
		tmpEntry.Remove();
		return result;
	}

	return B_OK;
}


status_t
KuraCrypto::DecryptFromFile(const char* path, const BString& password,
	BString& jsonData)
{
	BFile file(path, B_READ_ONLY);
	status_t result = file.InitCheck();
	if (result != B_OK) {
		fErrorString = "Failed to open file";
		return result;
	}

	// Read and verify header
	char magic[4];
	uint32 version;
	uint32 iterations;
	uint8 salt[kSaltLength];
	uint8 iv[kIVLength];
	uint8 tag[kTagLength];
	uint32 ctLen;

	if (file.Read(magic, 4) != 4 || memcmp(magic, kKuraMagic, 4) != 0) {
		fErrorString = "Not a Kura database file";
		return B_BAD_DATA;
	}

	if (file.Read(&version, 4) != 4
		|| B_LENDIAN_TO_HOST_INT32(version) != kKuraVersion) {
		fErrorString = "Unsupported database version";
		return B_BAD_DATA;
	}

	if (file.Read(&iterations, 4) != 4
		|| file.Read(salt, kSaltLength) != kSaltLength
		|| file.Read(iv, kIVLength) != kIVLength
		|| file.Read(tag, kTagLength) != kTagLength
		|| file.Read(&ctLen, 4) != 4) {
		fErrorString = "Corrupted file header";
		return B_BAD_DATA;
	}

	iterations = B_LENDIAN_TO_HOST_INT32(iterations);
	ctLen = B_LENDIAN_TO_HOST_INT32(ctLen);

	// Sanity check ciphertext length (max 100 MB)
	if (ctLen == 0 || ctLen > 100 * 1024 * 1024) {
		fErrorString = "Invalid ciphertext length";
		return B_BAD_DATA;
	}

	// Sanity check iterations (a corrupt header must not stall us)
	if (iterations == 0 || iterations > 10 * 1000 * 1000) {
		fErrorString = "Invalid KDF parameters";
		return B_BAD_DATA;
	}

	// Read ciphertext
	uint8* ciphertext = new(std::nothrow) uint8[ctLen];
	if (ciphertext == NULL) {
		fErrorString = "Out of memory";
		return B_NO_MEMORY;
	}

	if (file.Read(ciphertext, ctLen) != (ssize_t)ctLen) {
		fErrorString = "Truncated file";
		delete[] ciphertext;
		return B_BAD_DATA;
	}

	// Derive key from password
	uint8 key[kKeyLength];
	result = _DeriveKey(password, salt, kSaltLength, iterations,
		key, kKeyLength);
	if (result != B_OK) {
		fErrorString = "Key derivation failed";
		delete[] ciphertext;
		return result;
	}

	// Decrypt with AES-256-GCM
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		fErrorString = "Failed to create cipher context";
		memset(key, 0, kKeyLength);
		delete[] ciphertext;
		return B_NO_MEMORY;
	}

	int ok = 1;
	ok = ok && EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
	ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIVLength,
		NULL);
	ok = ok && EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv);

	memset(key, 0, kKeyLength);

	if (!ok) {
		fErrorString = "Decryption initialization failed";
		EVP_CIPHER_CTX_free(ctx);
		delete[] ciphertext;
		return B_ERROR;
	}

	uint8* plaintext = new(std::nothrow) uint8[ctLen + 1];
	if (plaintext == NULL) {
		fErrorString = "Out of memory";
		EVP_CIPHER_CTX_free(ctx);
		delete[] ciphertext;
		return B_NO_MEMORY;
	}

	int outLen = 0;
	int totalLen = 0;
	ok = EVP_DecryptUpdate(ctx, plaintext, &outLen, ciphertext, ctLen);
	totalLen = outLen;

	// Set expected GCM tag before finalization
	if (ok)
		ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLength,
			(void*)tag);

	int finalOk = 0;
	if (ok)
		finalOk = EVP_DecryptFinal_ex(ctx, plaintext + totalLen, &outLen);
	totalLen += outLen;

	EVP_CIPHER_CTX_free(ctx);
	memset(ciphertext, 0, ctLen);
	delete[] ciphertext;

	if (!ok || !finalOk) {
		// GCM tag mismatch means wrong password or tampered data
		fErrorString = "Wrong password or corrupted database";
		memset(plaintext, 0, ctLen + 1);
		delete[] plaintext;
		return B_PERMISSION_DENIED;
	}

	plaintext[totalLen] = '\0';
	jsonData.SetTo((const char*)plaintext, totalLen);

	memset(plaintext, 0, totalLen);
	delete[] plaintext;

	return B_OK;
}


status_t
KuraCrypto::_DeriveKey(const BString& password, const uint8* salt,
	int saltLen, uint32 iterations, uint8* keyOut, int keyLen)
{
	int result = PKCS5_PBKDF2_HMAC(
		password.String(), password.Length(),
		salt, saltLen,
		iterations, EVP_sha256(),
		keyLen, keyOut);

	return (result == 1) ? B_OK : B_ERROR;
}


status_t
KuraCrypto::_RandomBytes(uint8* buffer, int length)
{
	int result = RAND_bytes(buffer, length);
	return (result == 1) ? B_OK : B_ERROR;
}
