#include "manager_signing_identity.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <QObject>
#include <string>
#include <vector>

#if defined(Q_OS_MACOS)
#include <Security/SecRandom.h>
#include <Security/Security.h>
#elif defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
#include <wincred.h>
#endif

extern "C" int hs_manager_ed25519_public_key(const std::uint8_t* seed,
                                               std::size_t seedLength,
                                               std::uint8_t* output,
                                               std::size_t outputLength);
extern "C" int hs_manager_ed25519_sign(const std::uint8_t* seed,
                                        std::size_t seedLength,
                                        const std::uint8_t* message,
                                        std::size_t messageLength,
                                        std::uint8_t* signature,
                                        std::size_t signatureLength);
extern "C" int hs_manager_ed25519_verify(const std::uint8_t* publicKey,
                                          std::size_t publicKeyLength,
                                          const std::uint8_t* message,
                                          std::size_t messageLength,
                                          const std::uint8_t* signature,
                                          std::size_t signatureLength);

namespace helicoid::manager {
namespace {

constexpr std::size_t seedLength = 32;
constexpr std::size_t publicKeyLength = 32;
constexpr std::size_t signatureLength = 64;
constexpr std::size_t maxMessageLength = 65'536;

void secureClear(void* memory, std::size_t length) noexcept
{
    volatile auto* bytes = static_cast<volatile std::uint8_t*>(memory);
    while (length-- > 0) {
        *bytes++ = 0;
    }
}

int lowercaseHexDigit(char character) noexcept
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    return -1;
}

bool decodeLowercaseHex(const QByteArray& encoded, std::uint8_t* output,
                        std::size_t outputLength) noexcept
{
    if (output == nullptr || encoded.size() != static_cast<qsizetype>(outputLength * 2U)) {
        return false;
    }
    for (std::size_t index = 0; index < outputLength; ++index) {
        const int high = lowercaseHexDigit(encoded.at(static_cast<qsizetype>(index * 2U)));
        const int low = lowercaseHexDigit(encoded.at(static_cast<qsizetype>(index * 2U + 1U)));
        if (high < 0 || low < 0) return false;
        output[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return true;
}

bool derivePublicKey(const std::array<std::uint8_t, seedLength>& seed,
                     std::array<std::uint8_t, publicKeyLength>& publicKey)
{
    return hs_manager_ed25519_public_key(seed.data(), seed.size(), publicKey.data(), publicKey.size()) == 0;
}

#if defined(Q_OS_MACOS)

CFMutableDictionaryRef keychainQuery()
{
    auto query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    if (query == nullptr) {
        return nullptr;
    }
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, CFSTR("systems.helicoid.manager.signing-identity.v1"));
    CFDictionarySetValue(query, kSecAttrAccount, CFSTR("manager-ed25519"));
    CFDictionarySetValue(query, kSecAttrSynchronizable, kCFBooleanFalse);
    return query;
}

enum class StoreRead { Found, Missing, Failed };

// The previous Swift Manager uses this same Keychain item and stores a leading
// format byte before the Ed25519 seed. Normalize the earlier Qt raw-seed form
// in place so both adapters continue using one protected Manager identity.
bool rewriteLegacySeedFormat(const std::array<std::uint8_t, seedLength>& seed, QString& error)
{
    std::array<std::uint8_t, seedLength + 1> versioned{};
    versioned[0] = 1;
    std::copy(seed.begin(), seed.end(), versioned.begin() + 1);
    CFMutableDictionaryRef query = keychainQuery();
    if (query == nullptr) {
        secureClear(versioned.data(), versioned.size());
        error = QObject::tr("The macOS Keychain migration query could not be created.");
        return false;
    }
    CFDataRef value = CFDataCreate(kCFAllocatorDefault, versioned.data(),
                                   static_cast<CFIndex>(versioned.size()));
    secureClear(versioned.data(), versioned.size());
    if (value == nullptr) {
        CFRelease(query);
        error = QObject::tr("The existing Manager identity could not be prepared for Keychain migration.");
        return false;
    }
    CFMutableDictionaryRef updates = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (updates == nullptr) {
        CFRelease(value);
        CFRelease(query);
        error = QObject::tr("The existing Manager identity could not be prepared for Keychain migration.");
        return false;
    }
    CFDictionarySetValue(updates, kSecValueData, value);
    const OSStatus status = SecItemUpdate(query, updates);
    CFRelease(updates);
    CFRelease(value);
    CFRelease(query);
    if (status != errSecSuccess) {
        error = QObject::tr("The existing Manager identity could not be migrated in the unlocked Keychain.");
        return false;
    }
    return true;
}

StoreRead readSeed(std::array<std::uint8_t, seedLength>& seed, QString& error)
{
    CFMutableDictionaryRef query = keychainQuery();
    if (query == nullptr) {
        error = QObject::tr("The macOS Keychain query could not be created.");
        return StoreRead::Failed;
    }
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    CFTypeRef item = nullptr;
    const OSStatus status = SecItemCopyMatching(query, &item);
    CFRelease(query);
    if (status == errSecItemNotFound) {
        return StoreRead::Missing;
    }
    if (status != errSecSuccess || item == nullptr || CFGetTypeID(item) != CFDataGetTypeID()) {
        if (item != nullptr) CFRelease(item);
        error = QObject::tr("The Manager identity could not be read from the unlocked Keychain.");
        return StoreRead::Failed;
    }
    const auto data = static_cast<CFDataRef>(item);
    const CFIndex length = CFDataGetLength(data);
    const UInt8* bytes = CFDataGetBytePtr(data);
    if ((length != static_cast<CFIndex>(seed.size()) &&
         length != static_cast<CFIndex>(seed.size() + 1)) || bytes == nullptr ||
        (length == static_cast<CFIndex>(seed.size() + 1) && bytes[0] != 1)) {
        CFRelease(item);
        error = QObject::tr("The stored Manager identity is invalid and was not replaced.");
        return StoreRead::Failed;
    }
    const bool needsFormatMigration = length == static_cast<CFIndex>(seed.size());
    std::memcpy(seed.data(), bytes + (needsFormatMigration ? 0 : 1), seed.size());
    CFRelease(item);
    if (needsFormatMigration && !rewriteLegacySeedFormat(seed, error)) {
        secureClear(seed.data(), seed.size());
        return StoreRead::Failed;
    }
    return StoreRead::Found;
}

bool insertSeed(const std::array<std::uint8_t, seedLength>& seed, bool& inserted, QString& error)
{
    CFMutableDictionaryRef query = keychainQuery();
    if (query == nullptr) {
        error = QObject::tr("The macOS Keychain query could not be created.");
        return false;
    }
    std::array<std::uint8_t, seedLength + 1> versioned{};
    versioned[0] = 1;
    std::copy(seed.begin(), seed.end(), versioned.begin() + 1);
    CFDataRef value = CFDataCreate(kCFAllocatorDefault, versioned.data(),
                                   static_cast<CFIndex>(versioned.size()));
    secureClear(versioned.data(), versioned.size());
    if (value == nullptr) {
        CFRelease(query);
        error = QObject::tr("The Manager identity could not be prepared for Keychain storage.");
        return false;
    }
    CFDictionarySetValue(query, kSecAttrAccessible, kSecAttrAccessibleWhenUnlockedThisDeviceOnly);
    CFDictionarySetValue(query, kSecAttrLabel, CFSTR("Helicoid Manager signing identity"));
    CFDictionarySetValue(query, kSecValueData, value);
    const OSStatus status = SecItemAdd(query, nullptr);
    CFRelease(value);
    CFRelease(query);
    if (status == errSecDuplicateItem) {
        inserted = false;
        return true;
    }
    if (status != errSecSuccess) {
        error = QObject::tr("The Manager identity could not be saved in the Keychain.");
        return false;
    }
    inserted = true;
    return true;
}

bool generateSeed(std::array<std::uint8_t, seedLength>& seed)
{
    return SecRandomCopyBytes(kSecRandomDefault, seed.size(), seed.data()) == errSecSuccess;
}

#elif defined(Q_OS_WIN)

class StoreMutex final {
public:
    bool acquire(QString& error)
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            error = QObject::tr("The Manager identity lock could not be opened.");
            return false;
        }
        DWORD required = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &required);
        if (required == 0) {
            CloseHandle(token);
            error = QObject::tr("The Manager identity lock could not be prepared.");
            return false;
        }
        std::vector<std::uint8_t> tokenUser(required);
        if (!GetTokenInformation(token, TokenUser, tokenUser.data(), required, &required)) {
            CloseHandle(token);
            error = QObject::tr("The Manager identity lock could not be prepared.");
            return false;
        }
        CloseHandle(token);
        LPWSTR sidText = nullptr;
        const auto* user = reinterpret_cast<const TOKEN_USER*>(tokenUser.data());
        if (!ConvertSidToStringSidW(user->User.Sid, &sidText)) {
            error = QObject::tr("The Manager identity lock could not be prepared.");
            return false;
        }
        std::wstring name = L"Global\\Helicoid.Manager.SigningIdentity.v1." + std::wstring(sidText);
        LocalFree(sidText);
        mutex_ = CreateMutexW(nullptr, FALSE, name.c_str());
        if (mutex_ == nullptr) {
            error = QObject::tr("The Manager identity lock could not be acquired.");
            return false;
        }
        const DWORD wait = WaitForSingleObject(mutex_, 10'000);
        if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) {
            CloseHandle(mutex_);
            mutex_ = nullptr;
            error = QObject::tr("Another Manager process is updating the signing identity. Retry sign-in.");
            return false;
        }
        locked_ = true;
        return true;
    }

    ~StoreMutex()
    {
        if (locked_) ReleaseMutex(mutex_);
        if (mutex_ != nullptr) CloseHandle(mutex_);
    }

private:
    HANDLE mutex_ = nullptr;
    bool locked_ = false;
};

constexpr wchar_t credentialTarget[] = L"Helicoid.Manager.SigningIdentity.v1";

enum class StoreRead { Found, Missing, Failed };

StoreRead readSeed(std::array<std::uint8_t, seedLength>& seed, QString& error)
{
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(credentialTarget, CRED_TYPE_GENERIC, 0, &credential)) {
        if (GetLastError() == ERROR_NOT_FOUND) {
            return StoreRead::Missing;
        }
        error = QObject::tr("The Manager identity could not be read from Windows Credential Manager.");
        return StoreRead::Failed;
    }
    const bool valid = credential->CredentialBlobSize == seed.size() && credential->CredentialBlob != nullptr;
    if (valid) {
        std::memcpy(seed.data(), credential->CredentialBlob, seed.size());
    }
    CredFree(credential);
    if (!valid) {
        error = QObject::tr("The stored Manager identity is invalid and was not replaced.");
        return StoreRead::Failed;
    }
    return StoreRead::Found;
}

bool insertSeed(const std::array<std::uint8_t, seedLength>& seed, bool& inserted, QString& error)
{
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(credentialTarget);
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>(L"manager-ed25519");
    credential.CredentialBlobSize = static_cast<DWORD>(seed.size());
    credential.CredentialBlob = const_cast<LPBYTE>(seed.data());
    if (!CredWriteW(&credential, 0)) {
        error = QObject::tr("The Manager identity could not be saved in Windows Credential Manager.");
        return false;
    }
    inserted = true;
    return true;
}

bool generateSeed(std::array<std::uint8_t, seedLength>& seed)
{
    return BCryptGenRandom(nullptr, seed.data(), static_cast<ULONG>(seed.size()),
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

#else

bool loadOrCreatePlatformSeed(std::array<std::uint8_t, seedLength>&, QString& error)
{
    error = QObject::tr("Protected Manager identity storage is unavailable on this platform.");
    return false;
}

#endif

} // namespace

ManagerSigningIdentity::~ManagerSigningIdentity()
{
    secureClear(seed_.data(), seed_.size());
    loaded_ = false;
}

bool ManagerSigningIdentity::loadOrCreate(QString& error)
{
    if (loaded_) {
        return true;
    }
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
#if defined(Q_OS_WIN)
    StoreMutex storeMutex;
    if (!storeMutex.acquire(error)) return false;
#endif
    std::array<std::uint8_t, seedLength> candidate{};
    const StoreRead state = readSeed(candidate, error);
    if (state == StoreRead::Failed) {
        secureClear(candidate.data(), candidate.size());
        return false;
    }
    if (state == StoreRead::Missing) {
        if (!generateSeed(candidate)) {
            secureClear(candidate.data(), candidate.size());
            error = QObject::tr("The operating system could not generate a signing identity.");
            return false;
        }
        bool inserted = false;
        if (!insertSeed(candidate, inserted, error)) {
            secureClear(candidate.data(), candidate.size());
            return false;
        }
        if (!inserted) {
            secureClear(candidate.data(), candidate.size());
            const StoreRead winner = readSeed(candidate, error);
            if (winner != StoreRead::Found) {
                secureClear(candidate.data(), candidate.size());
                if (error.isEmpty()) {
                    error = QObject::tr("The saved Manager identity could not be re-read.");
                }
                return false;
            }
        }
    }
    std::array<std::uint8_t, publicKeyLength> publicKey{};
    const bool valid = derivePublicKey(candidate, publicKey);
    secureClear(publicKey.data(), publicKey.size());
    if (!valid) {
        secureClear(candidate.data(), candidate.size());
        error = QObject::tr("The stored Manager identity could not be validated.");
        return false;
    }
    seed_ = candidate;
    secureClear(candidate.data(), candidate.size());
    loaded_ = true;
    error.clear();
    return true;
#else
    return loadOrCreatePlatformSeed(seed_, error);
#endif
}

QByteArray ManagerSigningIdentity::publicKeyHex(QString& error) const
{
    if (!loaded_) {
        error = QObject::tr("The Manager signing identity is not loaded.");
        return {};
    }
    std::array<std::uint8_t, publicKeyLength> publicKey{};
    if (!derivePublicKey(seed_, publicKey)) {
        secureClear(publicKey.data(), publicKey.size());
        error = QObject::tr("The Manager signing identity could not derive its public key.");
        return {};
    }
    const QByteArray result(reinterpret_cast<const char*>(publicKey.data()),
                            static_cast<qsizetype>(publicKey.size()));
    secureClear(publicKey.data(), publicKey.size());
    error.clear();
    return result.toHex();
}

QByteArray ManagerSigningIdentity::sign(const QByteArray& message, QString& error) const
{
    if (!loaded_ || message.isEmpty() || static_cast<std::size_t>(message.size()) > maxMessageLength) {
        error = QObject::tr("The Manager signing identity or proof message is invalid.");
        return {};
    }
    std::array<std::uint8_t, signatureLength> signature{};
    if (hs_manager_ed25519_sign(seed_.data(), seed_.size(),
                                reinterpret_cast<const std::uint8_t*>(message.constData()),
                                static_cast<std::size_t>(message.size()),
                                signature.data(), signature.size()) != 0) {
        secureClear(signature.data(), signature.size());
        error = QObject::tr("The Manager device proof could not be created.");
        return {};
    }
    const QByteArray result(reinterpret_cast<const char*>(signature.data()),
                            static_cast<qsizetype>(signature.size()));
    secureClear(signature.data(), signature.size());
    error.clear();
    return result.toHex();
}

bool ManagerSigningIdentity::verifyEd25519Signature(
    const QByteArray& publicKeyHex,
    const QByteArray& message,
    const QByteArray& signatureHex,
    QString& error)
{
    std::array<std::uint8_t, publicKeyLength> publicKey{};
    std::array<std::uint8_t, signatureLength> signature{};
    if (message.isEmpty() || static_cast<std::size_t>(message.size()) > maxMessageLength ||
        !decodeLowercaseHex(publicKeyHex, publicKey.data(), publicKey.size()) ||
        !decodeLowercaseHex(signatureHex, signature.data(), signature.size())) {
        error = QObject::tr("The Ed25519 verification input is invalid.");
        return false;
    }

    const int result = hs_manager_ed25519_verify(
        publicKey.data(), publicKey.size(),
        reinterpret_cast<const std::uint8_t*>(message.constData()),
        static_cast<std::size_t>(message.size()),
        signature.data(), signature.size());
    secureClear(signature.data(), signature.size());
    if (result == 0) {
        error.clear();
        return true;
    }
    error = result == 1
        ? QObject::tr("The Ed25519 signature is invalid.")
        : QObject::tr("The Ed25519 signature could not be verified.");
    return false;
}

} // namespace helicoid::manager
