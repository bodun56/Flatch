#include "crypto.h"

#include "apppaths.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QtEndian>

#include <cstring>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <wincrypt.h>
#endif

namespace {

constexpr int kMasterKeySize = 64; // 32 bytes encryption key + 32 bytes MAC key material
constexpr int kNonceSize = 16;
constexpr int kTagSize = 32;
constexpr quint8 kEnvelopeVersion = 1;
const QByteArray kEnvelopePrefix = QByteArrayLiteral("FLT1:");
#ifdef Q_OS_WIN
const QByteArray kProtectionEntropy = QByteArrayLiteral("Flatch.KeyProtection.v1");
#endif

QByteArray s_masterKey;
bool s_loaded = false;

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

QByteArray randomBytes(int count)
{
    QByteArray result(count, Qt::Uninitialized);
    QRandomGenerator *generator = QRandomGenerator::system();
    int offset = 0;
    while (offset < count) {
        const quint64 value = generator->generate64();
        const int chunk = qMin<int>(8, count - offset);
        std::memcpy(result.data() + offset, &value, static_cast<size_t>(chunk));
        offset += chunk;
    }
    return result;
}

QByteArray hmacSha256(const QByteArray &key, const QByteArray &message)
{
    return QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha256);
}

bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a.at(i)) ^ static_cast<unsigned char>(b.at(i));
    return diff == 0;
}

#ifdef Q_OS_WIN

bool protectWithDpapi(const QByteArray &input, QByteArray *output, QString *error)
{
    DATA_BLOB inBlob{};
    inBlob.cbData = static_cast<DWORD>(input.size());
    inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(input.constData()));

    DATA_BLOB entropyBlob{};
    entropyBlob.cbData = static_cast<DWORD>(kProtectionEntropy.size());
    entropyBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(kProtectionEntropy.constData()));

    DATA_BLOB outBlob{};
    if (!CryptProtectData(&inBlob, L"Flatch encryption key", &entropyBlob,
                          nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        setError(error, QCoreApplication::translate(
                            "Crypto",
                            "Windows DPAPI не смогла защитить ключ шифрования (код ошибки %1).")
                            .arg(GetLastError()));
        return false;
    }

    *output = QByteArray(reinterpret_cast<const char *>(outBlob.pbData),
                         static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return true;
}

bool unprotectWithDpapi(const QByteArray &input, QByteArray *output, QString *error)
{
    DATA_BLOB inBlob{};
    inBlob.cbData = static_cast<DWORD>(input.size());
    inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(input.constData()));

    DATA_BLOB entropyBlob{};
    entropyBlob.cbData = static_cast<DWORD>(kProtectionEntropy.size());
    entropyBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(kProtectionEntropy.constData()));

    DATA_BLOB outBlob{};
    if (!CryptUnprotectData(&inBlob, nullptr, &entropyBlob,
                            nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        setError(error, QCoreApplication::translate(
                            "Crypto",
                            "Не удалось расшифровать файл ключа (%1). "
                            "Он был создан для другой учётной записи Windows?")
                            .arg(GetLastError()));
        return false;
    }

    *output = QByteArray(reinterpret_cast<const char *>(outBlob.pbData),
                         static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return true;
}

#endif // Q_OS_WIN

bool writeKeyFile(const QByteArray &rawKey, QString *error)
{
    QByteArray stored = rawKey;

#ifdef Q_OS_WIN
    if (!protectWithDpapi(rawKey, &stored, error))
        return false;
#endif

    const QString path = AppPaths::keyFile();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QCoreApplication::translate(
                            "Crypto", "Не удалось создать файл ключа «%1»: %2")
                            .arg(path, file.errorString()));
        return false;
    }
    if (file.write(stored) != stored.size()) {
        setError(error, QCoreApplication::translate(
                            "Crypto", "Не удалось записать файл ключа «%1»: %2")
                            .arg(path, file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setError(error, QCoreApplication::translate(
                            "Crypto", "Не удалось сохранить файл ключа «%1»: %2")
                            .arg(path, file.errorString()));
        return false;
    }

    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

bool readKeyFile(QString *error)
{
    const QString path = AppPaths::keyFile();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QCoreApplication::translate(
                            "Crypto", "Не удалось открыть файл ключа «%1»: %2")
                            .arg(path, file.errorString()));
        return false;
    }

    QByteArray stored = file.readAll();
    file.close();
    if (stored.isEmpty()) {
        setError(error, QCoreApplication::translate(
                            "Crypto",
                            "Файл ключа «%1» пуст. Удалите его, чтобы создать новый; "
                            "старые защищённые сниппеты после этого станут недоступны.")
                            .arg(path));
        return false;
    }

    QByteArray raw = stored;

#ifdef Q_OS_WIN
    if (!unprotectWithDpapi(stored, &raw, error))
        return false;
#endif

    if (raw.size() != kMasterKeySize) {
        setError(error, QCoreApplication::translate(
                            "Crypto", "Файл ключа «%1» повреждён (неверный размер).").arg(path));
        return false;
    }

    s_masterKey = raw;
    return true;
}

bool ensureLoaded(QString *error)
{
    if (s_loaded && !s_masterKey.isEmpty())
        return true;

    if (error)
        error->clear();

    if (!QFileInfo::exists(AppPaths::keyFile())) {
        const QByteArray newKey = randomBytes(kMasterKeySize);
        if (!writeKeyFile(newKey, error))
            return false;
        s_masterKey = newKey;
    } else if (!readKeyFile(error)) {
        return false;
    }

    s_loaded = true;
    return true;
}

QByteArray subKey(const char *label)
{
    return hmacSha256(s_masterKey, QByteArray(label));
}

} // namespace

namespace Crypto {

bool isAvailable(QString *error)
{
    return ensureLoaded(error);
}

bool encrypt(const QString &plainText, QString *out, QString *error)
{
    if (!out)
        return false;
    if (error)
        error->clear();

    if (!ensureLoaded(error))
        return false;

    const QByteArray encKey = subKey("flatch/enc/v1");
    const QByteArray macKey = subKey("flatch/mac/v1");
    const QByteArray nonce = randomBytes(kNonceSize);
    const QByteArray plain = plainText.toUtf8();

    QByteArray cipher = plain;
    quint32 counter = 0;
    for (int offset = 0; offset < cipher.size(); offset += 32, ++counter) {
        QByteArray blockInput = nonce;
        uchar counterBytes[4];
        qToBigEndian<quint32>(counter, counterBytes);
        blockInput.append(reinterpret_cast<const char *>(counterBytes), 4);
        const QByteArray block = hmacSha256(encKey, blockInput);
        const int chunk = qMin(32, cipher.size() - offset);
        for (int i = 0; i < chunk; ++i)
            cipher[offset + i] = static_cast<char>(cipher.at(offset + i) ^ block.at(i));
    }

    QByteArray macInput = kEnvelopePrefix;
    macInput.append(static_cast<char>(kEnvelopeVersion));
    macInput.append(nonce);
    macInput.append(cipher);
    const QByteArray tag = hmacSha256(macKey, macInput);

    QByteArray envelope;
    envelope.reserve(1 + nonce.size() + tag.size() + cipher.size());
    envelope.append(static_cast<char>(kEnvelopeVersion));
    envelope.append(nonce);
    envelope.append(tag);
    envelope.append(cipher);

    *out = QString::fromLatin1(kEnvelopePrefix)
           + QString::fromLatin1(envelope.toBase64());
    return true;
}

bool decrypt(const QString &envelope, QString *out, QString *error)
{
    if (!out)
        return false;
    if (error)
        error->clear();

    if (!ensureLoaded(error))
        return false;

    const QString trimmed = envelope.trimmed();
    if (!trimmed.startsWith(QString::fromLatin1(kEnvelopePrefix))) {
        setError(error, QCoreApplication::translate(
                            "Crypto", "Неизвестный формат зашифрованных данных."));
        return false;
    }

    const QByteArray raw = QByteArray::fromBase64(
        trimmed.mid(kEnvelopePrefix.size()).toLatin1());
    const int minimumSize = 1 + kNonceSize + kTagSize;
    if (raw.size() < minimumSize) {
        setError(error, QCoreApplication::translate(
                            "Crypto", "Зашифрованные данные повреждены (слишком короткие)."));
        return false;
    }

    const quint8 version = static_cast<quint8>(raw.at(0));
    if (version != kEnvelopeVersion) {
        setError(error, QCoreApplication::translate(
                            "Crypto", "Неподдерживаемая версия шифрования (%1).").arg(version));
        return false;
    }

    const QByteArray nonce = raw.mid(1, kNonceSize);
    const QByteArray tag = raw.mid(1 + kNonceSize, kTagSize);
    const QByteArray cipher = raw.mid(minimumSize);

    const QByteArray encKey = subKey("flatch/enc/v1");
    const QByteArray macKey = subKey("flatch/mac/v1");

    QByteArray macInput = kEnvelopePrefix;
    macInput.append(static_cast<char>(kEnvelopeVersion));
    macInput.append(nonce);
    macInput.append(cipher);
    const QByteArray expectedTag = hmacSha256(macKey, macInput);
    if (!constantTimeEquals(tag, expectedTag)) {
        setError(error, QCoreApplication::translate(
                            "Crypto",
                            "Не удалось расшифровать сниппет: данные повреждены "
                            "или файл ключа был заменён."));
        return false;
    }

    QByteArray plain = cipher;
    quint32 counter = 0;
    for (int offset = 0; offset < plain.size(); offset += 32, ++counter) {
        QByteArray blockInput = nonce;
        uchar counterBytes[4];
        qToBigEndian<quint32>(counter, counterBytes);
        blockInput.append(reinterpret_cast<const char *>(counterBytes), 4);
        const QByteArray block = hmacSha256(encKey, blockInput);
        const int chunk = qMin(32, plain.size() - offset);
        for (int i = 0; i < chunk; ++i)
            plain[offset + i] = static_cast<char>(plain.at(offset + i) ^ block.at(i));
    }

    *out = QString::fromUtf8(plain);
    return true;
}

} // namespace Crypto
