#include "observation_admission.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace helicoid::manager {
namespace {

constexpr char grantDomain[] = "hs-observation-grant-v1";
constexpr char channelBindingDomain[] = "HS-WEBRTC-DTLS-SCTP-V1";
constexpr char productChannelLabel[] = "hs-product";
constexpr std::size_t publicKeyBytes = 32;
constexpr std::size_t signatureBytes = 64;
constexpr std::size_t maxTokenBytes = 8 * 1024;
constexpr std::size_t maxPayloadBytes = 4 * 1024;
constexpr qint64 grantLifetimeSeconds = 10;

bool lowerHex(const QByteArray& value, std::size_t byteCount)
{
    if (value.size() != static_cast<qsizetype>(byteCount * 2U)) return false;
    return std::all_of(value.cbegin(), value.cend(), [](char character) {
        return (character >= '0' && character <= '9') ||
               (character >= 'a' && character <= 'f');
    });
}

bool isCanonicalCertificatePem(const QByteArray& value)
{
    constexpr auto begin = "-----BEGIN CERTIFICATE-----\n";
    constexpr auto end = "-----END CERTIFICATE-----\n";
    if (value.isEmpty() || value.size() > 4096 || !value.startsWith(begin) ||
        !value.endsWith(end) || value.contains('\0')) {
        return false;
    }
    const QByteArray body = value.mid(
        static_cast<qsizetype>(sizeof(begin) - 1U),
        value.size() - static_cast<qsizetype>(sizeof(begin) - 1U) -
            static_cast<qsizetype>(sizeof(end) - 1U));
    if (body.isEmpty()) return false;
    return std::all_of(body.cbegin(), body.cend(), [](unsigned char byte) {
        return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
               (byte >= '0' && byte <= '9') || byte == '+' || byte == '/' ||
               byte == '=' || byte == '\n';
    });
}

bool canonicalUuid(const QByteArray& value)
{
    if (value.size() != 36) return false;
    for (qsizetype index = 0; index < value.size(); ++index) {
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (value.at(index) != '-') return false;
        } else {
            const char character = value.at(index);
            if (!((character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f'))) {
                return false;
            }
        }
    }
    return true;
}

bool canonicalDeploymentId(const QByteArray& value)
{
    return value.size() == 36 && value.startsWith("dep_") &&
           lowerHex(value.sliced(4), 16);
}

bool jsonInteger(const QJsonObject& object, const QString& field, qint64& result)
{
    const QJsonValue value = object.value(field);
    if (!value.isDouble()) return false;
    const double raw = value.toDouble();
    constexpr double maxExactlyRepresentableInteger = 9'007'199'254'740'991.0;
    if (!std::isfinite(raw) || std::floor(raw) != raw || raw < 0 ||
        raw > maxExactlyRepresentableInteger) {
        return false;
    }
    result = static_cast<qint64>(raw);
    return true;
}

bool validChannelBinding(const QByteArray& binding)
{
    constexpr std::size_t domainBytes = sizeof(channelBindingDomain);
    constexpr std::size_t labelBytes = sizeof(productChannelLabel) - 1U;
    constexpr std::size_t expectedBytes = domainBytes + 2U + labelBytes + 2U + 64U;
    if (static_cast<std::size_t>(binding.size()) != expectedBytes ||
        !binding.startsWith(QByteArray(channelBindingDomain,
                                       static_cast<qsizetype>(domainBytes)))) {
        return false;
    }
    qsizetype offset = static_cast<qsizetype>(domainBytes);
    const auto encodedLabelBytes = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(static_cast<unsigned char>(binding.at(offset))) << 8U) |
        static_cast<std::uint16_t>(static_cast<unsigned char>(binding.at(offset + 1))));
    if (encodedLabelBytes != labelBytes) return false;
    offset += 2;
    if (binding.mid(offset, static_cast<qsizetype>(labelBytes)) !=
        QByteArray(productChannelLabel, static_cast<qsizetype>(labelBytes))) {
        return false;
    }
    offset += static_cast<qsizetype>(labelBytes);
    const auto streamId = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(static_cast<unsigned char>(binding.at(offset))) << 8U) |
        static_cast<std::uint16_t>(static_cast<unsigned char>(binding.at(offset + 1))));
    if (streamId == std::numeric_limits<std::uint16_t>::max()) return false;
    offset += 2;
    return binding.mid(offset, 32) <= binding.mid(offset + 32, 32);
}

std::optional<QByteArray> jsonString(const QJsonObject& object, const QString& field)
{
    const QJsonValue value = object.value(field);
    if (!value.isString()) return std::nullopt;
    const QByteArray result = value.toString().toLatin1();
    if (QString::fromLatin1(result) != value.toString()) return std::nullopt;
    return result;
}

} // namespace

VerifiedObservationGrant::VerifiedObservationGrant(
    QByteArray grantToken,
    QByteArray grantId,
    QByteArray deploymentId,
    QByteArray accountId,
    QByteArray managerId,
    QByteArray computerId,
    QByteArray endpointSessionId,
    QByteArray managerPublicKeyHex,
    QByteArray endpointPublicKeyHex,
    qint64 enrollmentEpoch,
    qint64 expiresAtUnix) noexcept
    : grantToken_(std::move(grantToken)),
      grantId_(std::move(grantId)),
      deploymentId_(std::move(deploymentId)),
      accountId_(std::move(accountId)),
      managerId_(std::move(managerId)),
      computerId_(std::move(computerId)),
      endpointSessionId_(std::move(endpointSessionId)),
      managerPublicKeyHex_(std::move(managerPublicKeyHex)),
      endpointPublicKeyHex_(std::move(endpointPublicKeyHex)),
      enrollmentEpoch_(enrollmentEpoch),
      expiresAtUnix_(expiresAtUnix)
{
}

VerifiedObservationGrant::VerifiedObservationGrant(VerifiedObservationGrant&& other) noexcept
    : grantToken_(std::move(other.grantToken_)),
      grantId_(std::move(other.grantId_)),
      deploymentId_(std::move(other.deploymentId_)),
      accountId_(std::move(other.accountId_)),
      managerId_(std::move(other.managerId_)),
      computerId_(std::move(other.computerId_)),
      endpointSessionId_(std::move(other.endpointSessionId_)),
      managerPublicKeyHex_(std::move(other.managerPublicKeyHex_)),
      endpointPublicKeyHex_(std::move(other.endpointPublicKeyHex_)),
      enrollmentEpoch_(std::exchange(other.enrollmentEpoch_, 0)),
      expiresAtUnix_(std::exchange(other.expiresAtUnix_, 0))
{
}

VerifiedObservationGrant& VerifiedObservationGrant::operator=(
    VerifiedObservationGrant&& other) noexcept
{
    if (this != &other) {
        clearToken();
        grantToken_ = std::move(other.grantToken_);
        grantId_ = std::move(other.grantId_);
        deploymentId_ = std::move(other.deploymentId_);
        accountId_ = std::move(other.accountId_);
        managerId_ = std::move(other.managerId_);
        computerId_ = std::move(other.computerId_);
        endpointSessionId_ = std::move(other.endpointSessionId_);
        managerPublicKeyHex_ = std::move(other.managerPublicKeyHex_);
        endpointPublicKeyHex_ = std::move(other.endpointPublicKeyHex_);
        enrollmentEpoch_ = std::exchange(other.enrollmentEpoch_, 0);
        expiresAtUnix_ = std::exchange(other.expiresAtUnix_, 0);
    }
    return *this;
}

VerifiedObservationGrant::~VerifiedObservationGrant()
{
    clearToken();
}

void VerifiedObservationGrant::clearToken() noexcept
{
    if (!grantToken_.isEmpty()) {
        grantToken_.detach();
        grantToken_.fill('\0');
        grantToken_.clear();
    }
}

std::unique_ptr<VerifiedObservationGrant> VerifiedObservationGrant::verify(
    QByteArray grantToken,
    const QByteArray& grantSigningPublicKeyHex,
    const QByteArray& expectedDeploymentId,
    const QByteArray& expectedAccountId,
    const QByteArray& expectedComputerId,
    const QByteArray& expectedManagerId,
    const QByteArray& expectedManagerPublicKeyHex,
    qint64 nowUnix,
    QString& error)
{
    const auto fail = [&grantToken, &error](const QString& detail)
        -> std::unique_ptr<VerifiedObservationGrant> {
        if (!grantToken.isEmpty()) {
            grantToken.detach();
            grantToken.fill('\0');
            grantToken.clear();
        }
        error = detail;
        return nullptr;
    };
    if (grantToken.isEmpty() || static_cast<std::size_t>(grantToken.size()) > maxTokenBytes ||
        std::any_of(grantToken.cbegin(), grantToken.cend(), [](unsigned char byte) {
            return byte == 0 || byte > 0x7fU;
        }) || !lowerHex(grantSigningPublicKeyHex, publicKeyBytes) ||
        !lowerHex(expectedManagerPublicKeyHex, publicKeyBytes) ||
        !canonicalDeploymentId(expectedDeploymentId) || !canonicalUuid(expectedAccountId) ||
        !canonicalUuid(expectedComputerId) || !canonicalUuid(expectedManagerId)) {
        return fail(QObject::tr("The observation grant context is invalid."));
    }

    const qsizetype separator = grantToken.indexOf('.');
    if (separator <= 0 || separator != grantToken.lastIndexOf('.') ||
        grantToken.size() - separator - 1 != static_cast<qsizetype>(signatureBytes * 2U)) {
        return fail(QObject::tr("The server returned an invalid observation grant."));
    }
    const QByteArray encodedPayload = grantToken.first(separator);
    const QByteArray signatureHex = grantToken.sliced(separator + 1);
    if (!lowerHex(signatureHex, signatureBytes)) {
        return fail(QObject::tr("The server returned an invalid observation grant signature."));
    }
    QByteArray payload = QByteArray::fromBase64(
        encodedPayload,
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (payload.isEmpty() || static_cast<std::size_t>(payload.size()) > maxPayloadBytes ||
        payload.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals) !=
            encodedPayload) {
        return fail(QObject::tr("The server returned a noncanonical observation grant."));
    }

    QByteArray signedPayload(grantDomain, static_cast<qsizetype>(sizeof(grantDomain)));
    signedPayload.append(payload);
    if (!ManagerSigningIdentity::verifyEd25519Signature(
            grantSigningPublicKeyHex, signedPayload, signatureHex, error)) {
        return fail(QObject::tr("The server observation grant signature could not be verified."));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(QObject::tr("The signed observation grant payload is invalid."));
    }
    const QJsonObject claims = document.object();
    const QSet<QString> expectedFields{
        QStringLiteral("version"), QStringLiteral("grant_id"), QStringLiteral("deployment_id"),
        QStringLiteral("account_id"), QStringLiteral("manager_id"),
        QStringLiteral("manager_public_key"), QStringLiteral("computer_id"),
        QStringLiteral("endpoint_public_key"), QStringLiteral("endpoint_session_id"),
        QStringLiteral("enrollment_epoch"), QStringLiteral("issued_at_unix"),
        QStringLiteral("expires_at_unix"), QStringLiteral("capability")};
    const QStringList fields = claims.keys();
    if (fields.size() != expectedFields.size() ||
        std::any_of(fields.cbegin(), fields.cend(), [&](const QString& field) {
            return !expectedFields.contains(field);
        })) {
        return fail(QObject::tr("The signed observation grant has an unsupported shape."));
    }

    const auto grantId = jsonString(claims, QStringLiteral("grant_id"));
    const auto deploymentId = jsonString(claims, QStringLiteral("deployment_id"));
    const auto accountId = jsonString(claims, QStringLiteral("account_id"));
    const auto managerId = jsonString(claims, QStringLiteral("manager_id"));
    const auto managerPublicKeyHex = jsonString(claims, QStringLiteral("manager_public_key"));
    const auto computerId = jsonString(claims, QStringLiteral("computer_id"));
    const auto endpointPublicKeyHex = jsonString(claims, QStringLiteral("endpoint_public_key"));
    const auto endpointSessionId = jsonString(claims, QStringLiteral("endpoint_session_id"));
    const QJsonValue version = claims.value(QStringLiteral("version"));
    const auto capability = jsonString(claims, QStringLiteral("capability"));
    qint64 enrollmentEpoch = 0;
    qint64 issuedAtUnix = 0;
    qint64 expiresAtUnix = 0;
    if (!grantId || !deploymentId || !accountId || !managerId || !managerPublicKeyHex ||
        !computerId || !endpointPublicKeyHex || !endpointSessionId || !capability ||
        !version.isDouble() || version.toDouble() != 1.0 ||
        !jsonInteger(claims, QStringLiteral("enrollment_epoch"), enrollmentEpoch) ||
        !jsonInteger(claims, QStringLiteral("issued_at_unix"), issuedAtUnix) ||
        !jsonInteger(claims, QStringLiteral("expires_at_unix"), expiresAtUnix) ||
        !canonicalUuid(*grantId) || !canonicalUuid(*accountId) ||
        !canonicalUuid(*endpointSessionId) || !canonicalDeploymentId(*deploymentId) ||
        !lowerHex(*managerPublicKeyHex, publicKeyBytes) ||
        !lowerHex(*endpointPublicKeyHex, publicKeyBytes) || enrollmentEpoch <= 0 ||
        *deploymentId != expectedDeploymentId || *accountId != expectedAccountId ||
        *computerId != expectedComputerId ||
        *managerId != expectedManagerId || *managerPublicKeyHex != expectedManagerPublicKeyHex ||
        *capability != QByteArrayLiteral("observe") || issuedAtUnix > nowUnix + 5 ||
        expiresAtUnix <= nowUnix || expiresAtUnix <= issuedAtUnix ||
        expiresAtUnix - issuedAtUnix > grantLifetimeSeconds) {
        return fail(QObject::tr("The signed observation grant does not match this Manager session and computer."));
    }

    error.clear();
    return std::unique_ptr<VerifiedObservationGrant>(new VerifiedObservationGrant(
        std::move(grantToken), *grantId, *deploymentId, *accountId, *managerId, *computerId,
        *endpointSessionId, *managerPublicKeyHex, *endpointPublicKeyHex,
        enrollmentEpoch, expiresAtUnix));
}

QByteArray VerifiedObservationGrant::signEndpointNonce(
    const ManagerSigningIdentity& identity,
    const QByteArray& endpointNonceHex,
    const QByteArray& liveChannelBinding,
    QString& error) const
{
    if (expiresAtUnix_ <= QDateTime::currentSecsSinceEpoch() ||
        !lowerHex(endpointNonceHex, 32) || !validChannelBinding(liveChannelBinding)) {
        error = QObject::tr("The endpoint challenge or verified WebRTC channel is no longer current.");
        return {};
    }
    const QByteArray managerPublicKeyHex = identity.publicKeyHex(error);
    if (managerPublicKeyHex.isEmpty() || managerPublicKeyHex != managerPublicKeyHex_) {
        if (error.isEmpty()) {
            error = QObject::tr("The active Manager identity does not match the signed observation grant.");
        }
        return {};
    }

    QByteArray message("hs-observation-peer-proof-v2|deployment:");
    message.append(deploymentId_);
    message.append("|grant:");
    message.append(grantId_);
    message.append("|computer:");
    message.append(computerId_);
    message.append("|endpoint_session:");
    message.append(endpointSessionId_);
    message.append("|epoch:");
    message.append(QByteArray::number(enrollmentEpoch_));
    message.append("|nonce:");
    message.append(endpointNonceHex);
    message.append("|channel_binding:");
    message.append(liveChannelBinding.toHex());
    return identity.sign(message, error);
}

QByteArray VerifiedObservationGrant::peerCertificateAttestationMessage(
    const QByteArray& certificatePem,
    const QByteArray& endpointNonceHex,
    ObservationPeerRole role,
    QString& error) const
{
    if (expiresAtUnix_ <= QDateTime::currentSecsSinceEpoch() ||
        !isCanonicalCertificatePem(certificatePem) || !lowerHex(endpointNonceHex, 32)) {
        error = QObject::tr("The WebRTC certificate attestation input is invalid.");
        return {};
    }
    const char* roleText = nullptr;
    switch (role) {
    case ObservationPeerRole::Manager: roleText = "manager"; break;
    case ObservationPeerRole::Endpoint: roleText = "endpoint"; break;
    default:
        error = QObject::tr("The WebRTC certificate attestation role is invalid.");
        return {};
    }
    const QByteArray digest = QCryptographicHash::hash(certificatePem, QCryptographicHash::Sha256).toHex();
    QByteArray message("hs-observation-peer-certificate-v1|deployment:");
    message.append(deploymentId_);
    message.append("|grant:");
    message.append(grantId_);
    message.append("|computer:");
    message.append(computerId_);
    message.append("|endpoint_session:");
    message.append(endpointSessionId_);
    message.append("|epoch:");
    message.append(QByteArray::number(enrollmentEpoch_));
    message.append("|nonce:");
    message.append(endpointNonceHex);
    message.append("|role:");
    message.append(roleText);
    message.append("|certificate_sha256:");
    message.append(digest);
    error.clear();
    return message;
}

bool VerifiedObservationGrant::verifyPeerCertificateAttestation(
    const QByteArray& certificatePem,
    const QByteArray& endpointNonceHex,
    ObservationPeerRole role,
    const QByteArray& signatureHex,
    QString& error) const
{
    const QByteArray publicKey = role == ObservationPeerRole::Manager
        ? managerPublicKeyHex_
        : role == ObservationPeerRole::Endpoint ? endpointPublicKeyHex_ : QByteArray{};
    QByteArray message = peerCertificateAttestationMessage(
        certificatePem, endpointNonceHex, role, error);
    if (message.isEmpty() || !lowerHex(signatureHex, signatureBytes)) {
        if (error.isEmpty()) {
            error = QObject::tr("The WebRTC certificate attestation signature is invalid.");
        }
        return false;
    }
    return ManagerSigningIdentity::verifyEd25519Signature(publicKey, message, signatureHex, error);
}

QByteArray VerifiedObservationGrant::signPeerCertificateAttestation(
    const ManagerSigningIdentity& identity,
    const QByteArray& certificatePem,
    const QByteArray& endpointNonceHex,
    QString& error) const
{
    const QByteArray publicKey = identity.publicKeyHex(error);
    if (publicKey.isEmpty() || publicKey != managerPublicKeyHex_) {
        if (error.isEmpty()) {
            error = QObject::tr("The active Manager identity does not match the signed observation grant.");
        }
        return {};
    }
    const QByteArray message = peerCertificateAttestationMessage(
        certificatePem, endpointNonceHex, ObservationPeerRole::Manager, error);
    if (message.isEmpty()) return {};
    return identity.sign(message, error);
}

QByteArray appendManagerCertificateAttestationToSdp(
    const QByteArray& sdp,
    const QByteArray& certificatePem,
    const QByteArray& signatureHex,
    QString& error)
{
    constexpr qsizetype maxSdpBytes = 64 * 1024;
    constexpr auto attributePrefix = "a=hs-observation-cert-v1:";
    if (sdp.isEmpty() || sdp.size() > maxSdpBytes ||
        !sdp.startsWith("v=0\r\n") ||
        !isCanonicalCertificatePem(certificatePem) ||
        !lowerHex(signatureHex, signatureBytes)) {
        error = QObject::tr("The Manager offer or certificate attestation is invalid.");
        return {};
    }

    for (qsizetype index = 0; index < sdp.size(); ++index) {
        if (sdp.at(index) == '\n' && (index == 0 || sdp.at(index - 1) != '\r')) {
            error = QObject::tr("The Manager offer has invalid line endings.");
            return {};
        }
        if (sdp.at(index) == '\r' &&
            (index + 1 >= sdp.size() || sdp.at(index + 1) != '\n')) {
            error = QObject::tr("The Manager offer has invalid line endings.");
            return {};
        }
    }

    const QByteArray marker = QByteArray("\r\n") + QByteArray(attributePrefix);
    if (sdp.contains(QByteArray(attributePrefix)) ||
        sdp.contains(marker)) {
        error = QObject::tr("The Manager offer already contains a certificate attestation.");
        return {};
    }

    const qsizetype firstMedia = sdp.indexOf("\r\nm=");
    if (firstMedia < 0) {
        error = QObject::tr("The Manager offer has no media section.");
        return {};
    }
    const QByteArray encodedCertificate = certificatePem.toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    QByteArray attribute(attributePrefix);
    attribute.append(encodedCertificate);
    attribute.append('.');
    attribute.append(signatureHex);
    attribute.append("\r\n");

    QByteArray result = sdp;
    result.insert(firstMedia + 2, attribute);
    if (result.size() > maxSdpBytes) {
        error = QObject::tr("The attested Manager offer exceeds the signaling limit.");
        return {};
    }
    error.clear();
    return result;
}

} // namespace helicoid::manager
