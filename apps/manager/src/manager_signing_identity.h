#pragma once

#include <QByteArray>
#include <QString>

#include <array>
#include <cstdint>

namespace helicoid::manager {

class ManagerSigningIdentity final {
public:
    ManagerSigningIdentity() = default;
    ManagerSigningIdentity(const ManagerSigningIdentity&) = delete;
    ManagerSigningIdentity& operator=(const ManagerSigningIdentity&) = delete;
    ~ManagerSigningIdentity();

    bool loadOrCreate(QString& error);
    QByteArray publicKeyHex(QString& error) const;
    QByteArray sign(const QByteArray& message, QString& error) const;
    static bool verifyEd25519Signature(const QByteArray& publicKeyHex,
                                       const QByteArray& message,
                                       const QByteArray& signatureHex,
                                       QString& error);

private:
    std::array<std::uint8_t, 32> seed_{};
    bool loaded_ = false;
};

} // namespace helicoid::manager
