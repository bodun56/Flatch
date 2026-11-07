#pragma once

#include <QString>

// Small self-contained encryption layer for snippets marked as secrets.
//
// A random 256-bit master key is generated on first use and stored in the
// user's profile.  On Windows the key file is additionally protected with
// DPAPI (so it is bound to the current Windows account); on Linux the file is
// readable by the owner only.  No master password is required from the user.
//
// Payload format: "FLT1:" + base64(version | nonce | HMAC-SHA256 tag | ciphertext)
// Cipher: HMAC-SHA256 based counter-mode keystream + encrypt-then-MAC.
namespace Crypto {

// Loads (or creates) the key. Returns false and fills *error on failure.
bool isAvailable(QString *error = nullptr);

bool encrypt(const QString &plainText, QString *out, QString *error = nullptr);
bool decrypt(const QString &envelope, QString *out, QString *error = nullptr);

} // namespace Crypto
