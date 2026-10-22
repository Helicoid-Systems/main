#pragma once

#include "manager_signing_identity.h"

#include <QByteArray>
#include <QString>

#include <memory>

namespace helicoid::manager {

enum class ObservationPeerRole {
    Manager,
    Endpoint,
};

/// A deployment-signed observation lease verified against the profile's
/// separately pinned grant-signing key and the current Manager session.
class VerifiedObservationGrant final {
public:
    VerifiedObservationGrant(const VerifiedObservationGrant&) = delete;
    VerifiedObservationGrant& operator=(const VerifiedObservationGrant&) = delete;
    VerifiedObservationGrant(VerifiedObservationGrant&& other) noexcept;
    VerifiedObservationGrant& operator=(VerifiedObservationGrant&& other) noexcept;
    ~VerifiedObservationGrant();

    static std::unique_ptr<VerifiedObservationGrant> verify(
        QByteArray grantToken,
        const QByteArray& grantSigningPublicKeyHex,
        const QByteArray& expectedDeploymentId,
        const QByteArray& expectedAccountId,
        const QByteArray& expectedComputerId,
        const QByteArray& expectedManagerId,
        const QByteArray& expectedManagerPublicKeyHex,
        qint64 nowUnix,
        QString& error);

    const QByteArray& grantToken() const noexcept { return grantToken_; }
    const QByteArray& grantId() const noexcept { return grantId_; }
    const QByteArray& deploymentId() const noexcept { return deploymentId_; }
    const QByteArray& accountId() const noexcept { return accountId_; }
    const QByteArray& managerId() const noexcept { return managerId_; }
    const QByteArray& computerId() const noexcept { return computerId_; }
    const QByteArray& endpointSessionId() const noexcept { return endpointSessionId_; }
    const QByteArray& managerPublicKeyHex() const noexcept { return managerPublicKeyHex_; }
    const QByteArray& endpointPublicKeyHex() const noexcept { return endpointPublicKeyHex_; }
    qint64 enrollmentEpoch() const noexcept { return enrollmentEpoch_; }
    qint64 expiresAtUnix() const noexcept { return expiresAtUnix_; }

    QByteArray signEndpointNonce(
        const ManagerSigningIdentity& identity,
        const QByteArray& endpointNonceHex,
        const QByteArray& liveChannelBinding,
        QString& error) const;

    bool verifyPeerCertificateAttestation(
        const QByteArray& certificatePem,
        const QByteArray& endpointNonceHex,
        ObservationPeerRole role,
        const QByteArray& signatureHex,
        QString& error) const;

    QByteArray signPeerCertificateAttestation(
        const ManagerSigningIdentity& identity,
        const QByteArray& certificatePem,
        const QByteArray& endpointNonceHex,
        QString& error) const;

private:
    VerifiedObservationGrant(QByteArray grantToken,
                             QByteArray grantId,
                             QByteArray deploymentId,
                             QByteArray accountId,
                             QByteArray managerId,
                             QByteArray computerId,
                             QByteArray endpointSessionId,
                             QByteArray managerPublicKeyHex,
                             QByteArray endpointPublicKeyHex,
                             qint64 enrollmentEpoch,
                             qint64 expiresAtUnix) noexcept;
    void clearToken() noexcept;
    QByteArray peerCertificateAttestationMessage(
        const QByteArray& certificatePem,
        const QByteArray& endpointNonceHex,
        ObservationPeerRole role,
        QString& error) const;

    QByteArray grantToken_;
    QByteArray grantId_;
    QByteArray deploymentId_;
    QByteArray accountId_;
    QByteArray managerId_;
    QByteArray computerId_;
    QByteArray endpointSessionId_;
    QByteArray managerPublicKeyHex_;
    QByteArray endpointPublicKeyHex_;
    qint64 enrollmentEpoch_ = 0;
    qint64 expiresAtUnix_ = 0;
};

/// Add the Manager's signed certificate statement as one session-level SDP
/// attribute. The signature binds the exact PEM to this grant and nonce.
QByteArray appendManagerCertificateAttestationToSdp(
    const QByteArray& sdp,
    const QByteArray& certificatePem,
    const QByteArray& signatureHex,
    QString& error);

} // namespace helicoid::manager
