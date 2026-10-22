#include "main_window.h"
#include "computer_preview_metrics.h"
#include "manager_signing_identity.h"
#include "native_webrtc_runtime.h"
#include "observation_admission.h"
#include "ssh_setup_preflight.h"

#include <QAbstractSocket>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QElapsedTimer>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QFutureWatcher>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QInputDialog>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMessageBox>
#include <QMenu>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPixmap>
#include <QRect>
#include <QSettings>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSet>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSslCertificate>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrlQuery>
#include <QVariantMap>
#include <QVector>
#include <QVBoxLayout>
#include <QUuid>

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

using helicoid::manager::Availability;
using helicoid::manager::AvailabilityRequest;
using helicoid::manager::ConnectionProfile;
using helicoid::manager::ComputerPreviewMetrics;
using helicoid::manager::computerPreviewMetrics;
using helicoid::manager::DeploymentId;
using helicoid::manager::DeploymentRegistry;
using helicoid::manager::SessionStamp;
using helicoid::manager::ScopedAccessAssignmentId;
using helicoid::manager::ScopedAccountId;
using helicoid::manager::ScopedComputerId;
using helicoid::manager::ScopedGroupId;
using helicoid::manager::ScopedManagerId;
using helicoid::manager::ScopedViewerId;

namespace {

constexpr qint64 maxHealthResponseBytes = 64 * 1024;
constexpr qint64 maxAuditResponseBytes = 2 * 1024 * 1024;
constexpr qint64 maxFileJobResponseBytes = 2 * 1024 * 1024;
constexpr qint64 maxManagerFileBytes = 128 * 1024 * 1024;
constexpr qint64 managerFileChunkBytes = 256 * 1024;
constexpr std::size_t maxManagerFileJobTargets = 50;
constexpr qint64 maxProfileDocumentBytes = 1024 * 1024;
constexpr std::size_t maxMigrationPreferenceScopes = 256;
constexpr std::size_t maxMigrationComputerIds = 10000;
constexpr qint64 maxInventoryCsvBytes = 8 * 1024 * 1024;
constexpr std::size_t maxInventoryCsvRows = 10000;
constexpr int requestTimeoutMs = 5000;
constexpr int sessionExpiryCheckIntervalMs = 1000;
constexpr double maxObservationSignalsPerGrant = 128.0;

class SensitiveBuffer final {
public:
    SensitiveBuffer() = default;
    explicit SensitiveBuffer(QByteArray bytes)
        : bytes_(std::move(bytes))
    {
    }

    SensitiveBuffer(const SensitiveBuffer&) = delete;
    SensitiveBuffer& operator=(const SensitiveBuffer&) = delete;
    SensitiveBuffer(SensitiveBuffer&& other) noexcept
        : bytes_(std::move(other.bytes_))
    {
    }
    SensitiveBuffer& operator=(SensitiveBuffer&& other) noexcept
    {
        if (this != &other) {
            clear();
            bytes_ = std::move(other.bytes_);
        }
        return *this;
    }
    ~SensitiveBuffer() { clear(); }

    const QByteArray& bytes() const noexcept { return bytes_; }
    QByteArray take() noexcept { return std::move(bytes_); }

private:
    void clear() noexcept
    {
        if (!bytes_.isEmpty()) {
            bytes_.detach();
            bytes_.fill('\0');
            bytes_.clear();
        }
    }

    QByteArray bytes_;
};

struct NativeObservationPeerState final {
    std::optional<helicoid::manager::NativeWebRtcRuntime> runtime;
    std::unique_ptr<helicoid::manager::NativeWebRtcEngine> engine;
    std::unique_ptr<helicoid::manager::NativeWebRtcPeer> peer;
};

class ObservationPeerOwner final : public QObject {
public:
    explicit ObservationPeerOwner(QObject* parent)
        : QObject(parent)
    {
        setObjectName(QStringLiteral("observationPeerOwner"));
    }

    bool initialize(const QByteArray& expectedRemoteCertificatePem, QString& error)
    {
        auto loaded = helicoid::manager::NativeWebRtcRuntime::loadBundled();
        if (!loaded.runtime) {
            const int code = loaded.error ? static_cast<int>(loaded.error->code) : -1;
            error = tr("The bundled native WebRTC bridge could not be loaded (error %1).").arg(code);
            return false;
        }
        state_->runtime.emplace(std::move(*loaded.runtime));
        if (!state_->runtime->supportsObservation()) {
            error = tr("The bundled native WebRTC bridge has not enabled observation capability.");
            return false;
        }

        std::uint32_t status = HS_STATUS_INTERNAL;
        state_->engine = state_->runtime->createEngine(status);
        if (!state_->engine) {
            error = tr("Native WebRTC engine initialization failed (status %1).")
                        .arg(static_cast<quint32>(status));
            return false;
        }
        state_->peer = state_->engine->createPeer(status);
        if (!state_->peer) {
            error = tr("Native WebRTC peer creation failed (status %1).")
                        .arg(static_cast<quint32>(status));
            return false;
        }
        if ((status = state_->peer->setExpectedRemoteCertificate(
                 std::string_view(expectedRemoteCertificatePem.constData(),
                                  static_cast<std::size_t>(expectedRemoteCertificatePem.size())))
             ) != HS_STATUS_OK ||
            (status = state_->peer->prepareVideo(HS_VIDEO_ROLE_RECEIVE_ONLY)) != HS_STATUS_OK ||
            (status = state_->peer->createProductDataChannel()) != HS_STATUS_OK) {
            error = tr("The native peer could not pin the endpoint certificate and prepare the receive channel (status %1).")
                        .arg(static_cast<quint32>(status));
            return false;
        }
        error.clear();
        return true;
    }

    helicoid::manager::NativeWebRtcPeer* peer() const noexcept { return state_->peer.get(); }
    std::shared_ptr<NativeObservationPeerState> state() const noexcept { return state_; }

private:
    std::shared_ptr<NativeObservationPeerState> state_ =
        std::make_shared<NativeObservationPeerState>();
};

struct BootstrapInput final {
    QString token;
    QString username;
    QString password;
    QString friendlyName;

    ~BootstrapInput()
    {
        token.fill(QChar(u'\0'));
        password.fill(QChar(u'\0'));
        username.fill(QChar(u'\0'));
        friendlyName.fill(QChar(u'\0'));
    }
};

struct ImportedManagerPreferences final {
    // Keys are the existing opaque deployment/account scope hashes. Importing
    // them never imports a server identity pin or grants server authority.
    QJsonObject monitoringInclusion;
    bool hasComputerView = false;
    QString computerView;
    bool hasComputerPreviewSize = false;
    QString computerPreviewSize;
};

QString computerPreviewSizeName(int index)
{
    switch (index) {
    case 0:
        return QStringLiteral("compact");
    case 1:
        return QStringLiteral("standard");
    case 2:
        return QStringLiteral("large");
    case 3:
        return QStringLiteral("extra_large");
    default:
        return QStringLiteral("standard");
    }
}

int computerPreviewSizeIndex(const QString& name)
{
    if (name == QStringLiteral("small")) {
        return 0;
    }
    if (name == QStringLiteral("compact")) {
        return 0;
    }
    if (name == QStringLiteral("medium")) {
        return 1;
    }
    if (name == QStringLiteral("standard")) {
        return 1;
    }
    if (name == QStringLiteral("large")) {
        return 2;
    }
    return name == QStringLiteral("extra_large") ? 3 : -1;
}

QString availabilityText(Availability availability)
{
    switch (availability) {
    case Availability::Unknown:
        return QObject::tr("Not checked");
    case Availability::Checking:
        return QObject::tr("Checking");
    case Availability::Reachable:
        return QObject::tr("Reachable, sign in to continue");
    case Availability::Degraded:
        return QObject::tr("Reachable, setup required");
    case Availability::Unreachable:
        return QObject::tr("Unavailable");
    }
    return QObject::tr("Unknown");
}

QString formatFileSize(std::uint64_t bytes)
{
    constexpr double kibibyte = 1024.0;
    constexpr double mebibyte = 1024.0 * 1024.0;
    if (bytes < 1024) {
        return QObject::tr("%1 B").arg(static_cast<qulonglong>(bytes));
    }
    if (bytes < 1024 * 1024) {
        return QObject::tr("%1 KiB").arg(static_cast<double>(bytes) / kibibyte, 0, 'f', 1);
    }
    return QObject::tr("%1 MiB").arg(static_cast<double>(bytes) / mebibyte, 0, 'f', 1);
}

QString computerInclusionScopeKey(const DeploymentId& deployment,
                                  const std::string& accountId)
{
    QByteArray identity;
    identity.append(QByteArray::number(static_cast<qulonglong>(deployment.value.size())));
    identity.append(':');
    identity.append(deployment.value.data(), static_cast<qsizetype>(deployment.value.size()));
    identity.append(QByteArray::number(static_cast<qulonglong>(accountId.size())));
    identity.append(':');
    identity.append(accountId.data(), static_cast<qsizetype>(accountId.size()));
    return QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
}

QString observationPreviewCooldownKey(const DeploymentId& deployment,
                                      const std::string& computerId)
{
    QByteArray identity;
    identity.append(QByteArray::number(static_cast<qulonglong>(deployment.value.size())));
    identity.append(':');
    identity.append(deployment.value.data(), static_cast<qsizetype>(deployment.value.size()));
    identity.append(QByteArray::number(static_cast<qulonglong>(computerId.size())));
    identity.append(':');
    identity.append(computerId.data(), static_cast<qsizetype>(computerId.size()));
    return QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
}

void setObservationPreviewFailure(const QPointer<QLabel>& statusLabel,
                                  const QPointer<QPushButton>& requestButton,
                                  const QString& message)
{
    if (!statusLabel.isNull()) {
        statusLabel->setText(message);
        statusLabel->setProperty("observationPreviewOccupied", false);
        statusLabel->setProperty("observationPreviewImage", QVariant());
        statusLabel->setProperty("observationPreviewFrameAtMonotonicMs", QVariant());
        statusLabel->setProperty("observationPreviewExpiryMonotonicMs", QVariant());
        if (statusLabel->property("observationScreenshotControl").toBool()) {
            if (auto* saveButton = statusLabel->window()->findChild<QPushButton*>(
                    QStringLiteral("saveObservationScreenshot"))) {
                saveButton->setEnabled(false);
            }
        }
        statusLabel->setProperty("observationPreviewRetryAfterUnixMs",
                                 QDateTime::currentMSecsSinceEpoch() + 5000);
    }
    if (!requestButton.isNull()) {
        requestButton->setEnabled(true);
        if (!statusLabel.isNull() &&
            statusLabel->property("overviewPreviewTile").toBool()) {
            requestButton->setVisible(true);
        }
    }
}

qint64 monotonicMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Keep the shared Manager ceiling within the known per-endpoint admission bound until profiling supports a larger budget.
constexpr std::size_t maxConcurrentObservationPreviews = 8;
constexpr qint64 observationPreviewGrantLifetimeMs = 10000;

bool isLoopbackHost(const QString& host)
{
    if (host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    QHostAddress address;
    return address.setAddress(host) && address.isLoopback();
}

bool isControlCharacter(QChar character) noexcept
{
    return character.isNull() || character.category() == QChar::Other_Control;
}

std::optional<QUrl> normalizedEndpoint(const QString& input, QString& error)
{
    QString value = input.trimmed();
    if (value.isEmpty()) {
        error = QObject::tr("Enter a server address.");
        return std::nullopt;
    }
    if (!value.contains(QStringLiteral("://"))) {
        value.prepend(QStringLiteral("https://"));
    }

    QUrl endpoint(value, QUrl::StrictMode);
    if (!endpoint.isValid() || endpoint.isRelative() || endpoint.host().isEmpty()) {
        error = QObject::tr("Enter a valid server URL.");
        return std::nullopt;
    }
    if (!endpoint.userInfo().isEmpty() || !endpoint.query().isEmpty() || !endpoint.fragment().isEmpty()) {
        error = QObject::tr("Server URLs cannot contain credentials, a query, or a fragment.");
        return std::nullopt;
    }
    if (!endpoint.path().isEmpty() && endpoint.path() != QStringLiteral("/")) {
        error = QObject::tr("Enter the server origin without a path.");
        return std::nullopt;
    }

    const QString scheme = endpoint.scheme().toLower();
    if (scheme != QStringLiteral("https") &&
        !(scheme == QStringLiteral("http") && isLoopbackHost(endpoint.host()))) {
        error = QObject::tr("Use HTTPS. HTTP is allowed only for a loopback development server.");
        return std::nullopt;
    }

    endpoint.setScheme(scheme);
    endpoint.setPath(QString());
    endpoint.setQuery(QUrlQuery());
    endpoint.setFragment(QString());
    return endpoint;
}

bool validDisplayName(const QString& input)
{
    const QString name = input.trimmed();
    if (name.isEmpty() || name.size() > 100) {
        return false;
    }
    return std::none_of(name.cbegin(), name.cend(), [](QChar character) {
        return isControlCharacter(character);
    });
}

bool isPrivateLanAddress(const QString& input)
{
    QHostAddress address;
    if (input.contains(QLatin1Char('%')) || !address.setAddress(input) || address.isNull() ||
        address.isLoopback() || address.isMulticast()) {
        return false;
    }
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        return address.isInSubnet(QHostAddress(QStringLiteral("10.0.0.0")), 8) ||
               address.isInSubnet(QHostAddress(QStringLiteral("172.16.0.0")), 12) ||
               address.isInSubnet(QHostAddress(QStringLiteral("192.168.0.0")), 16);
    }
    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        const Q_IPV6ADDR bytes = address.toIPv6Address();
        return (bytes.c[0] & 0xfeU) == 0xfcU;
    }
    return false;
}

bool isValidClientHostname(const QString& input)
{
    if (input.isEmpty() || input.size() > 253 ||
        input.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0 ||
        input.endsWith(QStringLiteral(".localhost"), Qt::CaseInsensitive)) {
        return false;
    }
    QHostAddress address;
    if (address.setAddress(input)) {
        return false;
    }
    const auto isAsciiAlphaNumeric = [](QChar character) {
        const ushort value = character.unicode();
        return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
               (value >= '0' && value <= '9');
    };
    bool hasAsciiLetter = false;
    const QStringList labels = input.split(QLatin1Char('.'), Qt::KeepEmptyParts);
    for (const QString& label : labels) {
        if (label.isEmpty() || label.size() > 63 || !isAsciiAlphaNumeric(label.front()) ||
            !isAsciiAlphaNumeric(label.back())) {
            return false;
        }
        for (const QChar character : label) {
            const ushort value = character.unicode();
            hasAsciiLetter = hasAsciiLetter || (value >= 'a' && value <= 'z') ||
                             (value >= 'A' && value <= 'Z');
            if (!isAsciiAlphaNumeric(character) && character != QLatin1Char('-')) {
                return false;
            }
        }
    }
    return hasAsciiLetter;
}

QJsonObject profileObject(const ConnectionProfile& profile)
{
    return QJsonObject{
        {QStringLiteral("profile_id"), QString::fromStdString(profile.profileId)},
        {QStringLiteral("display_name"), QString::fromStdString(profile.displayName)},
        {QStringLiteral("endpoint"), QString::fromStdString(profile.endpoint)},
    };
}

QByteArray encodeProfiles(const std::vector<ConnectionProfile>& profiles)
{
    QJsonArray items;
    for (const auto& profile : profiles) {
        items.append(profileObject(profile));
    }
    return QJsonDocument(QJsonObject{
                             {QStringLiteral("schema_version"), 1},
                             {QStringLiteral("profiles"), items},
                         })
        .toJson(QJsonDocument::Compact);
}

bool parseProfiles(const QByteArray& data,
                   std::vector<ConnectionProfile>& profiles,
                   QString& error,
                   ImportedManagerPreferences* importedPreferences = nullptr)
{
    if (data.size() > maxProfileDocumentBytes) {
        error = QObject::tr("The profile document exceeds the allowed size.");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QObject::tr("The profile document is not valid JSON.");
        return false;
    }

    const QJsonObject root = document.object();
    const int schemaVersion = root.value(QStringLiteral("schema_version")).toInt(-1);
    const bool isProfileDocumentV1 = schemaVersion == 1 && root.size() == 2 &&
        root.contains(QStringLiteral("profiles"));
    const bool isMigrationBundleV2 = schemaVersion == 2 && root.size() == 4 &&
        root.value(QStringLiteral("format")).toString() ==
            QStringLiteral("helicoid.manager.migration") &&
        root.contains(QStringLiteral("profiles")) &&
        root.contains(QStringLiteral("preferences"));
    if ((!isProfileDocumentV1 && !isMigrationBundleV2) ||
        !root.value(QStringLiteral("schema_version")).isDouble() ||
        !root.value(QStringLiteral("profiles")).isArray()) {
        error = QObject::tr("The profile document version or structure is unsupported.");
        return false;
    }

    ImportedManagerPreferences parsedPreferences;
    if (isMigrationBundleV2) {
        const QJsonValue preferencesValue = root.value(QStringLiteral("preferences"));
        if (!preferencesValue.isObject()) {
            error = QObject::tr("The migration preferences section is invalid.");
            return false;
        }
        const QJsonObject preferences = preferencesValue.toObject();
        const bool hasComputerView = preferences.contains(QStringLiteral("computer_view"));
        const bool hasComputerPreviewSize =
            preferences.contains(QStringLiteral("computer_preview_size"));
        bool supportedPreferenceFields = preferences.contains(QStringLiteral("monitoring_inclusion")) &&
            preferences.value(QStringLiteral("monitoring_inclusion")).isObject() &&
            preferences.size() == 1 + static_cast<int>(hasComputerView) +
                static_cast<int>(hasComputerPreviewSize);
        if (hasComputerView) {
            const QJsonValue computerView = preferences.value(QStringLiteral("computer_view"));
            supportedPreferenceFields = supportedPreferenceFields && computerView.isString() &&
                (computerView.toString() == QStringLiteral("grid") ||
                 computerView.toString() == QStringLiteral("list"));
        }
        if (hasComputerPreviewSize) {
            const QJsonValue previewSize =
                preferences.value(QStringLiteral("computer_preview_size"));
            supportedPreferenceFields = supportedPreferenceFields && previewSize.isString() &&
                computerPreviewSizeIndex(previewSize.toString()) >= 0;
        }
        if (!supportedPreferenceFields) {
            error = QObject::tr("The migration preferences contain unsupported fields.");
            return false;
        }
        parsedPreferences.hasComputerView = hasComputerView;
        if (hasComputerView) {
            parsedPreferences.computerView = preferences.value(QStringLiteral("computer_view")).toString();
        }
        parsedPreferences.hasComputerPreviewSize = hasComputerPreviewSize;
        if (hasComputerPreviewSize) {
            parsedPreferences.computerPreviewSize =
                preferences.value(QStringLiteral("computer_preview_size")).toString();
        }
        const QJsonObject monitoring = preferences.value(QStringLiteral("monitoring_inclusion")).toObject();
        if (monitoring.size() > static_cast<qsizetype>(maxMigrationPreferenceScopes)) {
            error = QObject::tr("The migration contains too many scoped monitoring preferences.");
            return false;
        }
        std::size_t importedComputerIdCount = 0;
        for (auto entry = monitoring.constBegin(); entry != monitoring.constEnd(); ++entry) {
            bool validScopeHash = entry.key().size() == 64;
            for (const QChar character : entry.key()) {
                const ushort value = character.unicode();
                validScopeHash = validScopeHash &&
                    ((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'));
            }
            if (!validScopeHash || !entry.value().isObject()) {
                error = QObject::tr("A monitoring preference has an invalid scope or value.");
                return false;
            }
            const QJsonObject value = entry.value().toObject();
            const QJsonValue allComputers = value.value(QStringLiteral("all_computers"));
            const QJsonValue computerIdsValue = value.value(QStringLiteral("computer_ids"));
            const QJsonValue panelVisible = value.value(QStringLiteral("panel_visible"));
            const bool hasComputerView = value.contains(QStringLiteral("computer_view"));
            const QJsonValue computerView = value.value(QStringLiteral("computer_view"));
            const bool hasPreviewSize = value.contains(QStringLiteral("preview_size"));
            const QJsonValue previewSize = value.value(QStringLiteral("preview_size"));
            const bool hasPanelVisible = value.contains(QStringLiteral("panel_visible"));
            if (value.size() != 2 + static_cast<int>(hasPanelVisible) +
                    static_cast<int>(hasComputerView) + static_cast<int>(hasPreviewSize) ||
                !allComputers.isBool() ||
                !computerIdsValue.isArray() ||
                (hasPanelVisible && !panelVisible.isBool()) ||
                (hasComputerView && (!computerView.isString() ||
                    (computerView.toString() != QStringLiteral("grid") &&
                     computerView.toString() != QStringLiteral("list")))) ||
                (hasPreviewSize && (!previewSize.isString() ||
                    computerPreviewSizeIndex(previewSize.toString()) < 0))) {
                error = QObject::tr("A monitoring preference has missing or unsupported fields.");
                return false;
            }
            const QJsonArray computerIds = computerIdsValue.toArray();
            if ((allComputers.toBool() && !computerIds.isEmpty()) ||
                importedComputerIdCount + static_cast<std::size_t>(computerIds.size()) >
                    maxMigrationComputerIds) {
                error = QObject::tr("A monitoring preference contains an invalid or oversized computer selection.");
                return false;
            }
            QSet<QString> uniqueComputerIds;
            for (const QJsonValue& computerIdValue : computerIds) {
                if (!computerIdValue.isString()) {
                    error = QObject::tr("A monitoring preference contains an invalid computer ID.");
                    return false;
                }
                const QString computerId = computerIdValue.toString();
                if (!helicoid::manager::validScopedEntityId(computerId.toStdString()) ||
                    uniqueComputerIds.contains(computerId)) {
                    error = QObject::tr("A monitoring preference contains an invalid or repeated computer ID.");
                    return false;
                }
                uniqueComputerIds.insert(computerId);
            }
            importedComputerIdCount += static_cast<std::size_t>(computerIds.size());
            parsedPreferences.monitoringInclusion.insert(entry.key(), value);
        }
    }

    const QJsonArray items = root.value(QStringLiteral("profiles")).toArray();
    if (items.size() > static_cast<qsizetype>(DeploymentRegistry::maxProfiles)) {
        error = QObject::tr("The profile document contains too many servers.");
        return false;
    }

    QSet<QString> ids;
    std::vector<ConnectionProfile> parsed;
    parsed.reserve(static_cast<std::size_t>(items.size()));
    for (const auto& value : items) {
        if (!value.isObject()) {
            error = QObject::tr("Every profile must be an object.");
            return false;
        }
        const QJsonObject item = value.toObject();
        if (item.size() != 3 || !item.value(QStringLiteral("profile_id")).isString() ||
            !item.value(QStringLiteral("display_name")).isString() ||
            !item.value(QStringLiteral("endpoint")).isString()) {
            error = QObject::tr("A profile has missing or unsupported fields.");
            return false;
        }

        const QString rawId = item.value(QStringLiteral("profile_id")).toString();
        const QUuid uuid(rawId);
        const QString canonicalId = uuid.toString(QUuid::WithoutBraces);
        const QString displayName = item.value(QStringLiteral("display_name")).toString();
        QString endpointError;
        const auto endpoint = normalizedEndpoint(item.value(QStringLiteral("endpoint")).toString(), endpointError);
        if (uuid.isNull() || canonicalId.compare(rawId, Qt::CaseInsensitive) != 0 ||
            !validDisplayName(displayName) || !endpoint) {
            error = QObject::tr("A profile contains an invalid ID, name, or server URL.");
            return false;
        }
        if (ids.contains(canonicalId)) {
            error = QObject::tr("The profile document repeats a profile ID.");
            return false;
        }
        ids.insert(canonicalId);
        parsed.push_back(ConnectionProfile{
            canonicalId.toStdString(),
            displayName.trimmed().toStdString(),
            endpoint->toString(QUrl::FullyEncoded).toStdString(),
            Availability::Unknown,
            {},
            {},
        });
    }

    profiles = std::move(parsed);
    if (importedPreferences != nullptr) {
        *importedPreferences = std::move(parsedPreferences);
    }
    error.clear();
    return true;
}

QLabel* heading(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("workspaceHeading"));
    label->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 600;"));
    return label;
}

QLabel* subheading(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("workspaceSubheading"));
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 13px;"));
    return label;
}

QPushButton* primaryButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setDefault(false);
    button->setMinimumHeight(34);
    return button;
}

QWidget* makeWorkspaceHeader(const QString& title,
                             const QString& description,
                             QWidget* parent,
                             QVBoxLayout*& layout)
{
    auto* page = new QWidget(parent);
    layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(heading(title, page));
    layout->addWidget(subheading(description, page));
    return page;
}

QTableWidget* emptyTable(const QStringList& headers, QWidget* parent)
{
    auto* table = new QTableWidget(0, headers.size(), parent);
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    return table;
}

std::optional<QUrl> apiEndpoint(const ConnectionProfile& profile, const QString& path)
{
    QString validationError;
    const auto normalized = normalizedEndpoint(QString::fromStdString(profile.endpoint), validationError);
    if (!normalized) {
        return std::nullopt;
    }
    QUrl endpoint = *normalized;
    endpoint.setPath(path);
    endpoint.setQuery(QUrlQuery());
    endpoint.setFragment(QString());
    if (!endpoint.isValid() || endpoint.host().isEmpty()) {
        return std::nullopt;
    }
    return endpoint;
}

QNetworkRequest apiRequest(const QUrl& endpoint)
{
    QNetworkRequest request(endpoint);
    request.setTransferTimeout(requestTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    return request;
}

QString replyFailure(QNetworkReply* reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 0) {
        return QObject::tr("The server returned HTTP %1.").arg(status);
    }
    return reply->errorString();
}

bool validWireText(const QString& value, qsizetype maxLength = 128)
{
    if (value.isEmpty() || value.size() > maxLength) {
        return false;
    }
    return std::none_of(value.cbegin(), value.cend(), [](QChar character) {
        return isControlCharacter(character);
    });
}

QString csvField(QString value)
{
    qsizetype firstContent = 0;
    while (firstContent < value.size() && value.at(firstContent).isSpace()) {
        ++firstContent;
    }
    if (firstContent < value.size() &&
        QStringLiteral("=+-@").contains(value.at(firstContent))) {
        value.prepend(QLatin1Char('\''));
    }
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}

bool validAccountDisplayLabel(const QString& value)
{
    return validWireText(value, 128) && value.trimmed() == value && value.toUtf8().size() <= 128;
}

bool validRouteSegment(const QString& value)
{
    if (value.isEmpty() || value.size() > 128) {
        return false;
    }
    return std::all_of(value.cbegin(), value.cend(), [](QChar character) {
        const ushort code = character.unicode();
        return (code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z') ||
               (code >= '0' && code <= '9') || code == '-' || code == '_';
    });
}

bool validUuid(const QString& value)
{
    const QUuid parsed = QUuid::fromString(value);
    return !parsed.isNull() && parsed.toString(QUuid::WithoutBraces).toLower() == value;
}

bool validComputerGroupName(const QString& value)
{
    return validWireText(value, 128) && value.toUcs4().size() <= 64;
}

bool validBearerToken(const QString& value)
{
    if (value.size() < 32 || value.size() > 128) {
        return false;
    }
    const QByteArray token = value.toLatin1();
    if (token.size() != value.size()) {
        return false;
    }
    return std::all_of(token.cbegin(), token.cend(), [](char character) {
        return (character >= 'A' && character <= 'Z') ||
               (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') || character == '-' || character == '_';
    });
}

bool validDeploymentIdentity(const QString& value)
{
    if (value.size() != 36 || !value.startsWith(QStringLiteral("dep_"))) {
        return false;
    }
    const QByteArray suffix = value.mid(4).toLatin1();
    return suffix.size() == 32 && std::all_of(suffix.cbegin(), suffix.cend(), [](char character) {
        return (character >= '0' && character <= '9') ||
               (character >= 'a' && character <= 'f') ||
               (character >= 'A' && character <= 'F');
    });
}

struct DeploymentAuthority {
    QString deploymentId;
    QString grantSigningPublicKey;
    quint64 authorityGeneration = 0;
};

bool hasTrustedDeploymentAuthority(const ConnectionProfile& profile)
{
    return DeploymentId{profile.trustedDeploymentId}.isValid() &&
           helicoid::manager::validGrantSigningPublicKey(
               profile.trustedGrantSigningPublicKey) &&
           profile.trustedAuthorityGeneration > 0;
}

bool authorityMatchesProfile(const ConnectionProfile& profile,
                             const DeploymentAuthority& authority)
{
    return profile.trustedDeploymentId == authority.deploymentId.toStdString() &&
           profile.trustedGrantSigningPublicKey ==
               authority.grantSigningPublicKey.toStdString() &&
           profile.trustedAuthorityGeneration == authority.authorityGeneration;
}

std::optional<DeploymentAuthority> parseDeploymentAuthority(const QByteArray& body)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    const QJsonValue deploymentValue = object.value(QStringLiteral("deployment_id"));
    const QJsonValue publicKeyValue = object.value(QStringLiteral("grant_signing_public_key"));
    const QJsonValue generationValue = object.value(QStringLiteral("authority_generation"));
    if (object.size() != 3 || !deploymentValue.isString() || !publicKeyValue.isString() ||
        !generationValue.isDouble()) {
        return std::nullopt;
    }
    const QString deploymentId = deploymentValue.toString();
    const QString publicKey = publicKeyValue.toString();
    constexpr double maxExactJsonInteger = 9007199254740991.0;
    const double generationNumber = generationValue.toDouble(-1.0);
    if (!validDeploymentIdentity(deploymentId) ||
        !helicoid::manager::validGrantSigningPublicKey(publicKey.toStdString()) ||
        !std::isfinite(generationNumber) || generationNumber < 1.0 ||
        generationNumber > maxExactJsonInteger || std::floor(generationNumber) != generationNumber) {
        return std::nullopt;
    }
    return DeploymentAuthority{deploymentId, publicKey, static_cast<quint64>(generationNumber)};
}

QString grantSigningKeyFingerprint(const QString& publicKeyHex)
{
    const QByteArray publicKey = QByteArray::fromHex(publicKeyHex.toLatin1());
    return QString::fromLatin1(
        QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256).toHex());
}

class ComputerGroupAdminDialog final : public QDialog {
public:
    ComputerGroupAdminDialog(QNetworkAccessManager& network,
                             DeploymentRegistry& registry,
                             helicoid::manager::ServerSessionRegistry& sessions,
                             std::string profileId,
                             helicoid::manager::DeploymentId deployment,
                             helicoid::manager::SessionStamp stamp,
                             QWidget* parent)
        : QDialog(parent),
          network_(network),
          registry_(registry),
          sessions_(sessions),
          profileId_(std::move(profileId)),
          deployment_(std::move(deployment)),
          stamp_(std::move(stamp))
    {
        const auto profile = registry_.profile(profileId_);
        const auto* session = currentSession();
        setWindowTitle(tr("Manage Endpoint Enrollment and Groups"));
        setWindowModality(Qt::WindowModal);
        resize(820, 540);

        auto* layout = new QVBoxLayout(this);
        const QString serverName = profile ? QString::fromStdString(profile->displayName)
                                           : tr("selected server");
        auto* heading = new QLabel(tr("Groups on %1").arg(serverName), this);
        QFont headingFont = heading->font();
        headingFont.setBold(true);
        headingFont.setPointSize(headingFont.pointSize() + 2);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* identity = new QLabel(
            tr("Deployment: %1\nSigned in as: %2")
                .arg(QString::fromStdString(deployment_.value),
                     session == nullptr ? tr("unavailable") : QString::fromStdString(session->username)),
            this);
        identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(identity);

        groups_ = new QListWidget(this);
        groups_->setObjectName(QStringLiteral("computerGroupAdminList"));
        layout->addWidget(groups_, 1);

        auto* note = new QLabel(
            tr("Groups organize inventory only and do not grant access. Endpoint invitations display their code once and expire after 15 minutes. "
               "Creating invitations, approving endpoints, and revoking endpoints require MFA from the last 120 seconds. "
               "Endpoint enrollment does not open capture or viewing access."),
            this);
        note->setWordWrap(true);
        layout->addWidget(note);

        auto* actions = new QHBoxLayout;
        refresh_ = new QPushButton(tr("Refresh"), this);
        create_ = new QPushButton(tr("Create"), this);
        rename_ = new QPushButton(tr("Rename"), this);
        membership_ = new QPushButton(tr("Set Membership"), this);
        remove_ = new QPushButton(tr("Delete"), this);
        actions->addWidget(refresh_);
        actions->addStretch(1);
        actions->addWidget(create_);
        actions->addWidget(rename_);
        actions->addWidget(membership_);
        actions->addWidget(remove_);
        layout->addLayout(actions);

        auto* endpointActions = new QHBoxLayout;
        invitations_ = new QPushButton(tr("Manage Invitations"), this);
        createInvitation_ = new QPushButton(tr("Invite Endpoint"), this);
        approveEndpoint_ = new QPushButton(tr("Approve Pending"), this);
        revokeEndpoint_ = new QPushButton(tr("Revoke Endpoint"), this);
        endpointActions->addWidget(invitations_);
        endpointActions->addStretch(1);
        endpointActions->addWidget(createInvitation_);
        endpointActions->addWidget(approveEndpoint_);
        endpointActions->addWidget(revokeEndpoint_);
        layout->addLayout(endpointActions);

        auto* close = new QDialogButtonBox(QDialogButtonBox::Close, this);
        connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(close, &QDialogButtonBox::accepted, this, &QDialog::accept);
        layout->addWidget(close);
        connect(groups_, &QListWidget::currentRowChanged, this, [this](int) { updateActions(); });
        connect(refresh_, &QPushButton::clicked, this, [this] { refreshGroups(); });
        connect(create_, &QPushButton::clicked, this, [this] { createGroup(); });
        connect(rename_, &QPushButton::clicked, this, [this] { renameGroup(); });
        connect(membership_, &QPushButton::clicked, this, [this] { changeMembership(); });
        connect(remove_, &QPushButton::clicked, this, [this] { deleteGroup(); });
        connect(invitations_, &QPushButton::clicked, this, [this] { manageEndpointInvitations(); });
        connect(createInvitation_, &QPushButton::clicked, this, [this] { createEndpointInvitation(); });
        connect(approveEndpoint_, &QPushButton::clicked, this, [this] { decideEndpointEnrollment(true); });
        connect(revokeEndpoint_, &QPushButton::clicked, this, [this] { decideEndpointEnrollment(false); });

        auto* sessionTimer = new QTimer(this);
        sessionTimer->setInterval(sessionExpiryCheckIntervalMs);
        connect(sessionTimer, &QTimer::timeout, this, [this] {
            if (currentSession() == nullptr) {
                groups_->clear();
                entries_.clear();
                enrollmentComputers_.clear();
                enrollmentInventoryLoaded_ = false;
                QMessageBox::warning(this, tr("Server Session Expired"),
                                     tr("The server session expired. Enrollment and group data has been cleared."));
                reject();
                return;
            }
            updateActions();
        });
        sessionTimer->start();
        refreshGroups();
    }

    bool changed() const noexcept { return changed_; }

private:
    struct Group {
        QString id;
        QString name;
        qint64 revision = 0;
    };

    struct EnrollmentComputer {
        QString id;
        QString name;
        QString platform;
        QString state;
        qint64 epoch = 0;
    };

    struct EndpointInvitation {
        QString id;
        QString name;
        QString platform;
        QString state;
        qint64 expiresAt = 0;
    };

    const helicoid::manager::AuthenticatedSession* currentSession() const
    {
        if (!sessions_.isCurrent(stamp_)) {
            return nullptr;
        }
        const auto* session = sessions_.session(deployment_);
        if (session == nullptr || session->profileId != profileId_ ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            return nullptr;
        }
        return session;
    }

    void updateActions()
    {
        const bool hasSelection = groups_->currentItem() != nullptr;
        const auto* session = currentSession();
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const bool freshMfa = session != nullptr && session->mfaAssuredAtUnix <= now &&
            session->mfaAssuredAtUnix >= now - 120;
        refresh_->setEnabled(!busy_);
        create_->setEnabled(!busy_);
        rename_->setEnabled(!busy_ && hasSelection);
        membership_->setEnabled(!busy_ && hasSelection && enrollmentInventoryLoaded_);
        remove_->setEnabled(!busy_ && hasSelection);
        invitations_->setEnabled(!busy_ && enrollmentInventoryLoaded_);
        createInvitation_->setEnabled(!busy_ && freshMfa);
        approveEndpoint_->setEnabled(!busy_ && freshMfa && enrollmentInventoryLoaded_ &&
            std::any_of(enrollmentComputers_.begin(), enrollmentComputers_.end(),
                        [](const EnrollmentComputer& computer) { return computer.state == QStringLiteral("pending"); }));
        revokeEndpoint_->setEnabled(!busy_ && freshMfa && enrollmentInventoryLoaded_ &&
            std::any_of(enrollmentComputers_.begin(), enrollmentComputers_.end(),
                        [](const EnrollmentComputer& computer) { return computer.state != QStringLiteral("revoked"); }));
        groups_->setEnabled(!busy_);
    }

    void setBusy(bool busy)
    {
        busy_ = busy;
        updateActions();
    }

    std::optional<Group> selectedGroup() const
    {
        const auto* item = groups_->currentItem();
        if (item == nullptr) {
            return std::nullopt;
        }
        const QString id = item->data(Qt::UserRole).toString();
        const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const Group& group) {
            return group.id == id;
        });
        return found == entries_.end() ? std::nullopt : std::optional<Group>(*found);
    }

    void sendRequest(const QByteArray& method,
                     const QString& path,
                     const QJsonObject& payload,
                     int expectedStatus,
                     std::function<void(QJsonObject&)> completion)
    {
        if (busy_) {
            return;
        }
        const auto* session = currentSession();
        const auto profile = registry_.profile(profileId_);
        if (session == nullptr || !profile) {
            groups_->clear();
            entries_.clear();
            QMessageBox::warning(this, tr("Server Session Unavailable"),
                                 tr("Sign in to this server again before managing groups."));
            reject();
            return;
        }
        const auto endpoint = apiEndpoint(*profile, path);
        if (!endpoint) {
            QMessageBox::warning(this, tr("Invalid Server Address"),
                                 tr("The saved server address is invalid."));
            return;
        }

        QNetworkRequest request = apiRequest(*endpoint);
        QByteArray authorization = QByteArrayLiteral("Bearer ") +
            QByteArray::fromStdString(session->bearerToken);
        request.setRawHeader("Authorization", authorization);
        authorization.fill('\0');
        QNetworkReply* reply = nullptr;
        if (method == QByteArrayLiteral("GET")) {
            reply = network_.get(request);
        } else {
            request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
            reply = network_.sendCustomRequest(request, method, body);
            body.fill('\0');
        }
        reply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
        connect(reply, &QNetworkReply::downloadProgress, this,
                [reply](qint64 received, qint64) {
            if (received > maxHealthResponseBytes) {
                reply->abort();
            }
        });
        setBusy(true);
        connect(this, &QObject::destroyed, reply, [reply] {
            reply->abort();
            reply->deleteLater();
        });
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, expectedStatus, completion = std::move(completion)] {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            SensitiveBuffer responseBody(reply->readAll());
            const QByteArray& body = responseBody.bytes();
            reply->deleteLater();
            if (currentSession() == nullptr) {
                groups_->clear();
                entries_.clear();
                reject();
                return;
            }
            if (reply->error() != QNetworkReply::NoError || status != expectedStatus ||
                body.size() > maxHealthResponseBytes) {
                setBusy(false);
                const QString reason = status == 0 ? reply->errorString()
                                                   : tr("The server returned HTTP %1.").arg(status);
                QMessageBox::warning(this, tr("Computer Group Request Failed"), reason);
                return;
            }
            QJsonObject response;
            QJsonDocument document;
            if (expectedStatus != 204) {
                QJsonParseError parseError;
                document = QJsonDocument::fromJson(body, &parseError);
                if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                    setBusy(false);
                    QMessageBox::warning(this, tr("Invalid Server Response"),
                                         tr("The server returned an invalid computer group response."));
                    return;
                }
                response = document.object();
            }
            setBusy(false);
            completion(response);
            if (response.contains(QStringLiteral("enrollment_code"))) {
                QString code = response.value(QStringLiteral("enrollment_code")).toString();
                response.insert(QStringLiteral("enrollment_code"), QString(code.size(), QChar(u'\0')));
                code.fill(QChar(u'\0'));
                document = QJsonDocument();
            }
        });
    }

    void refreshGroups()
    {
        enrollmentInventoryLoaded_ = false;
        enrollmentComputers_.clear();
        updateActions();
        sendRequest(QByteArrayLiteral("GET"), QStringLiteral("/v2/groups"), {}, 200,
                    [this](const QJsonObject& response) {
            const QJsonValue groupsValue = response.value(QStringLiteral("groups"));
            const QJsonArray groups = groupsValue.toArray();
            if (!groupsValue.isArray() || groups.size() > 100) {
                QMessageBox::warning(this, tr("Invalid Group List"),
                                     tr("The server returned an invalid or oversized group list."));
                return;
            }
            std::vector<Group> loaded;
            QSet<QString> seenIds;
            loaded.reserve(static_cast<std::size_t>(groups.size()));
            for (const QJsonValue& value : groups) {
                if (!value.isObject()) {
                    QMessageBox::warning(this, tr("Invalid Group List"),
                                         tr("The server returned a malformed group record."));
                    return;
                }
                const QJsonObject object = value.toObject();
                const QString id = object.value(QStringLiteral("id")).toString();
                const QString name = object.value(QStringLiteral("name")).toString();
                const QJsonValue revisionValue = object.value(QStringLiteral("revision"));
                const double revision = revisionValue.toDouble(-1);
        if (!validRouteSegment(id) || !validComputerGroupName(name) ||
                    !revisionValue.isDouble() || !std::isfinite(revision) ||
                    std::floor(revision) != revision || revision < 1 ||
                    revision > 9007199254740991.0 || seenIds.contains(id)) {
                    QMessageBox::warning(this, tr("Invalid Group List"),
                                         tr("The server returned invalid or duplicate group data."));
                    return;
                }
                seenIds.insert(id);
                loaded.push_back(Group{id, name, static_cast<qint64>(revision)});
            }
            entries_ = std::move(loaded);
            groups_->clear();
            for (const Group& group : entries_) {
                auto* item = new QListWidgetItem(
                    tr("%1  ·  revision %2").arg(group.name).arg(group.revision), groups_);
                item->setData(Qt::UserRole, group.id);
                item->setToolTip(group.id);
            }
            if (!entries_.empty()) {
                groups_->setCurrentRow(0);
            }
            updateActions();
            refreshEnrollmentComputers();
        });
    }

    void refreshEnrollmentComputers()
    {
        enrollmentInventoryLoaded_ = false;
        enrollmentComputers_.clear();
        updateActions();
        sendRequest(QByteArrayLiteral("GET"), QStringLiteral("/v2/enrollment/computers"), {}, 200,
                    [this](const QJsonObject& response) {
            const QJsonValue computersValue = response.value(QStringLiteral("computers"));
            const QJsonArray computers = computersValue.toArray();
            if (!computersValue.isArray() || computers.size() > 100) {
                QMessageBox::warning(this, tr("Invalid Enrollment Inventory"),
                                     tr("The server returned an invalid or oversized enrollment inventory."));
                return;
            }
            std::vector<EnrollmentComputer> loaded;
            QSet<QString> seenIds;
            loaded.reserve(static_cast<std::size_t>(computers.size()));
            for (const QJsonValue& value : computers) {
                if (!value.isObject()) {
                    QMessageBox::warning(this, tr("Invalid Enrollment Inventory"),
                                         tr("The server returned a malformed enrollment computer record."));
                    return;
                }
                const QJsonObject computer = value.toObject();
                const QString id = computer.value(QStringLiteral("id")).toString();
                const QString name = computer.value(QStringLiteral("friendly_name")).toString();
                const QString platform = computer.value(QStringLiteral("platform")).toString();
                const QString state = computer.value(QStringLiteral("enrollment_state")).toString();
                const QJsonValue epochValue = computer.value(QStringLiteral("enrollment_epoch"));
                const double epoch = epochValue.toDouble(-1);
                if (!validRouteSegment(id) || !validWireText(name) ||
                    !validWireText(platform) || !validWireText(state) ||
                    !epochValue.isDouble() || !std::isfinite(epoch) || std::floor(epoch) != epoch ||
                    epoch < 1 || epoch > 9007199254740991.0 || seenIds.contains(id)) {
                    QMessageBox::warning(this, tr("Invalid Enrollment Inventory"),
                                         tr("The server returned invalid or duplicate enrollment computer data."));
                    return;
                }
                seenIds.insert(id);
                loaded.push_back(EnrollmentComputer{
                    id,
                    name,
                    platform,
                    state,
                    static_cast<qint64>(epoch),
                });
            }
            enrollmentComputers_ = std::move(loaded);
            enrollmentInventoryLoaded_ = true;
            updateActions();
        });
    }

    bool hasFreshMfa() const
    {
        const auto* session = currentSession();
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        return session != nullptr && session->mfaAssuredAtUnix <= now &&
            session->mfaAssuredAtUnix >= now - 120;
    }

    bool requireFreshMfa(const QString& action)
    {
        if (hasFreshMfa()) {
            return true;
        }
        QMessageBox::information(
            this, tr("Fresh MFA Required"),
            tr("Sign out of this server and sign in again, then retry %1. The server accepts MFA assurance from the last 120 seconds only.")
                .arg(action));
        return false;
    }

    void createEndpointInvitation()
    {
        if (!requireFreshMfa(tr("endpoint invitation creation"))) {
            return;
        }
        bool accepted = false;
        QString name = QInputDialog::getText(
            this, tr("Invite Endpoint"), tr("Endpoint friendly name (1–128 characters):"),
            QLineEdit::Normal, QString(), &accepted).trimmed();
        if (!accepted) {
            return;
        }
        if (!validWireText(name, 128) ||
            std::any_of(name.cbegin(), name.cend(), isControlCharacter)) {
            QMessageBox::warning(this, tr("Invalid Endpoint Name"),
                                 tr("Enter a name of 1–128 characters without control characters."));
            name.fill(QChar(u'\0'));
            return;
        }
        const QStringList platforms{QStringLiteral("windows"), QStringLiteral("macos"), QStringLiteral("linux")};
        const QString platform = QInputDialog::getItem(
            this, tr("Invite Endpoint"), tr("Endpoint platform:"), platforms, 0, false, &accepted);
        if (!accepted) {
            name.fill(QChar(u'\0'));
            return;
        }
        const auto profile = registry_.profile(profileId_);
        if (QMessageBox::question(
                this, tr("Create One-Time Invitation"),
                tr("Create a 15-minute one-time endpoint invitation for %1 (%2) on %3?")
                    .arg(name, platform, profile ? QString::fromStdString(profile->displayName)
                                                  : tr("selected server"))) != QMessageBox::Yes) {
            name.fill(QChar(u'\0'));
            return;
        }
        sendRequest(QByteArrayLiteral("POST"), QStringLiteral("/v2/enrollment/invitations"),
                    QJsonObject{
                        {QStringLiteral("friendly_name"), name},
                        {QStringLiteral("platform"), platform},
                    }, 201,
                    [this, name, platform](QJsonObject& response) mutable {
            QString code = response.value(QStringLiteral("enrollment_code")).toString();
            const QString id = response.value(QStringLiteral("id")).toString();
            const QString deploymentId = response.value(QStringLiteral("deployment_id")).toString();
            const QString returnedName = response.value(QStringLiteral("friendly_name")).toString();
            const QString returnedPlatform = response.value(QStringLiteral("platform")).toString();
            const QJsonValue createdValue = response.value(QStringLiteral("created_at_unix"));
            const QJsonValue expiresValue = response.value(QStringLiteral("expires_at_unix"));
            const double createdAt = createdValue.toDouble(-1);
            const double expiresAt = expiresValue.toDouble(-1);
            const bool validCode = validBearerToken(code);
            if (!validUuid(id) || deploymentId.toStdString() != deployment_.value ||
                returnedName != name || returnedPlatform != platform || !validCode ||
                !createdValue.isDouble() || !expiresValue.isDouble() ||
                !std::isfinite(createdAt) || !std::isfinite(expiresAt) || createdAt <= 0 ||
                std::floor(createdAt) != createdAt || std::floor(expiresAt) != expiresAt ||
                expiresAt - createdAt != 900) {
                code.fill(QChar(u'\0'));
                response.insert(QStringLiteral("enrollment_code"), QString(43, QChar(u'\0')));
                name.fill(QChar(u'\0'));
                QMessageBox::warning(this, tr("Invalid Invitation Response"),
                                     tr("The server returned invalid or mismatched invitation details."));
                return;
            }

            QDialog codeDialog(this);
            codeDialog.setWindowTitle(tr("One-Time Endpoint Enrollment Code"));
            codeDialog.setMinimumWidth(540);
            auto* layout = new QVBoxLayout(&codeDialog);
            auto* details = new QLabel(
                tr("Invitation %1 for %2 (%3) on deployment %4\nExpires at %5. Show this code to the endpoint setup flow now; it will not be listed again.")
                    .arg(id, name, platform, QString::fromStdString(deployment_.value))
                    .arg(static_cast<qint64>(expiresAt)),
                &codeDialog);
            details->setWordWrap(true);
            details->setTextInteractionFlags(Qt::TextSelectableByMouse);
            layout->addWidget(details);
            auto* codeLabel = new QLabel(code, &codeDialog);
            codeLabel->setObjectName(QStringLiteral("oneTimeEndpointEnrollmentCode"));
            codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            QFont codeFont = codeLabel->font();
            codeFont.setFamily(QStringLiteral("monospace"));
            codeFont.setPointSize(codeFont.pointSize() + 2);
            codeLabel->setFont(codeFont);
            layout->addWidget(codeLabel);
            auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &codeDialog);
            connect(buttons, &QDialogButtonBox::rejected, &codeDialog, &QDialog::reject);
            connect(buttons, &QDialogButtonBox::accepted, &codeDialog, &QDialog::accept);
            layout->addWidget(buttons);
            codeDialog.exec();
            codeLabel->clear();
            code.fill(QChar(u'\0'));
            response.insert(QStringLiteral("enrollment_code"), QString(43, QChar(u'\0')));
            name.fill(QChar(u'\0'));
            changed_ = true;
            refreshGroups();
        });
        name.fill(QChar(u'\0'));
    }

    void manageEndpointInvitations()
    {
        sendRequest(QByteArrayLiteral("GET"), QStringLiteral("/v2/enrollment/invitations"), {}, 200,
                    [this](const QJsonObject& response) {
            const QJsonValue listValue = response.value(QStringLiteral("invitations"));
            const QJsonArray values = listValue.toArray();
            const QJsonValue maxPageValue = response.value(QStringLiteral("max_page_size"));
            const double maxPageSize = maxPageValue.toDouble(-1);
            if (!listValue.isArray() || values.size() > 100 || maxPageSize < values.size() ||
                !maxPageValue.isDouble() || !std::isfinite(maxPageSize) ||
                std::floor(maxPageSize) != maxPageSize || maxPageSize < 1 || maxPageSize > 100) {
                QMessageBox::warning(this, tr("Invalid Invitation List"),
                                     tr("The server returned an invalid or oversized invitation list."));
                return;
            }
            std::vector<EndpointInvitation> invitations;
            QStringList choices;
            QSet<QString> seen;
            for (const QJsonValue& value : values) {
                if (!value.isObject()) {
                    QMessageBox::warning(this, tr("Invalid Invitation List"),
                                         tr("The server returned a malformed invitation record."));
                    return;
                }
                const QJsonObject object = value.toObject();
                const double expiresAt = object.value(QStringLiteral("expires_at_unix")).toDouble(-1);
                EndpointInvitation invitation{
                    object.value(QStringLiteral("id")).toString(),
                    object.value(QStringLiteral("friendly_name")).toString(),
                    object.value(QStringLiteral("platform")).toString(),
                    object.value(QStringLiteral("state")).toString(),
                    static_cast<qint64>(expiresAt),
                };
                if (!validUuid(invitation.id) || !validWireText(invitation.name) ||
                    !QStringList{QStringLiteral("windows"), QStringLiteral("macos"), QStringLiteral("linux")}.contains(invitation.platform) ||
                    !QStringList{QStringLiteral("pending"), QStringLiteral("expired"), QStringLiteral("redeemed"), QStringLiteral("revoked")}.contains(invitation.state) ||
                    !std::isfinite(expiresAt) || std::floor(expiresAt) != expiresAt ||
                    invitation.expiresAt <= 0 || seen.contains(invitation.id)) {
                    QMessageBox::warning(this, tr("Invalid Invitation List"),
                                         tr("The server returned invalid or duplicate invitation data."));
                    return;
                }
                seen.insert(invitation.id);
                invitations.push_back(invitation);
                choices.append(tr("%1 (%2) — %3 — %4")
                                   .arg(invitation.name, invitation.platform, invitation.state, invitation.id));
            }
            if (choices.isEmpty()) {
                QMessageBox::information(this, tr("Endpoint Invitations"),
                                         tr("No endpoint invitations are recorded. Invitation codes are never listed."));
                return;
            }
            bool accepted = false;
            const QString selected = QInputDialog::getItem(
                this, tr("Endpoint Invitations"),
                tr("Choose an invitation to inspect or revoke. Codes are never returned by this list:"),
                choices, 0, false, &accepted);
            if (!accepted) {
                return;
            }
            const qsizetype index = choices.indexOf(selected);
            if (index < 0 || index >= static_cast<qsizetype>(invitations.size())) {
                return;
            }
            const EndpointInvitation invitation = invitations[static_cast<std::size_t>(index)];
            if (invitation.state != QStringLiteral("pending") && invitation.state != QStringLiteral("expired")) {
                QMessageBox::information(this, tr("Invitation Details"),
                                         tr("Invitation %1 is %2. Its secret code is not recoverable.")
                                             .arg(invitation.id, invitation.state));
                return;
            }
            if (!requireFreshMfa(tr("invitation revocation"))) {
                return;
            }
            const QString confirmation = QInputDialog::getText(
                this, tr("Revoke Endpoint Invitation"),
                tr("Type the exact invitation ID to invalidate its code:\n%1").arg(invitation.id),
                QLineEdit::Normal, QString(), &accepted).trimmed();
            if (!accepted || confirmation != invitation.id) {
                QMessageBox::information(this, tr("Invitation Not Revoked"),
                                         tr("The exact invitation ID was not entered."));
                return;
            }
            sendRequest(QByteArrayLiteral("POST"),
                        QStringLiteral("/v2/enrollment/invitations/%1/revoke").arg(invitation.id),
                        QJsonObject{{QStringLiteral("confirm_invitation_id"), invitation.id}},
                        200,
                        [this, invitation](const QJsonObject& revoked) {
                if (revoked.value(QStringLiteral("id")).toString() != invitation.id ||
                    revoked.value(QStringLiteral("state")).toString() != QStringLiteral("revoked")) {
                    QMessageBox::warning(this, tr("Invalid Revocation Response"),
                                         tr("The server did not confirm invitation revocation."));
                    return;
                }
                changed_ = true;
                refreshGroups();
            });
        });
    }

    void decideEndpointEnrollment(bool approve)
    {
        if (!requireFreshMfa(approve ? tr("endpoint approval") : tr("endpoint revocation"))) {
            return;
        }
        std::vector<EnrollmentComputer> candidates;
        QStringList choices;
        for (const EnrollmentComputer& computer : enrollmentComputers_) {
            const bool eligible = approve ? computer.state == QStringLiteral("pending")
                                          : computer.state != QStringLiteral("revoked");
            if (!eligible) {
                continue;
            }
            candidates.push_back(computer);
            choices.append(tr("%1 (%2) — %3 — epoch %4")
                               .arg(computer.name, computer.platform, computer.id)
                               .arg(computer.epoch));
        }
        if (choices.isEmpty()) {
            QMessageBox::information(this, tr("Endpoint Enrollment"),
                                     approve ? tr("No proof-backed pending endpoints are available for approval.")
                                             : tr("No non-revoked endpoint is available."));
            return;
        }
        bool accepted = false;
        const QString selected = QInputDialog::getItem(
            this, approve ? tr("Approve Endpoint") : tr("Revoke Endpoint"),
            tr("Computer on %1:").arg(QString::fromStdString(deployment_.value)),
            choices, 0, false, &accepted);
        if (!accepted) {
            return;
        }
        const qsizetype index = choices.indexOf(selected);
        if (index < 0 || index >= static_cast<qsizetype>(candidates.size())) {
            return;
        }
        const EnrollmentComputer computer = candidates[static_cast<std::size_t>(index)];
        const QString action = approve ? QStringLiteral("approve") : QStringLiteral("revoke");
        const QString confirmation = QInputDialog::getText(
            this, approve ? tr("Confirm Endpoint Approval") : tr("Confirm Endpoint Revocation"),
            tr("Type the exact computer ID to %1 %2 at enrollment epoch %3:")
                .arg(action, computer.id).arg(computer.epoch),
            QLineEdit::Normal, QString(), &accepted).trimmed();
        if (!accepted || confirmation != computer.id) {
            QMessageBox::information(this, tr("Endpoint Not Changed"),
                                     tr("The exact computer ID was not entered."));
            return;
        }
        const QString route = QStringLiteral("/v2/enrollment/computers/%1/%2")
                                  .arg(computer.id, action);
        sendRequest(QByteArrayLiteral("POST"), route,
                    QJsonObject{
                        {QStringLiteral("expected_enrollment_epoch"), computer.epoch},
                        {QStringLiteral("confirm_computer_id"), computer.id},
                    }, 200,
                    [this, computer, approve](const QJsonObject& updated) {
            const QString state = approve ? QStringLiteral("enrolled") : QStringLiteral("revoked");
            const double epoch = updated.value(QStringLiteral("enrollment_epoch")).toDouble(-1);
            if (updated.value(QStringLiteral("id")).toString() != computer.id ||
                updated.value(QStringLiteral("enrollment_state")).toString() != state ||
                epoch != static_cast<double>(computer.epoch + 1)) {
                QMessageBox::warning(this, tr("Invalid Enrollment Response"),
                                     tr("The server did not confirm the requested endpoint decision."));
                return;
            }
            changed_ = true;
            refreshGroups();
        });
    }

    void createGroup()
    {
        bool accepted = false;
        const QString name = QInputDialog::getText(this, tr("Create Computer Group"),
                                                   tr("Group name (1–64 characters):"),
                                                   QLineEdit::Normal, QString(), &accepted).trimmed();
        if (!accepted) {
            return;
        }
        if (!validComputerGroupName(name)) {
            QMessageBox::warning(this, tr("Invalid Group Name"),
                                 tr("Enter a non-empty group name of at most 64 characters without control characters."));
            return;
        }
        if (QMessageBox::question(this, tr("Create Computer Group"),
                                  tr("Create the group %1? This does not grant computer access.").arg(name)) !=
            QMessageBox::Yes) {
            return;
        }
        sendRequest(QByteArrayLiteral("POST"), QStringLiteral("/v2/groups"),
                    QJsonObject{{QStringLiteral("name"), name}}, 201,
                    [this, name](const QJsonObject& response) {
            if (response.value(QStringLiteral("name")).toString() != name ||
                !validRouteSegment(response.value(QStringLiteral("id")).toString()) ||
                response.value(QStringLiteral("revision")).toInt(-1) != 1) {
                QMessageBox::warning(this, tr("Invalid Group Response"),
                                     tr("The server did not confirm the new group."));
                return;
            }
            changed_ = true;
            refreshGroups();
        });
    }

    void renameGroup()
    {
        const auto group = selectedGroup();
        if (!group) {
            return;
        }
        bool accepted = false;
        const QString name = QInputDialog::getText(this, tr("Rename Computer Group"),
                                                   tr("New group name (1–64 characters):"),
                                                   QLineEdit::Normal, group->name, &accepted).trimmed();
        if (!accepted) {
            return;
        }
        if (!validComputerGroupName(name)) {
            QMessageBox::warning(this, tr("Invalid Group Name"),
                                 tr("Enter a non-empty group name of at most 64 characters without control characters."));
            return;
        }
        if (QMessageBox::question(this, tr("Rename Computer Group"),
                                  tr("Rename %1 to %2?").arg(group->name, name)) != QMessageBox::Yes) {
            return;
        }
        sendRequest(QByteArrayLiteral("PUT"), QStringLiteral("/v2/groups/%1").arg(group->id),
                    QJsonObject{
                        {QStringLiteral("expected_revision"), group->revision},
                        {QStringLiteral("name"), name},
                    }, 200,
                    [this, group = *group, name](const QJsonObject& response) {
            if (response.value(QStringLiteral("id")).toString() != group.id ||
                response.value(QStringLiteral("name")).toString() != name ||
                response.value(QStringLiteral("revision")).toDouble(-1) < group.revision) {
                QMessageBox::warning(this, tr("Invalid Group Response"),
                                     tr("The server did not confirm the requested rename."));
                return;
            }
            changed_ = true;
            refreshGroups();
        });
    }

    void changeMembership()
    {
        const auto group = selectedGroup();
        if (!group) {
            return;
        }
        bool accepted = false;
        QStringList computerChoices;
        for (const EnrollmentComputer& computer : enrollmentComputers_) {
            computerChoices.append(tr("%1 (%2)").arg(computer.name, computer.id));
        }
        const QString manualEntry = tr("Enter another computer ID…");
        computerChoices.append(manualEntry);
        QString computerId;
        const QString selectedComputer = QInputDialog::getItem(
            this, tr("Set Group Membership"), tr("Computer on this server:"),
            computerChoices, 0, false, &accepted);
        if (!accepted) {
            return;
        }
        if (selectedComputer == manualEntry) {
            computerId = QInputDialog::getText(
                this, tr("Set Group Membership"), tr("Computer ID on this server:"),
                QLineEdit::Normal, QString(), &accepted).trimmed();
            if (!accepted) {
                return;
            }
        } else {
            const qsizetype selectedIndex = computerChoices.indexOf(selectedComputer);
            if (selectedIndex < 0 || selectedIndex >= static_cast<qsizetype>(enrollmentComputers_.size())) {
                return;
            }
            computerId = enrollmentComputers_[static_cast<std::size_t>(selectedIndex)].id;
        }
        if (!validRouteSegment(computerId)) {
            QMessageBox::warning(this, tr("Invalid Computer ID"),
                                 tr("Enter a valid computer ID from this server."));
            return;
        }
        const QStringList choices{tr("Add to group"), tr("Remove from group")};
        const QString choice = QInputDialog::getItem(this, tr("Set Group Membership"),
                                                     tr("Membership action:"), choices, 0, false, &accepted);
        if (!accepted) {
            return;
        }
        const bool included = choice == choices.first();
        const QString action = included ? tr("Add") : tr("Remove");
        if (QMessageBox::question(
                this, tr("Confirm Group Membership"),
                tr("%1 computer %2 %3? This changes organization labels only and does not grant access.")
                    .arg(action, computerId, included ? tr("to %1").arg(group->name)
                                                       : tr("from %1").arg(group->name))) != QMessageBox::Yes) {
            return;
        }
        sendRequest(QByteArrayLiteral("PUT"),
                    QStringLiteral("/v2/groups/%1/computers/%2").arg(group->id, computerId),
                    QJsonObject{
                        {QStringLiteral("expected_revision"), group->revision},
                        {QStringLiteral("included"), included},
                    }, 200,
                    [this, group = *group](const QJsonObject& response) {
            if (response.value(QStringLiteral("id")).toString() != group.id ||
                response.value(QStringLiteral("revision")).toDouble(-1) < group.revision) {
                QMessageBox::warning(this, tr("Invalid Group Response"),
                                     tr("The server did not confirm the membership change."));
                return;
            }
            changed_ = true;
            refreshGroups();
        });
    }

    void deleteGroup()
    {
        const auto group = selectedGroup();
        if (!group) {
            return;
        }
        const QString confirmation = QInputDialog::getText(
            this, tr("Delete Computer Group"),
            tr("Type the exact group ID to delete %1 and remove its labels:\n%2")
                .arg(group->name, group->id)).trimmed();
        if (confirmation != group->id) {
            QMessageBox::information(this, tr("Group Not Deleted"),
                                     tr("The exact group ID was not entered."));
            return;
        }
        sendRequest(QByteArrayLiteral("DELETE"), QStringLiteral("/v2/groups/%1").arg(group->id),
                    QJsonObject{
                        {QStringLiteral("expected_revision"), group->revision},
                        {QStringLiteral("confirm_group_id"), group->id},
                    }, 204,
                    [this](const QJsonObject&) {
            changed_ = true;
            refreshGroups();
        });
    }

    QNetworkAccessManager& network_;
    DeploymentRegistry& registry_;
    helicoid::manager::ServerSessionRegistry& sessions_;
    std::string profileId_;
    helicoid::manager::DeploymentId deployment_;
    helicoid::manager::SessionStamp stamp_;
    std::vector<EnrollmentComputer> enrollmentComputers_;
    bool enrollmentInventoryLoaded_ = false;
    QListWidget* groups_ = nullptr;
    QPushButton* refresh_ = nullptr;
    QPushButton* create_ = nullptr;
    QPushButton* rename_ = nullptr;
    QPushButton* membership_ = nullptr;
    QPushButton* remove_ = nullptr;
    QPushButton* invitations_ = nullptr;
    QPushButton* createInvitation_ = nullptr;
    QPushButton* approveEndpoint_ = nullptr;
    QPushButton* revokeEndpoint_ = nullptr;
    std::vector<Group> entries_;
    bool busy_ = false;
    bool changed_ = false;
};

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : SARibbonMainWindow(parent)
{
    setWindowTitle(tr("Helicoid Manager"));
    setMinimumSize(720, 480);
    resize(1120, 760);

    workspaceStack_ = new QStackedWidget(this);
    workspaceStack_->addWidget(createComputersWorkspace());
    workspaceStack_->addWidget(createServersWorkspace());
    workspaceStack_->addWidget(createAccountsWorkspace());
    workspaceStack_->addWidget(createFilesWorkspace());
    workspaceStack_->addWidget(createSetupWorkspace());
    setCentralWidget(workspaceStack_);

    createRibbon();
    QSettings settings;
    const QString savedComputerView = settings.value(QStringLiteral("computerViewV1"),
                                                     QStringLiteral("grid")).toString();
    const int computerViewIndex = savedComputerView == QStringLiteral("list") ? 1 : 0;
    if (computerLayoutStack_ != nullptr) {
        computerLayoutStack_->setCurrentIndex(computerViewIndex);
    }
    if (auto* gridAction = findChild<QAction*>(QStringLiteral("actionComputerGrid"))) {
        gridAction->setChecked(computerViewIndex == 0);
    }
    if (auto* listAction = findChild<QAction*>(QStringLiteral("actionComputerList"))) {
        listAction->setChecked(computerViewIndex == 1);
    }
    const QString savedPreviewSize = settings.value(
        QStringLiteral("computerPreviewSizeV1"), QStringLiteral("standard")).toString();
    const int savedPreviewSizeIndex = computerPreviewSizeIndex(savedPreviewSize);
    applyComputerPreviewSize(savedPreviewSizeIndex >= 0 ? savedPreviewSizeIndex : 1, false);
    loadProfiles();
    refreshServerTable();
    sessionExpiryTimer_ = new QTimer(this);
    sessionExpiryTimer_->setInterval(sessionExpiryCheckIntervalMs);
    connect(sessionExpiryTimer_, &QTimer::timeout, this, &MainWindow::expireSessions);
    sessionExpiryTimer_->start();
    overviewPreviewTimer_ = new QTimer(this);
    overviewPreviewTimer_->setInterval(500);
    connect(overviewPreviewTimer_, &QTimer::timeout,
            this, &MainWindow::scheduleOverviewPreviews);
    overviewPreviewTimer_->start();
    statusBar()->showMessage(profileLoadWarning_.isEmpty()
                                 ? tr("No server session is active.")
                                 : profileLoadWarning_);
}

MainWindow::~MainWindow()
{
    registry_.disconnectAll();
    closeAllViewers();
    sessions_.invalidateAll();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (shutdownComplete_) {
        event->accept();
        return;
    }
    if (shutdownInProgress_) {
        event->ignore();
        return;
    }

    struct PendingLogout final {
        ConnectionProfile profile;
        helicoid::manager::DeploymentId deployment;
        SensitiveBuffer token;
    };
    std::vector<PendingLogout> pendingLogouts;
    std::unordered_set<std::string> seenDeployments;
    for (const auto& profileDeployment : deploymentsByProfile_) {
        const auto& deployment = profileDeployment.second;
        if (!seenDeployments.insert(deployment.value).second) {
            continue;
        }
        const auto* session = sessions_.session(deployment);
        if (session == nullptr || session->bearerToken.empty()) {
            continue;
        }
        const auto profile = registry_.profile(session->profileId);
        if (!profile) {
            continue;
        }
        pendingLogouts.push_back(PendingLogout{
            *profile,
            deployment,
            SensitiveBuffer(QByteArray::fromStdString(session->bearerToken)),
        });
    }

    shutdownInProgress_ = true;
    shutdownComplete_ = false;
    pendingShutdownLogoutRequests_ = pendingLogouts.size();
    if (sessionExpiryTimer_ != nullptr) {
        sessionExpiryTimer_->stop();
    }
    registry_.disconnectAll();
    closeAllViewers();
    sessions_.invalidateAll();
    deploymentsByProfile_.clear();
    signInsInProgress_.clear();
    authenticationProfileByDeployment_.clear();
    computers_.clear();
    computerRequestGenerations_.clear();
    observationScopeRequestGenerations_.clear();
    observationScopePending_.clear();
    observationScopes_.clear();
    accounts_.clear();
    managers_.clear();
    accessAssignments_.clear();
    accountDataErrors_.clear();
    accountRequestGenerations_.clear();
    accountDataPending_.clear();
    refreshComputerTable();
    refreshAccountTables();
    refreshServerTable();

    for (QNetworkReply* reply : network_.findChildren<QNetworkReply*>()) {
        if (reply->isFinished()) {
            continue;
        }
        ++pendingShutdownLogoutRequests_;
        connect(reply, &QNetworkReply::finished, this, [this] {
            QTimer::singleShot(0, this, [this] { completeShutdownOperation(); });
        });
        reply->abort();
    }

    if (pendingShutdownLogoutRequests_ == 0) {
        shutdownComplete_ = true;
        event->accept();
        return;
    }

    event->ignore();
    statusBar()->showMessage(tr("Signing out and waiting for the server responses before closing."));
    setEnabled(false);
    for (PendingLogout& logout : pendingLogouts) {
        sendLogoutRequestForVerifiedProfile(
            logout.profile, logout.deployment, logout.token.take(), false,
            [this] { QTimer::singleShot(0, this, [this] { completeShutdownOperation(); }); });
    }
}

void MainWindow::completeShutdownOperation()
{
    if (shutdownComplete_) {
        return;
    }
    if (pendingShutdownLogoutRequests_ > 0) {
        --pendingShutdownLogoutRequests_;
    }
    if (shutdownInProgress_ && pendingShutdownLogoutRequests_ == 0) {
        shutdownComplete_ = true;
        close();
    }
}

void MainWindow::createRibbon()
{
    auto* bar = ribbonBar();

    auto* computers = bar->addCategoryPage(tr("Computers"));
    computers->setObjectName(QStringLiteral("computersCategory"));
    auto* viewPanel = computers->addPanel(tr("View"));
    auto* viewGroup = new QActionGroup(this);
    viewGroup->setExclusive(true);
    auto* gridAction = new QAction(tr("Grid"), this);
    gridAction->setObjectName(QStringLiteral("actionComputerGrid"));
    gridAction->setCheckable(true);
    gridAction->setChecked(true);
    auto* listAction = new QAction(tr("List"), this);
    listAction->setObjectName(QStringLiteral("actionComputerList"));
    listAction->setCheckable(true);
    viewGroup->addAction(gridAction);
    viewGroup->addAction(listAction);
    viewPanel->addMediumAction(gridAction);
    viewPanel->addMediumAction(listAction);

    auto* previewSizePanel = computers->addPanel(tr("Preview Size"));
    auto* previewSizeGroup = new QActionGroup(this);
    previewSizeGroup->setExclusive(true);
    auto makePreviewSizeAction = [this, previewSizeGroup, previewSizePanel](
                                     const QString& label,
                                     const QString& objectName,
                                     const QString& toolTip) {
        auto* action = new QAction(label, this);
        action->setObjectName(objectName);
        action->setCheckable(true);
        action->setToolTip(toolTip);
        previewSizeGroup->addAction(action);
        previewSizePanel->addMediumAction(action);
        return action;
    };
    auto* compactPreviewAction = makePreviewSizeAction(
        tr("Compact 192"), QStringLiteral("actionPreviewSizeCompact"),
        tr("Use compact 192-pixel computer preview cards."));
    auto* standardPreviewAction = makePreviewSizeAction(
        tr("Standard 256"), QStringLiteral("actionPreviewSizeStandard"),
        tr("Use standard 256-pixel computer preview cards."));
    auto* largePreviewAction = makePreviewSizeAction(
        tr("Large 320"), QStringLiteral("actionPreviewSizeLarge"),
        tr("Use large 320-pixel computer preview cards."));
    auto* extraLargePreviewAction = makePreviewSizeAction(
        tr("Extra Large 448"), QStringLiteral("actionPreviewSizeExtraLarge"),
        tr("Use extra large 448-pixel computer preview cards."));
    standardPreviewAction->setChecked(true);

    auto* viewMenu = menuBar()->addMenu(tr("View"));
    viewMenu->addAction(gridAction);
    viewMenu->addAction(listAction);
    viewMenu->addSeparator();
    auto* previewSizeMenu = viewMenu->addMenu(tr("Preview Size"));
    previewSizeMenu->addAction(compactPreviewAction);
    previewSizeMenu->addAction(standardPreviewAction);
    previewSizeMenu->addAction(largePreviewAction);
    previewSizeMenu->addAction(extraLargePreviewAction);

    auto* computerActions = computers->addPanel(tr("Server"));
    auto* addComputerServer = new QAction(tr("Add Server"), this);
    addComputerServer->setObjectName(QStringLiteral("actionAddServerComputers"));
    computerActions->addLargeAction(addComputerServer);
    auto* setupComputerServer = new QAction(tr("Set Up Server"), this);
    setupComputerServer->setObjectName(QStringLiteral("actionSetupServerComputers"));
    computerActions->addLargeAction(setupComputerServer);
    auto* refreshInventory = new QAction(tr("Refresh Inventory"), this);
    refreshInventory->setObjectName(QStringLiteral("actionRefreshComputerInventory"));
    refreshInventory->setToolTip(tr("Reload authorized computer metadata from each signed-in server."));
    computerActions->addMediumAction(refreshInventory);
    auto* groupActions = computers->addPanel(tr("Organization"));
        auto* manageGroups = new QAction(tr("Enrollment and Groups"), this);
    manageGroups->setObjectName(QStringLiteral("actionManageComputerGroups"));
        manageGroups->setToolTip(tr("Issue one-time endpoint invitations, approve or revoke endpoints, and organize computers with labels."));
    groupActions->addLargeAction(manageGroups);

    auto* monitoring = computers->addPanel(tr("Monitoring"));
    openViewAction_ = new QAction(tr("Open View Screen"), this);
    openViewAction_->setObjectName(QStringLiteral("actionOpenComputerViewer"));
    openViewAction_->setToolTip(tr("Open a separate view-only window. Live preview requires a server-issued grant and a native runtime with observation support."));
    monitoring->addLargeAction(openViewAction_);
    connect(openViewAction_, &QAction::triggered, this, &MainWindow::openSelectedComputerViewer);
    auto* takeControl = new QAction(tr("Take Control"), this);
    takeControl->setObjectName(QStringLiteral("actionTakeControl"));
    takeControl->setEnabled(false);
    takeControl->setToolTip(tr("Control requires endpoint admission and acknowledgment."));
    monitoring->addLargeAction(takeControl);

    auto* servers = bar->addCategoryPage(tr("Server"));
    servers->setObjectName(QStringLiteral("serverCategory"));
    auto* deployment = servers->addPanel(tr("Deployment"));
    auto* addServer = new QAction(tr("Add Server"), this);
    addServer->setObjectName(QStringLiteral("actionAddServer"));
    deployment->addLargeAction(addServer);
    auto* setupServer = new QAction(tr("Set Up Server"), this);
    setupServer->setObjectName(QStringLiteral("actionSetupServer"));
    deployment->addLargeAction(setupServer);
    auto* checkServer = new QAction(tr("Check Availability"), this);
    checkServer->setObjectName(QStringLiteral("actionCheckServer"));
    deployment->addLargeAction(checkServer);
    auto* verifyServer = new QAction(tr("Verify Server Identity"), this);
    verifyServer->setObjectName(QStringLiteral("actionVerifyServerIdentity"));
    verifyServer->setToolTip(tr("Compare the server deployment ID with a value obtained through your organization's trusted channel."));
    deployment->addLargeAction(verifyServer);
    auto* signInServer = new QAction(tr("Sign In"), this);
    signInServer->setObjectName(QStringLiteral("actionSignInServer"));
    deployment->addLargeAction(signInServer);
    auto* signOutServer = new QAction(tr("Sign Out"), this);
    signOutServer->setObjectName(QStringLiteral("actionSignOutServer"));
    deployment->addLargeAction(signOutServer);
    auto* bootstrapServer = new QAction(tr("Initialize Administrator"), this);
    bootstrapServer->setObjectName(QStringLiteral("actionBootstrapServer"));
    bootstrapServer->setToolTip(tr("Create the first administrator and bind this Manager device to the server."));
    deployment->addLargeAction(bootstrapServer);
    auto* confirmBootstrap = new QAction(tr("Confirm MFA Enrollment"), this);
    confirmBootstrap->setObjectName(QStringLiteral("actionConfirmBootstrapMfa"));
    confirmBootstrap->setToolTip(tr("Activate an account after authenticator and recovery setup."));
    deployment->addLargeAction(confirmBootstrap);
    auto* forgetServer = new QAction(tr("Forget Profile"), this);
    forgetServer->setObjectName(QStringLiteral("actionForgetServerProfile"));
    deployment->addLargeAction(forgetServer);
    auto* serverOperations = servers->addPanel(tr("Operations"));
    auto* networkCertificates = new QAction(tr("Network & Certificates"), this);
    networkCertificates->setObjectName(QStringLiteral("actionServerNetworkCertificates"));
    networkCertificates->setToolTip(
        tr("Check the selected server's HTTPS protocol and peer certificate."));
    serverOperations->addMediumAction(networkCertificates);
    connect(networkCertificates, &QAction::triggered, this,
            &MainWindow::checkSelectedServerAvailability);
    auto* authorityCutover = new QAction(tr("Authority Cutover"), this);
    authorityCutover->setObjectName(QStringLiteral("actionAuthorityCutover"));
    authorityCutover->setToolTip(
        tr("Review or drain the selected server authority, or approve a restored replacement after host-fencing review."));
    serverOperations->addMediumAction(authorityCutover);
    connect(authorityCutover, &QAction::triggered, this,
            &MainWindow::manageAuthorityCutover);
    const struct UnavailableLifecycleAction {
        QString label;
        QString objectName;
        QString explanation;
    } unavailableLifecycleActions[] = {
        {
            tr("Services"),
            QStringLiteral("actionServerServices"),
            tr("Read-only service and storage status is available through the installed server-management TUI. Host service repair is unsupported, and the Qt console cannot invoke lifecycle operations yet."),
        },
        {
            tr("Backups"),
            QStringLiteral("actionServerBackups"),
            tr("Encrypted database backup, verification, and file restore are available through the server-management TUI. Lifecycle-managed deployment restore is not connected to the Qt console."),
        },
        {
            tr("Updates"),
            QStringLiteral("actionServerUpdates"),
            tr("Signed online updates, persistent downgrade protection, compatibility review, and health-gated rollback are not available yet."),
        },
    };
    for (const auto& actionDescription : unavailableLifecycleActions) {
        auto* unavailable = new QAction(actionDescription.label, this);
        unavailable->setObjectName(actionDescription.objectName);
        unavailable->setEnabled(false);
        unavailable->setToolTip(actionDescription.explanation);
        serverOperations->addMediumAction(unavailable);
    }

    auto* accounts = bar->addCategoryPage(tr("Accounts"));
    accounts->setObjectName(QStringLiteral("accountsCategory"));
    auto* accountPanel = accounts->addPanel(tr("Accounts & Access"));
    auto* refreshAccounts = new QAction(tr("Refresh Access"), this);
    refreshAccounts->setObjectName(QStringLiteral("actionRefreshAccounts"));
    accountPanel->addLargeAction(refreshAccounts);
    connect(refreshAccounts, &QAction::triggered, this, &MainWindow::refreshAuthorizedAccountData);
    auto* refreshAudit = new QAction(tr("Refresh Audit"), this);
    refreshAudit->setObjectName(QStringLiteral("actionRefreshAuditEvents"));
    refreshAudit->setToolTip(
        tr("Load the latest retained security-action metadata from one selected server. Fresh MFA is required."));
    accountPanel->addMediumAction(refreshAudit);
    connect(refreshAudit, &QAction::triggered, this, &MainWindow::refreshAuditEvents);
    auto* auditRetention = new QAction(tr("Audit Retention"), this);
    auditRetention->setObjectName(QStringLiteral("actionEditAuditRetention"));
    auditRetention->setToolTip(
        tr("Set how many days of security-action metadata the selected server retains. Fresh MFA is required."));
    accountPanel->addMediumAction(auditRetention);
    connect(auditRetention, &QAction::triggered, this, &MainWindow::editAuditRetention);
    auto* enrollThisDevice = new QAction(tr("Register This Device"), this);
    enrollThisDevice->setObjectName(QStringLiteral("actionEnrollManagerDevice"));
    enrollThisDevice->setToolTip(tr("Register this Manager's protected signing key as pending approval on the selected server."));
    accountPanel->addMediumAction(enrollThisDevice);
    connect(enrollThisDevice, &QAction::triggered, this, &MainWindow::enrollCurrentManagerDevice);
    auto* approveManager = new QAction(tr("Approve Selected Device"), this);
    approveManager->setObjectName(QStringLiteral("actionApproveManagerDevice"));
    accountPanel->addMediumAction(approveManager);
    connect(approveManager, &QAction::triggered, this, &MainWindow::approveSelectedManager);
    auto* revokeManager = new QAction(tr("Revoke Selected Device"), this);
    revokeManager->setObjectName(QStringLiteral("actionRevokeManagerDevice"));
    accountPanel->addMediumAction(revokeManager);
    connect(revokeManager, &QAction::triggered, this, &MainWindow::revokeSelectedManager);
    auto* addAccount = new QAction(tr("Add Account"), this);
    addAccount->setObjectName(QStringLiteral("actionAddAccount"));
    accountPanel->addMediumAction(addAccount);
    connect(addAccount, &QAction::triggered, this, &MainWindow::createServerAccount);
    auto* editCapabilities = new QAction(tr("Edit Capabilities"), this);
    editCapabilities->setObjectName(QStringLiteral("actionEditAccountCapabilities"));
    editCapabilities->setToolTip(tr("Change an active account's roles; recent MFA is required."));
    accountPanel->addMediumAction(editCapabilities);
    connect(editCapabilities, &QAction::triggered, this, &MainWindow::editSelectedAccountCapabilities);
    auto* accountState = new QAction(tr("Enable / Disable Account"), this);
    accountState->setObjectName(QStringLiteral("actionUpdateAccountState"));
    accountState->setToolTip(tr("Activate or disable an account on its owning server; disabling revokes its sessions."));
    accountPanel->addMediumAction(accountState);
    connect(accountState, &QAction::triggered, this, &MainWindow::updateSelectedAccountState);
    auto* confirmMfa = new QAction(tr("Confirm MFA Setup"), this);
    confirmMfa->setObjectName(QStringLiteral("actionConfirmAccountMfa"));
    confirmMfa->setToolTip(tr("Activate a pending account after its user confirms TOTP and independent recovery storage."));
    accountPanel->addMediumAction(confirmMfa);
    connect(confirmMfa, &QAction::triggered, this, &MainWindow::confirmSelectedAccountMfa);
    auto* setupMyMfa = new QAction(tr("Set Up My MFA"), this);
    setupMyMfa->setObjectName(QStringLiteral("actionBeginSelfMfa"));
    setupMyMfa->setToolTip(tr("Set up an authenticator for your signed-in account."));
    accountPanel->addMediumAction(setupMyMfa);
    connect(setupMyMfa, &QAction::triggered, this, &MainWindow::beginSelfMfaEnrollment);
    auto* confirmMyMfa = new QAction(tr("Confirm My MFA"), this);
    confirmMyMfa->setObjectName(QStringLiteral("actionConfirmSelfMfa"));
    accountPanel->addMediumAction(confirmMyMfa);
    connect(confirmMyMfa, &QAction::triggered, this, &MainWindow::confirmBootstrapMfa);
    auto* assignAccess = new QAction(tr("Assign Access"), this);
    assignAccess->setObjectName(QStringLiteral("actionAssignAccountAccess"));
    assignAccess->setToolTip(tr("Grant or replace one account capability on its owning server; recent MFA is required."));
    accountPanel->addMediumAction(assignAccess);
    connect(assignAccess, &QAction::triggered, this, &MainWindow::assignAccountAccess);
    auto* revokeAccess = new QAction(tr("Revoke Selected Access"), this);
    revokeAccess->setObjectName(QStringLiteral("actionRevokeAccountAccess"));
    revokeAccess->setToolTip(tr("Revoke the selected assignment on its owning server; the server checks its revision and fresh MFA."));
    accountPanel->addMediumAction(revokeAccess);
    connect(revokeAccess, &QAction::triggered, this, &MainWindow::revokeSelectedAccountAccess);
    auto* mfaPolicy = new QAction(tr("MFA Policy"), this);
    mfaPolicy->setObjectName(QStringLiteral("actionEditOperatorMfaPolicy"));
    mfaPolicy->setToolTip(tr("Set whether operator sign-in requires MFA; a fresh administrator MFA session is required."));
    accountPanel->addMediumAction(mfaPolicy);
    connect(mfaPolicy, &QAction::triggered, this, &MainWindow::editOperatorMfaPolicy);
    auto* recoverMfa = new QAction(tr("Recover MFA"), this);
    recoverMfa->setObjectName(QStringLiteral("actionRecoverAccountMfa"));
    recoverMfa->setToolTip(tr("Use the account password and one unused recovery code to set up a new authenticator."));
    accountPanel->addMediumAction(recoverMfa);
    connect(recoverMfa, &QAction::triggered, this, &MainWindow::recoverAccountMfa);

    auto* files = bar->addCategoryPage(tr("Files"));
    files->setObjectName(QStringLiteral("filesCategory"));
    auto* filePanel = files->addPanel(tr("File Workflows"));
    auto* sendFile = new QAction(tr("Send File"), this);
    sendFile->setObjectName(QStringLiteral("actionSendFile"));
    sendFile->setToolTip(tr("Send one file to enrolled computers on the selected server or across all signed-in servers."));
    filePanel->addMediumAction(sendFile);
    auto* refreshFiles = new QAction(tr("Refresh Jobs"), this);
    refreshFiles->setObjectName(QStringLiteral("actionRefreshFileJobs"));
    filePanel->addMediumAction(refreshFiles);
    auto* cancelFileJob = new QAction(tr("Cancel Selected Job"), this);
    cancelFileJob->setObjectName(QStringLiteral("actionCancelFileJob"));
    filePanel->addMediumAction(cancelFileJob);

    if (auto* applicationButton = qobject_cast<QToolButton*>(bar->applicationButton())) {
        applicationButton->setText(tr("&File"));
        auto* fileMenu = new QMenu(this);
        auto* fileAddServer = fileMenu->addAction(tr("Add Server"));
        fileAddServer->setObjectName(QStringLiteral("actionFileAddServer"));
        auto* fileSetupServer = fileMenu->addAction(tr("Set Up Server"));
        fileSetupServer->setObjectName(QStringLiteral("actionFileSetupServer"));
        fileMenu->addSeparator();
        auto* fileSignIn = fileMenu->addAction(tr("Sign In to Selected Server"));
        fileSignIn->setObjectName(QStringLiteral("actionFileSignInServer"));
        auto* fileSignOut = fileMenu->addAction(tr("Sign Out of Selected Server"));
        fileSignOut->setObjectName(QStringLiteral("actionFileSignOutServer"));
        fileMenu->addSeparator();
        auto* importProfilesAction = fileMenu->addAction(tr("Import Profiles & Preferences"));
        importProfilesAction->setObjectName(QStringLiteral("actionImportProfiles"));
        auto* exportProfilesAction = fileMenu->addAction(tr("Export Profiles & Preferences"));
        exportProfilesAction->setObjectName(QStringLiteral("actionExportProfiles"));
        auto* exportInventoryAction = fileMenu->addAction(tr("Export Authorized Inventory (CSV)"));
        exportInventoryAction->setObjectName(QStringLiteral("actionExportAuthorizedInventory"));
        fileMenu->addSeparator();
        auto* preferences = fileMenu->addAction(tr("Preferences"));
        preferences->setObjectName(QStringLiteral("actionPreferences"));
        fileMenu->addAction(tr("Help"), this, [this] {
            QMessageBox::information(this, tr("Helicoid Manager Help"),
                                     tr("Add a prepared server profile, verify its deployment ID through a trusted organization channel, or save a Linux setup draft. "
                                        "This build can sign in to a verified server, load authorized inventory, and manage one-time endpoint enrollment. "
                                        "Live endpoint admission, monitoring pixels, and control are not available in this build."));
        });
        fileMenu->addAction(tr("About Helicoid Manager"), this, [this] {
            QMessageBox::about(this, tr("About Helicoid Manager"),
                               tr("Helicoid Manager\nShared Qt desktop console\nVersion %1")
                                   .arg(QCoreApplication::applicationVersion()));
        });
        fileMenu->addSeparator();
        fileMenu->addAction(tr("Quit"), qApp, &QApplication::quit, QKeySequence::Quit);
        applicationButton->setMenu(fileMenu);

        connect(fileAddServer, &QAction::triggered, this, &MainWindow::addServerProfile);
        connect(fileSetupServer, &QAction::triggered, this, &MainWindow::openSetupDraft);
        connect(fileSignIn, &QAction::triggered, this, &MainWindow::signInSelectedServer);
        connect(fileSignOut, &QAction::triggered, this, &MainWindow::signOutSelectedServer);
        connect(importProfilesAction, &QAction::triggered, this, &MainWindow::importProfiles);
        connect(exportProfilesAction, &QAction::triggered, this, &MainWindow::exportProfiles);
        connect(exportInventoryAction, &QAction::triggered, this, &MainWindow::exportAuthorizedInventory);
        connect(preferences, &QAction::triggered, this, &MainWindow::showPreferences);
    }

    connect(addComputerServer, &QAction::triggered, this, &MainWindow::addServerProfile);
    connect(setupComputerServer, &QAction::triggered, this, &MainWindow::openSetupDraft);
    connect(refreshInventory, &QAction::triggered, this, &MainWindow::refreshAuthorizedComputers);
    connect(manageGroups, &QAction::triggered, this, &MainWindow::manageComputerGroups);
    connect(addServer, &QAction::triggered, this, &MainWindow::addServerProfile);
    connect(setupServer, &QAction::triggered, this, &MainWindow::openSetupDraft);
    connect(checkServer, &QAction::triggered, this, &MainWindow::checkSelectedServerAvailability);
    connect(verifyServer, &QAction::triggered, this, &MainWindow::verifySelectedServerIdentity);
    connect(signInServer, &QAction::triggered, this, &MainWindow::signInSelectedServer);
    connect(signOutServer, &QAction::triggered, this, &MainWindow::signOutSelectedServer);
    connect(bootstrapServer, &QAction::triggered, this, &MainWindow::bootstrapSelectedServer);
    connect(confirmBootstrap, &QAction::triggered, this, &MainWindow::confirmBootstrapMfa);
    connect(forgetServer, &QAction::triggered, this, &MainWindow::forgetSelectedServerProfile);
    connect(sendFile, &QAction::triggered, this, &MainWindow::sendFileToSelectedServer);
    connect(refreshFiles, &QAction::triggered, this, &MainWindow::refreshFileJobs);
    connect(cancelFileJob, &QAction::triggered, this, &MainWindow::cancelSelectedFileJob);
    connect(bar, &SARibbonBar::currentRibbonTabChanged, this, [this](int index) {
        showWorkspace(index);
    });
    connect(gridAction, &QAction::triggered, this,
            [this] { applyComputerView(0, true); });
    connect(listAction, &QAction::triggered, this,
            [this] { applyComputerView(1, true); });
    connect(compactPreviewAction, &QAction::triggered, this,
            [this] { applyComputerPreviewSize(0, true); });
    connect(standardPreviewAction, &QAction::triggered, this,
            [this] { applyComputerPreviewSize(1, true); });
    connect(largePreviewAction, &QAction::triggered, this,
            [this] { applyComputerPreviewSize(2, true); });
    connect(extraLargePreviewAction, &QAction::triggered, this,
            [this] { applyComputerPreviewSize(3, true); });

    QTimer::singleShot(0, this, [this] {
        setRibbonTheme(SARibbonTheme::RibbonThemeOffice2021Blue);
    });
}

QWidget* MainWindow::createComputersWorkspace()
{
    QVBoxLayout* layout = nullptr;
    auto* page = makeWorkspaceHeader(tr("Computers"),
                                     tr("Authorized computers from your connected servers appear here."),
                                     this, layout);

    auto* filters = new QWidget(page);
    auto* filterLayout = new QHBoxLayout(filters);
    filterLayout->setContentsMargins(0, 4, 0, 4);
    computerSearch_ = new QLineEdit(filters);
    computerSearch_->setObjectName(QStringLiteral("computerSearch"));
    computerSearch_->setPlaceholderText(tr("Search computers"));
    computerSearch_->setClearButtonEnabled(true);
    computerSearch_->setMaximumWidth(360);
    computerServerFilter_ = new QComboBox(filters);
    computerServerFilter_->setObjectName(QStringLiteral("computerServerFilter"));
    computerServerFilter_->addItem(tr("All servers"), QString());
    computerGroupFilter_ = new QComboBox(filters);
    computerGroupFilter_->setObjectName(QStringLiteral("computerGroupFilter"));
    computerGroupFilter_->addItem(tr("All groups"), QString());
    computerGroupFilter_->setEnabled(false);
    computerGroupFilter_->setToolTip(tr("Groups attached to computers in your authorized inventory."));
    computerInclusionToggle_ = new QToolButton(filters);
    computerInclusionToggle_->setObjectName(QStringLiteral("computerInclusionToggle"));
    computerInclusionToggle_->setText(tr("Computers"));
    computerInclusionToggle_->setCheckable(true);
    computerInclusionToggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    computerInclusionToggle_->setArrowType(Qt::RightArrow);
    computerStatusFilter_ = new QComboBox(filters);
    computerStatusFilter_->setObjectName(QStringLiteral("computerStatusFilter"));
    computerStatusFilter_->addItem(tr("All enrollment states"), QString());
    filterLayout->addWidget(computerSearch_, 1);
    filterLayout->addWidget(computerInclusionToggle_);
    filterLayout->addWidget(computerServerFilter_);
    filterLayout->addWidget(computerGroupFilter_);
    filterLayout->addWidget(computerStatusFilter_);
    layout->addWidget(filters);

    computerInclusionPanel_ = new QWidget(page);
    computerInclusionPanel_->setObjectName(QStringLiteral("computerInclusionPanel"));
    auto* inclusionLayout = new QVBoxLayout(computerInclusionPanel_);
    inclusionLayout->setContentsMargins(8, 4, 8, 8);
    allComputersIncluded_ = new QCheckBox(tr("All Computers"), computerInclusionPanel_);
    allComputersIncluded_->setObjectName(QStringLiteral("allComputersIncluded"));
    allComputersIncluded_->setTristate(true);
    inclusionLayout->addWidget(allComputersIncluded_);
    computerInclusionTree_ = new QTreeWidget(computerInclusionPanel_);
    computerInclusionTree_->setObjectName(QStringLiteral("computerInclusionTree"));
    computerInclusionTree_->setHeaderHidden(true);
    computerInclusionTree_->setMinimumHeight(72);
    computerInclusionTree_->setMaximumHeight(208);
    inclusionLayout->addWidget(computerInclusionTree_);
    auto* inclusionNote = subheading(
        tr("Inclusion is saved for this account and server. It does not grant observe access; previews still require current permission and endpoint admission."),
        computerInclusionPanel_);
    inclusionLayout->addWidget(inclusionNote);
    computerInclusionPanel_->setVisible(false);
    layout->addWidget(computerInclusionPanel_);

    computerContentStack_ = new QStackedWidget(page);
    auto* emptyState = new QWidget(computerContentStack_);
    auto* emptyLayout = new QVBoxLayout(emptyState);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(12);
    emptyLayout->addStretch(1);
    auto* title = heading(tr("No servers connected yet"), emptyState);
    title->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(title);
    auto* description = subheading(tr("Add a server profile to connect to an existing deployment, or prepare a setup draft."), emptyState);
    description->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(description);
    auto* actions = new QWidget(emptyState);
    auto* actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(0, 4, 0, 4);
    auto* addServer = primaryButton(tr("Add Server"), actions);
    addServer->setObjectName(QStringLiteral("buttonAddServerEmpty"));
    auto* setupServer = new QPushButton(tr("Set Up Server"), actions);
    setupServer->setObjectName(QStringLiteral("buttonSetupServerEmpty"));
    setupServer->setMinimumHeight(34);
    actionLayout->addWidget(addServer);
    actionLayout->addWidget(setupServer);
    emptyLayout->addWidget(actions, 0, Qt::AlignCenter);
    emptyLayout->addStretch(1);
    computerContentStack_->addWidget(emptyState);

    auto* profileState = new QWidget(computerContentStack_);
    auto* profileLayout = new QVBoxLayout(profileState);
    profileLayout->setAlignment(Qt::AlignCenter);
    profileLayout->addStretch(1);
    auto* profileTitle = heading(tr("No authorized inventory loaded"), profileState);
    profileTitle->setAlignment(Qt::AlignCenter);
    profileLayout->addWidget(profileTitle);
    auto* profileDescription = subheading(
        tr("Saved server profiles are not signed in. Sign in to load the computers you're authorized to view."),
        profileState);
    profileDescription->setAlignment(Qt::AlignCenter);
    profileLayout->addWidget(profileDescription);
    profileLayout->addStretch(1);
    computerContentStack_->addWidget(profileState);

    computerLayoutStack_ = new QStackedWidget(computerContentStack_);
    computerGrid_ = new QListWidget(computerLayoutStack_);
    computerGrid_->setObjectName(QStringLiteral("authorizedComputersGrid"));
    computerGrid_->setViewMode(QListView::IconMode);
    computerGrid_->setResizeMode(QListView::Adjust);
    computerGrid_->setMovement(QListView::Static);
    computerGrid_->setWrapping(true);
    computerGrid_->setWordWrap(true);
    computerGrid_->setUniformItemSizes(true);
    const auto standardMetrics = computerPreviewMetrics(1);
    computerGrid_->setGridSize(QSize(standardMetrics.gridCell.width,
                                     standardMetrics.gridCell.height));
    computerGrid_->setSpacing(8);
    computerLayoutStack_->addWidget(computerGrid_);
    computerTable_ = emptyTable({tr("Server"), tr("Computer"), tr("Platform"), tr("Enrollment"), tr("Groups"), tr("Endpoint session")},
                                computerLayoutStack_);
    computerTable_->setObjectName(QStringLiteral("authorizedComputersTable"));
    computerLayoutStack_->addWidget(computerTable_);
    computerContentStack_->addWidget(computerLayoutStack_);
    layout->addWidget(computerContentStack_, 1);

    connect(addServer, &QPushButton::clicked, this, &MainWindow::addServerProfile);
    connect(setupServer, &QPushButton::clicked, this, &MainWindow::openSetupDraft);
    connect(computerSearch_, &QLineEdit::textChanged, this, &MainWindow::refreshComputerTable);
    connect(computerInclusionToggle_, &QToolButton::toggled, this, [this](bool expanded) {
        computerInclusionToggle_->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        setComputerInclusionPanelVisible(expanded);
        if (expanded) {
            refreshComputerInclusionPanel();
        }
    });
    connect(allComputersIncluded_, &QCheckBox::clicked, this, [this] {
        setAllComputerInclusion(allComputersIncluded_->checkState() != Qt::Unchecked);
    });
    connect(computerInclusionTree_, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* item, int column) {
                if (column == 0 && !rebuildingComputerInclusionPanel_) {
                    setComputerInclusionItems(item);
                }
            });
    connect(computerServerFilter_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { refreshComputerTable(); });
    connect(computerGroupFilter_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { refreshComputerTable(); });
    connect(computerStatusFilter_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { refreshComputerTable(); });
    connect(computerTable_, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { openSelectedComputerViewer(); });
    connect(computerGrid_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) {
        computerGrid_->setCurrentItem(item);
        openSelectedComputerViewer();
    });
    return page;
}

QWidget* MainWindow::createServersWorkspace()
{
    QVBoxLayout* layout = nullptr;
    auto* page = makeWorkspaceHeader(tr("Servers"),
                                     tr("Profiles are kept separate. Verify each deployment ID through a trusted organization channel before sending credentials or recovery material."),
                                     this, layout);
    auto* actions = new QWidget(page);
    auto* actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    auto* add = primaryButton(tr("Add Server"), actions);
    auto* setup = new QPushButton(tr("Set Up Server"), actions);
    auto* check = new QPushButton(tr("Check Availability"), actions);
    auto* verify = new QPushButton(tr("Verify Identity"), actions);
    verify->setObjectName(QStringLiteral("buttonVerifyServerIdentity"));
    verify->setToolTip(tr("Compare the deployment ID against a value from a trusted organization channel."));
    auto* signIn = new QPushButton(tr("Sign In"), actions);
    signIn->setObjectName(QStringLiteral("buttonSignInServer"));
    auto* signOut = new QPushButton(tr("Sign Out"), actions);
    signOut->setObjectName(QStringLiteral("buttonSignOutServer"));
    auto* forget = new QPushButton(tr("Forget Profile"), actions);
    forget->setObjectName(QStringLiteral("buttonForgetServerProfile"));
    actionLayout->addWidget(add);
    actionLayout->addWidget(setup);
    actionLayout->addStretch(1);
    actionLayout->addWidget(check);
    actionLayout->addWidget(verify);
    actionLayout->addWidget(signIn);
    actionLayout->addWidget(signOut);
    actionLayout->addWidget(forget);
    layout->addWidget(actions);

    serverContentStack_ = new QStackedWidget(page);
    auto* emptyState = new QWidget(serverContentStack_);
    auto* emptyLayout = new QVBoxLayout(emptyState);
    emptyLayout->setAlignment(Qt::AlignCenter);
    auto* emptyTitle = heading(tr("No saved server profiles"), emptyState);
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);
    auto* emptyDescription = subheading(tr("Add a server profile or prepare a setup draft to continue."), emptyState);
    emptyDescription->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyDescription);
    serverContentStack_->addWidget(emptyState);

    auto* serverWorkspace = new QSplitter(Qt::Horizontal, serverContentStack_);
    serverTable_ = emptyTable({tr("Server"), tr("Deployment ID"), tr("Address"), tr("Session / Availability")},
                              nullptr);
    serverTable_->setObjectName(QStringLiteral("serverProfilesTable"));
    auto* detailPanel = new QWidget;
    detailPanel->setObjectName(QStringLiteral("serverDetailsPanel"));
    auto* detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(18, 16, 18, 16);
    detailLayout->setSpacing(12);
    auto* detailHeading = heading(tr("Server details"), detailPanel);
    detailLayout->addWidget(detailHeading);
    serverDetailName_ = subheading(tr("Select a server profile."), detailPanel);
    serverDetailEndpoint_ = subheading(QString(), detailPanel);
    serverDetailDeployment_ = subheading(QString(), detailPanel);
    serverDetailState_ = subheading(QString(), detailPanel);
    serverDetailHealth_ = subheading(tr("No public health report loaded."), detailPanel);
    for (QLabel* detail : {serverDetailName_, serverDetailEndpoint_, serverDetailDeployment_,
                           serverDetailState_, serverDetailHealth_}) {
        detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
        detailLayout->addWidget(detail);
    }
    serverDetailHealth_->setObjectName(QStringLiteral("serverHealthDetails"));
    detailLayout->addStretch(1);
    serverWorkspace->addWidget(serverTable_);
    serverWorkspace->addWidget(detailPanel);
    serverWorkspace->setStretchFactor(0, 3);
    serverWorkspace->setStretchFactor(1, 2);
    serverContentStack_->addWidget(serverWorkspace);
    layout->addWidget(serverContentStack_, 1);

    connect(add, &QPushButton::clicked, this, &MainWindow::addServerProfile);
    connect(setup, &QPushButton::clicked, this, &MainWindow::openSetupDraft);
    connect(check, &QPushButton::clicked, this, &MainWindow::checkSelectedServerAvailability);
    connect(verify, &QPushButton::clicked, this, &MainWindow::verifySelectedServerIdentity);
    connect(signIn, &QPushButton::clicked, this, &MainWindow::signInSelectedServer);
    connect(signOut, &QPushButton::clicked, this, &MainWindow::signOutSelectedServer);
    connect(forget, &QPushButton::clicked, this, &MainWindow::forgetSelectedServerProfile);
    connect(serverTable_, &QTableWidget::cellDoubleClicked, this, [this] {
        checkSelectedServerAvailability();
    });
    connect(serverTable_, &QTableWidget::currentCellChanged, this,
            [this](int, int, int, int) { refreshServerDetails(); });
    return page;
}

QWidget* MainWindow::createAccountsWorkspace()
{
    QVBoxLayout* layout = nullptr;
    auto* page = makeWorkspaceHeader(tr("Accounts"),
                                     tr("Authorized accounts, management devices, and access assignments are combined across signed-in servers."),
                                     this, layout);
    accountsServerFilter_ = new QComboBox(page);
    accountsServerFilter_->setObjectName(QStringLiteral("accountsServerFilter"));
    accountsServerFilter_->addItem(tr("All servers"), QString());
    layout->addWidget(accountsServerFilter_);
    auto* tabs = new QTabWidget(page);
    accountTable_ = emptyTable({tr("Server"), tr("Account"), tr("Account ID"), tr("State"), tr("Capabilities"), tr("Revision")}, tabs);
    accountTable_->setObjectName(QStringLiteral("accountTable"));
    managerTable_ = emptyTable({tr("Server"), tr("Device"), tr("Approval"), tr("Public identity"), tr("Approved")}, tabs);
    managerTable_->setObjectName(QStringLiteral("managerApprovalTable"));
    accessTable_ = emptyTable({tr("Server"), tr("Account ID"), tr("Capability"), tr("Computer scope"), tr("Revision")}, tabs);
    accessTable_->setObjectName(QStringLiteral("accessAssignmentTable"));
    auditTable_ = emptyTable({tr("Time (UTC)"), tr("Account ID"), tr("Manager ID"), tr("Action"),
                              tr("Targets"), tr("Result"), tr("Reason"), tr("Event ID")}, tabs);
    auditTable_->setObjectName(QStringLiteral("securityAuditTable"));
    tabs->addTab(accountTable_, tr("Accounts"));
    tabs->addTab(managerTable_, tr("Management devices"));
    tabs->addTab(accessTable_, tr("Access assignments"));
    tabs->addTab(auditTable_, tr("Audit"));
    layout->addWidget(tabs, 1);
    auditOlderButton_ = new QPushButton(tr("Load Older Audit Events"), page);
    auditOlderButton_->setObjectName(QStringLiteral("actionLoadOlderAuditEvents"));
    auditOlderButton_->setEnabled(false);
    layout->addWidget(auditOlderButton_);
    connect(auditOlderButton_, &QPushButton::clicked, this, &MainWindow::loadOlderAuditEvents);
    connect(accountsServerFilter_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) {
                refreshAccountTables();
                auditNextCursorId_.clear();
                auditNextCursorUnix_ = 0;
                auditLoading_ = false;
                if (auditRequestGeneration_ < std::numeric_limits<std::uint64_t>::max()) {
                    ++auditRequestGeneration_;
                }
                if (auditTable_ != nullptr) auditTable_->setRowCount(0);
                if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(false);
                if (auditStatus_ != nullptr) {
                    auditStatus_->setText(tr("Select one server and refresh to load its retained audit metadata."));
                }
            });
    accountsStatus_ = subheading(
        tr("Account, manager-device, and access changes are scoped to one selected server. Privileged writes require a current approved Manager session and recent MFA."),
        page);
    accountsStatus_->setObjectName(QStringLiteral("accountsStatus"));
    layout->addWidget(accountsStatus_);
    auditStatus_ = subheading(
        tr("Audit contains retained action metadata only. Select one server and refresh; the server requires a fresh MFA-backed security-admin session."),
        page);
    auditStatus_->setObjectName(QStringLiteral("auditStatus"));
    auditStatus_->setWordWrap(true);
    layout->addWidget(auditStatus_);
    return page;
}

QWidget* MainWindow::createFilesWorkspace()
{
    QVBoxLayout* layout = nullptr;
    auto* page = makeWorkspaceHeader(tr("Files"),
                                     tr("Queue durable file jobs for the most recently verified local users on enrolled computers. All servers creates one independent job per deployment; delivery waits for the same accounts if another user signs in."),
                                     this, layout);
    filesServerFilter_ = new QComboBox(page);
    filesServerFilter_->setObjectName(QStringLiteral("filesServerFilter"));
    filesServerFilter_->addItem(tr("All servers"), QString());
    layout->addWidget(filesServerFilter_);
    auto* storageRow = new QWidget(page);
    auto* storageLayout = new QHBoxLayout(storageRow);
    storageLayout->setContentsMargins(0, 0, 0, 0);
    filesQuotaStatus_ = subheading(
        tr("Logical file payload quota has not been loaded. It excludes database pages, backups, and filesystem usage."),
        storageRow);
    filesQuotaStatus_->setObjectName(QStringLiteral("filesQuotaStatus"));
    filesQuotaStatus_->setWordWrap(true);
    auto* refreshStorage = new QPushButton(tr("Refresh Quota"), storageRow);
    refreshStorage->setObjectName(QStringLiteral("buttonRefreshFileStorage"));
    storageLayout->addWidget(filesQuotaStatus_, 1);
    storageLayout->addWidget(refreshStorage);
    layout->addWidget(storageRow);

    fileTabs_ = new QTabWidget(page);
    fileTabs_->setObjectName(QStringLiteral("fileWorkflowTabs"));

    auto* transfers = new QWidget(fileTabs_);
    auto* transferLayout = new QVBoxLayout(transfers);
    fileJobTable_ = emptyTable(
        {tr("Server"), tr("File"), tr("Size"), tr("Targets and results"), tr("State"),
         tr("Expires (local)"), tr("Job ID")}, transfers);
    fileJobTable_->setObjectName(QStringLiteral("fileJobTable"));
    transferLayout->addWidget(fileJobTable_, 1);
    auto* actions = new QWidget(transfers);
    auto* actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    auto* send = primaryButton(tr("Send File"), actions);
    send->setObjectName(QStringLiteral("buttonSendFile"));
    auto* refresh = new QPushButton(tr("Refresh Jobs"), actions);
    refresh->setObjectName(QStringLiteral("buttonRefreshFileJobs"));
    auto* cancel = new QPushButton(tr("Cancel Selected Job"), actions);
    cancel->setObjectName(QStringLiteral("buttonCancelFileJob"));
    actionLayout->addWidget(send);
    actionLayout->addWidget(refresh);
    actionLayout->addWidget(cancel);
    actionLayout->addStretch(1);
    transferLayout->addWidget(actions);
    fileTabs_->addTab(transfers, tr("Transfers"));

    auto* requests = new QWidget(fileTabs_);
    auto* requestLayout = new QVBoxLayout(requests);
    fileRequestTable_ = emptyTable(
        {tr("Server"), tr("Request"), tr("State"), tr("Computers"),
         tr("Closes (local)"), tr("Revision"), tr("Request ID")}, requests);
    fileRequestTable_->setObjectName(QStringLiteral("fileRequestTable"));
    requestLayout->addWidget(fileRequestTable_, 1);
    auto* requestActions = new QWidget(requests);
    auto* requestActionLayout = new QVBoxLayout(requestActions);
    requestActionLayout->setContentsMargins(0, 0, 0, 0);
    auto* requestManagement = new QWidget(requestActions);
    auto* requestManagementLayout = new QHBoxLayout(requestManagement);
    requestManagementLayout->setContentsMargins(0, 0, 0, 0);
    auto* submissionManagement = new QWidget(requestActions);
    auto* submissionManagementLayout = new QHBoxLayout(submissionManagement);
    submissionManagementLayout->setContentsMargins(0, 0, 0, 0);
    auto* saveDraft = new QPushButton(tr("Save Request Draft"), requestManagement);
    saveDraft->setObjectName(QStringLiteral("buttonSaveFileRequestDraft"));
    auto* publish = primaryButton(tr("Publish Selected"), requestManagement);
    publish->setObjectName(QStringLiteral("buttonPublishFileRequest"));
    auto* closeRequest = new QPushButton(tr("Close Selected"), requestManagement);
    closeRequest->setObjectName(QStringLiteral("buttonCloseFileRequest"));
    auto* reviewSubmissions = new QPushButton(tr("Review Received Files"), submissionManagement);
    reviewSubmissions->setObjectName(QStringLiteral("buttonReviewFileRequestSubmissions"));
    auto* deleteSubmissionContent = new QPushButton(tr("Delete Received Content"), submissionManagement);
    deleteSubmissionContent->setObjectName(QStringLiteral("buttonDeleteFileRequestContent"));
    deleteSubmissionContent->setToolTip(
        tr("Delete retained file chunks after exact submission confirmation; metadata remains."));
    auto* refreshRequests = new QPushButton(tr("Refresh Requests"), submissionManagement);
    refreshRequests->setObjectName(QStringLiteral("buttonRefreshFileRequests"));
    requestManagementLayout->addWidget(saveDraft);
    requestManagementLayout->addWidget(publish);
    requestManagementLayout->addWidget(closeRequest);
    requestManagementLayout->addStretch(1);
    submissionManagementLayout->addWidget(reviewSubmissions);
    submissionManagementLayout->addWidget(deleteSubmissionContent);
    submissionManagementLayout->addWidget(refreshRequests);
    submissionManagementLayout->addStretch(1);
    requestActionLayout->addWidget(requestManagement);
    requestActionLayout->addWidget(submissionManagement);
    requestLayout->addWidget(requestActions);
    fileTabs_->addTab(requests, tr("File Requests"));
    layout->addWidget(fileTabs_, 1);

    filesStatus_ = subheading(
        tr("File requests are combined across signed-in servers by default. "
           "Each change is sent to the request's owning server. "
           "Authenticated request workflows use the server's v2 API. "
           "Recipient-bound file delivery uses authenticated v2 routes when the owning server advertises support."), page);
    filesStatus_->setObjectName(QStringLiteral("filesStatus"));
    layout->addWidget(filesStatus_);
    connect(filesServerFilter_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) {
                refreshFileJobTable();
                refreshFileRequestTable();
                refreshFileStorage();
                if (fileTabs_ != nullptr && fileTabs_->currentIndex() == 1) refreshFileRequests();
            });
    connect(fileTabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 0) refreshFileJobTable();
        else if (index == 1) refreshFileRequests();
    });
    connect(send, &QPushButton::clicked, this, &MainWindow::sendFileToSelectedServer);
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshFileJobs);
    connect(cancel, &QPushButton::clicked, this, &MainWindow::cancelSelectedFileJob);
    connect(saveDraft, &QPushButton::clicked, this, &MainWindow::saveFileRequestDraft);
    connect(publish, &QPushButton::clicked, this, &MainWindow::publishSelectedFileRequest);
    connect(closeRequest, &QPushButton::clicked, this, &MainWindow::closeSelectedFileRequest);
    connect(reviewSubmissions, &QPushButton::clicked, this, &MainWindow::reviewFileRequestSubmissions);
    connect(deleteSubmissionContent, &QPushButton::clicked,
            this, &MainWindow::deleteSelectedFileRequestSubmissionContent);
    connect(refreshRequests, &QPushButton::clicked, this, &MainWindow::refreshFileRequests);
    connect(refreshStorage, &QPushButton::clicked, this, &MainWindow::refreshFileStorage);
    return page;
}

QWidget* MainWindow::createSetupWorkspace()
{
    QVBoxLayout* layout = nullptr;
    auto* page = makeWorkspaceHeader(tr("Set Up a Server"),
                                     tr("Prepare a Linux deployment draft. Save it locally, or run a read-only SSH check before setup."),
                                     this, layout);

    auto* form = new QWidget(page);
    auto* formLayout = new QFormLayout(form);
    setupName_ = new QLineEdit(form);
    setupName_->setObjectName(QStringLiteral("setupDeploymentName"));
    setupName_->setMaxLength(100);
    setupHost_ = new QLineEdit(form);
    setupHost_->setObjectName(QStringLiteral("setupLinuxHost"));
    setupHost_->setPlaceholderText(tr("[user@]server.example.org[:port]"));
    setupDataDirectory_ = new QLineEdit(QStringLiteral("/var/lib/helicoid/data"), form);
    setupDataDirectory_->setObjectName(QStringLiteral("setupDataDirectory"));
    setupDataDirectory_->setMaxLength(4096);
    setupBackupDirectory_ = new QLineEdit(QStringLiteral("/var/lib/helicoid/backups"), form);
    setupBackupDirectory_->setObjectName(QStringLiteral("setupBackupDirectory"));
    setupBackupDirectory_->setMaxLength(4096);
    setupNetworkScope_ = new QComboBox(form);
    setupNetworkScope_->setObjectName(QStringLiteral("setupNetworkScope"));
    setupNetworkScope_->addItem(tr("LAN (recommended)"), QStringLiteral("lan"));
    setupNetworkScope_->addItem(tr("Private DNS"), QStringLiteral("private_dns"));
    setupNetworkScope_->addItem(tr("Loopback development"), QStringLiteral("loopback_development"));
    setupBindAddress_ = new QLineEdit(form);
    setupBindAddress_->setObjectName(QStringLiteral("setupBindAddress"));
    setupBindAddress_->setPlaceholderText(tr("192.168.1.10 or fd00::10"));
    setupClientHostname_ = new QLineEdit(form);
    setupClientHostname_->setObjectName(QStringLiteral("setupClientHostname"));
    setupClientHostname_->setPlaceholderText(tr("server.school.lan"));
    setupAuthorityPort_ = new QSpinBox(form);
    setupAuthorityPort_->setObjectName(QStringLiteral("setupAuthorityPort"));
    setupAuthorityPort_->setRange(1024, 65535);
    setupAuthorityPort_->setValue(8920);
    setupMode_ = new QComboBox(form);
    setupMode_->setObjectName(QStringLiteral("setupInstallMode"));
    setupMode_->addItem(tr("Native systemd"), QStringLiteral("systemd"));
    setupMode_->addItem(tr("Docker Compose"), QStringLiteral("compose"));
    formLayout->addRow(tr("Deployment name"), setupName_);
    formLayout->addRow(tr("Linux server address"), setupHost_);
    formLayout->addRow(tr("Installation type"), setupMode_);
    formLayout->addRow(tr("Network scope"), setupNetworkScope_);
    formLayout->addRow(tr("Private host bind address"), setupBindAddress_);
    formLayout->addRow(tr("Manager DNS hostname"), setupClientHostname_);
    formLayout->addRow(tr("HTTPS port"), setupAuthorityPort_);
    formLayout->addRow(tr("Server data directory"), setupDataDirectory_);
    formLayout->addRow(tr("Backup directory"), setupBackupDirectory_);
    layout->addWidget(form);

    setupNetworkHint_ = subheading(
        tr("LAN mode needs a private address assigned to the server, an optional Manager DNS hostname, and a matching trusted TLS certificate. The installer does not change firewall, DNS, proxy, or client trust settings."),
        page);
    setupNetworkHint_->setObjectName(QStringLiteral("setupNetworkHint"));
    setupNetworkHint_->setWordWrap(true);
    layout->addWidget(setupNetworkHint_);
    auto* footnote = subheading(
        tr("This workspace saves a local draft only. The current lifecycle backend "
           "supports Linux systemd and Docker Compose deployments. SSH apply is "
           "not available yet. Check Host performs read-only checks over "
           "strictly verified SSH; interface assignment and TLS certificate files "
           "are not inspected. Compose checks also verify the Docker Engine and "
           "Compose plugin. No SSH credential or private key is saved."),
        page);
    layout->addWidget(footnote);
    setupHostCheckStatus_ = new QLabel(page);
    setupHostCheckStatus_->setObjectName(QStringLiteral("setupHostCheckStatus"));
    setupHostCheckStatus_->setWordWrap(true);
    setupHostCheckStatus_->setTextFormat(Qt::PlainText);
    layout->addWidget(setupHostCheckStatus_);
    setupDraftStatus_ = new QLabel(page);
    setupDraftStatus_->setObjectName(QStringLiteral("setupDraftStatus"));
    setupDraftStatus_->setWordWrap(true);
    layout->addWidget(setupDraftStatus_);

    auto* actions = new QWidget(page);
    auto* actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    auto* save = primaryButton(tr("Save Draft"), actions);
    save->setObjectName(QStringLiteral("buttonSaveSetupDraft"));
    setupCheckHostButton_ = new QPushButton(tr("Check Host"), actions);
    setupCheckHostButton_->setObjectName(QStringLiteral("buttonCheckSetupHost"));
    auto* back = new QPushButton(tr("Back to Servers"), actions);
    actionLayout->addWidget(setupCheckHostButton_);
    actionLayout->addWidget(save);
    actionLayout->addWidget(back);
    actionLayout->addStretch(1);
    layout->addWidget(actions);
    layout->addStretch(1);

    QSettings settings;
    settings.beginGroup(QStringLiteral("serverSetupDraft"));
    setupName_->setText(settings.value(QStringLiteral("name")).toString());
    setupHost_->setText(settings.value(QStringLiteral("host")).toString());
    setupDataDirectory_->setText(settings.value(
        QStringLiteral("dataDirectory"), QStringLiteral("/var/lib/helicoid/data")).toString());
    setupBackupDirectory_->setText(settings.value(
        QStringLiteral("backupDirectory"), QStringLiteral("/var/lib/helicoid/backups")).toString());
    const QString networkScope = settings.value(QStringLiteral("networkScope"),
                                                QStringLiteral("lan")).toString();
    const int networkScopeIndex = setupNetworkScope_->findData(networkScope);
    setupNetworkScope_->setCurrentIndex(networkScopeIndex >= 0 ? networkScopeIndex : 0);
    setupBindAddress_->setText(settings.value(QStringLiteral("bindAddress")).toString());
    setupClientHostname_->setText(settings.value(QStringLiteral("clientHostname")).toString());
    const int port = settings.value(QStringLiteral("authorityPort"), 8920).toInt();
    setupAuthorityPort_->setValue(port >= 1024 && port <= 65535 ? port : 8920);
    const QString mode = settings.value(QStringLiteral("mode"), QStringLiteral("systemd")).toString();
    const int modeIndex = setupMode_->findData(mode);
    setupMode_->setCurrentIndex(
        modeIndex >= 0 ? modeIndex : setupMode_->findData(QStringLiteral("systemd")));
    settings.endGroup();
    if (!setupName_->text().isEmpty() || !setupHost_->text().isEmpty()) {
        setupDraftStatus_->setText(tr("A saved draft was restored."));
    }

    connect(save, &QPushButton::clicked, this, &MainWindow::saveSetupDraft);
    connect(setupCheckHostButton_, &QPushButton::clicked, this, &MainWindow::checkSetupHost);
    connect(setupNetworkScope_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) {
                const QString scope = setupNetworkScope_->currentData().toString();
                const bool privateNetwork = scope == QStringLiteral("lan") ||
                    scope == QStringLiteral("private_dns");
                const bool privateDns = scope == QStringLiteral("private_dns");
                setupBindAddress_->setEnabled(privateNetwork);
                setupClientHostname_->setEnabled(privateNetwork);
                setupAuthorityPort_->setEnabled(privateNetwork);
                if (!privateNetwork) setupAuthorityPort_->setValue(8787);
                else if (setupAuthorityPort_->value() == 8787) setupAuthorityPort_->setValue(8920);
                setupNetworkHint_->setText(privateDns
                    ? tr("Private DNS mode requires a private address assigned to the server, a Manager DNS hostname, and a matching trusted TLS certificate. Create the internal DNS record and allow the HTTPS port only from approved Manager networks. If you use a proxy, preserve the hostname and verify TLS to the configured address. Clients must trust the issuing CA; certificate renewal remains operator-managed.")
                    : privateNetwork
                    ? tr("LAN mode needs a private address assigned to the server and a matching, trusted TLS certificate chain and key at "
                         "/etc/ssl/helicoid/fullchain.pem and /etc/ssl/helicoid/private-key.pem. The certificate SAN must match the "
                         "Manager DNS hostname or bind IP used by clients, and clients must trust its issuing CA. Keep the directory root-owned mode 0700 "
                         "and both files root-owned mode 0600. The installer does not change firewall, DNS, proxy, or client trust settings.")
                    : tr("Loopback development keeps the authority on 127.0.0.1:8787 and does not accept remote Manager connections."));
            });
    const QString initialScope = setupNetworkScope_->currentData().toString();
    const bool privateNetworkScope = initialScope == QStringLiteral("lan") ||
        initialScope == QStringLiteral("private_dns");
    setupBindAddress_->setEnabled(privateNetworkScope);
    setupClientHostname_->setEnabled(privateNetworkScope);
    setupAuthorityPort_->setEnabled(privateNetworkScope);
    if (!privateNetworkScope) setupAuthorityPort_->setValue(8787);
    setupNetworkHint_->setText(initialScope == QStringLiteral("private_dns")
        ? tr("Private DNS mode requires a private address assigned to the server, a Manager DNS hostname, and a matching trusted TLS certificate. Create the internal DNS record and allow the HTTPS port only from approved Manager networks. If you use a proxy, preserve the hostname and verify TLS to the configured address. Clients must trust the issuing CA; certificate renewal remains operator-managed.")
        : privateNetworkScope
        ? tr("LAN mode needs a private address assigned to the server and a matching, trusted TLS certificate chain and key at "
             "/etc/ssl/helicoid/fullchain.pem and /etc/ssl/helicoid/private-key.pem. The certificate SAN must match the DNS name "
             "in the Manager DNS hostname field or the bind IP used by clients, and clients must trust its issuing CA. Keep the directory root-owned mode 0700 and both files "
             "root-owned mode 0600. The installer does not change firewall, DNS, proxy, or client trust settings.")
        : tr("Loopback development keeps the authority on 127.0.0.1:8787 and does not accept remote Manager connections."));
    connect(back, &QPushButton::clicked, this, [this] { showWorkspace(1); });
    return page;
}

void MainWindow::addServerProfile()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add Server"));
    dialog.setMinimumWidth(520);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* name = new QLineEdit(&dialog);
    name->setObjectName(QStringLiteral("serverProfileName"));
    name->setMaxLength(100);
    name->setPlaceholderText(tr("e.g. Main office"));
    auto* endpoint = new QLineEdit(&dialog);
    endpoint->setObjectName(QStringLiteral("serverProfileAddress"));
    endpoint->setPlaceholderText(tr("https://server.example.org"));
    form->addRow(tr("Server name"), name);
    form->addRow(tr("Server URL"), endpoint);
    layout->addLayout(form);
    auto* note = subheading(tr("HTTPS is required. HTTP is accepted only for localhost or a loopback IP used for development."), &dialog);
    layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, name, endpoint] {
        QString validationError;
        if (!validDisplayName(name->text())) {
            validationError = QObject::tr("Enter a name for this server.");
        } else if (!normalizedEndpoint(endpoint->text(), validationError)) {
            // The validator supplies the address-specific message.
        }
        if (!validationError.isEmpty()) {
            QMessageBox::warning(&dialog, QObject::tr("Check Server Details"), validationError);
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString endpointError;
    const auto address = normalizedEndpoint(endpoint->text(), endpointError);
    if (!address) {
        QMessageBox::warning(this, tr("Check Server Details"), endpointError);
        return;
    }

    ConnectionProfile profile;
    profile.profileId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    profile.displayName = name->text().trimmed().toStdString();
    profile.endpoint = address->toString(QUrl::FullyEncoded).toStdString();
    const std::string profileId = profile.profileId;
    std::string error;
    if (!registry_.addProfile(std::move(profile), error)) {
        QMessageBox::warning(this, tr("Unable to Add Server"), QString::fromStdString(error));
        return;
    }

    if (!saveProfiles()) {
        registry_.removeProfile(profileId);
        refreshServerTable();
        QMessageBox::warning(this, tr("Unable to Save Server"),
                             tr("The profile could not be saved to this user's application settings."));
        return;
    }
    refreshServerTable();
    showWorkspace(1);
    statusBar()->showMessage(tr("Server profile saved. It is not signed in."), 5000);
}

void MainWindow::importProfiles()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Profiles & Preferences"),
        QString(), tr("Helicoid Migration Bundles (*.json);;JSON Files (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Unable to Import Profiles"), tr("The selected file could not be opened."));
        return;
    }
    const QByteArray data = file.read(maxProfileDocumentBytes + 1);
    if (file.error() != QFileDevice::NoError || data.size() > maxProfileDocumentBytes) {
        QMessageBox::warning(
            this, tr("Unable to Import Profiles"),
            tr("The selected file could not be read within the allowed size."));
        return;
    }

    std::vector<ConnectionProfile> imported;
    ImportedManagerPreferences importedPreferences;
    QString parseError;
    if (!parseProfiles(data, imported, parseError, &importedPreferences)) {
        QMessageBox::warning(this, tr("Unable to Import Profiles & Preferences"), parseError);
        return;
    }

    std::vector<ConnectionProfile> additions;
    for (const auto& profile : imported) {
        const auto existing = registry_.profile(profile.profileId);
        if (!existing) {
            additions.push_back(profile);
            continue;
        }
        if (existing->displayName != profile.displayName || existing->endpoint != profile.endpoint) {
            QMessageBox::warning(this, tr("Profile ID Conflict"),
                                 tr("A saved profile already uses an imported ID with different details."));
            return;
        }
    }
    if (registry_.profiles().size() + additions.size() > DeploymentRegistry::maxProfiles) {
        QMessageBox::warning(this, tr("Unable to Import Profiles & Preferences"),
                             tr("The import would exceed the maximum number of saved servers."));
        return;
    }
    const std::size_t preferenceCount =
        static_cast<std::size_t>(importedPreferences.monitoringInclusion.size());
    const std::size_t previewSizePreferenceCount = static_cast<std::size_t>(std::count_if(
        importedPreferences.monitoringInclusion.constBegin(),
        importedPreferences.monitoringInclusion.constEnd(), [](const QJsonValue& value) {
            return value.toObject().contains(QStringLiteral("preview_size"));
        }));
    const std::size_t computerViewPreferenceCount = static_cast<std::size_t>(std::count_if(
        importedPreferences.monitoringInclusion.constBegin(),
        importedPreferences.monitoringInclusion.constEnd(), [](const QJsonValue& value) {
            return value.toObject().contains(QStringLiteral("computer_view"));
        }));
    if (additions.empty() && preferenceCount == 0 && !importedPreferences.hasComputerView &&
        !importedPreferences.hasComputerPreviewSize) {
        statusBar()->showMessage(tr("All imported profiles and preferences already exist."), 4000);
        return;
    }

    QStringList preview;
    preview.reserve(static_cast<qsizetype>(additions.size()));
    for (const auto& profile : additions) {
        preview.append(QStringLiteral("%1. %2")
                            .arg(preview.size() + 1)
                            .arg(QString::fromStdString(profile.displayName)));
    }
    QString description;
    if (additions.empty()) {
        description = tr("No new connection profiles.");
    } else {
        description = tr("Import %1 connection profile(s):\n%2")
                          .arg(static_cast<qulonglong>(additions.size()))
                          .arg(preview.join(QLatin1Char('\n')));
    }
    if (preferenceCount > 0) {
        description += tr("\n\nImport %1 scoped monitoring preference set(s)? "
                           "Existing values for matching scopes will be replaced.")
                           .arg(static_cast<qulonglong>(preferenceCount));
    }
    if (previewSizePreferenceCount > 0) {
        description += tr("\n\nRestore preview sizes for %1 signed-in account and server scope(s)?")
                           .arg(static_cast<qulonglong>(previewSizePreferenceCount));
    }
    if (computerViewPreferenceCount > 0) {
        description += tr("\n\nRestore Grid/List choices for %1 signed-in account and server scope(s)?")
                           .arg(static_cast<qulonglong>(computerViewPreferenceCount));
    }
    if (importedPreferences.hasComputerView) {
        description += tr("\n\nSet the default computer layout to %1?")
                           .arg(importedPreferences.computerView == QStringLiteral("list")
                                    ? tr("List") : tr("Grid"));
    }
    if (importedPreferences.hasComputerPreviewSize) {
        description += tr("\n\nSet computer preview cards to %1 size?")
                           .arg(importedPreferences.computerPreviewSize.left(1).toUpper() +
                                importedPreferences.computerPreviewSize.mid(1));
    }
    description += tr("\n\nCredentials, private keys, sign-in sessions, and server identity trust "
                      "are not included. Newly added servers must be verified before sign-in; "
                      "matching local profiles keep their existing trust.");
    const auto confirmation = QMessageBox::question(
        this, tr("Review Migration Bundle"), description,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirmation != QMessageBox::Yes) {
        return;
    }

    std::vector<std::string> addedIds;
    for (auto& profile : additions) {
        const std::string id = profile.profileId;
        std::string error;
        if (!registry_.addProfile(std::move(profile), error)) {
            for (const auto& addedId : addedIds) {
                registry_.removeProfile(addedId);
            }
            QMessageBox::warning(this, tr("Unable to Import Profiles & Preferences"),
                                 QString::fromStdString(error));
            return;
        }
        addedIds.push_back(id);
    }

    struct PreviousSetting final {
        QString key;
        bool existed = false;
        QVariant value;
    };
    QSettings settings;
    const bool hadComputerView = settings.contains(QStringLiteral("computerViewV1"));
    const QVariant previousComputerView = settings.value(QStringLiteral("computerViewV1"));
    const bool hadComputerPreviewSize = settings.contains(QStringLiteral("computerPreviewSizeV1"));
    const QVariant previousComputerPreviewSize = settings.value(QStringLiteral("computerPreviewSizeV1"));
    settings.beginGroup(QStringLiteral("serverProfiles"));
    const bool hadSavedProfiles = settings.contains(QStringLiteral("profiles_v1"));
    const QVariant previousSavedProfiles = settings.value(QStringLiteral("profiles_v1"));
    settings.setValue(QStringLiteral("profiles_v1"), encodeProfiles(registry_.profiles()));
    settings.endGroup();
    settings.beginGroup(QStringLiteral("monitoringInclusionV1"));
    std::vector<PreviousSetting> previousPreferences;
    previousPreferences.reserve(static_cast<std::size_t>(importedPreferences.monitoringInclusion.size()));
    for (auto preference = importedPreferences.monitoringInclusion.constBegin();
         preference != importedPreferences.monitoringInclusion.constEnd(); ++preference) {
        const QJsonObject value = preference.value().toObject();
        const QVariantMap previousStored = settings.value(preference.key()).toMap();
        previousPreferences.push_back(PreviousSetting{
            preference.key(), settings.contains(preference.key()), previousStored});
        QVariantMap stored;
        stored.insert(QStringLiteral("allComputers"),
                      value.value(QStringLiteral("all_computers")).toBool());
        QStringList computerIds;
        for (const QJsonValue& id : value.value(QStringLiteral("computer_ids")).toArray()) {
            computerIds.append(id.toString());
        }
        stored.insert(QStringLiteral("computerIds"), computerIds);
        const bool panelVisibilityConfigured = value.contains(QStringLiteral("panel_visible"));
        if (panelVisibilityConfigured) {
            stored.insert(QStringLiteral("panelVisible"),
                          value.value(QStringLiteral("panel_visible")).toBool());
        } else if (previousStored.contains(QStringLiteral("panelVisible"))) {
            stored.insert(QStringLiteral("panelVisible"),
                          previousStored.value(QStringLiteral("panelVisible")));
        }
        if (value.contains(QStringLiteral("computer_view"))) {
            stored.insert(QStringLiteral("computerView"),
                          value.value(QStringLiteral("computer_view")).toString());
        } else if (previousStored.contains(QStringLiteral("computerView"))) {
            stored.insert(QStringLiteral("computerView"),
                          previousStored.value(QStringLiteral("computerView")));
        }
        if (value.contains(QStringLiteral("preview_size"))) {
            stored.insert(QStringLiteral("previewSize"), computerPreviewSizeName(
                computerPreviewSizeIndex(value.value(QStringLiteral("preview_size")).toString())));
        } else if (previousStored.contains(QStringLiteral("previewSize"))) {
            stored.insert(QStringLiteral("previewSize"),
                          previousStored.value(QStringLiteral("previewSize")));
        }
        settings.setValue(preference.key(), stored);
    }
    settings.endGroup();
    if (importedPreferences.hasComputerView) {
        settings.setValue(QStringLiteral("computerViewV1"), importedPreferences.computerView);
    }
    if (importedPreferences.hasComputerPreviewSize) {
        settings.setValue(QStringLiteral("computerPreviewSizeV1"),
                          importedPreferences.computerPreviewSize);
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        settings.beginGroup(QStringLiteral("serverProfiles"));
        if (hadSavedProfiles) {
            settings.setValue(QStringLiteral("profiles_v1"), previousSavedProfiles);
        } else {
            settings.remove(QStringLiteral("profiles_v1"));
        }
        settings.endGroup();
        settings.beginGroup(QStringLiteral("monitoringInclusionV1"));
        for (const PreviousSetting& previous : previousPreferences) {
            if (previous.existed) {
                settings.setValue(previous.key, previous.value);
            } else {
                settings.remove(previous.key);
            }
        }
        settings.endGroup();
        if (hadComputerView) {
            settings.setValue(QStringLiteral("computerViewV1"), previousComputerView);
        } else {
            settings.remove(QStringLiteral("computerViewV1"));
        }
        if (hadComputerPreviewSize) {
            settings.setValue(QStringLiteral("computerPreviewSizeV1"), previousComputerPreviewSize);
        } else {
            settings.remove(QStringLiteral("computerPreviewSizeV1"));
        }
        settings.sync();
        for (const auto& addedId : addedIds) {
            registry_.removeProfile(addedId);
        }
        refreshServerTable();
        QMessageBox::warning(this, tr("Unable to Import Profiles & Preferences"),
                             tr("The imported data could not be saved to this user's application settings."));
        return;
    }

    settings.beginGroup(QStringLiteral("monitoringInclusionV1"));
    for (auto preference = importedPreferences.monitoringInclusion.constBegin();
         preference != importedPreferences.monitoringInclusion.constEnd(); ++preference) {
        const QJsonObject value = preference.value().toObject();
        const QVariantMap finalStored = settings.value(preference.key()).toMap();
        ComputerInclusionPreference stored;
        stored.allComputers = value.value(QStringLiteral("all_computers")).toBool();
        stored.panelVisibilityConfigured = finalStored.contains(QStringLiteral("panelVisible"));
        stored.panelVisible = finalStored.value(QStringLiteral("panelVisible")).toBool();
        const QString importedView = finalStored.value(QStringLiteral("computerView")).toString();
        if (importedView == QStringLiteral("grid") || importedView == QStringLiteral("list")) {
            stored.computerViewIndex = importedView == QStringLiteral("list") ? 1 : 0;
            stored.computerViewConfigured = true;
        }
        const int previewSizeIndex = computerPreviewSizeIndex(
            finalStored.value(QStringLiteral("previewSize")).toString());
        if (previewSizeIndex >= 0) {
            stored.previewSizeIndex = previewSizeIndex;
            stored.previewSizeConfigured = true;
        }
        for (const QJsonValue& id : value.value(QStringLiteral("computer_ids")).toArray()) {
            stored.computerIds.insert(id.toString().toStdString());
        }
        computerInclusionByScope_.insert_or_assign(preference.key().toStdString(), std::move(stored));
    }
    settings.endGroup();
    if (importedPreferences.hasComputerView && computerLayoutStack_ != nullptr) {
        const int viewIndex = importedPreferences.computerView == QStringLiteral("list") ? 1 : 0;
        computerLayoutStack_->setCurrentIndex(viewIndex);
        if (auto* gridAction = findChild<QAction*>(QStringLiteral("actionComputerGrid"))) {
            gridAction->setChecked(viewIndex == 0);
        }
        if (auto* listAction = findChild<QAction*>(QStringLiteral("actionComputerList"))) {
            listAction->setChecked(viewIndex == 1);
        }
    }
    if (importedPreferences.hasComputerPreviewSize) {
        applyComputerPreviewSize(computerPreviewSizeIndex(importedPreferences.computerPreviewSize), false);
    }
    refreshServerTable();
    refreshComputerTable();
    refreshComputerInclusionPanel();
    showWorkspace(1);
    QString importStatus;
    if (!additions.empty()) {
        importStatus = tr("Migration data imported. Verify each newly added server before signing in.");
    } else if (preferenceCount > 0 &&
               (importedPreferences.hasComputerView || importedPreferences.hasComputerPreviewSize ||
                previewSizePreferenceCount > 0)) {
        importStatus = tr("Computer display and scoped preferences imported. Monitoring "
            "preferences apply only to their original server and account IDs.");
    } else if (preferenceCount > 0) {
        importStatus = tr("Scoped monitoring preferences imported. They apply only to their original "
                          "server and account IDs.");
    } else {
        importStatus = tr("Computer display preferences imported.");
    }
    statusBar()->showMessage(importStatus, 6000);
}

void MainWindow::exportProfiles()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Profiles & Preferences"),
        QStringLiteral("Helicoid-manager-migration.json"),
        tr("Helicoid Migration Bundle (*.json);;JSON Files (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QJsonArray profiles;
    for (const auto& profile : registry_.profiles()) {
        profiles.append(profileObject(profile));
    }

    QJsonObject monitoringInclusion;
    QSettings settings;
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        QMessageBox::warning(this, tr("Unable to Export Migration Bundle"),
                             tr("Saved preferences could not be read from this user's application settings."));
        return;
    }
    settings.beginGroup(QStringLiteral("monitoringInclusionV1"));
    QStringList scopeKeys = settings.childKeys();
    scopeKeys.sort(Qt::CaseSensitive);
    for (const QString& scopeKey : scopeKeys) {
        const QVariantMap stored = settings.value(scopeKey).toMap();
        if (!stored.contains(QStringLiteral("allComputers")) ||
            !stored.contains(QStringLiteral("computerIds"))) {
            settings.endGroup();
            QMessageBox::warning(this, tr("Unable to Export Migration Bundle"),
                                 tr("A saved monitoring preference has an unsupported format."));
            return;
        }
        QJsonArray computerIds;
        QStringList ids = stored.value(QStringLiteral("computerIds")).toStringList();
        ids.sort(Qt::CaseSensitive);
        for (const QString& id : ids) {
            computerIds.append(id);
        }
        QJsonObject preference{
            {QStringLiteral("all_computers"), stored.value(QStringLiteral("allComputers")).toBool()},
            {QStringLiteral("computer_ids"), computerIds},
        };
        if (stored.contains(QStringLiteral("panelVisible"))) {
            preference.insert(QStringLiteral("panel_visible"),
                              stored.value(QStringLiteral("panelVisible")).toBool());
        }
        if (stored.contains(QStringLiteral("computerView"))) {
            const QString view = stored.value(QStringLiteral("computerView")).toString();
            if (view != QStringLiteral("grid") && view != QStringLiteral("list")) {
                settings.endGroup();
                QMessageBox::warning(this, tr("Unable to Export Migration Bundle"),
                                     tr("A saved scoped computer view has an unsupported format."));
                return;
            }
            preference.insert(QStringLiteral("computer_view"), view);
        }
        if (stored.contains(QStringLiteral("previewSize"))) {
            const QString previewSize = stored.value(QStringLiteral("previewSize")).toString();
            if (computerPreviewSizeIndex(previewSize) < 0) {
                settings.endGroup();
                QMessageBox::warning(this, tr("Unable to Export Migration Bundle"),
                                     tr("A saved scoped preview size has an unsupported format."));
                return;
            }
            preference.insert(QStringLiteral("preview_size"),
                              computerPreviewSizeName(computerPreviewSizeIndex(previewSize)));
        }
        monitoringInclusion.insert(scopeKey, preference);
    }
    settings.endGroup();

    const bool hasLegacyComputerView = settings.contains(QStringLiteral("computerViewV1"));
    const QString legacyComputerView = settings.value(QStringLiteral("computerViewV1")).toString();
    if (hasLegacyComputerView && legacyComputerView != QStringLiteral("grid") &&
        legacyComputerView != QStringLiteral("list")) {
        QMessageBox::warning(this, tr("Unable to Export Migration Bundle"),
                             tr("The saved legacy computer view preference has an unsupported format."));
        return;
    }
    const bool hasLegacyPreviewSize = settings.contains(QStringLiteral("computerPreviewSizeV1"));
    const QString legacyPreviewSize = settings.value(
        QStringLiteral("computerPreviewSizeV1")).toString();
    if (hasLegacyPreviewSize && computerPreviewSizeIndex(legacyPreviewSize) < 0) {
        QMessageBox::warning(this, tr("Unable to Export Migration Bundle"),
                             tr("The saved legacy computer preview size preference has an unsupported format."));
        return;
    }

    QJsonObject preferences{{QStringLiteral("monitoring_inclusion"), monitoringInclusion}};
    if (hasLegacyComputerView) {
        // Preserve bundles created by the earlier global computer-view format.
        preferences.insert(QStringLiteral("computer_view"), legacyComputerView);
    }
    if (hasLegacyPreviewSize) {
        // Preserve bundles created by the earlier global-preview-size format.
        preferences.insert(QStringLiteral("computer_preview_size"), legacyPreviewSize);
    }

    const QJsonObject document{
        {QStringLiteral("format"), QStringLiteral("helicoid.manager.migration")},
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("profiles"), profiles},
        {QStringLiteral("preferences"), preferences},
    };
    const QByteArray data = QJsonDocument(document).toJson(QJsonDocument::Compact);
    std::vector<ConnectionProfile> validatedProfiles;
    ImportedManagerPreferences validatedPreferences;
    QString validationError;
    if (!parseProfiles(data, validatedProfiles, validationError, &validatedPreferences)) {
        QMessageBox::warning(this, tr("Unable to Export Migration Bundle"), validationError);
        return;
    }
    if (data.size() > maxProfileDocumentBytes) {
        QMessageBox::warning(this, tr("Unable to Export Migration Bundle"),
                             tr("The migration bundle exceeds the allowed size."));
        return;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        file.cancelWriting();
        QMessageBox::warning(this, tr("Unable to Export Migration Bundle"),
                             tr("The migration bundle could not be saved."));
        return;
    }
    statusBar()->showMessage(
        tr("Profiles and preferences exported. Credentials and server trust were excluded."), 6000);
}

void MainWindow::exportAuthorizedInventory()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    std::vector<const ComputerRow*> rows;
    rows.reserve(computers_.size());
    for (const ComputerRow& computer : computers_) {
        const auto* session = sessions_.session(computer.id.deployment);
        if (session != nullptr && session->profileId == computer.profileId &&
            session->expiresAtUnix > now) {
            rows.push_back(&computer);
        }
    }
    if (rows.empty()) {
        statusBar()->showMessage(tr("Sign in and load authorized inventory before exporting."), 6000);
        return;
    }
    if (rows.size() > maxInventoryCsvRows) {
        QMessageBox::warning(this, tr("Inventory Is Too Large"),
                             tr("Export is limited to 10,000 authorized computer records."));
        return;
    }
    std::sort(rows.begin(), rows.end(), [](const ComputerRow* left, const ComputerRow* right) {
        if (left->serverName != right->serverName) return left->serverName < right->serverName;
        if (left->friendlyName != right->friendlyName) return left->friendlyName < right->friendlyName;
        if (left->id.deployment != right->id.deployment) {
            return left->id.deployment.value < right->id.deployment.value;
        }
        return left->id.serverComputerId < right->id.serverComputerId;
    });

    QByteArray csv = QByteArray::fromHex("efbbbf");
    csv += "deployment_id,server,computer_id,friendly_name,platform,enrollment_state,groups\r\n";
    QSet<QString> seen;
    for (const ComputerRow* computer : rows) {
        const QString deploymentId = QString::fromStdString(computer->id.deployment.value);
        const QString computerId = QString::fromStdString(computer->id.serverComputerId);
        const QString scopedKey = deploymentId + QLatin1Char('/') + computerId;
        if (seen.contains(scopedKey)) continue;
        seen.insert(scopedKey);

        QStringList groups;
        for (const ComputerGroupRow& group : computer->groups) {
            groups.append(QString::fromStdString(group.name));
        }
        groups.sort(Qt::CaseInsensitive);
        const QStringList fields{
            deploymentId,
            QString::fromStdString(computer->serverName),
            computerId,
            QString::fromStdString(computer->friendlyName),
            QString::fromStdString(computer->platform),
            QString::fromStdString(computer->enrollmentState),
            groups.join(QStringLiteral("; ")),
        };
        QStringList escaped;
        escaped.reserve(fields.size());
        for (const QString& field : fields) escaped.append(csvField(field));
        csv += escaped.join(QLatin1Char(',')).toUtf8();
        csv += "\r\n";
        if (csv.size() > maxInventoryCsvBytes) {
            QMessageBox::warning(this, tr("Inventory Is Too Large"),
                                 tr("The exported file would exceed the 8 MiB limit."));
            return;
        }
    }

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Authorized Inventory"), QStringLiteral("Helicoid-inventory.csv"),
        tr("CSV files (*.csv)"));
    if (path.isEmpty()) return;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(csv) != csv.size() || !file.commit()) {
        file.cancelWriting();
        QMessageBox::warning(this, tr("Unable to Export Inventory"),
                             tr("The inventory file could not be saved."));
        return;
    }
    statusBar()->showMessage(
        tr("Exported %1 loaded computer records from currently signed-in servers.")
            .arg(static_cast<qulonglong>(seen.size())), 6000);
}

void MainWindow::openSetupDraft()
{
    showWorkspace(4);
    setupName_->setFocus(Qt::OtherFocusReason);
    statusBar()->showMessage(tr("Setup draft opened. No host has been contacted."), 4000);
}

void MainWindow::saveSetupDraft()
{
    const QString name = setupName_->text().trimmed();
    const QString host = setupHost_->text().trimmed();
    const QString dataDirectory = setupDataDirectory_->text().trimmed();
    const QString backupDirectory = setupBackupDirectory_->text().trimmed();
    const QString networkScope = setupNetworkScope_->currentData().toString();
    const QString bindAddress = setupBindAddress_->text().trimmed();
    const QString clientHostname = setupClientHostname_->text().trimmed().toLower();
    const int authorityPort = setupAuthorityPort_->value();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Save Setup Draft"), tr("Enter a deployment name."));
        setupName_->setFocus(Qt::OtherFocusReason);
        return;
    }
    if (host.isEmpty()) {
        QMessageBox::warning(this, tr("Save Setup Draft"), tr("Enter the Linux server address."));
        setupHost_->setFocus(Qt::OtherFocusReason);
        return;
    }
    QString sshAddressError;
    if (!helicoid::manager::parseSSHSetupAddress(host, &sshAddressError)) {
        QMessageBox::warning(this, tr("Save Setup Draft"), sshAddressError);
        setupHost_->setFocus(Qt::OtherFocusReason);
        return;
    }
    const bool privateNetworkScope = networkScope == QStringLiteral("lan") ||
        networkScope == QStringLiteral("private_dns");
    if (privateNetworkScope) {
        if (!isPrivateLanAddress(bindAddress)) {
            QMessageBox::warning(this, tr("Save Setup Draft"),
                                 tr("Enter an RFC1918 IPv4 or unique-local IPv6 address assigned to the server."));
            setupBindAddress_->setFocus(Qt::OtherFocusReason);
            return;
        }
        if (networkScope == QStringLiteral("private_dns") && clientHostname.isEmpty()) {
            QMessageBox::warning(this, tr("Save Setup Draft"),
                                 tr("Private DNS mode requires the Manager DNS hostname."));
            setupClientHostname_->setFocus(Qt::OtherFocusReason);
            return;
        }
        if (!clientHostname.isEmpty() && !isValidClientHostname(clientHostname)) {
            QMessageBox::warning(this, tr("Save Setup Draft"),
                                 tr("Enter a DNS hostname without a URL, port, IP address, localhost name, or trailing dot."));
            setupClientHostname_->setFocus(Qt::OtherFocusReason);
            return;
        }
        if (authorityPort == 8787) {
            QMessageBox::warning(this, tr("Save Setup Draft"),
                                 tr("Choose an HTTPS port other than 8787."));
            setupAuthorityPort_->setFocus(Qt::OtherFocusReason);
            return;
        }
    } else if (networkScope != QStringLiteral("loopback_development")) {
        QMessageBox::warning(this, tr("Save Setup Draft"), tr("Choose a supported network scope."));
        setupNetworkScope_->setFocus(Qt::OtherFocusReason);
        return;
    }
    const auto validLinuxPath = [](const QString& path, const QStringList& roots) {
        if (path.isEmpty() || path == QStringLiteral("/") || !path.startsWith(QLatin1Char('/')) ||
            QDir::cleanPath(path) != path || path.endsWith(QLatin1Char('/')) ||
            path.contains(QStringLiteral("//")) || path.toUtf8().size() > 512) {
            return false;
        }
        const auto segments = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        for (qsizetype index = 1; index < segments.size(); ++index) {
            const QString& segment = segments.at(index);
            if (segment.isEmpty() || segment == QStringLiteral(".") || segment == QStringLiteral("..")) {
                return false;
            }
        }
        for (char rawByte : path.toUtf8()) {
            const auto byte = static_cast<unsigned char>(rawByte);
            const bool allowed = byte == '/' || (byte >= 'a' && byte <= 'z') ||
                (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') ||
                byte == '-' || byte == '_' || byte == '.';
            if (!allowed) return false;
        }
        return std::any_of(roots.cbegin(), roots.cend(), [&path](const QString& root) {
            return path == root || path.startsWith(root + QLatin1Char('/'));
        });
    };
    if (!validLinuxPath(dataDirectory, {QStringLiteral("/var/lib/helicoid/data")})) {
        QMessageBox::warning(this, tr("Save Setup Draft"),
                             tr("Enter a normalized data path under /var/lib/helicoid/data."));
        setupDataDirectory_->setFocus(Qt::OtherFocusReason);
        return;
    }
    if (!validLinuxPath(backupDirectory,
                        {QStringLiteral("/var/lib/helicoid/backups"),
                         QStringLiteral("/var/backups/helicoid")})) {
        QMessageBox::warning(this, tr("Save Setup Draft"),
                             tr("Enter a normalized backup path under /var/lib/helicoid/backups or /var/backups/helicoid."));
        setupBackupDirectory_->setFocus(Qt::OtherFocusReason);
        return;
    }
    const bool pathsOverlap = dataDirectory == backupDirectory ||
        dataDirectory.startsWith(backupDirectory + QLatin1Char('/')) ||
        backupDirectory.startsWith(dataDirectory + QLatin1Char('/'));
    if (pathsOverlap) {
        QMessageBox::warning(this, tr("Save Setup Draft"),
                             tr("Keep server data and backup directories separate."));
        setupBackupDirectory_->setFocus(Qt::OtherFocusReason);
        return;
    }
    const QString mode = setupMode_->currentData().toString();
    if (mode != QStringLiteral("systemd") && mode != QStringLiteral("compose")) {
        QMessageBox::warning(this, tr("Save Setup Draft"),
                             tr("Choose a listed installation type."));
        setupMode_->setFocus(Qt::OtherFocusReason);
        return;
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("serverSetupDraft"));
    settings.setValue(QStringLiteral("name"), name);
    settings.setValue(QStringLiteral("host"), host);
    settings.setValue(QStringLiteral("dataDirectory"), dataDirectory);
    settings.setValue(QStringLiteral("backupDirectory"), backupDirectory);
    settings.setValue(QStringLiteral("networkScope"), networkScope);
    settings.setValue(QStringLiteral("bindAddress"), privateNetworkScope ? bindAddress : QString());
    settings.setValue(QStringLiteral("clientHostname"),
                      privateNetworkScope ? clientHostname : QString());
    settings.setValue(QStringLiteral("authorityPort"),
                      privateNetworkScope ? authorityPort : 8787);
    settings.setValue(QStringLiteral("mode"), mode);
    settings.setValue(QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setupDraftStatus_->setText(tr("The draft could not be saved to this user's application settings."));
        QMessageBox::warning(this, tr("Unable to Save Draft"), setupDraftStatus_->text());
        return;
    }
    setupDraftStatus_->setText(tr("Draft saved locally. No SSH connection or host change was made."));
    statusBar()->showMessage(tr("Server setup draft saved."), 4000);
}

void MainWindow::checkSetupHost()
{
    if (setupCheckHostButton_ == nullptr || setupHostCheckStatus_ == nullptr ||
        !setupCheckHostButton_->isEnabled()) {
        return;
    }
    const auto deploymentKind = setupMode_->currentData().toString() == QStringLiteral("compose")
        ? helicoid::manager::SSHSetupDeploymentKind::Compose
        : helicoid::manager::SSHSetupDeploymentKind::Systemd;
    QString bindAddress;
    const QString networkScope = setupNetworkScope_->currentData().toString();
    const bool privateNetworkScope = networkScope == QStringLiteral("lan") ||
        networkScope == QStringLiteral("private_dns");
    if (privateNetworkScope) {
        QHostAddress parsedAddress;
        if (!isPrivateLanAddress(setupBindAddress_->text().trimmed()) ||
            !parsedAddress.setAddress(setupBindAddress_->text().trimmed())) {
            setupHostCheckStatus_->setText(
                tr("Enter the server's RFC1918 or unique-local IPv6 address before checking the host."));
            setupBindAddress_->setFocus(Qt::OtherFocusReason);
            return;
        }
        bindAddress = parsedAddress.toString();
        if (setupAuthorityPort_->value() == 8787) {
            setupHostCheckStatus_->setText(tr("Choose an HTTPS port other than 8787."));
            setupAuthorityPort_->setFocus(Qt::OtherFocusReason);
            return;
        }
        const QString hostname = setupClientHostname_->text().trimmed().toLower();
        if ((networkScope == QStringLiteral("private_dns") && hostname.isEmpty()) ||
            (!hostname.isEmpty() && !isValidClientHostname(hostname))) {
            setupHostCheckStatus_->setText(networkScope == QStringLiteral("private_dns")
                ? tr("Enter the required private DNS hostname.")
                : tr("Enter a valid DNS hostname, or leave it empty."));
            setupClientHostname_->setFocus(Qt::OtherFocusReason);
            return;
        }
    }
    if (!helicoid::manager::validSSHSetupDataDirectory(setupDataDirectory_->text())) {
        setupHostCheckStatus_->setText(
            tr("Enter a normalized data directory under /var/lib/helicoid/data before checking host capacity."));
        setupDataDirectory_->setFocus(Qt::OtherFocusReason);
        return;
    }
    QString error;
    const auto address = helicoid::manager::parseSSHSetupAddress(setupHost_->text(), &error);
    if (!address) {
        setupHostCheckStatus_->setText(error);
        setupHost_->setFocus(Qt::OtherFocusReason);
        return;
    }

    setupCheckHostButton_->setEnabled(false);
    setupHostCheckStatus_->setText(
        tr("Checking the host through OpenSSH. Existing known_hosts trust and key authentication are required. This check makes no host changes."));
    const auto authorityPort = static_cast<std::uint16_t>(setupAuthorityPort_->value());
    helicoid::manager::runSSHSetupPreflight(
        this, *address, authorityPort, deploymentKind, bindAddress,
        setupDataDirectory_->text().trimmed(),
        [this](helicoid::manager::SSHSetupCheckResult result) {
            if (setupCheckHostButton_ != nullptr) {
                setupCheckHostButton_->setEnabled(true);
            }
            if (setupHostCheckStatus_ != nullptr) {
                setupHostCheckStatus_->setText(result.report);
            }
            statusBar()->showMessage(
                result.succeeded ? tr("Read-only SSH host check completed.")
                                 : tr("SSH host check did not complete."),
                5000);
        });
}

void MainWindow::checkSelectedServerAvailability()
{
    const auto selectedId = selectedProfileId();
    if (!selectedId) {
        statusBar()->showMessage(tr("Select a server profile first."), 3000);
        return;
    }
    if (signInsInProgress_.contains(*selectedId)) {
        statusBar()->showMessage(tr("Sign-in is already in progress for this server."), 4000);
        return;
    }
    const auto profile = registry_.profile(*selectedId);
    if (!profile) {
        refreshServerTable();
        return;
    }
    const auto healthEndpoint = apiEndpoint(*profile, QStringLiteral("/health"));
    if (!healthEndpoint) {
        statusBar()->showMessage(
            tr("This server URL is invalid or insecure. Use HTTPS, or HTTP on a loopback development server."),
            7000);
        return;
    }
    const auto stamp = registry_.beginAvailabilityCheck(*selectedId);
    if (!stamp) {
        statusBar()->showMessage(tr("This profile cannot start another availability check."), 4000);
        return;
    }
    serverHealthByProfile_.erase(*selectedId);
    refreshServerTable();

    const bool secureEndpoint =
        healthEndpoint->scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
    QNetworkReply* reply = network_.get(apiRequest(*healthEndpoint));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) {
                    reply->abort();
                }
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, stamp = *stamp, secureEndpoint] {
        if (!registry_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }

        QString failure;
        QString healthDetails;
        Availability availability = Availability::Unreachable;
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            failure = reply->errorString();
        } else if (statusCode < 200 || statusCode >= 300) {
            failure = tr("The server returned HTTP %1.").arg(statusCode);
        } else {
            const QByteArray body = reply->readAll();
            if (body.size() > maxHealthResponseBytes) {
                failure = tr("The health response exceeded the allowed size.");
            } else {
                QJsonParseError parseError;
                const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
                const QJsonObject health = document.object();
                const auto schema = health.value(QStringLiteral("schema_version"));
                const auto status = health.value(QStringLiteral("status"));
                const auto accepting = health.value(QStringLiteral("accepting_authorized_operations"));
                if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
                    !schema.isDouble() || schema.toInt(-1) != 1 || !status.isString() ||
                    !accepting.isBool()) {
                    failure = tr("The server returned an unsupported health response.");
                } else if (status.toString() == QStringLiteral("ok") && accepting.toBool()) {
                    availability = Availability::Reachable;
                } else if (status.toString() == QStringLiteral("degraded") && !accepting.toBool()) {
                    availability = Availability::Degraded;
                } else {
                    failure = tr("The server health state is not recognized.");
                }
                if (failure.isEmpty()) {
                    QStringList details;
                    details.append(tr("Checked: %1")
                        .arg(QDateTime::currentDateTime().toLocalTime().toString(Qt::ISODate)));
                    details.append(tr("Public health report: %1; authorized operations: %2")
                        .arg(status.toString(), accepting.toBool() ? tr("accepting") : tr("paused")));
                    details.append(tr("API schema: %1").arg(schema.toInt()));
                    const auto storageSchema = health.value(QStringLiteral("storage_schema_version"));
                    if (storageSchema.isDouble() && storageSchema.toInt(-1) >= 0) {
                        details.append(tr("Storage schema: %1").arg(storageSchema.toInt()));
                    }
                    const auto bootstrap = health.value(QStringLiteral("bootstrap_completed"));
                    if (bootstrap.isBool()) {
                        details.append(tr("Administrator bootstrap: %1")
                            .arg(bootstrap.toBool() ? tr("complete") : tr("pending")));
                    }
                    const QJsonObject features = health.value(QStringLiteral("features")).toObject();
                    const auto appendFeature = [&features, &details](const QString& key, const QString& label) {
                        const auto value = features.value(key);
                        if (value.isBool()) {
                            details.append(QObject::tr("%1: %2")
                                .arg(label, value.toBool() ? QObject::tr("available")
                                                          : QObject::tr("unavailable")));
                        }
                    };
                    appendFeature(QStringLiteral("local_accounts"), tr("Local accounts"));
                    appendFeature(QStringLiteral("manager_enrollment"), tr("Manager enrollment"));
                    appendFeature(QStringLiteral("scoped_inventory"), tr("Scoped inventory"));
                    appendFeature(QStringLiteral("mfa"), tr("MFA"));
                    appendFeature(QStringLiteral("file_jobs"), tr("File jobs"));
                    appendFeature(QStringLiteral("security_recovery"), tr("Security recovery"));
                    appendFeature(QStringLiteral("remote_control"), tr("Remote control"));
                    appendFeature(QStringLiteral("screen_observation"), tr("Screen observation"));
                    if (secureEndpoint) {
                        const QSslConfiguration tls = reply->sslConfiguration();
                        const QSslCipher cipher = tls.sessionCipher();
                        if (!cipher.isNull() && !cipher.protocolString().isEmpty()) {
                            details.append(tr("TLS protocol: %1").arg(cipher.protocolString()));
                        } else {
                            details.append(tr("TLS protocol: unavailable."));
                        }

                        const auto peerChain = tls.peerCertificateChain();
                        if (peerChain.isEmpty()) {
                            details.append(tr("TLS transport peer certificate: unavailable."));
                        } else {
                            const QByteArray fingerprint =
                                peerChain.constFirst().digest(QCryptographicHash::Sha256).toHex();
                            if (fingerprint.isEmpty()) {
                                details.append(
                                    tr("TLS transport leaf certificate fingerprint: unavailable."));
                            } else {
                                details.append(tr("TLS transport leaf certificate SHA-256: %1")
                                                   .arg(QString::fromLatin1(fingerprint)));
                            }
                        }
                        details.append(
                            tr("TLS certificate identity and deployment signing-key identity are separate checks."));
                    } else {
                        details.append(tr("HTTP loopback development endpoint; no TLS certificate was used."));
                    }
                    healthDetails = details.join(QLatin1Char('\n'));
                }
            }
        }

        registry_.finishAvailabilityCheck(stamp, availability);
        if (availability == Availability::Unreachable || healthDetails.isEmpty()) {
            serverHealthByProfile_.erase(stamp.profileId);
        } else {
            serverHealthByProfile_[stamp.profileId] = std::move(healthDetails);
        }
        refreshServerTable();
        if (availability == Availability::Unreachable) {
            statusBar()->showMessage(tr("Availability check failed: %1").arg(failure), 8000);
        } else if (availability == Availability::Degraded) {
            statusBar()->showMessage(tr("The server is reachable but needs setup or recovery."), 6000);
        } else {
            statusBar()->showMessage(tr("The server is reachable. Sign-in and endpoint authorization are separate."), 6000);
        }
        reply->deleteLater();
    });
}

void MainWindow::verifySelectedServerIdentity()
{
    const auto selectedId = selectedProfileId();
    if (!selectedId) {
        statusBar()->showMessage(tr("Select a server profile first."), 4000);
        return;
    }
    const auto profile = registry_.profile(*selectedId);
    if (!profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 5000);
        return;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/deployment/authority"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }

    const auto trustRequest = registry_.beginTrustVerification(*selectedId);
    if (!trustRequest) {
        statusBar()->showMessage(tr("This server profile cannot start another identity verification."), 6000);
        return;
    }
    serverHealthByProfile_.erase(*selectedId);
    refreshServerDetails();

    const QString endpointSnapshot = QString::fromStdString(profile->endpoint);
    QNetworkReply* reply = network_.get(apiRequest(*endpoint));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId = *selectedId, endpointSnapshot, trustRequest = *trustRequest] {
        if (shutdownInProgress_) {
            reply->deleteLater();
            return;
        }
        const auto currentProfile = registry_.profile(profileId);
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (!currentProfile || QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
            !registry_.isCurrent(trustRequest)) {
            statusBar()->showMessage(tr("The server profile or identity verification changed before the response completed."), 6000);
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(tr("Could not read the server deployment identity: %1")
                                         .arg(replyFailure(reply)), 9000);
            reply->deleteLater();
            return;
        }

        const auto authority = parseDeploymentAuthority(body);
        if (!authority) {
            statusBar()->showMessage(tr("The server returned an invalid deployment authority."), 8000);
            reply->deleteLater();
            return;
        }
        reply->deleteLater();

        const QString previousTrust = QString::fromStdString(currentProfile->trustedDeploymentId);
        const QString previousKey = QString::fromStdString(
            currentProfile->trustedGrantSigningPublicKey);
        const quint64 previousGeneration = currentProfile->trustedAuthorityGeneration;
        if (previousTrust == authority->deploymentId &&
            previousKey == authority->grantSigningPublicKey &&
            previousGeneration == authority->authorityGeneration) {
            serverAuthorityMismatches_.erase(profileId);
            deploymentsByProfile_[profileId] =
                DeploymentId{authority->deploymentId.toStdString()};
            refreshServerTable();
            statusBar()->showMessage(
                tr("The server identity, authority key, and generation %1 match this profile's saved verification.")
                    .arg(authority->authorityGeneration), 7000);
            return;
        }
        if (previousTrust == authority->deploymentId && previousGeneration > 0 &&
            (authority->authorityGeneration < previousGeneration ||
             (authority->authorityGeneration == previousGeneration &&
              previousKey != authority->grantSigningPublicKey))) {
            QMessageBox::warning(
                this, tr("Authority Checkpoint Rejected"),
                tr("This server returned an older authority generation or changed its key without advancing the generation. The saved checkpoint is %1. Use the documented local recovery process to review this server.")
                    .arg(previousGeneration));
            return;
        }
        const bool authorityChanged =
            previousTrust != authority->deploymentId ||
            previousKey != authority->grantSigningPublicKey ||
            previousGeneration != authority->authorityGeneration;
        if (authorityChanged && !previousTrust.isEmpty()) {
            serverAuthorityMismatches_.insert(profileId);
            invalidateProfileSessionLocally(
                profileId, DeploymentId{previousTrust.toStdString()});
        }
        const QString currentFingerprint =
            grantSigningKeyFingerprint(authority->grantSigningPublicKey);
        const QString warning = previousTrust.isEmpty()
            ? tr("Ask the server administrator to read the authority locally with `curl --fail http://127.0.0.1:8787/v2/deployment/authority`. They can calculate the key fingerprint with `curl --fail http://127.0.0.1:8787/v2/deployment/authority | jq -r '.grant_signing_public_key' | xxd -r -p | openssl dgst -sha256 | awk '{print $NF}'`. Compare the deployment ID, key fingerprint, and generation through a trusted organization channel. Do not trust values copied from this connection alone.")
            : tr("This profile has a different identity, key, or authority generation. Restore or cutover may rotate them. Replace the local trust checkpoint only after confirming the deployment ID, key fingerprint, and generation through a trusted organization channel. The previous checkpoint was %1 generation %2.")
                  .arg(previousTrust, QString::number(previousGeneration));
        bool accepted = false;
        const QString enteredIdentity = QInputDialog::getText(
            this, tr("Verify Server Identity"),
            tr("Server: %1\nCurrent deployment ID: %2\n\n%3\n\nEnter the trusted deployment ID to continue.")
                .arg(QString::fromStdString(currentProfile->displayName),
                     authority->deploymentId, warning),
            QLineEdit::Normal, QString(), &accepted).trimmed();
        if (!accepted) {
            return;
        }
        if (!validDeploymentIdentity(enteredIdentity) ||
            enteredIdentity != authority->deploymentId) {
            QMessageBox::warning(this, tr("Server Identity Not Verified"),
                                 tr("The entered deployment ID did not match the server. No credentials or recovery material were sent."));
            return;
        }

        const QString enteredFingerprint = QInputDialog::getText(
            this, tr("Verify Authority Key"),
            tr("Current grant authority key fingerprint (SHA-256):\n%1\n\nEnter the fingerprint supplied through your trusted organization channel.")
                .arg(currentFingerprint),
            QLineEdit::Normal, QString(), &accepted).trimmed().toLower();
        if (!accepted) {
            return;
        }
        if (enteredFingerprint != currentFingerprint) {
            QMessageBox::warning(this, tr("Authority Key Not Verified"),
                                 tr("The entered fingerprint did not match the server. No credentials or recovery material were sent."));
            return;
        }

        const QString enteredGeneration = QInputDialog::getText(
            this, tr("Verify Authority Generation"),
            tr("Current authority generation: %1\n\nEnter the generation supplied through your trusted organization channel.")
                .arg(authority->authorityGeneration),
            QLineEdit::Normal, QString(), &accepted).trimmed();
        bool generationOk = false;
        const quint64 trustedGeneration = enteredGeneration.toULongLong(&generationOk, 10);
        if (!accepted) {
            return;
        }
        if (!generationOk || trustedGeneration != authority->authorityGeneration) {
            QMessageBox::warning(this, tr("Authority Generation Not Verified"),
                                 tr("The entered generation did not match the server. The saved trust checkpoint was not changed."));
            return;
        }

        if (!registry_.isCurrent(trustRequest)) {
            statusBar()->showMessage(tr("A newer identity verification replaced this result; verify the server again."), 7000);
            return;
        }

        const bool hadPreviousTrust = !currentProfile->trustedDeploymentId.empty();
        if (!registry_.setTrustedAuthority(
                profileId, authority->deploymentId.toStdString(),
                authority->grantSigningPublicKey.toStdString(),
                authority->authorityGeneration)) {
            statusBar()->showMessage(tr("The server identity and authority key could not be saved to this profile."), 7000);
            return;
        }
        if (!saveProfiles()) {
            if (hadPreviousTrust) {
                if (helicoid::manager::validGrantSigningPublicKey(previousKey.toStdString())) {
                    registry_.restoreTrustedAuthorityState(
                        profileId, previousTrust.toStdString(), previousKey.toStdString(),
                        previousGeneration);
                } else {
                    registry_.setTrustedDeploymentId(profileId, previousTrust.toStdString());
                }
            } else {
                registry_.clearTrustedDeploymentId(profileId);
            }
            refreshServerTable();
            const QString detail = hadPreviousTrust
                ? tr("The previous saved identity remains in use. Verify again before connecting to a server with another ID.")
                : tr("This profile remains unverified.");
            QMessageBox::warning(this, tr("Unable to Save Server Identity"),
                                 tr("The verified identity could not be saved. %1").arg(detail));
            return;
        }
        serverAuthorityMismatches_.erase(profileId);
        deploymentsByProfile_[profileId] =
            DeploymentId{authority->deploymentId.toStdString()};
        refreshServerTable();
        statusBar()->showMessage(
            tr("Server identity and authority generation %1 verified and saved for this profile.")
                .arg(authority->authorityGeneration), 7000);
    });
}

void MainWindow::manageAuthorityCutover()
{
    const auto selectedId = selectedProfileId();
    if (!selectedId) {
        statusBar()->showMessage(tr("Select a server profile first."), 4000);
        return;
    }
    const auto profile = registry_.profile(*selectedId);
    const auto deploymentEntry = deploymentsByProfile_.find(*selectedId);
    if (!profile || deploymentEntry == deploymentsByProfile_.end() ||
        !hasTrustedDeploymentAuthority(*profile) ||
        profile->trustedDeploymentId != deploymentEntry->second.value) {
        statusBar()->showMessage(
            tr("Verify and sign in to the selected server before reviewing authority cutover."), 7000);
        return;
    }
    const DeploymentId deployment = deploymentEntry->second;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const auto* session = sessions_.session(deployment, now);
    const auto stamp = sessions_.currentStamp(deployment, now);
    if (session == nullptr || !stamp) {
        statusBar()->showMessage(tr("Sign in to the selected server before reviewing authority cutover."), 7000);
        return;
    }

    const std::string profileId = *selectedId;
    const QString serverName = QString::fromStdString(profile->displayName);
    checkPinnedDeploymentIdentity(profileId, [this, profileId, deployment, stamp = *stamp,
                                               serverName] {
        if (shutdownInProgress_ ||
            !sessions_.isCurrent(stamp, QDateTime::currentSecsSinceEpoch())) {
            statusBar()->showMessage(tr("The server session expired before cutover review opened."), 7000);
            return;
        }

        const auto currentProfile = registry_.profile(profileId);
        if (!currentProfile || !hasTrustedDeploymentAuthority(*currentProfile) ||
            currentProfile->trustedDeploymentId != deployment.value) {
            statusBar()->showMessage(tr("The saved server identity changed before cutover review opened."), 7000);
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Authority Cutover · %1").arg(serverName));
        dialog.setWindowModality(Qt::WindowModal);
        dialog.setMinimumWidth(600);
        auto* layout = new QVBoxLayout(&dialog);
        auto* heading = new QLabel(tr("Authority status"), &dialog);
        QFont headingFont = heading->font();
        headingFont.setBold(true);
        headingFont.setPointSize(headingFont.pointSize() + 2);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto* statusLabel = new QLabel(tr("Loading authority status…"), &dialog);
        statusLabel->setObjectName(QStringLiteral("authorityCutoverStatus"));
        statusLabel->setTextFormat(Qt::PlainText);
        statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        statusLabel->setWordWrap(true);
        layout->addWidget(statusLabel);

        auto* limitation = new QLabel(
            tr("Approval records the reviewed host-fencing method and evidence reference. The server cannot independently verify that the old host is stopped or that endpoint trust has been updated."),
            &dialog);
        limitation->setWordWrap(true);
        layout->addWidget(limitation);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        auto* refreshButton = buttons->addButton(tr("Refresh"), QDialogButtonBox::ActionRole);
        auto* drainButton = buttons->addButton(tr("Drain Authority"), QDialogButtonBox::ActionRole);
        auto* approveButton = buttons->addButton(tr("Approve Replacement"), QDialogButtonBox::ActionRole);
        auto* cancelButton = buttons->addButton(tr("Cancel Drain"), QDialogButtonBox::ActionRole);
        auto* releaseRecoveryButton = buttons->addButton(
            tr("Release Recovery Quarantine"), QDialogButtonBox::ActionRole);
        refreshButton->setObjectName(QStringLiteral("refreshAuthorityCutover"));
        drainButton->setObjectName(QStringLiteral("drainAuthority"));
        approveButton->setObjectName(QStringLiteral("approveAuthorityCutover"));
        cancelButton->setObjectName(QStringLiteral("cancelAuthorityCutover"));
        releaseRecoveryButton->setObjectName(QStringLiteral("releaseRecoveryQuarantine"));
        drainButton->setEnabled(false);
        approveButton->setEnabled(false);
        cancelButton->setEnabled(false);
        releaseRecoveryButton->setEnabled(false);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

        const QPointer<QDialog> dialogGuard(&dialog);
        const QPointer<QLabel> statusGuard(statusLabel);
        const QPointer<QPushButton> refreshGuard(refreshButton);
        const QPointer<QPushButton> drainGuard(drainButton);
        const QPointer<QPushButton> approveGuard(approveButton);
        const QPointer<QPushButton> cancelGuard(cancelButton);
        const QPointer<QPushButton> releaseRecoveryGuard(releaseRecoveryButton);
        auto status = std::make_shared<QJsonObject>();
        auto recoveryStatus = std::make_shared<QJsonObject>();
        auto cutoverStatusValid = std::make_shared<bool>(false);
        auto statusGeneration = std::make_shared<std::uint64_t>(0);

        using CutoverCompletion = std::function<void(bool, QJsonObject)>;
        const auto issueRequest = [this, profileId, deployment, stamp,
                                   endpointSnapshot = currentProfile->endpoint,
                                   trustedIdentity = currentProfile->trustedDeploymentId,
                                   trustedKey = currentProfile->trustedGrantSigningPublicKey,
                                   dialogGuard, statusGuard, drainGuard, approveGuard,
                                   cancelGuard, releaseRecoveryGuard](const QByteArray& method,
                                                            const QString& path,
                                                            const QJsonObject& payload,
                                                            CutoverCompletion completion) {
            const qint64 requestNow = QDateTime::currentSecsSinceEpoch();
            const auto* currentSession = sessions_.session(deployment, requestNow);
            const auto currentProfile = registry_.profile(profileId);
            if (!dialogGuard || !currentProfile || currentSession == nullptr ||
                !sessions_.isCurrent(stamp, requestNow) ||
                currentSession->profileId != profileId ||
                currentProfile->endpoint != endpointSnapshot ||
                currentProfile->trustedDeploymentId != trustedIdentity ||
                currentProfile->trustedGrantSigningPublicKey != trustedKey ||
                currentProfile->endpoint.empty()) {
                if (statusGuard) {
                    statusGuard->setText(tr("The verified server profile or signed-in session changed. Close this dialog and sign in again."));
                }
                if (drainGuard) drainGuard->setEnabled(false);
                if (approveGuard) approveGuard->setEnabled(false);
                if (cancelGuard) cancelGuard->setEnabled(false);
                if (releaseRecoveryGuard) releaseRecoveryGuard->setEnabled(false);
                completion(false, {});
                return;
            }
            if (method != QByteArrayLiteral("GET")) {
                if (drainGuard) drainGuard->setEnabled(false);
                if (approveGuard) approveGuard->setEnabled(false);
                if (cancelGuard) cancelGuard->setEnabled(false);
                if (releaseRecoveryGuard) releaseRecoveryGuard->setEnabled(false);
            }
            const auto endpoint = apiEndpoint(*currentProfile, path);
            if (!endpoint) {
                if (statusGuard) statusGuard->setText(tr("The saved server address is invalid."));
                completion(false, {});
                return;
            }

            QNetworkRequest request = apiRequest(*endpoint);
            QByteArray authorization = QByteArrayLiteral("Bearer ") +
                QByteArray::fromStdString(currentSession->bearerToken);
            request.setRawHeader("Authorization", authorization);
            authorization.fill('\0');
            QNetworkReply* reply = nullptr;
            if (method == QByteArrayLiteral("GET")) {
                reply = network_.get(request);
            } else {
                request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
                QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
                reply = network_.sendCustomRequest(request, method, body);
                body.fill('\0');
            }
            reply->setReadBufferSize(maxHealthResponseBytes);
            QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
            connect(reply, &QNetworkReply::downloadProgress, reply,
                    [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
            connect(reply, &QNetworkReply::finished, this,
                    [this, reply, dialogGuard, statusGuard,
                     completion = std::move(completion)]() mutable {
                const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                SensitiveBuffer response(reply->readAll());
                const bool transportOk = reply->error() == QNetworkReply::NoError &&
                    statusCode == 200 && response.bytes().size() <= maxHealthResponseBytes;
                QString failure = replyFailure(reply);
                if (!transportOk && response.bytes().size() <= maxHealthResponseBytes) {
                    QJsonParseError errorParse;
                    const QJsonDocument errorDocument =
                        QJsonDocument::fromJson(response.bytes(), &errorParse);
                    if (errorParse.error == QJsonParseError::NoError && errorDocument.isObject()) {
                        const QString message = errorDocument.object()
                            .value(QStringLiteral("error")).toObject()
                            .value(QStringLiteral("message")).toString();
                        if (!message.isEmpty() && message.size() <= 512 &&
                            std::none_of(message.cbegin(), message.cend(),
                                         isControlCharacter)) {
                            failure = message;
                        }
                    }
                }
                reply->deleteLater();
                if (!dialogGuard || shutdownInProgress_) {
                    completion(false, {});
                    return;
                }
                if (!transportOk) {
                    if (statusGuard) {
                        statusGuard->setText(tr("The authority request failed: %1").arg(failure));
                    }
                    completion(false, {});
                    return;
                }
                QJsonParseError parseError;
                const QJsonDocument document = QJsonDocument::fromJson(response.bytes(), &parseError);
                if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                    if (statusGuard) statusGuard->setText(tr("The server returned an invalid authority response."));
                    completion(false, {});
                    return;
                }
                completion(true, document.object());
            });
        };

        const auto renderStatus = [statusGuard, drainGuard, approveGuard, cancelGuard,
                                   releaseRecoveryGuard,
                                   status, cutoverStatusValid,
                                   expectedDeployment = QString::fromStdString(deployment.value)](
                                      bool ok, const QJsonObject& response) {
            if (!statusGuard || !drainGuard || !approveGuard || !cancelGuard ||
                !releaseRecoveryGuard) return;
            *cutoverStatusValid = false;
            if (!ok) {
                drainGuard->setEnabled(false);
                approveGuard->setEnabled(false);
                cancelGuard->setEnabled(false);
                releaseRecoveryGuard->setEnabled(false);
                return;
            }
            const QString authorityId = response.value(QStringLiteral("authority_id")).toString();
            const QString authorityState = response.value(QStringLiteral("authority_state")).toString();
            const QJsonValue generationValue = response.value(QStringLiteral("authority_generation"));
            const QJsonValue predecessorValue = response.value(QStringLiteral("predecessor_authority_id"));
            const QJsonValue cutoverValue = response.value(QStringLiteral("cutover"));
            const bool validState = authorityState == QStringLiteral("active") ||
                authorityState == QStringLiteral("draining") ||
                authorityState == QStringLiteral("awaiting_cutover") ||
                authorityState == QStringLiteral("retired") ||
                authorityState == QStringLiteral("needs_review");
            const double generation = generationValue.toDouble(-1);
            if (!validDeploymentIdentity(authorityId) || authorityId != expectedDeployment ||
                !generationValue.isDouble() ||
                generation < 1 || std::floor(generation) != generation || !validState ||
                (!predecessorValue.isNull() &&
                 (!predecessorValue.isString() ||
                  !validDeploymentIdentity(predecessorValue.toString()))) ||
                (!cutoverValue.isNull() && !cutoverValue.isObject())) {
                statusGuard->setText(QObject::tr("The server returned incomplete or invalid cutover status."));
                drainGuard->setEnabled(false);
                approveGuard->setEnabled(false);
                cancelGuard->setEnabled(false);
                releaseRecoveryGuard->setEnabled(false);
                return;
            }
            const QJsonObject cutover = cutoverValue.toObject();
            const QJsonValue cutoverGenerationValue = cutover.value(QStringLiteral("generation"));
            const double cutoverGeneration = cutoverGenerationValue.toDouble(-1);
            if (cutoverValue.isObject() &&
                (!validUuid(cutover.value(QStringLiteral("id")).toString()) ||
                 !validDeploymentIdentity(cutover.value(QStringLiteral("predecessor_authority_id")).toString()) ||
                 (!cutover.value(QStringLiteral("replacement_authority_id")).isNull() &&
                  (!validDeploymentIdentity(cutover.value(QStringLiteral("replacement_authority_id")).toString()) ||
                   cutover.value(QStringLiteral("replacement_authority_id")).toString() != authorityId)) ||
                 !cutoverGenerationValue.isDouble() || cutoverGeneration < 1 ||
                 std::floor(cutoverGeneration) != cutoverGeneration ||
                 !cutover.value(QStringLiteral("state")).isString())) {
                statusGuard->setText(QObject::tr("The server returned invalid cutover review details."));
                drainGuard->setEnabled(false);
                approveGuard->setEnabled(false);
                cancelGuard->setEnabled(false);
                releaseRecoveryGuard->setEnabled(false);
                return;
            }
            *status = response;
            *cutoverStatusValid = true;
            QString details = QObject::tr("Authority: %1\nState: %2\nGeneration: %3")
                                  .arg(authorityId, authorityState,
                                       QString::number(generation, 'f', 0));
            if (predecessorValue.isString()) {
                details += QObject::tr("\nPredecessor: %1").arg(predecessorValue.toString());
            }
            if (!cutover.isEmpty()) {
                details += QObject::tr("\n\nCutover ID: %1\nCutover state: %2\nPredecessor: %3\nReplacement: %4\nGeneration: %5")
                    .arg(cutover.value(QStringLiteral("id")).toString(),
                         cutover.value(QStringLiteral("state")).toString(),
                         cutover.value(QStringLiteral("predecessor_authority_id")).toString(),
                         cutover.value(QStringLiteral("replacement_authority_id")).toString())
                    .arg(QString::number(cutoverGeneration, 'f', 0));
                if (cutover.value(QStringLiteral("fencing_method")).isString()) {
                    details += QObject::tr("\nFencing method: %1")
                                   .arg(cutover.value(QStringLiteral("fencing_method")).toString());
                }
                if (cutover.value(QStringLiteral("fencing_evidence_reference")).isString()) {
                    details += QObject::tr("\nEvidence reference: %1")
                                   .arg(cutover.value(QStringLiteral("fencing_evidence_reference")).toString());
                }
            } else {
                details += QObject::tr("\nNo cutover review is recorded.");
            }
            statusGuard->setText(details);
            const QString cutoverState = cutover.value(QStringLiteral("state")).toString();
            drainGuard->setEnabled(authorityState == QStringLiteral("active"));
            approveGuard->setEnabled(authorityState == QStringLiteral("awaiting_cutover") &&
                                      cutoverState == QStringLiteral("awaiting_review"));
            cancelGuard->setEnabled(authorityState == QStringLiteral("draining") &&
                                     cutoverState == QStringLiteral("draining"));
        };

        const auto renderRecoveryStatus = [statusGuard, releaseRecoveryGuard, status,
                                           cutoverStatusValid,
                                           recoveryStatus,
                                           expectedDeployment = QString::fromStdString(deployment.value)](
                                              bool ok, const QJsonObject& response) {
            if (!statusGuard || !releaseRecoveryGuard) return;
            if (!ok || !*cutoverStatusValid) {
                releaseRecoveryGuard->setEnabled(false);
                return;
            }
            const QJsonValue deploymentValue = response.value(QStringLiteral("deployment_id"));
            const QJsonValue quarantinedValue = response.value(QStringLiteral("recovery_quarantined"));
            const QJsonValue accountCount = response.value(QStringLiteral("quarantined_account_count"));
            const QJsonValue managerCount = response.value(QStringLiteral("revoked_manager_count"));
            const QJsonValue endpointCount = response.value(QStringLiteral("quarantined_endpoint_count"));
            const auto validCount = [](const QJsonValue& value) {
                const double count = value.toDouble(-1);
                return value.isDouble() && std::isfinite(count) && count >= 0 &&
                    count <= 9007199254740991.0 && std::floor(count) == count;
            };
            if (!deploymentValue.isString() || deploymentValue.toString() != expectedDeployment ||
                !quarantinedValue.isBool() || !validCount(accountCount) ||
                !validCount(managerCount) || !validCount(endpointCount)) {
                statusGuard->setText(QObject::tr("The server returned an invalid recovery-quarantine summary."));
                releaseRecoveryGuard->setEnabled(false);
                return;
            }
            *recoveryStatus = response;
            QString details = statusGuard->text();
            details += QObject::tr("\n\nRecovery quarantine: %1\nQuarantined accounts: %2\nRevoked management devices: %3\nQuarantined endpoints: %4")
                           .arg(quarantinedValue.toBool() ? QObject::tr("active") : QObject::tr("released"))
                           .arg(QString::number(accountCount.toDouble(), 'f', 0),
                                QString::number(managerCount.toDouble(), 'f', 0),
                                QString::number(endpointCount.toDouble(), 'f', 0));
            statusGuard->setText(details);
            const bool authorityActive = status->value(QStringLiteral("authority_state")).toString() ==
                QStringLiteral("active");
            releaseRecoveryGuard->setEnabled(quarantinedValue.toBool() && authorityActive);
        };

        const auto refresh = std::make_shared<std::function<void()>>();
        *refresh = [issueRequest, renderStatus, renderRecoveryStatus, refreshGuard, statusGuard,
                    drainGuard, approveGuard, cancelGuard, releaseRecoveryGuard,
                    cutoverStatusValid, statusGeneration] {
            if (*statusGeneration == std::numeric_limits<std::uint64_t>::max()) {
                if (statusGuard) statusGuard->setText(QObject::tr("Authority refresh limit reached. Close and reopen this workflow."));
                return;
            }
            const std::uint64_t requestGeneration = ++(*statusGeneration);
            *cutoverStatusValid = false;
            if (refreshGuard) refreshGuard->setEnabled(false);
            if (statusGuard) statusGuard->setText(QObject::tr("Loading authority status…"));
            if (drainGuard) drainGuard->setEnabled(false);
            if (approveGuard) approveGuard->setEnabled(false);
            if (cancelGuard) cancelGuard->setEnabled(false);
            if (releaseRecoveryGuard) releaseRecoveryGuard->setEnabled(false);
            issueRequest(QByteArrayLiteral("GET"), QStringLiteral("/v2/authority/cutover"), {},
                         [issueRequest, renderStatus, renderRecoveryStatus, refreshGuard,
                          statusGeneration, requestGeneration](
                             bool ok, QJsonObject response) {
                if (*statusGeneration != requestGeneration) return;
                renderStatus(ok, response);
                if (!ok) {
                    if (refreshGuard) refreshGuard->setEnabled(true);
                    return;
                }
                issueRequest(QByteArrayLiteral("GET"), QStringLiteral("/v2/recovery/quarantine"), {},
                             [renderRecoveryStatus, refreshGuard, statusGeneration,
                              requestGeneration](bool recoveryOk, QJsonObject recovery) {
                    if (*statusGeneration != requestGeneration) return;
                    renderRecoveryStatus(recoveryOk, recovery);
                    if (refreshGuard) refreshGuard->setEnabled(true);
                });
            });
        };

        const auto requireFreshCutoverMfa = [this, deployment, stamp] {
            const qint64 current = QDateTime::currentSecsSinceEpoch();
            const auto* currentSession = sessions_.session(deployment, current);
            return currentSession != nullptr && sessions_.isCurrent(stamp, current) &&
                currentSession->mfaAssuredAtUnix <= current &&
                currentSession->mfaAssuredAtUnix >= current - 120;
        };
        const auto reportFreshMfaRequirement = [this](const QString& action) {
            QMessageBox::information(
                this, tr("Fresh MFA Required"),
                tr("Sign out of this server and sign in again with MFA, then retry %1. The server accepts MFA assurance from the last 120 seconds only.")
                    .arg(action));
        };

        connect(refreshButton, &QPushButton::clicked, &dialog, [refresh] {
            if (*refresh) (*refresh)();
        });
        connect(drainButton, &QPushButton::clicked, &dialog,
                [this, &dialog, status, issueRequest, refresh, requireFreshCutoverMfa,
                 reportFreshMfaRequirement] {
            if (!requireFreshCutoverMfa()) {
                reportFreshMfaRequirement(tr("draining this authority"));
                return;
            }
            const QString authorityId = status->value(QStringLiteral("authority_id")).toString();
            const QString phrase = QStringLiteral("drain authority %1").arg(authorityId);
            bool accepted = false;
            const QString entered = QInputDialog::getText(
                &dialog, tr("Drain Authority"),
                tr("This pauses new authority and revokes outstanding endpoint sessions. End live work first. Type exactly:\n%1")
                    .arg(phrase), QLineEdit::Normal, QString(), &accepted);
            if (!accepted) return;
            if (entered != phrase) {
                QMessageBox::information(&dialog, tr("Drain Not Started"),
                                         tr("The exact authority phrase was not entered."));
                return;
            }
            issueRequest(QByteArrayLiteral("POST"), QStringLiteral("/v2/authority/cutover/drain"),
                         QJsonObject{{QStringLiteral("confirmation_phrase"), phrase}},
                         [refresh](bool ok, QJsonObject) { if (ok && *refresh) (*refresh)(); });
        });
        connect(approveButton, &QPushButton::clicked, &dialog,
                [this, &dialog, status, issueRequest, refresh, requireFreshCutoverMfa,
                 reportFreshMfaRequirement] {
            if (!requireFreshCutoverMfa()) {
                reportFreshMfaRequirement(tr("approving the restored authority"));
                return;
            }
            const QJsonObject cutover = status->value(QStringLiteral("cutover")).toObject();
            const QString cutoverId = cutover.value(QStringLiteral("id")).toString();
            const QString predecessor = cutover.value(QStringLiteral("predecessor_authority_id")).toString();
            const QString replacement = cutover.value(QStringLiteral("replacement_authority_id")).toString();
            if (cutoverId.isEmpty() || !validDeploymentIdentity(predecessor) ||
                !validDeploymentIdentity(replacement)) {
                QMessageBox::warning(&dialog, tr("Cutover Details Unavailable"),
                                     tr("Refresh the server status before approving a cutover."));
                return;
            }
            const QStringList methodLabels{
                tr("Host powered off"), tr("Services and routes isolated"),
                tr("Network isolated"), tr("Locally verified profile")};
            const QStringList methodValues{
                QStringLiteral("host_powered_off"), QStringLiteral("service_and_routes_isolated"),
                QStringLiteral("network_isolated"), QStringLiteral("locally_verified_profile")};
            bool accepted = false;
            const QString selectedMethod = QInputDialog::getItem(
                &dialog, tr("Fencing Method"),
                tr("Select the procedure used to fence the predecessor host:"),
                methodLabels, 0, false, &accepted);
            if (!accepted) return;
            const qsizetype methodIndex = methodLabels.indexOf(selectedMethod);
            if (methodIndex < 0 || methodIndex >= methodValues.size()) return;
            const QString evidence = QInputDialog::getText(
                &dialog, tr("Fencing Evidence"),
                tr("Enter a reference to evidence reviewed outside this server (maximum 512 characters):"),
                QLineEdit::Normal, QString(), &accepted).trimmed();
            if (!accepted) return;
            const QByteArray evidenceUtf8 = evidence.toUtf8();
            if (evidence.isEmpty() || evidence.size() > 512 ||
                evidenceUtf8.size() > 512 ||
                std::any_of(evidence.cbegin(), evidence.cend(),
                            isControlCharacter)) {
                QMessageBox::warning(&dialog, tr("Invalid Evidence Reference"),
                                     tr("Enter a non-empty reference of at most 512 characters."));
                return;
            }
            const QString phrase = QStringLiteral("approve authority cutover %1 to %2")
                                       .arg(predecessor, replacement);
            const QString entered = QInputDialog::getText(
                &dialog, tr("Approve Replacement Authority"),
                tr("Confirm the exact predecessor and replacement identities:\n%1")
                    .arg(phrase), QLineEdit::Normal, QString(), &accepted);
            if (!accepted) return;
            if (entered != phrase) {
                QMessageBox::information(&dialog, tr("Cutover Not Approved"),
                                         tr("The exact authority identities were not entered."));
                return;
            }
            const QJsonObject payload{
                {QStringLiteral("cutover_id"), cutoverId},
                {QStringLiteral("expected_predecessor_authority_id"), predecessor},
                {QStringLiteral("expected_replacement_authority_id"), replacement},
                {QStringLiteral("fencing_method"), methodValues.at(methodIndex)},
                {QStringLiteral("fencing_evidence_reference"), evidence},
                {QStringLiteral("confirmation_phrase"), phrase},
            };
            issueRequest(QByteArrayLiteral("POST"), QStringLiteral("/v2/authority/cutover/approve"),
                         payload, [refresh](bool ok, QJsonObject) { if (ok && *refresh) (*refresh)(); });
        });
        connect(cancelButton, &QPushButton::clicked, &dialog,
                [this, &dialog, status, issueRequest, refresh, requireFreshCutoverMfa,
                 reportFreshMfaRequirement] {
            if (!requireFreshCutoverMfa()) {
                reportFreshMfaRequirement(tr("cancelling this drain"));
                return;
            }
            const QJsonObject cutover = status->value(QStringLiteral("cutover")).toObject();
            const QString cutoverId = cutover.value(QStringLiteral("id")).toString();
            if (cutoverId.isEmpty()) return;
            const QString phrase = QStringLiteral("cancel authority cutover %1").arg(cutoverId);
            bool accepted = false;
            const QString entered = QInputDialog::getText(
                &dialog, tr("Cancel Authority Drain"),
                tr("Cancel only works on the still-running predecessor. Type exactly:\n%1")
                    .arg(phrase), QLineEdit::Normal, QString(), &accepted);
            if (!accepted) return;
            if (entered != phrase) {
                QMessageBox::information(&dialog, tr("Drain Not Cancelled"),
                                         tr("The exact cutover phrase was not entered."));
                return;
            }
            issueRequest(QByteArrayLiteral("POST"), QStringLiteral("/v2/authority/cutover/cancel"),
                         QJsonObject{{QStringLiteral("cutover_id"), cutoverId},
                                     {QStringLiteral("confirmation_phrase"), phrase}},
                         [refresh](bool ok, QJsonObject) { if (ok && *refresh) (*refresh)(); });
        });
        connect(releaseRecoveryButton, &QPushButton::clicked, &dialog,
                [this, &dialog, recoveryStatus, issueRequest, refresh,
                 requireFreshCutoverMfa, reportFreshMfaRequirement] {
            if (!requireFreshCutoverMfa()) {
                reportFreshMfaRequirement(tr("releasing restore recovery quarantine"));
                return;
            }
            const QString deploymentId = recoveryStatus->value(QStringLiteral("deployment_id")).toString();
            const QJsonValue accountCount = recoveryStatus->value(QStringLiteral("quarantined_account_count"));
            const QJsonValue managerCount = recoveryStatus->value(QStringLiteral("revoked_manager_count"));
            const QJsonValue endpointCount = recoveryStatus->value(QStringLiteral("quarantined_endpoint_count"));
            const auto safeCount = [](const QJsonValue& value) {
                const double count = value.toDouble(-1);
                return value.isDouble() && std::isfinite(count) && count >= 0 &&
                    count <= 9007199254740991.0 && std::floor(count) == count;
            };
            if (!validDeploymentIdentity(deploymentId) ||
                !recoveryStatus->value(QStringLiteral("recovery_quarantined")).toBool() ||
                !safeCount(accountCount) || !safeCount(managerCount) || !safeCount(endpointCount)) {
                QMessageBox::warning(&dialog, tr("Recovery Summary Unavailable"),
                                     tr("Refresh and review the current recovery-quarantine summary first."));
                return;
            }
            QMessageBox::warning(
                &dialog, tr("Review Quarantined Identities"),
                tr("This releases the deployment-level recovery gate only. %1 account(s) and %2 endpoint(s) remain individually quarantined, and %3 management device(s) remain revoked.")
                    .arg(QString::number(accountCount.toDouble(), 'f', 0),
                         QString::number(endpointCount.toDouble(), 'f', 0),
                         QString::number(managerCount.toDouble(), 'f', 0)));
            if (QMessageBox::question(
                    &dialog, tr("Acknowledge Missing History"),
                    tr("Do you acknowledge that actions after the backup time are missing from this restored database?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
                return;
            }
            if (QMessageBox::question(
                    &dialog, tr("Retain Quarantined Identities"),
                    tr("Do you acknowledge that restored accounts and endpoints remain individually quarantined, and revoked management devices remain revoked?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
                return;
            }
            const QString phrase = QStringLiteral("release restored deployment %1").arg(deploymentId);
            bool accepted = false;
            const QString entered = QInputDialog::getText(
                &dialog, tr("Release Recovery Quarantine"),
                tr("Confirm the restored deployment identity. Type exactly:\n%1")
                    .arg(phrase), QLineEdit::Normal, QString(), &accepted);
            if (!accepted) return;
            if (entered != phrase) {
                QMessageBox::information(&dialog, tr("Recovery Quarantine Remains Active"),
                                         tr("The exact deployment phrase was not entered."));
                return;
            }
            const QJsonObject payload{
                {QStringLiteral("expected_deployment_id"), deploymentId},
                {QStringLiteral("expected_quarantined_account_count"),
                 static_cast<qint64>(accountCount.toDouble())},
                {QStringLiteral("expected_revoked_manager_count"),
                 static_cast<qint64>(managerCount.toDouble())},
                {QStringLiteral("expected_quarantined_endpoint_count"),
                 static_cast<qint64>(endpointCount.toDouble())},
                {QStringLiteral("acknowledge_missing_post_backup_history"), true},
                {QStringLiteral("acknowledge_restored_authorities_remain_quarantined"), true},
                {QStringLiteral("confirmation_phrase"), phrase},
            };
            issueRequest(QByteArrayLiteral("POST"), QStringLiteral("/v2/recovery/quarantine/release"),
                         payload, [refresh](bool ok, QJsonObject) {
                if (ok && *refresh) (*refresh)();
            });
        });

        dialog.exec();
    });
}

void MainWindow::checkPinnedDeploymentIdentity(const std::string& profileId,
                                               std::function<void()> continuation)
{
    const auto profile = registry_.profile(profileId);
    if (!profile || !hasTrustedDeploymentAuthority(*profile)) {
        statusBar()->showMessage(tr("Verify this server's deployment identity and authority key through your organization's trusted channel first."), 8000);
        return;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/deployment/authority"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }

    const QString endpointSnapshot = QString::fromStdString(profile->endpoint);
    const std::string trustedIdentity = profile->trustedDeploymentId;
    const std::string trustedPublicKey = profile->trustedGrantSigningPublicKey;
    const std::uint64_t trustedGeneration = profile->trustedAuthorityGeneration;
    QNetworkReply* reply = network_.get(apiRequest(*endpoint));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, endpointSnapshot, trustedIdentity, trustedPublicKey,
             trustedGeneration,
             continuation = std::move(continuation)]() mutable {
        if (shutdownInProgress_) {
            reply->deleteLater();
            return;
        }
        const auto currentProfile = registry_.profile(profileId);
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (!currentProfile || QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
            currentProfile->trustedDeploymentId != trustedIdentity ||
            currentProfile->trustedGrantSigningPublicKey != trustedPublicKey ||
            currentProfile->trustedAuthorityGeneration != trustedGeneration) {
            statusBar()->showMessage(tr("The server profile changed before identity confirmation completed."), 7000);
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(tr("Could not verify the current server identity: %1")
                                         .arg(replyFailure(reply)), 9000);
            reply->deleteLater();
            return;
        }

        const auto authority = parseDeploymentAuthority(body);
        if (!authority) {
            statusBar()->showMessage(tr("The server returned an invalid deployment authority."), 8000);
            reply->deleteLater();
            return;
        }
        reply->deleteLater();
        if (!authorityMatchesProfile(*currentProfile, *authority)) {
            serverAuthorityMismatches_.insert(profileId);
            invalidateProfileSessionLocally(profileId, DeploymentId{trustedIdentity});
            statusBar()->showMessage(tr("Server identity, authority key, or generation mismatch. Re-verify all three through your organization's trusted channel before continuing."), 10000);
            return;
        }
        if (serverAuthorityMismatches_.erase(profileId) != 0) {
            refreshServerTable();
        }
        continuation();
    });
}

void MainWindow::bootstrapSelectedServer()
{
    const auto selectedId = selectedProfileId();
    if (!selectedId) {
        statusBar()->showMessage(tr("Select the server to initialize first."), 4000);
        return;
    }
    const auto profile = registry_.profile(*selectedId);
    if (!profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 5000);
        return;
    }
    if (!hasTrustedDeploymentAuthority(*profile)) {
        statusBar()->showMessage(tr("Verify this server's deployment identity and authority key through your organization's trusted channel before initializing it."), 9000);
        return;
    }
    const auto healthEndpoint = apiEndpoint(*profile, QStringLiteral("/health"));
    const auto deploymentEndpoint = apiEndpoint(*profile, QStringLiteral("/v2/deployment/authority"));
    const auto bootstrapEndpoint = apiEndpoint(*profile, QStringLiteral("/v2/bootstrap"));
    if (!healthEndpoint || !deploymentEndpoint || !bootstrapEndpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Initialize Server Administrator"));
    dialog.setMinimumWidth(520);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(subheading(
        tr("Use the single-use bootstrap token created on this server. This creates the first administrator, binds this Manager device, and returns MFA recovery material once."),
        &dialog));
    auto* form = new QFormLayout;
    auto* token = new QLineEdit(&dialog);
    token->setEchoMode(QLineEdit::Password);
    token->setMaxLength(256);
    token->setObjectName(QStringLiteral("bootstrapToken"));
    auto* username = new QLineEdit(QStringLiteral("admin"), &dialog);
    username->setMaxLength(64);
    username->setObjectName(QStringLiteral("bootstrapUsername"));
    auto* password = new QLineEdit(&dialog);
    password->setEchoMode(QLineEdit::Password);
    password->setMaxLength(256);
    password->setObjectName(QStringLiteral("bootstrapPassword"));
    auto* friendlyName = new QLineEdit(QCoreApplication::applicationName(), &dialog);
    friendlyName->setMaxLength(128);
    friendlyName->setObjectName(QStringLiteral("bootstrapManagerName"));
    form->addRow(tr("Single-use bootstrap token"), token);
    form->addRow(tr("Administrator username"), username);
    form->addRow(tr("Administrator password"), password);
    form->addRow(tr("This Manager device name"), friendlyName);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Initialize Administrator"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const QString name = username->text().trimmed();
        const QString tokenValue = token->text().trimmed();
        const QString friendlyValue = friendlyName->text().trimmed();
        const QByteArray passwordBytes = password->text().toUtf8();
        const bool validName = name.size() >= 3 && name.size() <= 64 && std::all_of(
            name.cbegin(), name.cend(), [](QChar c) {
                const ushort value = c.unicode();
                return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
                       (value >= '0' && value <= '9') || value == '.' || value == '_' || value == '-';
            });
        if (!validWireText(tokenValue, 256) || !validName || passwordBytes.size() < 12 ||
            passwordBytes.size() > 256 || !validWireText(friendlyValue, 128)) {
            QMessageBox::warning(&dialog, tr("Check Bootstrap Details"),
                                 tr("Enter the single-use token, a 3–64 character username, a 12–256 byte password, and a device name."));
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        token->clear();
        password->clear();
        return;
    }

    auto input = std::make_shared<BootstrapInput>();
    input->token = token->text().trimmed();
    input->username = username->text().trimmed();
    input->password = password->text();
    input->friendlyName = friendlyName->text().trimmed();
    token->clear();
    password->clear();
    const QString serverName = QString::fromStdString(profile->displayName);
    if (QMessageBox::question(
            this, tr("Initialize %1").arg(serverName),
            tr("Create the first administrator and approve this Manager device on %1? The bootstrap token can be used only once.")
                .arg(serverName),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }

    auto identity = std::make_shared<helicoid::manager::ManagerSigningIdentity>();
    QString identityError;
    if (!identity->loadOrCreate(identityError)) {
        statusBar()->showMessage(tr("Manager device identity unavailable: %1").arg(identityError), 9000);
        return;
    }
    const QByteArray publicKeyHex = identity->publicKeyHex(identityError);
    if (publicKeyHex.size() != 64) {
        statusBar()->showMessage(tr("Manager device identity unavailable: %1").arg(identityError), 9000);
        return;
    }

    const std::string profileId = *selectedId;
    const QString endpointSnapshot = QString::fromStdString(profile->endpoint);
    const std::string trustedIdentity = profile->trustedDeploymentId;
    const std::string trustedPublicKey = profile->trustedGrantSigningPublicKey;
    const std::uint64_t trustedGeneration = profile->trustedAuthorityGeneration;
    QNetworkRequest healthRequest = apiRequest(*healthEndpoint);
    QNetworkReply* healthReply = network_.get(healthRequest);
    healthReply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, healthReply, [healthReply] { healthReply->abort(); });
    connect(healthReply, &QNetworkReply::downloadProgress, this,
            [healthReply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) healthReply->abort();
            });
    connect(healthReply, &QNetworkReply::finished, this,
            [this, healthReply, profileId, endpointSnapshot, trustedIdentity, trustedPublicKey,
             trustedGeneration,
             serverName, input, identity,
             publicKeyHex, deploymentEndpoint = *deploymentEndpoint,
             bootstrapEndpoint = *bootstrapEndpoint] {
        if (shutdownInProgress_) {
            healthReply->deleteLater();
            return;
        }
        const int healthStatus = healthReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray healthBody = healthReply->readAll();
        const auto currentProfile = registry_.profile(profileId);
        if (!currentProfile || QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
            currentProfile->trustedDeploymentId != trustedIdentity ||
            currentProfile->trustedGrantSigningPublicKey != trustedPublicKey ||
            currentProfile->trustedAuthorityGeneration != trustedGeneration) {
            statusBar()->showMessage(tr("The server profile changed before bootstrap could begin."), 6000);
            healthReply->deleteLater();
            return;
        }
        if (healthReply->error() != QNetworkReply::NoError || healthStatus != 200 ||
            healthBody.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(tr("Server bootstrap health check failed: %1")
                                         .arg(replyFailure(healthReply)), 9000);
            healthReply->deleteLater();
            return;
        }
        QJsonParseError healthParseError;
        const QJsonDocument healthDocument = QJsonDocument::fromJson(healthBody, &healthParseError);
        const QJsonObject health = healthDocument.object();
        const QJsonValue schema = health.value(QStringLiteral("schema_version"));
        const QJsonValue bootstrapComplete = health.value(QStringLiteral("bootstrap_completed"));
        if (healthParseError.error != QJsonParseError::NoError || !healthDocument.isObject() ||
            !schema.isDouble() || schema.toInt(-1) != 1 || !bootstrapComplete.isBool()) {
            statusBar()->showMessage(tr("The server returned an unsupported bootstrap status."), 8000);
            healthReply->deleteLater();
            return;
        }
        if (bootstrapComplete.toBool()) {
            statusBar()->showMessage(tr("This server has already completed bootstrap."), 7000);
            healthReply->deleteLater();
            return;
        }
        healthReply->deleteLater();

        QNetworkRequest deploymentRequest = apiRequest(deploymentEndpoint);
        QNetworkReply* deploymentReply = network_.get(deploymentRequest);
        deploymentReply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, deploymentReply, [deploymentReply] { deploymentReply->abort(); });
        connect(deploymentReply, &QNetworkReply::downloadProgress, this,
                [deploymentReply](qint64 received, qint64) {
                    if (received > maxHealthResponseBytes) deploymentReply->abort();
                });
        connect(deploymentReply, &QNetworkReply::finished, this,
                [this, deploymentReply, profileId, endpointSnapshot, trustedIdentity, trustedPublicKey,
                 trustedGeneration,
                 serverName, input, identity,
                 publicKeyHex, bootstrapEndpoint] {
            if (shutdownInProgress_) {
                deploymentReply->deleteLater();
                return;
            }
            const int statusCode = deploymentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = deploymentReply->readAll();
            const auto currentProfile = registry_.profile(profileId);
            if (!currentProfile || QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
                currentProfile->trustedDeploymentId != trustedIdentity ||
                currentProfile->trustedGrantSigningPublicKey != trustedPublicKey ||
                currentProfile->trustedAuthorityGeneration != trustedGeneration) {
                statusBar()->showMessage(tr("The server profile changed before bootstrap could begin."), 6000);
                deploymentReply->deleteLater();
                return;
            }
            if (deploymentReply->error() != QNetworkReply::NoError || statusCode != 200 ||
                body.size() > maxHealthResponseBytes) {
                statusBar()->showMessage(tr("Could not read the server deployment identity: %1")
                                             .arg(replyFailure(deploymentReply)), 9000);
                deploymentReply->deleteLater();
                return;
            }
            const auto authority = parseDeploymentAuthority(body);
            if (!authority) {
                statusBar()->showMessage(tr("The server returned an invalid deployment authority."), 8000);
                deploymentReply->deleteLater();
                return;
            }
            if (!authorityMatchesProfile(*currentProfile, *authority)) {
                serverAuthorityMismatches_.insert(profileId);
                invalidateProfileSessionLocally(
                    profileId, DeploymentId{trustedIdentity});
                statusBar()->showMessage(tr("The server identity, authority key, or generation changed. Verify all three before sending bootstrap material."), 9000);
                deploymentReply->deleteLater();
                return;
            }
            if (serverAuthorityMismatches_.erase(profileId) != 0) {
                refreshServerTable();
            }
            deploymentReply->deleteLater();

            const QString deploymentId = authority->deploymentId;

            QByteArray tokenBytes = input->token.toUtf8();
            const QByteArray tokenHash = QCryptographicHash::hash(tokenBytes, QCryptographicHash::Sha256).toHex();
            tokenBytes.fill('\0');
            const QByteArray proofMessage = QStringLiteral("hs-bootstrap-v2|deployment:%1|token:%2|manager:%3")
                .arg(deploymentId, QString::fromLatin1(tokenHash), QString::fromLatin1(publicKeyHex)).toUtf8();
            QString signingError;
            const QByteArray proof = identity->sign(proofMessage, signingError);
            if (proof.size() != 128) {
                statusBar()->showMessage(tr("Manager bootstrap proof failed: %1").arg(signingError), 9000);
                return;
            }

            QJsonObject bootstrapObject{
                {QStringLiteral("bootstrap_token"), input->token},
                {QStringLiteral("username"), input->username},
                {QStringLiteral("password"), input->password},
                {QStringLiteral("manager_public_key"), QString::fromLatin1(publicKeyHex)},
                {QStringLiteral("manager_proof"), QString::fromLatin1(proof)},
                {QStringLiteral("manager_friendly_name"), input->friendlyName},
            };
            SensitiveBuffer requestBody(QJsonDocument(bootstrapObject).toJson(QJsonDocument::Compact));
            input->token.fill(QChar(u'\0'));
            input->password.fill(QChar(u'\0'));
            bootstrapObject.insert(QStringLiteral("bootstrap_token"), QString());
            bootstrapObject.insert(QStringLiteral("password"), QString());
            QNetworkRequest request = apiRequest(bootstrapEndpoint);
            request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            QNetworkReply* reply = network_.post(request, requestBody.bytes());
            reply->setReadBufferSize(maxHealthResponseBytes);
            QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
            connect(reply, &QNetworkReply::downloadProgress, this,
                    [reply](qint64 received, qint64) {
                        if (received > maxHealthResponseBytes) reply->abort();
                    });
            connect(reply, &QNetworkReply::finished, this,
                    [this, reply, profileId, endpointSnapshot, serverName, publicKeyHex] {
                if (shutdownInProgress_) {
                    reply->deleteLater();
                    return;
                }
                const int responseStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                SensitiveBuffer response(reply->readAll());
                const auto currentProfile = registry_.profile(profileId);
                if (!currentProfile || QString::fromStdString(currentProfile->endpoint) != endpointSnapshot) {
                    statusBar()->showMessage(tr("The server profile changed during bootstrap."), 6000);
                    reply->deleteLater();
                    return;
                }
                if (reply->error() != QNetworkReply::NoError || responseStatus != 201 ||
                    response.bytes().size() > maxHealthResponseBytes) {
                    statusBar()->showMessage(tr("Server bootstrap failed on %1: %2")
                                                 .arg(serverName, replyFailure(reply)), 10000);
                    reply->deleteLater();
                    return;
                }
                QJsonParseError parseError;
                const QJsonDocument document = QJsonDocument::fromJson(response.bytes(), &parseError);
                const QJsonObject object = document.object();
                QString accountId = object.value(QStringLiteral("account_id")).toString();
                QString returnedName = object.value(QStringLiteral("username")).toString();
                QString enrollmentId = object.value(QStringLiteral("mfa_enrollment_id")).toString();
                QString totpSecret = object.value(QStringLiteral("totp_secret_base32")).toString();
                const QJsonArray recoveryArray = object.value(QStringLiteral("recovery_codes")).toArray();
                QStringList recoveryCodes;
                bool validCodes = !recoveryArray.isEmpty() && recoveryArray.size() <= 20;
                for (const QJsonValue& value : recoveryArray) {
                    const QString code = value.toString();
                    if (!value.isString() || !validWireText(code, 64)) validCodes = false;
                    recoveryCodes.append(code);
                }
                const bool valid = parseError.error == QJsonParseError::NoError && document.isObject() &&
                    validWireText(accountId) && validWireText(returnedName) &&
                    validWireText(enrollmentId) && validWireText(totpSecret, 128) &&
                    totpSecret.size() >= 16 && validCodes &&
                    object.value(QStringLiteral("manager_public_key")).toString() == QString::fromLatin1(publicKeyHex) &&
                    object.value(QStringLiteral("account_state")).toString() == QStringLiteral("pending_mfa") &&
                    object.value(QStringLiteral("server_state")).toString() == QStringLiteral("awaiting_mfa_confirmation");
                if (!valid) {
                    accountId.fill(QChar(u'\0'));
                    returnedName.fill(QChar(u'\0'));
                    enrollmentId.fill(QChar(u'\0'));
                    totpSecret.fill(QChar(u'\0'));
                    for (QString& code : recoveryCodes) code.fill(QChar(u'\0'));
                    statusBar()->showMessage(tr("The server returned invalid bootstrap enrollment material."), 8000);
                    reply->deleteLater();
                    return;
                }

                QDialog materialDialog(this);
                materialDialog.setWindowTitle(tr("Store First Administrator MFA Material"));
                materialDialog.setMinimumSize(620, 500);
                auto* materialLayout = new QVBoxLayout(&materialDialog);
                materialLayout->addWidget(subheading(
                    tr("The first administrator on %1 is inactive until MFA confirmation. Store these recovery codes independently; this material is shown once and Manager does not save it.")
                        .arg(serverName), &materialDialog));
                auto* material = new QPlainTextEdit(&materialDialog);
                material->setReadOnly(true);
                material->setLineWrapMode(QPlainTextEdit::NoWrap);
                material->setPlainText(
                    tr("Account ID: %1\nUsername: %2\nMFA enrollment ID: %3\nTOTP secret: %4\n\nRecovery codes:\n%5\n\nNext, use Confirm MFA Enrollment in the Server ribbon after the user has configured an authenticator and stored the recovery codes.")
                        .arg(accountId, returnedName, enrollmentId, totpSecret, recoveryCodes.join(QLatin1Char('\n'))));
                materialLayout->addWidget(material, 1);
                auto* materialButtons = new QDialogButtonBox(QDialogButtonBox::Close, &materialDialog);
                connect(materialButtons, &QDialogButtonBox::rejected, &materialDialog, &QDialog::reject);
                materialLayout->addWidget(materialButtons);
                materialDialog.exec();
                material->clear();
                accountId.fill(QChar(u'\0'));
                returnedName.fill(QChar(u'\0'));
                enrollmentId.fill(QChar(u'\0'));
                totpSecret.fill(QChar(u'\0'));
                for (QString& code : recoveryCodes) code.fill(QChar(u'\0'));
                statusBar()->showMessage(tr("First administrator created on %1 in pending MFA state.").arg(serverName), 8000);
                reply->deleteLater();
            });
        });
    });
}

void MainWindow::confirmBootstrapMfa()
{
    std::string profileId;
    if (workspaceStack_ != nullptr && workspaceStack_->currentIndex() == 2) {
        if (accountsServerFilter_ != nullptr) {
            profileId = accountsServerFilter_->currentData().toString().toStdString();
        }
    } else if (const auto selectedId = selectedProfileId()) {
        profileId = *selectedId;
    }
    if (profileId.empty()) {
        statusBar()->showMessage(tr("Choose one owning server before confirming MFA enrollment."), 6000);
        return;
    }
    confirmMfaEnrollment(profileId);
}

void MainWindow::confirmMfaEnrollment(const std::string& profileId,
                                      const QString& initialEnrollmentId)
{
    checkPinnedDeploymentIdentity(profileId, [this, profileId, initialEnrollmentId] {
        showMfaEnrollmentConfirmation(profileId, initialEnrollmentId);
    });
}

void MainWindow::showMfaEnrollmentConfirmation(const std::string& profileId,
                                                const QString& initialEnrollmentId)
{
    const auto profile = registry_.profile(profileId);
    if (!profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 5000);
        return;
    }
    if (!hasTrustedDeploymentAuthority(*profile)) {
        statusBar()->showMessage(tr("Verify this server's deployment identity and authority key through your organization's trusted channel before sending the authenticator code."), 9000);
        return;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/mfa/enroll/confirm"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }
    const QString serverName = QString::fromStdString(profile->displayName);
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Confirm MFA Enrollment"));
    dialog.setMinimumWidth(520);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* enrollmentId = new QLineEdit(&dialog);
    enrollmentId->setMaxLength(128);
    enrollmentId->setObjectName(QStringLiteral("mfaEnrollmentId"));
    enrollmentId->setText(initialEnrollmentId);
    auto* code = new QLineEdit(&dialog);
    code->setMaxLength(6);
    code->setEchoMode(QLineEdit::Password);
    code->setObjectName(QStringLiteral("mfaEnrollmentCode"));
    form->addRow(tr("MFA enrollment ID"), enrollmentId);
    form->addRow(tr("Authenticator code"), code);
    layout->addLayout(form);
    auto* stored = new QCheckBox(tr("The user stored the recovery codes independently of this server."), &dialog);
    layout->addWidget(stored);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Confirm MFA Enrollment"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const QString id = enrollmentId->text().trimmed();
        const QString totp = code->text().trimmed();
        const bool validCode = totp.size() == 6 && std::all_of(totp.cbegin(), totp.cend(), [](QChar c) {
            return c.unicode() >= '0' && c.unicode() <= '9';
        });
        if (!validWireText(id) || !validCode || !stored->isChecked()) {
            QMessageBox::warning(&dialog, tr("Enrollment Details Required"),
                                 tr("Enter the enrollment ID and six-digit authenticator code, and confirm independent recovery storage."));
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        enrollmentId->clear();
        code->clear();
        return;
    }
    QString enrollment = enrollmentId->text().trimmed();
    QString totp = code->text().trimmed();
    enrollmentId->clear();
    code->clear();
    QJsonObject bodyObject{
        {QStringLiteral("mfa_enrollment_id"), enrollment},
        {QStringLiteral("totp_code"), totp},
        {QStringLiteral("recovery_material_stored"), true},
    };
    SensitiveBuffer body(QJsonDocument(bodyObject).toJson(QJsonDocument::Compact));
    enrollment.fill(QChar(u'\0'));
    totp.fill(QChar(u'\0'));
    bodyObject.insert(QStringLiteral("mfa_enrollment_id"), QString());
    bodyObject.insert(QStringLiteral("totp_code"), QString());
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply* reply = network_.post(request, body.bytes());
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply, serverName] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            response.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(tr("MFA enrollment confirmation failed on %1: %2")
                                         .arg(serverName, replyFailure(reply)), 10000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        const QJsonObject result = document.object();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !validWireText(result.value(QStringLiteral("account_id")).toString()) ||
            result.value(QStringLiteral("mfa_status")).toString() != QStringLiteral("active") ||
            result.value(QStringLiteral("account_state")).toString() != QStringLiteral("active")) {
            statusBar()->showMessage(tr("The server returned an invalid MFA enrollment confirmation."), 8000);
            reply->deleteLater();
            return;
        }
        statusBar()->showMessage(tr("MFA enrollment is active on %1. Sign out and sign in again to use MFA-backed authority.").arg(serverName), 9000);
        reply->deleteLater();
    });
}

void MainWindow::signInSelectedServer()
{
    const auto selectedId = selectedProfileId();
    if (!selectedId) {
        statusBar()->showMessage(tr("Select a server profile first."), 3000);
        return;
    }
    const auto profile = registry_.profile(*selectedId);
    if (!profile) {
        refreshServerTable();
        return;
    }
    if (!hasTrustedDeploymentAuthority(*profile)) {
        statusBar()->showMessage(tr("Verify this server's deployment identity and authority key through your organization's trusted channel before signing in."), 9000);
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Sign In to %1").arg(QString::fromStdString(profile->displayName)));
    dialog.setMinimumWidth(520);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* username = new QLineEdit(&dialog);
    username->setObjectName(QStringLiteral("serverLoginUsername"));
    auto* password = new QLineEdit(&dialog);
    password->setObjectName(QStringLiteral("serverLoginPassword"));
    password->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Username"), username);
    form->addRow(tr("Password"), password);
    layout->addLayout(form);
    layout->addWidget(subheading(
        tr("Sign-in proves this Manager device with its protected signing identity. The server may also request your six-digit MFA code."),
        &dialog));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Sign In"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString loginUsername = username->text().trimmed();
    QString loginPassword = password->text();
    if (!validWireText(loginUsername) || loginPassword.isEmpty() || loginPassword.size() > 1024) {
        QMessageBox::warning(this, tr("Invalid Sign-In"), tr("Enter a valid username and password."));
        return;
    }
    QByteArray loginBody = QJsonDocument(QJsonObject{
        {QStringLiteral("username"), loginUsername},
        {QStringLiteral("password"), loginPassword},
    }).toJson(QJsonDocument::Compact);
    loginPassword.fill(QChar(u'\0'));
    password->clear();
    SensitiveBuffer loginCredentials(std::move(loginBody));

    const auto requestStamp = registry_.beginAvailabilityCheck(*selectedId);
    if (!requestStamp) {
        statusBar()->showMessage(tr("This profile cannot start another connection request."), 4000);
        return;
    }
    signInsInProgress_.insert(*selectedId);
    refreshServerTable();
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/deployment/authority"));
    if (!endpoint) {
        signInsInProgress_.erase(*selectedId);
        registry_.finishAvailabilityCheck(*requestStamp, Availability::Unreachable);
        refreshServerTable();
        statusBar()->showMessage(tr("The server address is invalid."), 5000);
        return;
    }
    const QString endpointSnapshot = QString::fromStdString(profile->endpoint);
    const std::string trustedIdentity = profile->trustedDeploymentId;
    const std::string trustedPublicKey = profile->trustedGrantSigningPublicKey;
    const std::uint64_t trustedGeneration = profile->trustedAuthorityGeneration;

    QNetworkReply* reply = network_.get(apiRequest(*endpoint));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) {
                    reply->abort();
                }
            });
    auto pendingCredentials = std::make_shared<SensitiveBuffer>(std::move(loginCredentials));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, request = *requestStamp, profileId = *selectedId,
             endpointSnapshot, trustedIdentity, trustedPublicKey, trustedGeneration,
             pendingCredentials]() mutable {
        SensitiveBuffer credentials = std::move(*pendingCredentials);
        pendingCredentials.reset();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (!registry_.isCurrent(request)) {
            signInsInProgress_.erase(profileId);
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            signInsInProgress_.erase(profileId);
            registry_.finishAvailabilityCheck(request, Availability::Unreachable);
            refreshServerTable();
            statusBar()->showMessage(tr("Could not read the server deployment identity: %1")
                                         .arg(replyFailure(reply)), 8000);
            reply->deleteLater();
            return;
        }
        const auto authority = parseDeploymentAuthority(body);
        if (!authority) {
            signInsInProgress_.erase(profileId);
            registry_.finishAvailabilityCheck(request, Availability::Unreachable);
            refreshServerTable();
            statusBar()->showMessage(tr("The server returned an invalid deployment identity."), 8000);
            reply->deleteLater();
            return;
        }
        const std::string identity = authority->deploymentId.toStdString();
        const DeploymentId deployment{identity};
        if (!deployment.isValid()) {
            signInsInProgress_.erase(profileId);
            registry_.finishAvailabilityCheck(request, Availability::Unreachable);
            refreshServerTable();
            statusBar()->showMessage(tr("The server returned an unsupported deployment identity."), 8000);
            reply->deleteLater();
            return;
        }
        const auto currentProfile = registry_.profile(profileId);
        if (!currentProfile || QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
            currentProfile->trustedDeploymentId != trustedIdentity ||
            currentProfile->trustedGrantSigningPublicKey != trustedPublicKey ||
            currentProfile->trustedAuthorityGeneration != trustedGeneration ||
            trustedIdentity != identity ||
            currentProfile->trustedGrantSigningPublicKey !=
                authority->grantSigningPublicKey.toStdString() ||
            !authorityMatchesProfile(*currentProfile, *authority)) {
            if (currentProfile && validDeploymentIdentity(
                    QString::fromStdString(currentProfile->trustedDeploymentId))) {
                serverAuthorityMismatches_.insert(profileId);
                invalidateProfileSessionLocally(
                    profileId, DeploymentId{currentProfile->trustedDeploymentId});
            } else {
                signInsInProgress_.erase(profileId);
            }
            statusBar()->showMessage(tr("Server identity, authority key, or generation mismatch. Verify all three through your organization's trusted channel before signing in."), 10000);
            reply->deleteLater();
            return;
        }
        if (serverAuthorityMismatches_.erase(profileId) != 0) {
            refreshServerTable();
        }
        deploymentsByProfile_[profileId] = deployment;
        const auto* activeSession = sessions_.session(deployment);
        if (activeSession != nullptr && activeSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            invalidateDeploymentSession(deployment);
            computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
                [&](const ComputerRow& row) { return row.id.deployment == deployment; }), computers_.end());
            clearDeploymentAccountData(deployment);
            activeSession = nullptr;
        }
        if (activeSession != nullptr || authenticationProfileByDeployment_.contains(identity)) {
            signInsInProgress_.erase(profileId);
            registry_.finishAvailabilityCheck(request, Availability::Reachable);
            refreshServerTable();
            statusBar()->showMessage(tr("This deployment already has a session or sign-in in progress. Sign out before starting another session."), 8000);
            reply->deleteLater();
            return;
        }
        authenticationProfileByDeployment_[identity] = profileId;
        registry_.finishAvailabilityCheck(request, Availability::Reachable);
        refreshServerTable();
        reply->deleteLater();
        authenticateSelectedServer(profileId, request, deployment, credentials.take());
    });
}

void MainWindow::authenticateSelectedServer(const std::string& profileId,
                                            const AvailabilityRequest& requestStamp,
                                            const DeploymentId& deployment,
                                            QByteArray loginBody)
{
    SensitiveBuffer credentials(std::move(loginBody));
    const auto fail = [this, profileId, deployment](const QString& message) {
        signInsInProgress_.erase(profileId);
        clearDeploymentAuthentication(deployment, profileId);
        refreshServerTable();
        statusBar()->showMessage(message, 10000);
    };
    if (!registry_.isCurrent(requestStamp)) {
        signInsInProgress_.erase(profileId);
        clearDeploymentAuthentication(deployment, profileId);
        return;
    }
    const auto profile = registry_.profile(profileId);
    if (!profile) {
        signInsInProgress_.erase(profileId);
        clearDeploymentAuthentication(deployment, profileId);
        return;
    }
    QJsonParseError credentialsParseError;
    const QJsonDocument credentialsDocument = QJsonDocument::fromJson(credentials.bytes(), &credentialsParseError);
    if (credentialsParseError.error != QJsonParseError::NoError || !credentialsDocument.isObject()) {
        fail(tr("Sign-in credentials could not be read."));
        return;
    }
    const QJsonObject credentialsObject = credentialsDocument.object();
    QString username = credentialsObject.value(QStringLiteral("username")).toString().trimmed();
    QString password = credentialsObject.value(QStringLiteral("password")).toString();
    credentials = SensitiveBuffer();
    if (!validWireText(username) || password.isEmpty() || password.size() > 1024) {
        password.fill(QChar(u'\0'));
        fail(tr("Enter a valid username and password."));
        return;
    }

    auto identity = std::make_shared<helicoid::manager::ManagerSigningIdentity>();
    QString identityError;
    if (!identity->loadOrCreate(identityError)) {
        password.fill(QChar(u'\0'));
        fail(tr("Manager device identity unavailable: %1").arg(identityError));
        return;
    }
    const QByteArray managerPublicKeyHex = identity->publicKeyHex(identityError);
    if (managerPublicKeyHex.size() != 64) {
        password.fill(QChar(u'\0'));
        fail(tr("Manager device identity unavailable: %1").arg(identityError));
        return;
    }

    const QString serverName = QString::fromStdString(profile->displayName);
    const auto challengeEndpoint = apiEndpoint(*profile, QStringLiteral("/v2/auth/challenge"));
    const auto completionEndpoint = apiEndpoint(*profile, QStringLiteral("/v2/auth/challenge/complete"));
    const auto logoutEndpoint = apiEndpoint(*profile, QStringLiteral("/v2/auth/logout"));
    if (!challengeEndpoint || !completionEndpoint) {
        password.fill(QChar(u'\0'));
        fail(tr("The server address is invalid."));
        return;
    }
    QNetworkRequest request = apiRequest(*challengeEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QByteArray body = QJsonDocument(QJsonObject{
        {QStringLiteral("username"), username},
        {QStringLiteral("password"), password},
        {QStringLiteral("operation"), QStringLiteral("session")},
        {QStringLiteral("manager_public_key"), QString::fromLatin1(managerPublicKeyHex)},
    }).toJson(QJsonDocument::Compact);
    password.fill(QChar(u'\0'));
    SensitiveBuffer challengeRequestBody(body);
    QNetworkReply* reply = network_.post(request, challengeRequestBody.bytes());
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, requestStamp, deployment, serverName, logoutEndpoint,
             completionEndpoint, managerPublicKeyHex, identity, fail] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        SensitiveBuffer responseBuffer(reply->readAll());
        const QByteArray& responseBody = responseBuffer.bytes();
        if (!registry_.isCurrent(requestStamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 201 ||
            responseBody.size() > maxHealthResponseBytes) {
            fail(tr("Sign-in challenge failed: %1").arg(replyFailure(reply)));
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
        const QJsonObject challenge = document.object();
        QString challengeId = challenge.value(QStringLiteral("challenge_id")).toString();
        QString challengeToken = challenge.value(QStringLiteral("challenge_token")).toString();
        const QString operation = challenge.value(QStringLiteral("operation")).toString();
        const QString challengeManagerKey = challenge.value(QStringLiteral("manager_public_key")).toString();
        const QString challengeDeployment = challenge.value(QStringLiteral("deployment_id")).toString();
        const QJsonValue expiresValue = challenge.value(QStringLiteral("expires_at_unix"));
        const QJsonValue mfaRequiredValue = challenge.value(QStringLiteral("mfa_required"));
        const double rawExpires = expiresValue.toDouble(0);
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const bool validExpiration = expiresValue.isDouble() && std::isfinite(rawExpires) &&
            std::floor(rawExpires) == rawExpires && rawExpires > static_cast<double>(now) &&
            rawExpires <= static_cast<double>(now + 120);
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !validWireText(challengeId) || !validBearerToken(challengeToken) ||
            operation != QStringLiteral("session") ||
            challengeManagerKey.toLatin1() != managerPublicKeyHex ||
            challengeDeployment.toStdString() != deployment.value || !validExpiration ||
            !mfaRequiredValue.isBool()) {
            challengeToken.fill(QChar(u'\0'));
            fail(tr("The server returned an invalid or mismatched sign-in challenge."));
            reply->deleteLater();
            return;
        }
        reply->deleteLater();

        QString totpCode;
        if (mfaRequiredValue.toBool()) {
            QInputDialog factorDialog(this);
            factorDialog.setWindowTitle(tr("Multi-Factor Authentication"));
            factorDialog.setLabelText(tr("Enter the six-digit code from your authenticator."));
            factorDialog.setTextEchoMode(QLineEdit::Password);
            factorDialog.setInputMode(QInputDialog::TextInput);
            factorDialog.setWindowModality(Qt::ApplicationModal);
            if (factorDialog.exec() != QDialog::Accepted) {
                challengeToken.fill(QChar(u'\0'));
                fail(tr("Sign-in was cancelled."));
                return;
            }
            totpCode = factorDialog.textValue().trimmed();
            factorDialog.setTextValue(QString());
            const bool validCode = totpCode.size() == 6 && std::all_of(
                totpCode.cbegin(), totpCode.cend(), [](QChar c) {
                    return c.unicode() >= '0' && c.unicode() <= '9';
                });
            if (!validCode) {
                challengeToken.fill(QChar(u'\0'));
                totpCode.fill(QChar(u'\0'));
                fail(tr("Enter exactly six digits for the MFA code."));
                return;
            }
        }
        if (!registry_.isCurrent(requestStamp)) {
            challengeToken.fill(QChar(u'\0'));
            totpCode.fill(QChar(u'\0'));
            return;
        }

        const QString proofMessage = QStringLiteral(
            "hs-auth-v2|deployment:%1|challenge:%2|operation:session|manager:%3")
            .arg(QString::fromStdString(deployment.value), challengeId,
                 QString::fromLatin1(managerPublicKeyHex));
        QString signingError;
        const QByteArray signatureHex = identity->sign(proofMessage.toUtf8(), signingError);
        if (signatureHex.size() != 128) {
            challengeToken.fill(QChar(u'\0'));
            totpCode.fill(QChar(u'\0'));
            fail(tr("Manager device proof failed: %1").arg(signingError));
            return;
        }
        QJsonObject completionBodyObject{
            {QStringLiteral("challenge_id"), challengeId},
            {QStringLiteral("challenge_token"), challengeToken},
            {QStringLiteral("manager_signature"), QString::fromLatin1(signatureHex)},
        };
        if (!totpCode.isEmpty()) {
            completionBodyObject.insert(QStringLiteral("totp_code"), totpCode);
        }
        SensitiveBuffer completionBody(
            QJsonDocument(completionBodyObject).toJson(QJsonDocument::Compact));
        challengeToken.fill(QChar(u'\0'));
        totpCode.fill(QChar(u'\0'));

        QNetworkRequest completionRequest = apiRequest(*completionEndpoint);
        completionRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                                    QStringLiteral("application/json"));
        QNetworkReply* completionReply = network_.post(completionRequest, completionBody.bytes());
        completionReply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, completionReply,
                           [completionReply] { completionReply->abort(); });
        connect(completionReply, &QNetworkReply::downloadProgress, this,
                [completionReply](qint64 received, qint64) {
                    if (received > maxHealthResponseBytes) completionReply->abort();
                });
        connect(completionReply, &QNetworkReply::finished, this,
                [this, completionReply, profileId, requestStamp, deployment, serverName,
                 logoutEndpoint, managerPublicKeyHex,
                 mfaAssured = mfaRequiredValue.toBool()] {
            const int completionStatus = completionReply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            SensitiveBuffer completedSession(completionReply->readAll());
            if (completionReply->error() != QNetworkReply::NoError ||
                completionStatus != 200 || completedSession.bytes().size() > maxHealthResponseBytes) {
                const QString error = tr("Sign-in failed: %1").arg(replyFailure(completionReply));
                signInsInProgress_.erase(profileId);
                clearDeploymentAuthentication(deployment, profileId);
                refreshServerTable();
                statusBar()->showMessage(error, 10000);
                completionReply->deleteLater();
                return;
            }
            acceptV2Session(profileId, requestStamp, deployment, serverName,
                            logoutEndpoint, completedSession.take(), completionStatus,
                            managerPublicKeyHex, mfaAssured);
            completionReply->deleteLater();
        });
    });
}

void MainWindow::acceptV2Session(const std::string& profileId,
                                 const AvailabilityRequest& requestStamp,
                                 const DeploymentId& deployment,
                                 const QString& serverName,
                                 const std::optional<QUrl>& logoutEndpoint,
                                 QByteArray responseBody,
                                 int statusCode,
                                 const QByteArray& expectedManagerPublicKey,
                                 bool mfaAssured)
{
    signInsInProgress_.erase(profileId);
    clearDeploymentAuthentication(deployment, profileId);
    SensitiveBuffer response(std::move(responseBody));
    const QByteArray& body = response.bytes();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    const QJsonObject object = document.object();
    QString token = object.value(QStringLiteral("token")).toString();
    const auto discardToken = [this, &token, &logoutEndpoint, profileId, deployment] {
        if (logoutEndpoint && validBearerToken(token)) {
            const auto profile = registry_.profile(profileId);
            if (profile) {
                sendLogoutRequestForVerifiedProfile(
                    *profile, deployment, token.toLatin1(), false);
            }
        }
        token.fill(QChar(u'\0'));
    };
    if (!registry_.isCurrent(requestStamp)) {
        if (statusCode == 200 && document.isObject()) discardToken();
        else token.fill(QChar(u'\0'));
        return;
    }
    if (statusCode != 200 || parseError.error != QJsonParseError::NoError ||
        !document.isObject() || body.size() > maxHealthResponseBytes ||
        object.value(QStringLiteral("manager_public_key")).toString().toLatin1() != expectedManagerPublicKey) {
        if (statusCode == 200 && document.isObject()) discardToken();
        else token.fill(QChar(u'\0'));
        statusBar()->showMessage(tr("The server returned an invalid or mismatched authenticated session."), 8000);
        return;
    }

    const QJsonObject account = object.value(QStringLiteral("account")).toObject();
    const QJsonObject manager = object.value(QStringLiteral("manager")).toObject();
    const QString accountId = account.value(QStringLiteral("id")).toString();
    const QString accountName = account.value(QStringLiteral("username")).toString();
    const QString managerId = manager.value(QStringLiteral("id")).toString();
    const QString managerIdentity = manager.value(QStringLiteral("public_identity")).toString();
    const QJsonValue expirationValue = object.value(QStringLiteral("expires_at_unix"));
    const double rawExpiresAt = expirationValue.toDouble(0);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const bool validExpiration = expirationValue.isDouble() && std::isfinite(rawExpiresAt) &&
        std::floor(rawExpiresAt) == rawExpiresAt && rawExpiresAt > static_cast<double>(now) &&
        rawExpiresAt <= static_cast<double>(now + 2 * 60 * 60);
    const qint64 expiresAt = validExpiration ? static_cast<qint64>(rawExpiresAt) : 0;
    if (!validBearerToken(token) || !validWireText(accountId) || !validWireText(accountName) ||
        !validWireText(managerId) ||
        managerIdentity != QStringLiteral("key:") + QString::fromLatin1(expectedManagerPublicKey) ||
        !validExpiration || expiresAt <= now) {
        if (validBearerToken(token)) discardToken();
        else token.fill(QChar(u'\0'));
        statusBar()->showMessage(tr("The server returned an invalid or expired session."), 8000);
        return;
    }
    const auto currentDeployment = deploymentsByProfile_.find(profileId);
    if (currentDeployment == deploymentsByProfile_.end() || currentDeployment->second != deployment) {
        discardToken();
        return;
    }

    computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
        [&](const ComputerRow& row) { return row.id.deployment == deployment; }), computers_.end());
    clearDeploymentAccountData(deployment);
    clearDeploymentFileJobs(deployment);
    clearDeploymentFileRequests(deployment);
    SensitiveBuffer tokenForRevocation(token.toLatin1());
    helicoid::manager::AuthenticatedSession session{
        deployment,
        profileId,
        accountId.toStdString(),
        managerId.toStdString(),
        accountName.toStdString(),
        token.toStdString(),
        expiresAt,
        mfaAssured ? QDateTime::currentSecsSinceEpoch() : 0,
    };
    token.fill(QChar(u'\0'));
    const auto sessionStamp = sessions_.activate(std::move(session));
    if (!sessionStamp) {
        const auto profile = registry_.profile(profileId);
        if (logoutEndpoint && profile) {
            sendLogoutRequestForVerifiedProfile(
                *profile, deployment, tokenForRevocation.take(), false);
        }
        statusBar()->showMessage(tr("This deployment cannot accept another session generation."), 8000);
        return;
    }
    refreshServerTable();
    statusBar()->showMessage(tr("Signed in to %1 as %2; loading authorized inventory.")
                                 .arg(serverName, accountName), 5000);
    loadAuthorizedComputers(profileId, deployment, *sessionStamp);
    loadAuthorizedAccountData(profileId, deployment, *sessionStamp);
    refreshFileJobs();
    if (fileTabs_ != nullptr && fileTabs_->currentIndex() == 1 && filesServerFilter_ != nullptr) {
        const QString serverFilter = filesServerFilter_->currentData().toString();
        if (serverFilter.isEmpty() || serverFilter == QString::fromStdString(profileId)) {
            refreshFileRequests();
        }
    }
}

void MainWindow::loadAuthorizedComputers(const std::string& profileId,
                                         const DeploymentId& deployment,
                                         const SessionStamp& stamp)
{
    if (!sessions_.isCurrent(stamp)) {
        return;
    }
    const auto* session = sessions_.session(deployment);
    const auto profile = registry_.profile(profileId);
    if (session == nullptr || session->profileId != profileId || !profile ||
        serverAuthorityMismatches_.contains(profileId) ||
        !hasTrustedDeploymentAuthority(*profile) ||
        profile->trustedDeploymentId != deployment.value ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        invalidateDeploymentSession(deployment);
        computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
            [&](const ComputerRow& row) { return row.id.deployment == deployment; }), computers_.end());
        clearDeploymentAccountData(deployment);
        refreshServerTable();
        refreshComputerTable();
        statusBar()->showMessage(tr("The server session expired. Sign in again."), 6000);
        return;
    }
    loadObservationScope(profileId, deployment, stamp, true);
    auto& currentRequestGeneration = computerRequestGenerations_[deployment.value];
    if (currentRequestGeneration == std::numeric_limits<std::uint64_t>::max()) {
        statusBar()->showMessage(tr("Computer inventory refresh generation is exhausted for this server. Sign out and reconnect."), 7000);
        return;
    }
    const std::uint64_t requestGeneration = ++currentRequestGeneration;
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/computers"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The server address is invalid."), 5000);
        return;
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + QByteArray::fromStdString(session->bearerToken));
    QNetworkReply* reply = network_.get(request);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) {
                    reply->abort();
                }
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp, requestGeneration] {
        const auto currentGeneration = computerRequestGenerations_.find(deployment.value);
        if (!sessions_.isCurrent(stamp) || currentGeneration == computerRequestGenerations_.end() ||
            currentGeneration->second != requestGeneration) {
            reply->deleteLater();
            return;
        }
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            if (statusCode == 401) {
                invalidateDeploymentSession(deployment);
                computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
                    [&](const ComputerRow& row) { return row.id.deployment == deployment; }), computers_.end());
                clearDeploymentAccountData(deployment);
                refreshServerTable();
                refreshComputerTable();
            }
            statusBar()->showMessage(tr("Could not load authorized computers: %1")
                                         .arg(replyFailure(reply)), 8000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject response = document.object();
        const QJsonArray entries = response.value(QStringLiteral("computers")).toArray();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !response.value(QStringLiteral("computers")).isArray() || entries.size() > 100) {
            statusBar()->showMessage(tr("The server returned an invalid inventory response."), 8000);
            reply->deleteLater();
            return;
        }
        const auto profile = registry_.profile(profileId);
        if (!profile) {
            reply->deleteLater();
            return;
        }
        std::vector<ComputerRow> loaded;
        loaded.reserve(static_cast<std::size_t>(entries.size()));
        QSet<QString> seenIds;
        const auto parseOptionalUnixTimestamp = [](const QJsonValue& value,
                                                   std::optional<std::int64_t>& output) {
            if (value.isUndefined() || value.isNull()) {
                output.reset();
                return true;
            }
            if (!value.isDouble()) {
                return false;
            }
            const double number = value.toDouble();
            if (!std::isfinite(number) || std::floor(number) != number || number < 0 ||
                number >= std::ldexp(1.0, 63)) {
                return false;
            }
            output = static_cast<std::int64_t>(number);
            return true;
        };
        for (const QJsonValue& value : entries) {
            if (!value.isObject()) {
                statusBar()->showMessage(tr("The server returned a malformed computer record."), 8000);
                reply->deleteLater();
                return;
            }
            const QJsonObject computer = value.toObject();
            const QString id = computer.value(QStringLiteral("id")).toString();
            const QString name = computer.value(QStringLiteral("friendly_name")).toString();
            const QString platform = computer.value(QStringLiteral("platform")).toString();
            const QString state = computer.value(QStringLiteral("enrollment_state")).toString();
            const QJsonValue groupValue = computer.value(QStringLiteral("groups"));
            const QJsonValue endpointSessionValue = computer.value(QStringLiteral("endpoint_session"));
            bool endpointSessionReported = false;
            std::optional<EndpointSessionRow> endpointSession;
            if (!endpointSessionValue.isUndefined() && !endpointSessionValue.isNull()) {
                if (!endpointSessionValue.isObject()) {
                    statusBar()->showMessage(tr("The server returned malformed endpoint session metadata."), 8000);
                    reply->deleteLater();
                    return;
                }
                const QJsonObject session = endpointSessionValue.toObject();
                EndpointSessionRow parsedSession;
                const QJsonValue consoleUserValue = session.value(QStringLiteral("console_user_reported"));
                const QJsonValue currentSessionIdValue = session.value(QStringLiteral("current_session_id"));
                if (!parseOptionalUnixTimestamp(
                        session.value(QStringLiteral("last_authenticated_at_unix")),
                        parsedSession.lastAuthenticatedAtUnix) ||
                    !parseOptionalUnixTimestamp(
                        session.value(QStringLiteral("current_session_expires_at_unix")),
                        parsedSession.currentSessionExpiresAtUnix)) {
                    statusBar()->showMessage(tr("The server returned invalid endpoint session times."), 8000);
                    reply->deleteLater();
                    return;
                }
                if (!consoleUserValue.isUndefined() && !consoleUserValue.isNull()) {
                    if (!consoleUserValue.isBool()) {
                        statusBar()->showMessage(tr("The server returned invalid console-user session metadata."), 8000);
                        reply->deleteLater();
                        return;
                    }
                    parsedSession.consoleUserReported = consoleUserValue.toBool();
                }
                if (!currentSessionIdValue.isUndefined() && !currentSessionIdValue.isNull()) {
                    if (!currentSessionIdValue.isString() ||
                        !validUuid(currentSessionIdValue.toString())) {
                        statusBar()->showMessage(tr("The server returned an invalid endpoint session identity."), 8000);
                        reply->deleteLater();
                        return;
                    }
                    parsedSession.currentSessionId = currentSessionIdValue.toString().toStdString();
                }
                if (parsedSession.currentSessionExpiresAtUnix.has_value() !=
                        parsedSession.consoleUserReported.has_value() ||
                    parsedSession.currentSessionExpiresAtUnix.has_value() !=
                        parsedSession.currentSessionId.has_value() ||
                    (parsedSession.currentSessionExpiresAtUnix &&
                     (!parsedSession.lastAuthenticatedAtUnix ||
                      *parsedSession.lastAuthenticatedAtUnix >
                          *parsedSession.currentSessionExpiresAtUnix))) {
                    statusBar()->showMessage(tr("The server returned inconsistent endpoint session metadata."), 8000);
                    reply->deleteLater();
                    return;
                }
                endpointSessionReported = true;
                endpointSession = std::move(parsedSession);
            }
            const auto scopedId = ScopedComputerId::create(deployment, id.toStdString());
            if (!validWireText(id) || !validWireText(name) || !validWireText(platform) ||
                !validWireText(state) || !scopedId || seenIds.contains(id) ||
                (!groupValue.isUndefined() && !groupValue.isArray()) ||
                groupValue.toArray().size() > 100) {
                statusBar()->showMessage(tr("The server returned an invalid or duplicate computer identity."), 8000);
                reply->deleteLater();
                return;
            }
            std::vector<ComputerGroupRow> groups;
            QSet<QString> seenGroupIds;
            for (const QJsonValue& groupValueItem : groupValue.toArray()) {
                if (!groupValueItem.isObject()) {
                    statusBar()->showMessage(tr("The server returned malformed computer group metadata."), 8000);
                    reply->deleteLater();
                    return;
                }
                const QJsonObject group = groupValueItem.toObject();
                const QString groupId = group.value(QStringLiteral("id")).toString();
                const QString groupName = group.value(QStringLiteral("name")).toString();
                const auto scopedGroupId = ScopedGroupId::create(deployment, groupId.toStdString());
                if (!validWireText(groupId) || !validWireText(groupName) || !scopedGroupId ||
                    seenGroupIds.contains(groupId)) {
                    statusBar()->showMessage(tr("The server returned invalid or duplicate computer group metadata."), 8000);
                    reply->deleteLater();
                    return;
                }
                seenGroupIds.insert(groupId);
                groups.push_back(ComputerGroupRow{*scopedGroupId, groupName.toStdString()});
            }
            seenIds.insert(id);
            loaded.push_back(ComputerRow{
                *scopedId,
                profileId,
                profile->displayName,
                name.toStdString(),
                platform.toStdString(),
                state.toStdString(),
                std::move(groups),
                endpointSessionReported,
                std::move(endpointSession),
            });
        }
        std::vector<std::string> enrolledComputerIds;
        for (const ComputerRow& computer : loaded) {
            if (computer.enrollmentState == "enrolled") {
                enrolledComputerIds.push_back(computer.id.serverComputerId);
            }
        }
        retainEnrolledDeploymentViewers(deployment, enrolledComputerIds);
        computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
            [&](const ComputerRow& row) { return row.id.deployment == deployment; }), computers_.end());
        computers_.insert(computers_.end(), loaded.begin(), loaded.end());
        refreshComputerTable();
        refreshServerTable();
        statusBar()->showMessage(tr("Loaded %1 authorized computer records from %2.")
                                     .arg(loaded.size())
                                     .arg(QString::fromStdString(profile->displayName)), 6000);
        reply->deleteLater();
    });
}

void MainWindow::loadObservationScope(const std::string& profileId,
                                     const DeploymentId& deployment,
                                     const SessionStamp& stamp,
                                     bool force)
{
    if (!sessions_.isCurrent(stamp)) {
        return;
    }
    const auto* session = sessions_.session(deployment);
    const auto profile = registry_.profile(profileId);
    if (session == nullptr || session->profileId != profileId || !profile ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        observationScopes_.invalidate(deployment);
        observationScopePending_.erase(deployment.value);
        closeViewersForDeployment(deployment);
        return;
    }
    if (!force && (observationScopePending_.contains(deployment.value) ||
                   observationScopes_.hasCurrent(
                       stamp, helicoid::manager::ObservationScopeRegistry::Clock::now()))) {
        return;
    }

    auto& currentGeneration = observationScopeRequestGenerations_[deployment.value];
    if (currentGeneration == std::numeric_limits<std::uint64_t>::max()) {
        observationScopes_.invalidate(deployment);
        observationScopePending_.erase(deployment.value);
        observationScopeRequestGenerations_.erase(deployment.value);
        closeViewersForDeployment(deployment);
        statusBar()->showMessage(tr("Observe scope refresh generation is exhausted for this server. Sign out and reconnect."), 7000);
        return;
    }
    const std::uint64_t requestGeneration = ++currentGeneration;
    observationScopes_.invalidate(deployment);
    observationScopePending_.insert(deployment.value);
    closeViewersForDeployment(deployment);

    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/access/observe-scope"));
    if (!endpoint) {
        observationScopePending_.erase(deployment.value);
        statusBar()->showMessage(tr("The server address is invalid; observe scope remains unavailable."), 6000);
        return;
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + QByteArray::fromStdString(session->bearerToken));
    QNetworkReply* reply = network_.get(request);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) {
                    reply->abort();
                }
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp, requestGeneration] {
        const auto currentGeneration = observationScopeRequestGenerations_.find(deployment.value);
        if (!sessions_.isCurrent(stamp) || currentGeneration == observationScopeRequestGenerations_.end() ||
            currentGeneration->second != requestGeneration) {
            reply->deleteLater();
            return;
        }
        observationScopePending_.erase(deployment.value);
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            observationScopes_.invalidate(deployment);
            closeViewersForDeployment(deployment);
            if (statusCode == 401) {
                invalidateDeploymentSession(deployment);
                computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
                    [&](const ComputerRow& row) { return row.id.deployment == deployment; }), computers_.end());
                clearDeploymentAccountData(deployment);
                refreshServerTable();
                refreshComputerTable();
            }
            statusBar()->showMessage(tr("Could not load current observe scope: %1")
                                         .arg(replyFailure(reply)), 8000);
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject response = document.object();
        const QJsonValue allValue = response.value(QStringLiteral("scope_all"));
        const QJsonValue idsValue = response.value(QStringLiteral("computer_ids"));
        const QJsonArray ids = idsValue.toArray();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !allValue.isBool() || !idsValue.isArray() || ids.size() > 100) {
            observationScopes_.invalidate(deployment);
            closeViewersForDeployment(deployment);
            statusBar()->showMessage(tr("The server returned an invalid observe scope response."), 8000);
            reply->deleteLater();
            return;
        }
        const bool allComputers = allValue.toBool();
        std::unordered_set<std::string> computerIds;
        QSet<QString> seenIds;
        for (const QJsonValue& idValue : ids) {
            if (!idValue.isString()) {
                observationScopes_.invalidate(deployment);
                closeViewersForDeployment(deployment);
                statusBar()->showMessage(tr("The server returned a malformed computer in observe scope."), 8000);
                reply->deleteLater();
                return;
            }
            const QString id = idValue.toString();
            if (!validWireText(id) || seenIds.contains(id)) {
                observationScopes_.invalidate(deployment);
                closeViewersForDeployment(deployment);
                statusBar()->showMessage(tr("The server returned an invalid or duplicate observe scope computer ID."), 8000);
                reply->deleteLater();
                return;
            }
            seenIds.insert(id);
            computerIds.insert(id.toStdString());
        }
        if (allComputers && !computerIds.empty()) {
            observationScopes_.invalidate(deployment);
            closeViewersForDeployment(deployment);
            statusBar()->showMessage(tr("The server returned a contradictory observe scope response."), 8000);
            reply->deleteLater();
            return;
        }
        const auto scopeStoredAt = helicoid::manager::ObservationScopeRegistry::Clock::now();
        if (!observationScopes_.replace(stamp, allComputers, std::move(computerIds),
                                        scopeStoredAt + std::chrono::seconds(30), scopeStoredAt)) {
            observationScopes_.invalidate(deployment);
            closeViewersForDeployment(deployment);
            statusBar()->showMessage(tr("The server returned an invalid observe scope."), 8000);
            reply->deleteLater();
            return;
        }
        QTimer::singleShot(31000, this, [this, deployment, stamp, requestGeneration] {
            const auto currentGeneration = observationScopeRequestGenerations_.find(deployment.value);
            if (currentGeneration == observationScopeRequestGenerations_.end() ||
                currentGeneration->second != requestGeneration ||
                !observationScopes_.expireIfCurrent(
                    stamp, helicoid::manager::ObservationScopeRegistry::Clock::now())) {
                return;
            }
            closeViewersForDeployment(deployment);
        });
        statusBar()->showMessage(tr("Current observe scope loaded for %1.")
                                     .arg(QString::fromStdString(profileId)), 4000);
        reply->deleteLater();
    });
}

void MainWindow::manageComputerGroups()
{
    expireSessions();
    struct Candidate {
        std::string profileId;
        helicoid::manager::DeploymentId deployment;
        helicoid::manager::SessionStamp stamp;
        QString label;
    };
    std::vector<Candidate> candidates;
    for (const ConnectionProfile& profile : registry_.profiles()) {
        const auto deployment = deploymentsByProfile_.find(profile.profileId);
        if (deployment == deploymentsByProfile_.end()) {
            continue;
        }
        const auto stamp = sessions_.currentStamp(deployment->second);
        const auto* session = sessions_.session(deployment->second);
        if (!stamp || !sessions_.isCurrent(*stamp) || session == nullptr ||
            session->profileId != profile.profileId ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            continue;
        }
        candidates.push_back(Candidate{
            profile.profileId,
            deployment->second,
            *stamp,
            tr("%1 — %2 (%3)")
                .arg(QString::fromStdString(profile.displayName),
                     QString::fromStdString(session->username),
                     QString::fromStdString(deployment->second.value)),
        });
    }
    if (candidates.empty()) {
        QMessageBox::information(this, tr("Manage Computer Groups"),
                                 tr("Sign in to a server before managing its computer groups."));
        return;
    }
    QStringList labels;
    labels.reserve(static_cast<qsizetype>(candidates.size()));
    for (const Candidate& candidate : candidates) {
        labels.append(candidate.label);
    }
    bool accepted = false;
    const QString selected = QInputDialog::getItem(
        this, tr("Choose Owning Server"), tr("Manage groups on this server:"), labels, 0, false, &accepted);
    if (!accepted) {
        return;
    }
    const qsizetype index = labels.indexOf(selected);
    if (index < 0 || index >= static_cast<qsizetype>(candidates.size())) {
        statusBar()->showMessage(tr("The owning server selection is no longer available."), 6000);
        return;
    }
    Candidate owner = candidates[static_cast<std::size_t>(index)];
    ComputerGroupAdminDialog dialog(network_, registry_, sessions_, owner.profileId,
                                    owner.deployment, owner.stamp, this);
    dialog.exec();
    if (dialog.changed() && sessions_.isCurrent(owner.stamp)) {
        loadAuthorizedComputers(owner.profileId, owner.deployment, owner.stamp);
    }
}

void MainWindow::refreshAuthorizedComputers()
{
    expireSessions();
    std::unordered_set<std::string> refreshedDeployments;
    std::size_t requestCount = 0;
    for (const auto& [profileId, deployment] : deploymentsByProfile_) {
        if (refreshedDeployments.contains(deployment.value)) {
            continue;
        }
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        const auto profile = registry_.profile(profileId);
        if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) || !profile ||
            session->profileId != profileId ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            continue;
        }
        refreshedDeployments.insert(deployment.value);
        loadAuthorizedComputers(profileId, deployment, *stamp);
        ++requestCount;
    }
    if (requestCount == 0) {
        statusBar()->showMessage(tr("Sign in to a server before refreshing authorized inventory."), 6000);
    } else {
        statusBar()->showMessage(tr("Refreshing authorized computer inventory from %1 server(s).")
                                     .arg(static_cast<qulonglong>(requestCount)), 5000);
    }
}

void MainWindow::loadAuthorizedAccountData(const std::string& profileId,
                                           const DeploymentId& deployment,
                                           const SessionStamp& stamp)
{
    if (!sessions_.isCurrent(stamp)) {
        return;
    }
    const auto* session = sessions_.session(deployment);
    const auto profile = registry_.profile(profileId);
    if (session == nullptr || !profile || session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        invalidateDeploymentSession(deployment);
        computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
            [&](const ComputerRow& row) { return row.id.deployment == deployment; }), computers_.end());
        clearDeploymentAccountData(deployment);
        refreshServerTable();
        refreshComputerTable();
        statusBar()->showMessage(tr("The server session expired. Sign in again."), 6000);
        return;
    }

    clearDeploymentAccountData(deployment);

    const QByteArray bearer = QByteArray("Bearer ") + QByteArray::fromStdString(session->bearerToken);
    const QString serverName = QString::fromStdString(profile->displayName);
    auto& currentRequestGeneration = accountRequestGenerations_[deployment.value];
    if (currentRequestGeneration == std::numeric_limits<std::uint64_t>::max()) {
        accountDataErrors_[deployment.value + "/managers"] =
            (serverName + tr(": account refresh generation is exhausted; sign out and reconnect")).toStdString();
        refreshAccountTables();
        return;
    }
    const std::uint64_t requestGeneration = ++currentRequestGeneration;

    const auto requestCollection = [this, profileId, deployment, stamp, requestGeneration, bearer, serverName]
        (const QString& path, const QString& collectionName) {
        const std::string errorKey = deployment.value + "/" + collectionName.toStdString();
        const auto currentProfile = registry_.profile(profileId);
        const auto endpoint = currentProfile ? apiEndpoint(*currentProfile, path) : std::nullopt;
        if (!endpoint) {
            accountDataErrors_[errorKey] = (serverName + tr(": invalid server address")).toStdString();
            accountDataPending_.erase(errorKey);
            refreshAccountTables();
            return;
        }
        QNetworkRequest request = apiRequest(*endpoint);
        request.setRawHeader("Authorization", bearer);
        QNetworkReply* reply = network_.get(request);
        reply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
        connect(reply, &QNetworkReply::downloadProgress, this,
                [reply](qint64 received, qint64) {
                    if (received > maxHealthResponseBytes) {
                        reply->abort();
                    }
                });
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, profileId, deployment, stamp, requestGeneration, collectionName, serverName, errorKey] {
            const auto currentGeneration = accountRequestGenerations_.find(deployment.value);
            if (!sessions_.isCurrent(stamp) || currentGeneration == accountRequestGenerations_.end() ||
                currentGeneration->second != requestGeneration) {
                reply->deleteLater();
                return;
            }
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            accountDataPending_.erase(errorKey);
            if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
                body.size() > maxHealthResponseBytes) {
                if (statusCode == 401) {
                    invalidateDeploymentSession(deployment);
                    computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
                        [&](const ComputerRow& row) { return row.id.deployment == deployment; }), computers_.end());
                    clearDeploymentAccountData(deployment);
                    refreshServerTable();
                    refreshComputerTable();
                    statusBar()->showMessage(tr("A server session expired; its authorized records were cleared."), 6000);
                } else {
                    accountDataErrors_[errorKey] =
                        (serverName + tr(": %1 — %2").arg(collectionName, replyFailure(reply))).toStdString();
                    refreshAccountTables();
                }
                reply->deleteLater();
                return;
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                accountDataErrors_[errorKey] =
                    (serverName + tr(": malformed %1 response").arg(collectionName)).toStdString();
                refreshAccountTables();
                reply->deleteLater();
                return;
            }
            const auto currentProfile = registry_.profile(profileId);
            if (!currentProfile) {
                reply->deleteLater();
                return;
            }

            if (collectionName == QStringLiteral("accounts")) {
                const QJsonValue valuesValue = document.object().value(QStringLiteral("accounts"));
                const QJsonArray values = valuesValue.toArray();
                if (!valuesValue.isArray() || values.size() > 100) {
                    accountDataErrors_[errorKey] = (serverName + tr(": invalid account list")).toStdString();
                    refreshAccountTables();
                    reply->deleteLater();
                    return;
                }
                std::vector<AccountRow> loaded;
                loaded.reserve(static_cast<std::size_t>(values.size()));
                QSet<QString> seenIds;
                for (const QJsonValue& value : values) {
                    const QJsonObject account = value.toObject();
                    const QString id = account.value(QStringLiteral("id")).toString();
                    const QString username = account.value(QStringLiteral("username")).toString();
                    const QString state = account.value(QStringLiteral("state")).toString();
                    const QJsonValue recoveryQuarantinedValue =
                        account.value(QStringLiteral("recovery_quarantined"));
                    const bool recoveryQuarantinedFieldValid =
                        recoveryQuarantinedValue.isUndefined() || recoveryQuarantinedValue.isBool();
                    const bool recoveryQuarantined = recoveryQuarantinedValue.toBool();
                    const QJsonValue capabilityValue = account.value(QStringLiteral("capabilities"));
                    const QJsonObject capabilityValues = capabilityValue.toObject();
                    const QJsonValue revisionValue = account.value(QStringLiteral("revision"));
                    const double rawRevision = revisionValue.toDouble(-1);
                    const QStringList capabilityFields{
                        QStringLiteral("operate"), QStringLiteral("manage_enrollment"),
                        QStringLiteral("manage_access"), QStringLiteral("manage_accounts"),
                        QStringLiteral("manage_security"),
                    };
                    QStringList grantedCapabilities;
                    bool capabilitiesValid = capabilityValue.isObject();
                    for (const QString& capability : capabilityFields) {
                        const QJsonValue enabled = capabilityValues.value(capability);
                        if (!enabled.isBool()) {
                            capabilitiesValid = false;
                            break;
                        }
                        if (enabled.toBool()) {
                            grantedCapabilities.append(capability);
                        }
                    }
                    const auto scopedId = ScopedAccountId::create(deployment, id.toStdString());
                    const bool validState = state == QStringLiteral("active") ||
                        state == QStringLiteral("disabled") || state == QStringLiteral("pending_mfa");
                    if (!value.isObject() || !validWireText(id) || !validWireText(username) ||
                        !validWireText(state) || !validState || !capabilitiesValid ||
                        !recoveryQuarantinedFieldValid ||
                        !revisionValue.isDouble() || !std::isfinite(rawRevision) ||
                        std::floor(rawRevision) != rawRevision || rawRevision < 1 ||
                        rawRevision > 9007199254740991.0 || !scopedId || seenIds.contains(id)) {
                        accountDataErrors_[errorKey] =
                            (serverName + tr(": invalid or duplicate account record")).toStdString();
                        refreshAccountTables();
                        reply->deleteLater();
                        return;
                    }
                    seenIds.insert(id);
                    const QString capabilitiesText = grantedCapabilities.isEmpty()
                        ? tr("No administrative capabilities")
                        : grantedCapabilities.join(QStringLiteral(", "));
                    loaded.push_back(AccountRow{
                        *scopedId,
                        profileId,
                        currentProfile->displayName,
                        username.toStdString(),
                        state.toStdString(),
                        recoveryQuarantined,
                        capabilityValues.value(QStringLiteral("operate")).toBool(),
                        capabilityValues.value(QStringLiteral("manage_enrollment")).toBool(),
                        capabilityValues.value(QStringLiteral("manage_access")).toBool(),
                        capabilityValues.value(QStringLiteral("manage_accounts")).toBool(),
                        capabilityValues.value(QStringLiteral("manage_security")).toBool(),
                        capabilitiesText.toStdString(),
                        static_cast<std::int64_t>(rawRevision),
                    });
                }
                accounts_.erase(std::remove_if(accounts_.begin(), accounts_.end(),
                    [&](const AccountRow& row) { return row.id.deployment == deployment; }), accounts_.end());
                accounts_.insert(accounts_.end(), loaded.begin(), loaded.end());
            } else if (collectionName == QStringLiteral("managers")) {
                const QJsonValue valuesValue = document.object().value(QStringLiteral("managers"));
                const QJsonArray values = valuesValue.toArray();
                if (!valuesValue.isArray() || values.size() > 100) {
                    accountDataErrors_[errorKey] = (serverName + tr(": invalid manager list")).toStdString();
                    refreshAccountTables();
                    reply->deleteLater();
                    return;
                }
                std::vector<ManagerRow> loaded;
                loaded.reserve(static_cast<std::size_t>(values.size()));
                QSet<QString> seenIds;
                for (const QJsonValue& value : values) {
                    const QJsonObject manager = value.toObject();
                    const QString id = manager.value(QStringLiteral("id")).toString();
                    const QString name = manager.value(QStringLiteral("friendly_name")).toString();
                    const QString identity = manager.value(QStringLiteral("public_identity")).toString();
                    const QString state = manager.value(QStringLiteral("lifecycle_state")).toString();
                    const QJsonValue approvedValue = manager.value(QStringLiteral("approved_at_unix"));
                    std::optional<std::int64_t> approvedAt;
                    if (!approvedValue.isNull()) {
                        const double raw = approvedValue.toDouble(-1);
                        if (!approvedValue.isDouble() || !std::isfinite(raw) || std::floor(raw) != raw ||
                            raw < 0 || raw > 4102444800.0) {
                            accountDataErrors_[errorKey] =
                                (serverName + tr(": invalid manager approval time")).toStdString();
                            refreshAccountTables();
                            reply->deleteLater();
                            return;
                        }
                        approvedAt = static_cast<std::int64_t>(raw);
                    }
                    const auto scopedId = ScopedManagerId::create(deployment, id.toStdString());
                    if (!value.isObject() || !validWireText(id) || !validWireText(name) ||
                        !validWireText(identity, 256) || !validWireText(state) || !scopedId ||
                        seenIds.contains(id)) {
                        accountDataErrors_[errorKey] =
                            (serverName + tr(": invalid or duplicate manager identity")).toStdString();
                        refreshAccountTables();
                        reply->deleteLater();
                        return;
                    }
                    seenIds.insert(id);
                    loaded.push_back(ManagerRow{
                        *scopedId,
                        profileId,
                        currentProfile->displayName,
                        name.toStdString(),
                        identity.toStdString(),
                        state.toStdString(),
                        approvedAt,
                    });
                }
                managers_.erase(std::remove_if(managers_.begin(), managers_.end(),
                    [&](const ManagerRow& row) { return row.id.deployment == deployment; }), managers_.end());
                managers_.insert(managers_.end(), loaded.begin(), loaded.end());
            } else {
                const QJsonValue valuesValue = document.object().value(QStringLiteral("access_assignments"));
                const QJsonArray values = valuesValue.toArray();
                if (!valuesValue.isArray() || values.size() > 100) {
                    accountDataErrors_[errorKey] =
                        (serverName + tr(": invalid access assignment list")).toStdString();
                    refreshAccountTables();
                    reply->deleteLater();
                    return;
                }
                std::vector<AccessAssignmentRow> loaded;
                loaded.reserve(static_cast<std::size_t>(values.size()));
                QSet<QString> seenIds;
                for (const QJsonValue& value : values) {
                    const QJsonObject assignment = value.toObject();
                    const QString id = assignment.value(QStringLiteral("id")).toString();
                    const QString accountId = assignment.value(QStringLiteral("account_id")).toString();
                    const QString capability = assignment.value(QStringLiteral("capability")).toString();
                    const QJsonValue allScopeValue = assignment.value(QStringLiteral("computer_scope_all"));
                    const QJsonArray computerIds = assignment.value(QStringLiteral("computer_ids")).toArray();
                    const QJsonValue revisionValue = assignment.value(QStringLiteral("revision"));
                    const double rawRevision = revisionValue.toDouble(-1);
                    const bool allComputers = allScopeValue.toBool();
                    const auto scopedId = ScopedAccessAssignmentId::create(deployment, id.toStdString());
                    const auto scopedAccount = ScopedAccountId::create(deployment, accountId.toStdString());
                    const bool validCapability = capability == QStringLiteral("operate") ||
                        capability == QStringLiteral("observe") || capability == QStringLiteral("install_maintain");
                    if (!value.isObject() || !validWireText(id) || !validWireText(accountId) ||
                        !validCapability || !allScopeValue.isBool() || computerIds.size() > 100 ||
                        (allComputers && !computerIds.isEmpty()) || (!allComputers && computerIds.isEmpty()) ||
                        !revisionValue.isDouble() || !std::isfinite(rawRevision) ||
                        std::floor(rawRevision) != rawRevision || rawRevision < 1 ||
                        rawRevision > 9007199254740991.0 ||
                        !scopedId || !scopedAccount || seenIds.contains(id)) {
                        accountDataErrors_[errorKey] =
                            (serverName + tr(": invalid or duplicate access assignment")).toStdString();
                        refreshAccountTables();
                        reply->deleteLater();
                        return;
                    }
                    QSet<QString> seenComputers;
                    for (const QJsonValue& computerIdValue : computerIds) {
                        const QString computerId = computerIdValue.toString();
                        if (!computerIdValue.isString() || !validWireText(computerId) || seenComputers.contains(computerId)) {
                            accountDataErrors_[errorKey] =
                                (serverName + tr(": invalid computer scope in assignment")).toStdString();
                            refreshAccountTables();
                            reply->deleteLater();
                            return;
                        }
                        seenComputers.insert(computerId);
                    }
                    seenIds.insert(id);
                    loaded.push_back(AccessAssignmentRow{
                        *scopedId,
                        *scopedAccount,
                        profileId,
                        currentProfile->displayName,
                        capability.toStdString(),
                        allComputers,
                        static_cast<std::size_t>(computerIds.size()),
                        static_cast<std::int64_t>(rawRevision),
                    });
                }
                accessAssignments_.erase(std::remove_if(accessAssignments_.begin(), accessAssignments_.end(),
                    [&](const AccessAssignmentRow& row) { return row.id.deployment == deployment; }), accessAssignments_.end());
                accessAssignments_.insert(accessAssignments_.end(), loaded.begin(), loaded.end());
            }
            accountDataErrors_.erase(errorKey);
            refreshAccountTables();
            reply->deleteLater();
        });
    };

    accountDataErrors_.erase(deployment.value + "/accounts");
    accountDataErrors_.erase(deployment.value + "/managers");
    accountDataErrors_.erase(deployment.value + "/access");
    accountDataPending_.insert(deployment.value + "/accounts");
    accountDataPending_.insert(deployment.value + "/managers");
    accountDataPending_.insert(deployment.value + "/access");
    refreshAccountTables();
    requestCollection(QStringLiteral("/v2/accounts"), QStringLiteral("accounts"));
    requestCollection(QStringLiteral("/v2/managers"), QStringLiteral("managers"));
    requestCollection(QStringLiteral("/v2/access"), QStringLiteral("access"));
}

void MainWindow::refreshAuthorizedAccountData()
{
    std::unordered_set<std::string> refreshed;
    bool requested = false;
    for (const auto& profileDeployment : deploymentsByProfile_) {
        const auto& deployment = profileDeployment.second;
        if (refreshed.contains(deployment.value)) {
            continue;
        }
        const auto* session = sessions_.session(deployment);
        if (session == nullptr || session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            continue;
        }
        refreshed.insert(deployment.value);
        const auto stamp = sessions_.currentStamp(deployment);
        if (stamp && sessions_.isCurrent(*stamp)) {
            requested = true;
            loadAuthorizedAccountData(session->profileId, deployment, *stamp);
        }
    }
    if (!requested) {
        accountsStatus_->setText(tr("Sign in to a server before refreshing authorized account data."));
    }
}

void MainWindow::enrollCurrentManagerDevice()
{
    if (accountsServerFilter_ == nullptr) {
        return;
    }
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    if (selectedProfile.isEmpty()) {
        statusBar()->showMessage(tr("Choose one owning server in the Accounts server filter first."), 6000);
        return;
    }
    const std::string profileId = selectedProfile.toStdString();
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (deploymentEntry == deploymentsByProfile_.end() || !profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 6000);
        return;
    }
    const DeploymentId deployment = deploymentEntry->second;
    const auto* session = sessions_.session(deployment);
    const auto stamp = sessions_.currentStamp(deployment);
    if (session == nullptr || session->profileId != profileId || !stamp ||
        !sessions_.isCurrent(*stamp) || session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the selected server before registering this Manager device."), 6000);
        return;
    }

    bool accepted = false;
    QString friendlyName = QInputDialog::getText(
        this, tr("Register Manager Device"), tr("Name this Manager device:"),
        QLineEdit::Normal, tr("Manager workstation"), &accepted).trimmed();
    if (!accepted) {
        return;
    }
    if (!validWireText(friendlyName, 128)) {
        QMessageBox::warning(this, tr("Invalid Device Name"),
                             tr("Enter a device name between 1 and 128 characters."));
        return;
    }
    if (QMessageBox::question(
            this, tr("Submit Device for Approval"),
            tr("Register this device on %1 as a pending manager? An administrator must approve it before it can sign in.")
                .arg(QString::fromStdString(profile->displayName)),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    const auto* currentSession = sessions_.session(deployment);
    if (!currentSession || !sessions_.isCurrent(*stamp) || currentSession->profileId != profileId ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("The selected server session expired before registration."), 6000);
        return;
    }

    helicoid::manager::ManagerSigningIdentity identity;
    QString identityError;
    if (!identity.loadOrCreate(identityError)) {
        statusBar()->showMessage(tr("Manager device identity unavailable: %1").arg(identityError), 10000);
        return;
    }
    const QByteArray publicKeyHex = identity.publicKeyHex(identityError);
    if (publicKeyHex.size() != 64) {
        statusBar()->showMessage(tr("Manager device identity unavailable: %1").arg(identityError), 10000);
        return;
    }
    const QString proofMessage = QStringLiteral(
        "hs-manager-enroll-v2|deployment:%1|manager:%2|friendly_name:%3")
        .arg(QString::fromStdString(deployment.value), QString::fromLatin1(publicKeyHex), friendlyName);
    const QByteArray proofHex = identity.sign(proofMessage.toUtf8(), identityError);
    if (proofHex.size() != 128) {
        statusBar()->showMessage(tr("Manager device proof failed: %1").arg(identityError), 10000);
        return;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/managers"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 6000);
        return;
    }
    QJsonObject requestObject{
        {QStringLiteral("public_key"), QString::fromLatin1(publicKeyHex)},
        {QStringLiteral("friendly_name"), friendlyName},
        {QStringLiteral("proof"), QString::fromLatin1(proofHex)},
    };
    SensitiveBuffer requestBody(QJsonDocument(requestObject).toJson(QJsonDocument::Compact));
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
    QNetworkReply* reply = network_.post(request, requestBody.bytes());
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, publicKeyHex,
             friendlyName, serverName = QString::fromStdString(profile->displayName)] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 201 ||
            body.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(tr("Manager registration failed on %1: %2")
                                         .arg(serverName, replyFailure(reply)), 10000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject manager = document.object();
        const QString id = manager.value(QStringLiteral("id")).toString();
        const QString identity = manager.value(QStringLiteral("public_identity")).toString();
        const QString returnedName = manager.value(QStringLiteral("friendly_name")).toString();
        const QString state = manager.value(QStringLiteral("lifecycle_state")).toString();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !validWireText(id) || identity != QStringLiteral("key:") + QString::fromLatin1(publicKeyHex) ||
            returnedName != friendlyName || state != QStringLiteral("pending")) {
            statusBar()->showMessage(tr("The server returned an invalid manager registration."), 8000);
            reply->deleteLater();
            return;
        }
        loadAuthorizedAccountData(profileId, deployment, stamp);
        statusBar()->showMessage(tr("Manager device registered on %1 and is pending administrator approval.")
                                     .arg(serverName), 8000);
        reply->deleteLater();
    });
}

void MainWindow::approveSelectedManager()
{
    if (managerTable_ == nullptr || managerTable_->currentRow() < 0) {
        statusBar()->showMessage(tr("Select a manager device in the Accounts workspace first."), 5000);
        return;
    }
    const int row = managerTable_->currentRow();
    const auto* serverItem = managerTable_->item(row, 0);
    const auto* deviceItem = managerTable_->item(row, 1);
    const auto* stateItem = managerTable_->item(row, 2);
    if (serverItem == nullptr || deviceItem == nullptr || stateItem == nullptr) {
        statusBar()->showMessage(tr("The selected manager record is incomplete."), 5000);
        return;
    }
    const std::string profileId = serverItem->data(Qt::UserRole).toString().toStdString();
    const std::string managerId = deviceItem->data(Qt::UserRole).toString().toStdString();
    const QString managerIdText = QString::fromStdString(managerId);
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    if (profileId.empty() || !validRouteSegment(managerIdText) ||
        deploymentEntry == deploymentsByProfile_.end()) {
        statusBar()->showMessage(tr("The selected manager record has no owning server identity."), 6000);
        return;
    }
    const DeploymentId deployment = deploymentEntry->second;
    const auto* session = sessions_.session(deployment);
    const auto stamp = sessions_.currentStamp(deployment);
    if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
        session->profileId != profileId || session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the manager's owning server before approving it."), 6000);
        return;
    }
    if (stateItem->text() != QStringLiteral("pending")) {
        statusBar()->showMessage(tr("Only a pending manager device can be approved."), 5000);
        return;
    }
    const QString serverName = serverItem->text();
    const QString deviceName = deviceItem->text();
    if (QMessageBox::question(
            this, tr("Approve Manager Device"),
            tr("Approve %1 on %2? This grants the device eligibility to request manager sessions.")
                .arg(deviceName, serverName),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    const auto* currentSession = sessions_.session(deployment);
    if (!currentSession || !sessions_.isCurrent(*stamp) || currentSession->profileId != profileId ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("The manager's server session expired before approval."), 6000);
        return;
    }
    const auto profile = registry_.profile(profileId);
    if (!profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 6000);
        return;
    }
    const auto endpoint = apiEndpoint(
        *profile, QStringLiteral("/v2/managers/%1/approve").arg(managerIdText));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 6000);
        return;
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
    QNetworkReply* reply = network_.post(request, QByteArray("{\"confirm\":true}"));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, managerId, deviceName, serverName] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(tr("Manager approval failed on %1: %2")
                                         .arg(serverName, replyFailure(reply)), 10000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject result = document.object();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            result.value(QStringLiteral("id")).toString().toStdString() != managerId ||
            result.value(QStringLiteral("lifecycle_state")).toString() != QStringLiteral("approved")) {
            statusBar()->showMessage(tr("The server returned an invalid manager approval result."), 8000);
            reply->deleteLater();
            return;
        }
        loadAuthorizedAccountData(profileId, deployment, stamp);
        statusBar()->showMessage(tr("Approved %1 on %2.").arg(deviceName, serverName), 7000);
        reply->deleteLater();
    });
}

void MainWindow::revokeSelectedManager()
{
    if (managerTable_ == nullptr || managerTable_->currentRow() < 0) {
        statusBar()->showMessage(tr("Select a manager device in the Accounts workspace first."), 5000);
        return;
    }
    const int row = managerTable_->currentRow();
    const auto* serverItem = managerTable_->item(row, 0);
    const auto* deviceItem = managerTable_->item(row, 1);
    const auto* stateItem = managerTable_->item(row, 2);
    if (serverItem == nullptr || deviceItem == nullptr || stateItem == nullptr) {
        statusBar()->showMessage(tr("The selected manager record is incomplete."), 5000);
        return;
    }
    const std::string profileId = serverItem->data(Qt::UserRole).toString().toStdString();
    const std::string managerId = deviceItem->data(Qt::UserRole).toString().toStdString();
    const QString managerIdText = QString::fromStdString(managerId);
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    if (profileId.empty() || !validRouteSegment(managerIdText) ||
        deploymentEntry == deploymentsByProfile_.end()) {
        statusBar()->showMessage(tr("The selected manager record has no owning server identity."), 6000);
        return;
    }
    const DeploymentId deployment = deploymentEntry->second;
    const auto* session = sessions_.session(deployment);
    const auto stamp = sessions_.currentStamp(deployment);
    if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
        session->profileId != profileId || session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the manager's owning server before revoking it."), 6000);
        return;
    }
    if (stateItem->text() == QStringLiteral("revoked")) {
        statusBar()->showMessage(tr("The selected manager device is already revoked."), 5000);
        return;
    }
    const QString serverName = serverItem->text();
    const QString deviceName = deviceItem->text();
    const bool revokesCurrentDevice = session->managerId == managerId;
    const QString confirmation = revokesCurrentDevice
        ? tr("Revoke %1 on %2? This also ends the current Manager session.").arg(deviceName, serverName)
        : tr("Revoke %1 on %2? The device will no longer be eligible for manager sessions.").arg(deviceName, serverName);
    if (QMessageBox::question(
            this, tr("Revoke Manager Device"), confirmation,
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    const auto* currentSession = sessions_.session(deployment);
    if (!currentSession || !sessions_.isCurrent(*stamp) || currentSession->profileId != profileId ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("The manager's server session expired before revocation."), 6000);
        return;
    }
    const auto profile = registry_.profile(profileId);
    if (!profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 6000);
        return;
    }
    const auto endpoint = apiEndpoint(
        *profile, QStringLiteral("/v2/managers/%1/revoke").arg(managerIdText));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 6000);
        return;
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
    const QByteArray revokeBody = QJsonDocument(QJsonObject{
        {QStringLiteral("confirm"), true},
        {QStringLiteral("confirm_manager_id"), managerIdText},
    }).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = network_.post(request, revokeBody);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, managerId, deviceName,
             serverName, revokesCurrentDevice] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(tr("Manager revocation failed on %1: %2")
                                         .arg(serverName, replyFailure(reply)), 10000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject result = document.object();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            result.value(QStringLiteral("id")).toString().toStdString() != managerId ||
            result.value(QStringLiteral("lifecycle_state")).toString() != QStringLiteral("revoked")) {
            statusBar()->showMessage(tr("The server returned an invalid manager revocation result."), 8000);
            reply->deleteLater();
            return;
        }
        if (revokesCurrentDevice) {
            invalidateDeploymentSession(deployment);
            computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
                [&](const ComputerRow& computer) { return computer.id.deployment == deployment; }), computers_.end());
            clearDeploymentAccountData(deployment);
            refreshComputerTable();
            refreshServerTable();
            statusBar()->showMessage(tr("Revoked %1 on %2; this Manager session has ended.")
                                         .arg(deviceName, serverName), 8000);
        } else {
            loadAuthorizedAccountData(profileId, deployment, stamp);
            statusBar()->showMessage(tr("Revoked %1 on %2.").arg(deviceName, serverName), 7000);
        }
        reply->deleteLater();
    });
}

void MainWindow::createServerAccount()
{
    if (accountsServerFilter_ == nullptr) {
        return;
    }
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    if (selectedProfile.isEmpty()) {
        statusBar()->showMessage(tr("Choose one owning server in the Accounts server filter first."), 6000);
        return;
    }
    const std::string profileId = selectedProfile.toStdString();
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (deploymentEntry == deploymentsByProfile_.end() || !profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 6000);
        return;
    }
    const DeploymentId deployment = deploymentEntry->second;
    const auto* session = sessions_.session(deployment);
    const auto stamp = sessions_.currentStamp(deployment);
    if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
        session->profileId != profileId || session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the selected server before creating an account."), 6000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Create Server Account"));
    dialog.setMinimumWidth(540);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* username = new QLineEdit(&dialog);
    username->setMaxLength(64);
    username->setObjectName(QStringLiteral("newAccountUsername"));
    auto* password = new QLineEdit(&dialog);
    password->setEchoMode(QLineEdit::Password);
    password->setMaxLength(256);
    password->setObjectName(QStringLiteral("newAccountPassword"));
    form->addRow(tr("Username"), username);
    form->addRow(tr("Temporary password"), password);
    auto* operate = new QCheckBox(tr("Operate computers"), &dialog);
    auto* manageEnrollment = new QCheckBox(tr("Manage enrollment and manager devices"), &dialog);
    auto* manageAccess = new QCheckBox(tr("Manage computer access"), &dialog);
    auto* manageAccounts = new QCheckBox(tr("Manage accounts"), &dialog);
    auto* manageSecurity = new QCheckBox(tr("Manage security and MFA policy"), &dialog);
    form->addRow(tr("Capabilities"), operate);
    form->addRow(QString(), manageEnrollment);
    form->addRow(QString(), manageAccess);
    form->addRow(QString(), manageAccounts);
    form->addRow(QString(), manageSecurity);
    layout->addLayout(form);
    layout->addWidget(subheading(
        tr("The account starts inactive. The user must configure an authenticator and store the recovery codes before the 15-minute enrollment expires."),
        &dialog));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Create Pending Account"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const QString name = username->text().trimmed();
        const QByteArray passwordBytes = password->text().toUtf8();
        const bool validName = name.size() >= 3 && name.size() <= 64 && std::all_of(
            name.cbegin(), name.cend(), [](QChar c) {
                const ushort value = c.unicode();
                return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
                       (value >= '0' && value <= '9') || value == '.' || value == '_' || value == '-';
            });
        const bool validPassword = passwordBytes.size() >= 12 && passwordBytes.size() <= 256;
        if (!validName || !validPassword) {
            QMessageBox::warning(&dialog, tr("Invalid Account"),
                                 tr("Use a 3–64 character username and a password of 12–256 UTF-8 bytes."));
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString accountName = username->text().trimmed();
    QString accountPassword = password->text();
    password->clear();
    if (QMessageBox::question(
            this, tr("Create Pending Account"),
            tr("Create %1 on %2 with the selected capabilities? The account will remain inactive until MFA enrollment is confirmed.")
                .arg(accountName, QString::fromStdString(profile->displayName)),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        accountPassword.fill(QChar(u'\0'));
        return;
    }
    const auto* currentSession = sessions_.session(deployment);
    if (!currentSession || !sessions_.isCurrent(*stamp) || currentSession->profileId != profileId ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        accountPassword.fill(QChar(u'\0'));
        statusBar()->showMessage(tr("The selected server session expired before account creation."), 6000);
        return;
    }

    QJsonObject requestObject{
        {QStringLiteral("username"), accountName},
        {QStringLiteral("password"), accountPassword},
        {QStringLiteral("can_operate"), operate->isChecked()},
        {QStringLiteral("can_manage_enrollment"), manageEnrollment->isChecked()},
        {QStringLiteral("can_manage_access"), manageAccess->isChecked()},
        {QStringLiteral("can_manage_accounts"), manageAccounts->isChecked()},
        {QStringLiteral("can_manage_security"), manageSecurity->isChecked()},
    };
    SensitiveBuffer requestBody(QJsonDocument(requestObject).toJson(QJsonDocument::Compact));
    requestObject.insert(QStringLiteral("password"), QString());
    accountPassword.fill(QChar(u'\0'));
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/accounts"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 6000);
        return;
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
    QNetworkReply* reply = network_.post(request, requestBody.bytes());
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp,
             serverName = QString::fromStdString(profile->displayName), accountName] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        SensitiveBuffer response(reply->readAll());
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 201 ||
            response.bytes().size() > maxHealthResponseBytes) {
            const QString detail = statusCode == 403
                ? tr("Check account-management permission and sign in again to refresh MFA.")
                : replyFailure(reply);
            statusBar()->showMessage(tr("Account creation failed on %1: %2").arg(serverName, detail), 10000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response.bytes(), &parseError);
        const QJsonObject object = document.object();
        const QJsonObject account = object.value(QStringLiteral("account")).toObject();
        const QString accountId = account.value(QStringLiteral("id")).toString();
        const QString returnedName = account.value(QStringLiteral("username")).toString();
        const QString state = account.value(QStringLiteral("state")).toString();
        QString enrollmentId = object.value(QStringLiteral("mfa_enrollment_id")).toString();
        QString totpSecret = object.value(QStringLiteral("totp_secret_base32")).toString();
        const QJsonValue recoveryValue = object.value(QStringLiteral("recovery_codes"));
        const QJsonArray recoveryArray = recoveryValue.toArray();
        bool validRecoveryCodes = recoveryValue.isArray() && !recoveryArray.isEmpty() &&
            recoveryArray.size() <= 20;
        QStringList recoveryCodes;
        for (const QJsonValue& value : recoveryArray) {
            const QString code = value.toString();
            if (!value.isString() || !validWireText(code, 64)) validRecoveryCodes = false;
            recoveryCodes.append(code);
        }
        const bool validSecret = validWireText(totpSecret, 128) && totpSecret.size() >= 16;
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !validWireText(accountId) || returnedName != accountName ||
            state != QStringLiteral("pending_mfa") || !validWireText(enrollmentId) ||
            !validSecret || !validRecoveryCodes) {
            enrollmentId.fill(QChar(u'\0'));
            totpSecret.fill(QChar(u'\0'));
            for (QString& code : recoveryCodes) code.fill(QChar(u'\0'));
            statusBar()->showMessage(tr("The server returned invalid account enrollment material."), 8000);
            reply->deleteLater();
            return;
        }
        loadAuthorizedAccountData(profileId, deployment, stamp);

        QDialog materialDialog(this);
        materialDialog.setWindowTitle(tr("Store MFA Enrollment Material"));
        materialDialog.setMinimumSize(620, 500);
        auto* materialLayout = new QVBoxLayout(&materialDialog);
        materialLayout->addWidget(subheading(
            tr("%1 on %2 is pending MFA. Give this material only to the intended account holder and store the recovery codes independently. The values are not saved by Manager and are shown once.")
                .arg(accountName, serverName), &materialDialog));
        auto* material = new QPlainTextEdit(&materialDialog);
        material->setReadOnly(true);
        material->setLineWrapMode(QPlainTextEdit::NoWrap);
        material->setPlainText(
            tr("MFA enrollment ID: %1\nTOTP secret: %2\n\nRecovery codes:\n%3\n\nThe user must configure an authenticator and confirm enrollment within 15 minutes.")
                .arg(enrollmentId, totpSecret, recoveryCodes.join(QLatin1Char('\n'))));
        materialLayout->addWidget(material, 1);
        auto* materialButtons = new QDialogButtonBox(QDialogButtonBox::Close, &materialDialog);
        connect(materialButtons, &QDialogButtonBox::rejected, &materialDialog, &QDialog::reject);
        materialLayout->addWidget(materialButtons);
        materialDialog.exec();
        material->clear();
        enrollmentId.fill(QChar(u'\0'));
        totpSecret.fill(QChar(u'\0'));
        for (QString& code : recoveryCodes) code.fill(QChar(u'\0'));
        statusBar()->showMessage(tr("Created %1 on %2 in pending MFA state.").arg(accountName, serverName), 7000);
        reply->deleteLater();
    });
}

void MainWindow::beginSelfMfaEnrollment()
{
    if (accountsServerFilter_ == nullptr) {
        return;
    }
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    if (selectedProfile.isEmpty()) {
        statusBar()->showMessage(tr("Choose one owning server in the Accounts server filter first."), 6000);
        return;
    }
    const std::string profileId = selectedProfile.toStdString();
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (deploymentEntry == deploymentsByProfile_.end() || !profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 5000);
        return;
    }
    const DeploymentId deployment = deploymentEntry->second;
    const auto stamp = sessions_.currentStamp(deployment);
    const auto* session = sessions_.session(deployment);
    if (!stamp || !session || !sessions_.isCurrent(*stamp) || session->profileId != profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the selected server before setting up MFA."), 6000);
        return;
    }
    if (QMessageBox::question(
            this, tr("Set Up MFA"),
            tr("Create a new authenticator enrollment for %1 on %2? Existing active factors cannot be replaced here.")
                .arg(QString::fromStdString(session->username), QString::fromStdString(profile->displayName)),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    const auto* currentSession = sessions_.session(deployment);
    if (!currentSession || !sessions_.isCurrent(*stamp) || currentSession->profileId != profileId ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("The selected server session expired before MFA setup."), 6000);
        return;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/mfa/enroll/begin"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }
    const std::string accountId = currentSession->accountId;
    const QString serverName = QString::fromStdString(profile->displayName);
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
    QNetworkReply* reply = network_.post(request, QByteArray("{}"));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, accountId, serverName] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        SensitiveBuffer response(reply->readAll());
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 201 ||
            response.bytes().size() > maxHealthResponseBytes) {
            const QString detail = statusCode == 409
                ? tr("An active MFA factor already exists or enrollment capacity is unavailable.")
                : replyFailure(reply);
            statusBar()->showMessage(tr("MFA setup failed on %1: %2").arg(serverName, detail), 10000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response.bytes(), &parseError);
        const QJsonObject object = document.object();
        QString returnedAccountId = object.value(QStringLiteral("account_id")).toString();
        QString enrollmentId = object.value(QStringLiteral("mfa_enrollment_id")).toString();
        QString totpSecret = object.value(QStringLiteral("totp_secret_base32")).toString();
        const QJsonValue recoveryValue = object.value(QStringLiteral("recovery_codes"));
        const QJsonArray recoveryArray = recoveryValue.toArray();
        QStringList recoveryCodes;
        bool validCodes = recoveryValue.isArray() && !recoveryArray.isEmpty() && recoveryArray.size() <= 20;
        for (const QJsonValue& value : recoveryArray) {
            const QString code = value.toString();
            if (!value.isString() || !validWireText(code, 64)) validCodes = false;
            recoveryCodes.append(code);
        }
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            returnedAccountId.toStdString() != accountId || !validWireText(enrollmentId) ||
            !validWireText(totpSecret, 128) || totpSecret.size() < 16 || !validCodes) {
            returnedAccountId.fill(QChar(u'\0'));
            enrollmentId.fill(QChar(u'\0'));
            totpSecret.fill(QChar(u'\0'));
            for (QString& code : recoveryCodes) code.fill(QChar(u'\0'));
            statusBar()->showMessage(tr("The server returned invalid MFA enrollment material."), 8000);
            reply->deleteLater();
            return;
        }
        const QString enrollmentToConfirm = enrollmentId;
        QDialog materialDialog(this);
        materialDialog.setWindowTitle(tr("Store MFA Enrollment Material"));
        materialDialog.setMinimumSize(620, 500);
        auto* materialLayout = new QVBoxLayout(&materialDialog);
        materialLayout->addWidget(subheading(
            tr("Store the recovery codes independently. This one-time material is not saved by Manager. After setup, enter a current authenticator code to confirm enrollment."),
            &materialDialog));
        auto* material = new QPlainTextEdit(&materialDialog);
        material->setReadOnly(true);
        material->setLineWrapMode(QPlainTextEdit::NoWrap);
        material->setPlainText(
            tr("MFA enrollment ID: %1\nTOTP secret: %2\n\nRecovery codes:\n%3")
                .arg(enrollmentId, totpSecret, recoveryCodes.join(QLatin1Char('\n'))));
        materialLayout->addWidget(material, 1);
        auto* materialButtons = new QDialogButtonBox(QDialogButtonBox::Close, &materialDialog);
        connect(materialButtons, &QDialogButtonBox::rejected, &materialDialog, &QDialog::reject);
        materialLayout->addWidget(materialButtons);
        materialDialog.exec();
        material->clear();
        returnedAccountId.fill(QChar(u'\0'));
        enrollmentId.fill(QChar(u'\0'));
        totpSecret.fill(QChar(u'\0'));
        for (QString& code : recoveryCodes) code.fill(QChar(u'\0'));
        reply->deleteLater();
        confirmMfaEnrollment(profileId, enrollmentToConfirm);
    });
}

void MainWindow::assignAccountAccess()
{
    if (accountsServerFilter_ == nullptr) {
        return;
    }
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    if (selectedProfile.isEmpty()) {
        statusBar()->showMessage(tr("Choose one owning server in the Accounts server filter first."), 6000);
        return;
    }
    const std::string profileId = selectedProfile.toStdString();
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (deploymentEntry == deploymentsByProfile_.end() || !profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 5000);
        return;
    }
    const DeploymentId deployment = deploymentEntry->second;
    const auto stamp = sessions_.currentStamp(deployment);
    const auto* session = sessions_.session(deployment);
    if (!stamp || !session || !sessions_.isCurrent(*stamp) || session->profileId != profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the selected server before changing account access."), 6000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Assign Account Access"));
    dialog.setMinimumSize(560, 560);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(subheading(
        tr("This replaces the selected account's assignment for the chosen capability. The server requires an approved Manager session and MFA within the last two minutes. Observe permission does not itself enable pixels or capture."),
        &dialog));
    auto* form = new QFormLayout;
    auto* account = new QComboBox(&dialog);
    account->setObjectName(QStringLiteral("accessTargetAccount"));
    for (const auto& row : accounts_) {
        if (row.profileId != profileId || row.state != "active") {
            continue;
        }
        account->addItem(QStringLiteral("%1 (%2)")
                             .arg(QString::fromStdString(row.username),
                                  QString::fromStdString(row.id.serverEntityId)),
                         QString::fromStdString(row.id.serverEntityId));
    }
    auto* capability = new QComboBox(&dialog);
    capability->setObjectName(QStringLiteral("accessCapability"));
    capability->addItem(tr("Operate computers"), QStringLiteral("operate"));
    capability->addItem(tr("Observe computers"), QStringLiteral("observe"));
    capability->addItem(tr("Install and maintain"), QStringLiteral("install_maintain"));
    form->addRow(tr("Active account"), account);
    form->addRow(tr("Capability"), capability);
    layout->addLayout(form);
    auto* scopeAll = new QCheckBox(tr("Apply to all current and future computers"), &dialog);
    scopeAll->setObjectName(QStringLiteral("accessScopeAll"));
    layout->addWidget(scopeAll);
    auto* computerList = new QListWidget(&dialog);
    computerList->setObjectName(QStringLiteral("accessComputerScope"));
    computerList->setSelectionMode(QAbstractItemView::NoSelection);
    for (const auto& row : computers_) {
        if (row.profileId != profileId) {
            continue;
        }
        auto* item = new QListWidgetItem(
            QStringLiteral("%1 · %2 · %3")
                .arg(QString::fromStdString(row.friendlyName),
                     QString::fromStdString(row.platform),
                     QString::fromStdString(row.id.serverComputerId)),
            computerList);
        item->setData(Qt::UserRole, QString::fromStdString(row.id.serverComputerId));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    computerList->setEnabled(!scopeAll->isChecked());
    connect(scopeAll, &QCheckBox::toggled, computerList, [computerList](bool checked) {
        computerList->setEnabled(!checked);
    });
    layout->addWidget(computerList, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Review Assignment"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (account->currentIndex() < 0) {
            QMessageBox::warning(&dialog, tr("No Active Account"),
                                 tr("There are no active accounts loaded for this server."));
            return;
        }
        if (!scopeAll->isChecked()) {
            bool selectedComputer = false;
            for (int i = 0; i < computerList->count(); ++i) {
                selectedComputer = selectedComputer ||
                    computerList->item(i)->checkState() == Qt::Checked;
            }
            if (!selectedComputer) {
                QMessageBox::warning(&dialog, tr("Computer Scope Required"),
                                     tr("Choose one or more loaded computers, or select all current and future computers."));
                return;
            }
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString accountId = account->currentData().toString();
    const QString capabilityName = capability->currentData().toString();
    QStringList computerIds;
    if (!scopeAll->isChecked()) {
        for (int i = 0; i < computerList->count(); ++i) {
            const auto* item = computerList->item(i);
            if (item->checkState() == Qt::Checked) {
                computerIds.append(item->data(Qt::UserRole).toString());
            }
        }
    }
    const QString scopeDescription = scopeAll->isChecked()
        ? tr("all current and future computers")
        : tr("%1 selected computer(s)").arg(computerIds.size());
    if (QMessageBox::question(
            this, tr("Confirm Access Assignment"),
            tr("Grant %1 to account %2 on %3 for %4? This replaces the existing assignment for that capability.")
                .arg(capability->currentText(), account->currentText(),
                     QString::fromStdString(profile->displayName), scopeDescription),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    const auto* currentSession = sessions_.session(deployment);
    if (!currentSession || !sessions_.isCurrent(*stamp) || currentSession->profileId != profileId ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("The selected server session expired before the assignment."), 6000);
        return;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/access"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }
    QJsonArray idArray;
    for (const QString& id : computerIds) idArray.append(id);
    const bool expectedScopeAll = scopeAll->isChecked();
    QStringList expectedComputerIds = computerIds;
    expectedComputerIds.sort(Qt::CaseSensitive);
    const QByteArray body = QJsonDocument(QJsonObject{
        {QStringLiteral("account_id"), accountId},
        {QStringLiteral("capability"), capabilityName},
        {QStringLiteral("computer_scope_all"), scopeAll->isChecked()},
        {QStringLiteral("computer_ids"), idArray},
    }).toJson(QJsonDocument::Compact);
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
    QNetworkReply* reply = network_.post(request, body);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, accountId, capabilityName,
             expectedScopeAll, expectedComputerIds,
             serverName = QString::fromStdString(profile->displayName)] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 201 ||
            response.size() > maxHealthResponseBytes) {
            const QString detail = statusCode == 403
                ? tr("The server requires access-management permission and fresh MFA; sign in again with MFA.")
                : replyFailure(reply);
            statusBar()->showMessage(tr("Access assignment failed on %1: %2").arg(serverName, detail), 10000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        const QJsonObject result = document.object();
        const QString assignmentId = result.value(QStringLiteral("id")).toString();
        const QJsonValue revision = result.value(QStringLiteral("revision"));
        const QJsonValue returnedScopeAll = result.value(QStringLiteral("computer_scope_all"));
        const QJsonValue returnedComputerIdsValue = result.value(QStringLiteral("computer_ids"));
        QStringList returnedComputerIds;
        if (returnedComputerIdsValue.isArray()) {
            for (const QJsonValue& value : returnedComputerIdsValue.toArray()) {
                if (!value.isString()) {
                    returnedComputerIds.append(QString());
                } else {
                    returnedComputerIds.append(value.toString());
                }
            }
        }
        returnedComputerIds.sort(Qt::CaseSensitive);
        const double revisionNumber = revision.toDouble(0);
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !validWireText(assignmentId) || result.value(QStringLiteral("account_id")).toString() != accountId ||
            result.value(QStringLiteral("capability")).toString() != capabilityName ||
            !revision.isDouble() || !std::isfinite(revisionNumber) || revisionNumber < 1 ||
            std::floor(revisionNumber) != revisionNumber || !returnedScopeAll.isBool() ||
            returnedScopeAll.toBool() != expectedScopeAll || !returnedComputerIdsValue.isArray() ||
            returnedComputerIds != expectedComputerIds) {
            statusBar()->showMessage(tr("The server returned an invalid access assignment."), 8000);
            reply->deleteLater();
            return;
        }
        const auto* currentSession = sessions_.session(deployment);
        if (currentSession != nullptr && currentSession->accountId == accountId.toStdString()) {
            loadObservationScope(profileId, deployment, stamp, true);
        }
        loadAuthorizedAccountData(profileId, deployment, stamp);
        statusBar()->showMessage(tr("Updated %1 access on %2.").arg(capabilityName, serverName), 7000);
        reply->deleteLater();
    });
}

void MainWindow::revokeSelectedAccountAccess()
{
    if (accessTable_ == nullptr || accessTable_->currentRow() < 0) {
        statusBar()->showMessage(tr("Select an access assignment in the Accounts workspace first."), 5000);
        return;
    }
    const int row = accessTable_->currentRow();
    const auto* serverItem = accessTable_->item(row, 0);
    const auto* accountItem = accessTable_->item(row, 1);
    const auto* capabilityItem = accessTable_->item(row, 2);
    if (serverItem == nullptr || accountItem == nullptr || capabilityItem == nullptr) {
        statusBar()->showMessage(tr("The selected access assignment is incomplete."), 5000);
        return;
    }

    const QString profileIdText = serverItem->data(Qt::UserRole).toString();
    const QString assignmentId = serverItem->data(Qt::UserRole + 1).toString();
    bool revisionValid = false;
    const qlonglong expectedRevision = serverItem->data(Qt::UserRole + 2).toLongLong(&revisionValid);
    const std::string profileId = profileIdText.toStdString();
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    if (profileId.empty() || !validRouteSegment(assignmentId) || !revisionValid || expectedRevision < 1 ||
        deploymentEntry == deploymentsByProfile_.end()) {
        statusBar()->showMessage(tr("The selected assignment has no valid owning server, ID, or revision."), 6000);
        return;
    }

    const DeploymentId deployment = deploymentEntry->second;
    const auto stamp = sessions_.currentStamp(deployment);
    const auto* session = sessions_.session(deployment);
    if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
        session->profileId != profileId || session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the assignment's owning server before revoking it."), 6000);
        return;
    }
    const auto profile = registry_.profile(profileId);
    if (!profile) {
        statusBar()->showMessage(tr("The assignment's server profile is unavailable."), 5000);
        return;
    }

    const QString serverName = serverItem->text();
    const QString accountId = accountItem->text();
    const QString capability = capabilityItem->text();
    const QString confirmation = QStringLiteral("revoke access %1 revision %2")
                                    .arg(assignmentId)
                                    .arg(expectedRevision);
    bool accepted = false;
    const QString entered = QInputDialog::getText(
        this, tr("Confirm Access Revocation"),
        tr("Revoke %1 for account %2 on %3? This removes the assignment and its computer scope.\n\nType exactly:\n%4")
            .arg(capability, accountId, serverName, confirmation),
        QLineEdit::Normal, QString(), &accepted);
    if (!accepted) {
        return;
    }
    if (entered != confirmation) {
        statusBar()->showMessage(tr("Access revocation was not confirmed."), 5000);
        return;
    }

    const auto* currentSession = sessions_.session(deployment);
    if (currentSession == nullptr || !sessions_.isCurrent(*stamp) || currentSession->profileId != profileId ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("The owning server session expired before revocation."), 6000);
        return;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/access/%1").arg(assignmentId));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }
    const QByteArray body = QJsonDocument(QJsonObject{
        {QStringLiteral("expected_revision"), static_cast<qint64>(expectedRevision)},
        {QStringLiteral("confirm"), true},
    }).toJson(QJsonDocument::Compact);
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
    QNetworkReply* reply = network_.sendCustomRequest(request, QByteArrayLiteral("DELETE"), body);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, assignmentId,
             accountId, capability, expectedRevision, serverName] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            response.size() > maxHealthResponseBytes) {
            const QString detail = statusCode == 403
                ? tr("The server requires access-management permission and fresh MFA; sign in again with MFA.")
                : statusCode == 409
                    ? tr("The assignment changed. Refresh the Accounts data and review the current revision.")
                    : replyFailure(reply);
            if (statusCode == 409) {
                loadAuthorizedAccountData(profileId, deployment, stamp);
            }
            statusBar()->showMessage(tr("Access revocation failed on %1: %2").arg(serverName, detail), 10000);
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        const QJsonObject result = document.object();
        const QJsonValue revokedRevision = result.value(QStringLiteral("revoked_revision"));
        const double revisionNumber = revokedRevision.toDouble(-1);
        const bool valid = parseError.error == QJsonParseError::NoError && document.isObject() &&
            result.value(QStringLiteral("id")).toString() == assignmentId &&
            result.value(QStringLiteral("account_id")).toString() == accountId &&
            result.value(QStringLiteral("capability")).toString() == capability &&
            result.value(QStringLiteral("state")).toString() == QStringLiteral("revoked") &&
            revokedRevision.isDouble() && std::isfinite(revisionNumber) &&
            revisionNumber == static_cast<double>(expectedRevision);
        if (!valid) {
            statusBar()->showMessage(tr("The server returned an invalid access-revocation result."), 8000);
            reply->deleteLater();
            return;
        }
        const auto* currentSession = sessions_.session(deployment);
        if (currentSession != nullptr && currentSession->accountId == accountId.toStdString()) {
            loadObservationScope(profileId, deployment, stamp, true);
        }
        loadAuthorizedAccountData(profileId, deployment, stamp);
        statusBar()->showMessage(tr("Revoked access assignment %1 on %2.").arg(assignmentId, serverName), 7000);
        reply->deleteLater();
    });
}

void MainWindow::editSelectedAccountCapabilities()
{
    if (accountsServerFilter_ == nullptr || accountTable_ == nullptr) {
        return;
    }
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    const int row = accountTable_->currentRow();
    if (selectedProfile.isEmpty() || row < 0) {
        statusBar()->showMessage(tr("Choose one server and select its active account first."), 6000);
        return;
    }
    const auto* serverItem = accountTable_->item(row, 0);
    const auto* idItem = accountTable_->item(row, 2);
    const auto* stateItem = accountTable_->item(row, 3);
    if (serverItem == nullptr || idItem == nullptr || stateItem == nullptr ||
        serverItem->data(Qt::UserRole).toString() != selectedProfile ||
        stateItem->text() != QStringLiteral("active")) {
        statusBar()->showMessage(tr("Select an active account owned by the selected server."), 6000);
        return;
    }
    const std::string profileId = selectedProfile.toStdString();
    const std::string accountId = idItem->text().toStdString();
    const auto accountIt = std::find_if(accounts_.begin(), accounts_.end(), [&](const AccountRow& account) {
        return account.profileId == profileId && account.id.serverEntityId == accountId;
    });
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (accountIt == accounts_.end() || deploymentEntry == deploymentsByProfile_.end() || !profile ||
        !validRouteSegment(QString::fromStdString(accountId))) {
        statusBar()->showMessage(tr("The selected account has no valid owning server identity."), 6000);
        return;
    }
    const AccountRow selectedAccount = *accountIt;
    const DeploymentId deployment = deploymentEntry->second;
    const auto stamp = sessions_.currentStamp(deployment);
    const auto* session = sessions_.session(deployment);
    if (!stamp || !session || !sessions_.isCurrent(*stamp) || session->profileId != profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the selected server before changing capabilities."), 6000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit Account Capabilities"));
    dialog.setMinimumWidth(520);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(subheading(
        tr("The server requires a current approved Manager session and MFA within the last two minutes. Only a security administrator can grant or revoke security administration."),
        &dialog));
    auto* form = new QFormLayout;
    auto* operate = new QCheckBox(tr("Operate computers"), &dialog);
    auto* enrollment = new QCheckBox(tr("Manage enrollment and Manager devices"), &dialog);
    auto* access = new QCheckBox(tr("Manage computer access"), &dialog);
    auto* accounts = new QCheckBox(tr("Manage accounts"), &dialog);
    auto* security = new QCheckBox(tr("Manage security and MFA policy"), &dialog);
    operate->setChecked(selectedAccount.canOperate);
    enrollment->setChecked(selectedAccount.canManageEnrollment);
    access->setChecked(selectedAccount.canManageAccess);
    accounts->setChecked(selectedAccount.canManageAccounts);
    security->setChecked(selectedAccount.canManageSecurity);
    form->addRow(tr("Capabilities"), operate);
    form->addRow(QString(), enrollment);
    form->addRow(QString(), access);
    form->addRow(QString(), accounts);
    form->addRow(QString(), security);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Review Changes"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const bool nextOperate = operate->isChecked();
    const bool nextEnrollment = enrollment->isChecked();
    const bool nextAccess = access->isChecked();
    const bool nextAccounts = accounts->isChecked();
    const bool nextSecurity = security->isChecked();
    if (nextOperate == selectedAccount.canOperate && nextEnrollment == selectedAccount.canManageEnrollment &&
        nextAccess == selectedAccount.canManageAccess && nextAccounts == selectedAccount.canManageAccounts &&
        nextSecurity == selectedAccount.canManageSecurity) {
        statusBar()->showMessage(tr("No account capability changes were selected."), 4000);
        return;
    }
    const QString accountName = QString::fromStdString(selectedAccount.username);
    const QString serverName = QString::fromStdString(profile->displayName);
    if (QMessageBox::question(
            this, tr("Confirm Account Capabilities"),
            tr("Replace the capabilities for %1 on %2? Removing a capability takes effect on the next server request.")
                .arg(accountName, serverName),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    const auto* currentSession = sessions_.session(deployment);
    if (!currentSession || !sessions_.isCurrent(*stamp) || currentSession->profileId != profileId ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("The selected server session expired before the capability update."), 6000);
        return;
    }
    const auto endpoint = apiEndpoint(
        *profile, QStringLiteral("/v2/accounts/%1/capabilities").arg(QString::fromStdString(accountId)));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }
    const QByteArray body = QJsonDocument(QJsonObject{
        {QStringLiteral("expected_revision"), static_cast<qint64>(selectedAccount.revision)},
        {QStringLiteral("can_operate"), nextOperate},
        {QStringLiteral("can_manage_enrollment"), nextEnrollment},
        {QStringLiteral("can_manage_access"), nextAccess},
        {QStringLiteral("can_manage_accounts"), nextAccounts},
        {QStringLiteral("can_manage_security"), nextSecurity},
    }).toJson(QJsonDocument::Compact);
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
    QNetworkReply* reply = network_.put(request, body);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, accountId,
             expectedRevision = selectedAccount.revision, nextOperate, nextEnrollment,
             nextAccess, nextAccounts, nextSecurity, accountName, serverName] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            response.size() > maxHealthResponseBytes) {
            const QString detail = statusCode == 403
                ? tr("The server requires account-management permission and fresh MFA; security-role changes also require a security administrator.")
                : replyFailure(reply);
            statusBar()->showMessage(tr("Capability update failed on %1: %2").arg(serverName, detail), 10000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        const QJsonObject object = document.object();
        const QJsonObject capabilities = object.value(QStringLiteral("capabilities")).toObject();
        const QJsonValue revision = object.value(QStringLiteral("revision"));
        const double revisionNumber = revision.toDouble(0);
        const bool valid = parseError.error == QJsonParseError::NoError && document.isObject() &&
            object.value(QStringLiteral("id")).toString().toStdString() == accountId &&
            object.value(QStringLiteral("state")).toString() == QStringLiteral("active") &&
            revision.isDouble() && std::isfinite(revisionNumber) &&
            revisionNumber > static_cast<double>(expectedRevision) &&
            std::floor(revisionNumber) == revisionNumber &&
            capabilities.value(QStringLiteral("operate")).isBool() &&
            capabilities.value(QStringLiteral("operate")).toBool() == nextOperate &&
            capabilities.value(QStringLiteral("manage_enrollment")).isBool() &&
            capabilities.value(QStringLiteral("manage_enrollment")).toBool() == nextEnrollment &&
            capabilities.value(QStringLiteral("manage_access")).isBool() &&
            capabilities.value(QStringLiteral("manage_access")).toBool() == nextAccess &&
            capabilities.value(QStringLiteral("manage_accounts")).isBool() &&
            capabilities.value(QStringLiteral("manage_accounts")).toBool() == nextAccounts &&
            capabilities.value(QStringLiteral("manage_security")).isBool() &&
            capabilities.value(QStringLiteral("manage_security")).toBool() == nextSecurity;
        if (!valid) {
            statusBar()->showMessage(tr("The server returned an invalid capability update."), 8000);
            reply->deleteLater();
            return;
        }
        loadAuthorizedAccountData(profileId, deployment, stamp);
        statusBar()->showMessage(tr("Updated capabilities for %1 on %2.").arg(accountName, serverName), 7000);
        reply->deleteLater();
    });
}

void MainWindow::updateSelectedAccountState()
{
    if (accountsServerFilter_ == nullptr || accountTable_ == nullptr) {
        return;
    }
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    const int row = accountTable_->currentRow();
    if (selectedProfile.isEmpty() || row < 0) {
        statusBar()->showMessage(tr("Choose one server and select an active or disabled account first."), 6000);
        return;
    }
    const auto* serverItem = accountTable_->item(row, 0);
    const auto* idItem = accountTable_->item(row, 2);
    const auto* stateItem = accountTable_->item(row, 3);
    const auto* revisionItem = accountTable_->item(row, 5);
    if (serverItem == nullptr || idItem == nullptr || stateItem == nullptr || revisionItem == nullptr ||
        serverItem->data(Qt::UserRole).toString() != selectedProfile ||
        (stateItem->text() != QStringLiteral("active") && stateItem->text() != QStringLiteral("disabled"))) {
        statusBar()->showMessage(tr("Select an active or disabled account owned by the chosen server."), 6000);
        return;
    }
    bool revisionOk = false;
    const qlonglong expectedRevision = revisionItem->text().toLongLong(&revisionOk);
    const std::string profileId = selectedProfile.toStdString();
    const std::string accountId = idItem->text().toStdString();
    const auto accountIt = std::find_if(accounts_.begin(), accounts_.end(), [&](const AccountRow& candidate) {
        return candidate.profileId == profileId && candidate.id.serverEntityId == accountId;
    });
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (!revisionOk || expectedRevision < 1 || accountIt == accounts_.end() ||
        deploymentEntry == deploymentsByProfile_.end() || !profile ||
        !validRouteSegment(QString::fromStdString(accountId))) {
        statusBar()->showMessage(tr("The selected account has no valid revision or owning server identity."), 6000);
        return;
    }
    const AccountRow selectedAccount = *accountIt;
    if (selectedAccount.recoveryQuarantined) {
        statusBar()->showMessage(
            tr("This restored account remains quarantined and cannot be reactivated."),
            8000);
        return;
    }
    if (selectedAccount.state != stateItem->text().toStdString() ||
        selectedAccount.revision != static_cast<std::int64_t>(expectedRevision)) {
        statusBar()->showMessage(tr("The account table changed while it was selected. Refresh and retry."), 6000);
        return;
    }
    const bool enabling = selectedAccount.state == "disabled";
    const QString nextState = enabling ? QStringLiteral("active") : QStringLiteral("disabled");
    const QString accountName = QString::fromStdString(selectedAccount.username);
    const QString serverName = QString::fromStdString(profile->displayName);
    const DeploymentId deployment = deploymentEntry->second;
    const auto stamp = sessions_.currentStamp(deployment);
    const auto* session = sessions_.session(deployment);
    if (!stamp || !session || !sessions_.isCurrent(*stamp) || session->profileId != profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the selected server before changing account state."), 6000);
        return;
    }

    const QString effect = enabling
        ? tr("The account will be able to sign in again. A disabled security administrator needs active MFA.")
        : tr("All active sessions for this account will be revoked. The last active security administrator cannot be disabled.");
    if (QMessageBox::question(
            this, enabling ? tr("Enable Account") : tr("Disable Account"),
            tr("Set %1 on %2 to %3?\n\n%4")
                .arg(accountName, serverName, nextState, effect),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }

    const auto* currentSession = sessions_.session(deployment);
    if (!currentSession || !sessions_.isCurrent(*stamp) || currentSession->profileId != profileId ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("The selected server session expired before the account-state update."), 6000);
        return;
    }
    const std::string actorAccountId = currentSession->accountId;
    const auto endpoint = apiEndpoint(
        *profile, QStringLiteral("/v2/accounts/%1/state").arg(QString::fromStdString(accountId)));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }
    const QByteArray body = QJsonDocument(QJsonObject{
        {QStringLiteral("expected_revision"), static_cast<qint64>(selectedAccount.revision)},
        {QStringLiteral("state"), nextState},
    }).toJson(QJsonDocument::Compact);
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
    QNetworkReply* reply = network_.put(request, body);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, accountId, actorAccountId,
             expectedRevision = selectedAccount.revision, nextState, accountName, serverName] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            response.size() > maxHealthResponseBytes) {
            const QString detail = statusCode == 403
                ? tr("The server requires account-management permission and fresh MFA; security administrators have extra protections.")
                : statusCode == 409
                    ? tr("The account changed or cannot enter that state. Refresh the account list and retry.")
                    : replyFailure(reply);
            statusBar()->showMessage(tr("Account-state update failed on %1: %2").arg(serverName, detail), 10000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        const QJsonObject object = document.object();
        const QJsonValue revision = object.value(QStringLiteral("revision"));
        const double revisionNumber = revision.toDouble(0);
        const bool valid = parseError.error == QJsonParseError::NoError && document.isObject() &&
            object.value(QStringLiteral("id")).toString().toStdString() == accountId &&
            object.value(QStringLiteral("state")).toString() == nextState &&
            revision.isDouble() && std::isfinite(revisionNumber) &&
            revisionNumber >= static_cast<double>(expectedRevision) &&
            std::floor(revisionNumber) == revisionNumber;
        if (!valid) {
            statusBar()->showMessage(tr("The server returned an invalid account-state update."), 8000);
            reply->deleteLater();
            return;
        }
        if (nextState == QStringLiteral("disabled") && actorAccountId == accountId) {
            registry_.disconnect(profileId);
            invalidateDeploymentSession(deployment);
            computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
                [&](const ComputerRow& computer) { return computer.id.deployment == deployment; }), computers_.end());
            clearDeploymentAccountData(deployment);
            clearDeploymentAuthentication(deployment, profileId);
            refreshServerTable();
            refreshComputerTable();
            statusBar()->showMessage(tr("Disabled %1 and signed out its Manager session on %2.")
                                         .arg(accountName, serverName), 8000);
        } else {
            loadAuthorizedAccountData(profileId, deployment, stamp);
            statusBar()->showMessage(tr("Set %1 on %2 to %3.")
                                         .arg(accountName, serverName, nextState), 7000);
        }
        reply->deleteLater();
    });
}

void MainWindow::refreshAuditEvents()
{
    if (auditLoading_) return;
    auditNextCursorId_.clear();
    auditNextCursorUnix_ = 0;
    if (auditTable_ != nullptr) auditTable_->setRowCount(0);
    if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(false);
    requestAuditEvents(false);
}

void MainWindow::loadOlderAuditEvents()
{
    requestAuditEvents(true);
}

void MainWindow::requestAuditEvents(bool olderPage)
{
    if (accountsServerFilter_ == nullptr || auditTable_ == nullptr || auditStatus_ == nullptr) {
        return;
    }
    if (auditLoading_) return;
    if (olderPage && auditNextCursorId_.isEmpty()) return;
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    if (selectedProfile.isEmpty()) {
        auditStatus_->setText(tr("Choose one server in the Accounts server filter before loading audit metadata."));
        statusBar()->showMessage(tr("Choose one server to view its security audit events."), 6000);
        return;
    }
    const std::string profileId = selectedProfile.toStdString();
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (!profile || deploymentEntry == deploymentsByProfile_.end()) {
        auditStatus_->setText(tr("The selected server profile is unavailable."));
        return;
    }
    const DeploymentId deployment = deploymentEntry->second;
    const auto stamp = sessions_.currentStamp(deployment);
    const auto* session = sessions_.session(deployment);
    if (!stamp || !session || !sessions_.isCurrent(*stamp) || session->profileId != profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        auditStatus_->setText(tr("Sign in to the selected server with an approved Manager device before loading audit metadata."));
        return;
    }
    auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/audit/events"));
    if (!endpoint) {
        auditStatus_->setText(tr("The selected server address is invalid."));
        return;
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));
    if (olderPage) {
        query.addQueryItem(QStringLiteral("before_unix"), QString::number(auditNextCursorUnix_));
        query.addQueryItem(QStringLiteral("before_id"), auditNextCursorId_);
    }
    endpoint->setQuery(query);

    QNetworkRequest request = apiRequest(*endpoint);
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(session->bearerToken));
    QNetworkReply* reply = network_.get(request);
    reply->setReadBufferSize(maxAuditResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxAuditResponseBytes) reply->abort();
            });
    auditLoading_ = true;
    if (auditRequestGeneration_ < std::numeric_limits<std::uint64_t>::max()) {
        ++auditRequestGeneration_;
    }
    const std::uint64_t auditRequestGeneration = auditRequestGeneration_;
    if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(false);
    auditStatus_->setText(olderPage
        ? tr("Loading the next retained audit page from %1…").arg(QString::fromStdString(profile->displayName))
        : tr("Loading the latest 50 retained audit events from %1…").arg(QString::fromStdString(profile->displayName)));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, olderPage, stamp = *stamp,
             auditRequestGeneration, serverName = QString::fromStdString(profile->displayName)] {
        if (auditRequestGeneration != auditRequestGeneration_) {
            reply->deleteLater();
            return;
        }
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();
        if (!sessions_.isCurrent(stamp) || accountsServerFilter_ == nullptr ||
            accountsServerFilter_->currentData().toString() != QString::fromStdString(profileId)) {
            auditLoading_ = false;
            if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(!auditNextCursorId_.isEmpty());
            reply->deleteLater();
            return;
        }
        auditLoading_ = false;
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            response.size() > maxAuditResponseBytes) {
            const QString detail = statusCode == 403
                ? tr("The server requires a current security-administrator role and fresh MFA.")
                : replyFailure(reply);
            auditStatus_->setText(tr("Could not load audit metadata from %1: %2").arg(serverName, detail));
            if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(!auditNextCursorId_.isEmpty());
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        const QJsonObject root = document.object();
        const QJsonValue eventsValue = root.value(QStringLiteral("events"));
        const QJsonValue cursorValue = root.value(QStringLiteral("next_cursor"));
        const QJsonArray events = eventsValue.toArray();
        qint64 nextCursorUnix = 0;
        QString nextCursorId;
        bool cursorValid = cursorValue.isNull();
        if (cursorValue.isObject()) {
            const QJsonObject cursor = cursorValue.toObject();
            const QJsonValue timeValue = cursor.value(QStringLiteral("before_unix"));
            const double rawTime = timeValue.toDouble(-1);
            nextCursorId = cursor.value(QStringLiteral("before_id")).toString();
            cursorValid = timeValue.isDouble() && std::isfinite(rawTime) && rawTime >= 0 &&
                std::floor(rawTime) == rawTime && rawTime <= static_cast<double>(std::numeric_limits<qint64>::max()) &&
                validRouteSegment(nextCursorId);
            if (cursorValid) nextCursorUnix = static_cast<qint64>(rawTime);
        }
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !eventsValue.isArray() || events.size() > 50 || !cursorValid || cursorValue.isUndefined()) {
            auditStatus_->setText(tr("The server returned an invalid audit response."));
            if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(!auditNextCursorId_.isEmpty());
            reply->deleteLater();
            return;
        }

        auto validAuditCode = [](const QString& value) {
            if (value.isEmpty() || value.size() > 64) return false;
            for (const QChar character : value) {
                const ushort code = character.unicode();
                if (!((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z') ||
                      (code >= '0' && code <= '9') || code == '.' || code == '_' || code == '-')) {
                    return false;
                }
            }
            return true;
        };
        QVector<QStringList> rows;
        rows.reserve(events.size());
        for (const QJsonValue& value : events) {
            if (!value.isObject()) {
                auditStatus_->setText(tr("The server returned a malformed audit event."));
                if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(!auditNextCursorId_.isEmpty());
                reply->deleteLater();
                return;
            }
            const QJsonObject event = value.toObject();
            const QJsonObject actor = event.value(QStringLiteral("actor")).toObject();
            const QString id = event.value(QStringLiteral("id")).toString();
            const QString accountId = actor.value(QStringLiteral("account_id")).toString();
            const QJsonValue managerValue = actor.value(QStringLiteral("manager_id"));
            const QString managerId = managerValue.isNull() ? QString() : managerValue.toString();
            const QString action = event.value(QStringLiteral("action")).toString();
            const QJsonValue targetsValue = event.value(QStringLiteral("target_ids"));
            const QJsonArray targets = targetsValue.toArray();
            const QJsonValue rawTime = event.value(QStringLiteral("time_unix"));
            const double timeValue = rawTime.toDouble(-1);
            const QString result = event.value(QStringLiteral("result")).toString();
            const QString reason = event.value(QStringLiteral("reason_code")).toString();
            if (!validRouteSegment(id) || !validRouteSegment(accountId) ||
                (!managerValue.isNull() && !validRouteSegment(managerId)) ||
                !validAuditCode(action) || !targetsValue.isArray() || targets.isEmpty() ||
                targets.size() > 102 || !rawTime.isDouble() || !std::isfinite(timeValue) ||
                timeValue < 0 || std::floor(timeValue) != timeValue ||
                (result != QStringLiteral("succeeded") && result != QStringLiteral("denied") &&
                 result != QStringLiteral("failed")) || !validAuditCode(reason)) {
                auditStatus_->setText(tr("The server returned an invalid audit event."));
                if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(!auditNextCursorId_.isEmpty());
                reply->deleteLater();
                return;
            }
            QStringList targetIds;
            for (const QJsonValue& target : targets) {
                const QString targetId = target.toString();
                if (!target.isString() || !validRouteSegment(targetId)) {
                    auditStatus_->setText(tr("The server returned an invalid audit target."));
                    if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(!auditNextCursorId_.isEmpty());
                    reply->deleteLater();
                    return;
                }
                targetIds.append(targetId);
            }
            const QDateTime time = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(timeValue), Qt::UTC);
            rows.append({time.toString(Qt::ISODate), accountId,
                         managerId.isEmpty() ? tr("Unavailable") : managerId,
                         action, targetIds.join(QStringLiteral(", ")), result, reason, id});
        }

        const int startRow = olderPage ? auditTable_->rowCount() : 0;
        auditTable_->setRowCount(startRow + rows.size());
        for (int row = 0; row < rows.size(); ++row) {
            for (int column = 0; column < rows.at(row).size(); ++column) {
                auditTable_->setItem(startRow + row, column, new QTableWidgetItem(rows.at(row).at(column)));
            }
        }
        auditNextCursorId_ = nextCursorId;
        auditNextCursorUnix_ = nextCursorUnix;
        if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(!auditNextCursorId_.isEmpty());
        auditStatus_->setText(tr("Loaded %1 metadata-only event(s) from %2.")
                                  .arg(rows.size())
                                  .arg(serverName));
        reply->deleteLater();
    });
}

void MainWindow::editAuditRetention()
{
    if (accountsServerFilter_ == nullptr || auditStatus_ == nullptr) return;
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    if (selectedProfile.isEmpty()) {
        statusBar()->showMessage(tr("Choose one server in the Accounts server filter first."), 6000);
        return;
    }
    const std::string profileId = selectedProfile.toStdString();
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (!profile || deploymentEntry == deploymentsByProfile_.end()) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 5000);
        return;
    }
    const DeploymentId deployment = deploymentEntry->second;
    const auto stamp = sessions_.currentStamp(deployment);
    const auto* session = sessions_.session(deployment);
    if (!stamp || !session || !sessions_.isCurrent(*stamp) || session->profileId != profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the selected server before changing audit retention."), 6000);
        return;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/security/audit-retention"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }
    QNetworkRequest getRequest = apiRequest(*endpoint);
    getRequest.setRawHeader("Authorization",
                            QByteArray("Bearer ") + QByteArray::fromStdString(session->bearerToken));
    QNetworkReply* getReply = network_.get(getRequest);
    getReply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, getReply, [getReply] { getReply->abort(); });
    connect(getReply, &QNetworkReply::downloadProgress, this,
            [getReply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) getReply->abort();
            });
    const QString serverName = QString::fromStdString(profile->displayName);
    connect(getReply, &QNetworkReply::finished, this,
            [this, getReply, profileId, deployment, stamp = *stamp, endpoint = *endpoint, serverName] {
        const int statusCode = getReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = getReply->readAll();
        if (!sessions_.isCurrent(stamp)) {
            getReply->deleteLater();
            return;
        }
        if (getReply->error() != QNetworkReply::NoError || statusCode != 200 ||
            response.size() > maxHealthResponseBytes) {
            const QString detail = statusCode == 403
                ? tr("The server requires a current security-administrator role and fresh MFA.")
                : replyFailure(getReply);
            statusBar()->showMessage(tr("Could not read audit retention on %1: %2").arg(serverName, detail), 9000);
            getReply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        const QJsonObject current = document.object();
        const QJsonValue daysValue = current.value(QStringLiteral("retention_days"));
        const QJsonValue updatedValue = current.value(QStringLiteral("updated_at_unix"));
        const double daysNumber = daysValue.toDouble(-1);
        const double updatedNumber = updatedValue.toDouble(-1);
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !daysValue.isDouble() || daysNumber < 1 || daysNumber > 3650 ||
            std::floor(daysNumber) != daysNumber || !updatedValue.isDouble() ||
            !std::isfinite(updatedNumber) || updatedNumber < 0 || std::floor(updatedNumber) != updatedNumber) {
            statusBar()->showMessage(tr("The server returned an invalid audit-retention policy."), 8000);
            getReply->deleteLater();
            return;
        }
        bool accepted = false;
        const int nextDays = QInputDialog::getInt(
            this, tr("Audit Retention"),
            tr("Keep security-action metadata on %1 for how many days? Choose from 1 to 3650. Older events are removed when this policy is saved.")
                .arg(serverName),
            static_cast<int>(daysNumber), 1, 3650, 1, &accepted);
        if (!accepted) {
            getReply->deleteLater();
            return;
        }
        if (nextDays == static_cast<int>(daysNumber)) {
            statusBar()->showMessage(tr("Audit retention on %1 remains %2 days.").arg(serverName).arg(nextDays), 6000);
            getReply->deleteLater();
            return;
        }
        const auto* currentSession = sessions_.session(deployment);
        if (!currentSession || !sessions_.isCurrent(stamp) || currentSession->profileId != profileId ||
            currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            statusBar()->showMessage(tr("The selected server session expired before the retention change."), 6000);
            getReply->deleteLater();
            return;
        }
        if (QMessageBox::question(
                this, tr("Confirm Audit Retention"),
                tr("Keep audit metadata on %1 for %2 days? Events older than the new limit will be removed when the policy is saved.")
                    .arg(serverName).arg(nextDays),
                QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Save) {
            getReply->deleteLater();
            return;
        }
        const QByteArray body = QJsonDocument(QJsonObject{
            {QStringLiteral("retention_days"), nextDays},
        }).toJson(QJsonDocument::Compact);
        QNetworkRequest putRequest = apiRequest(endpoint);
        putRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        putRequest.setRawHeader("Authorization",
                                QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
        QNetworkReply* putReply = network_.put(putRequest, body);
        putReply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, putReply, [putReply] { putReply->abort(); });
        connect(putReply, &QNetworkReply::downloadProgress, this,
                [putReply](qint64 received, qint64) {
                    if (received > maxHealthResponseBytes) putReply->abort();
                });
        connect(putReply, &QNetworkReply::finished, this,
                [this, putReply, profileId, deployment, stamp, nextDays, serverName] {
            const int putStatus = putReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray putResponse = putReply->readAll();
            if (!sessions_.isCurrent(stamp)) {
                putReply->deleteLater();
                return;
            }
            if (putReply->error() != QNetworkReply::NoError || putStatus != 200 ||
                putResponse.size() > maxHealthResponseBytes) {
                const QString detail = putStatus == 403
                    ? tr("The server requires a current security-administrator role and fresh MFA. Sign in again and retry.")
                    : replyFailure(putReply);
                statusBar()->showMessage(tr("Audit retention update failed on %1: %2").arg(serverName, detail), 10000);
                putReply->deleteLater();
                return;
            }
            QJsonParseError putParseError;
            const QJsonDocument putDocument = QJsonDocument::fromJson(putResponse, &putParseError);
            const QJsonValue returnedDays = putDocument.object().value(QStringLiteral("retention_days"));
            if (putParseError.error != QJsonParseError::NoError || !putDocument.isObject() ||
                !returnedDays.isDouble() || returnedDays.toInt(-1) != nextDays) {
                statusBar()->showMessage(tr("The server returned an invalid retention-update result."), 8000);
                putReply->deleteLater();
                return;
            }
            statusBar()->showMessage(tr("Audit metadata retention on %1 is now %2 days.").arg(serverName).arg(nextDays), 8000);
            refreshAuditEvents();
            putReply->deleteLater();
        });
        getReply->deleteLater();
    });
}

void MainWindow::editOperatorMfaPolicy()
{
    if (accountsServerFilter_ == nullptr) {
        return;
    }
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    if (selectedProfile.isEmpty()) {
        statusBar()->showMessage(tr("Choose one owning server in the Accounts server filter first."), 6000);
        return;
    }
    const std::string profileId = selectedProfile.toStdString();
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (deploymentEntry == deploymentsByProfile_.end() || !profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 5000);
        return;
    }
    const DeploymentId deployment = deploymentEntry->second;
    const auto stamp = sessions_.currentStamp(deployment);
    const auto* session = sessions_.session(deployment);
    if (!stamp || !session || !sessions_.isCurrent(*stamp) || session->profileId != profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the selected server before changing MFA policy."), 6000);
        return;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/security/operator-mfa-policy"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }
    const QString serverName = QString::fromStdString(profile->displayName);
    const auto* currentSession = sessions_.session(deployment);
    if (!currentSession || !sessions_.isCurrent(*stamp) || currentSession->profileId != profileId ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("The selected server session expired before policy review."), 6000);
        return;
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
    QNetworkReply* reply = network_.get(request);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, endpoint = *endpoint, serverName] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            response.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(tr("Could not read MFA policy on %1: %2")
                                         .arg(serverName, replyFailure(reply)), 9000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        const QString currentPolicy = document.object().value(QStringLiteral("policy")).toString();
        const QJsonValue mfaRequired = document.object().value(QStringLiteral("mfa_required_for_operators"));
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            (currentPolicy != QStringLiteral("optional") && currentPolicy != QStringLiteral("required")) ||
            !mfaRequired.isBool() || mfaRequired.toBool() != (currentPolicy == QStringLiteral("required"))) {
            statusBar()->showMessage(tr("The server returned an invalid operator MFA policy."), 8000);
            reply->deleteLater();
            return;
        }
        const QStringList choices{tr("Optional"), tr("Required")};
        const int currentIndex = currentPolicy == QStringLiteral("required") ? 1 : 0;
        bool accepted = false;
        const QString choice = QInputDialog::getItem(
            this, tr("Operator MFA Policy"),
            tr("Choose whether non-administrator accounts must enroll and use MFA to sign in to %1.")
                .arg(serverName),
            choices, currentIndex, false, &accepted);
        if (!accepted) {
            reply->deleteLater();
            return;
        }
        const QString nextPolicy = choice == choices.at(1)
            ? QStringLiteral("required")
            : QStringLiteral("optional");
        if (nextPolicy == currentPolicy) {
            statusBar()->showMessage(tr("Operator MFA policy on %1 is unchanged.").arg(serverName), 5000);
            reply->deleteLater();
            return;
        }
        const QString impact = nextPolicy == QStringLiteral("required")
            ? tr("All operator sessions without active MFA will end, and operators must set up MFA before their next sign-in.")
            : tr("New operator accounts may sign in without MFA unless they already have an active factor.");
        if (QMessageBox::question(
                this, tr("Confirm Operator MFA Policy"),
                tr("Set the policy on %1 to %2?\n\n%3")
                    .arg(serverName, choice.toLower(), impact),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
            reply->deleteLater();
            return;
        }
        const auto* currentSession = sessions_.session(deployment);
        if (!currentSession || !sessions_.isCurrent(stamp) || currentSession->profileId != profileId ||
            currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            statusBar()->showMessage(tr("The selected server session expired before policy update."), 6000);
            reply->deleteLater();
            return;
        }
        QNetworkRequest updateRequest = apiRequest(endpoint);
        updateRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        updateRequest.setRawHeader("Authorization",
                                   QByteArray("Bearer ") + QByteArray::fromStdString(currentSession->bearerToken));
        const QByteArray body = QJsonDocument(QJsonObject{
            {QStringLiteral("policy"), nextPolicy},
        }).toJson(QJsonDocument::Compact);
        QNetworkReply* updateReply = network_.put(updateRequest, body);
        updateReply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, updateReply, [updateReply] { updateReply->abort(); });
        connect(updateReply, &QNetworkReply::downloadProgress, this,
                [updateReply](qint64 received, qint64) {
                    if (received > maxHealthResponseBytes) updateReply->abort();
                });
        connect(updateReply, &QNetworkReply::finished, this,
                [this, updateReply, profileId, deployment, stamp, serverName, nextPolicy] {
            const int updateStatus = updateReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray updateResponse = updateReply->readAll();
            if (!sessions_.isCurrent(stamp)) {
                updateReply->deleteLater();
                return;
            }
            if (updateReply->error() != QNetworkReply::NoError || updateStatus != 200 ||
                updateResponse.size() > maxHealthResponseBytes) {
                const QString detail = updateStatus == 403
                    ? tr("Only a security administrator with recent MFA can change this setting.")
                    : replyFailure(updateReply);
                statusBar()->showMessage(tr("MFA policy update failed on %1: %2")
                                             .arg(serverName, detail), 10000);
                updateReply->deleteLater();
                return;
            }
            QJsonParseError updateParseError;
            const QJsonDocument updateDocument = QJsonDocument::fromJson(updateResponse, &updateParseError);
            const QJsonObject result = updateDocument.object();
            if (updateParseError.error != QJsonParseError::NoError || !updateDocument.isObject() ||
                result.value(QStringLiteral("policy")).toString() != nextPolicy ||
                !result.value(QStringLiteral("mfa_required_for_operators")).isBool() ||
                result.value(QStringLiteral("mfa_required_for_operators")).toBool() !=
                    (nextPolicy == QStringLiteral("required"))) {
                statusBar()->showMessage(tr("The server returned an invalid MFA policy update."), 8000);
                updateReply->deleteLater();
                return;
            }
            loadAuthorizedAccountData(profileId, deployment, stamp);
            statusBar()->showMessage(tr("Operator MFA is now %1 on %2.")
                                         .arg(nextPolicy, serverName), 8000);
            updateReply->deleteLater();
        });
        reply->deleteLater();
    });
}

void MainWindow::recoverAccountMfa()
{
    if (accountsServerFilter_ == nullptr) {
        return;
    }
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    if (selectedProfile.isEmpty()) {
        statusBar()->showMessage(tr("Choose the owning server in the Accounts filter first."), 6000);
        return;
    }
    const std::string profileId = selectedProfile.toStdString();
    checkPinnedDeploymentIdentity(profileId, [this, profileId] {
        beginAccountMfaRecovery(profileId);
    });
}

void MainWindow::beginAccountMfaRecovery(const std::string& profileId)
{
    const auto profile = registry_.profile(profileId);
    if (!profile) {
        statusBar()->showMessage(tr("The selected server profile is unavailable."), 5000);
        return;
    }
    if (!hasTrustedDeploymentAuthority(*profile)) {
        statusBar()->showMessage(tr("Verify this server's deployment identity and authority key through your organization's trusted channel before sending recovery credentials."), 9000);
        return;
    }
    const auto beginEndpoint = apiEndpoint(*profile, QStringLiteral("/v2/auth/recovery/begin"));
    if (!beginEndpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
        return;
    }

    QDialog credentialsDialog(this);
    credentialsDialog.setWindowTitle(tr("Recover Account MFA"));
    credentialsDialog.setMinimumWidth(500);
    auto* layout = new QVBoxLayout(&credentialsDialog);
    auto* form = new QFormLayout;
    auto* username = new QLineEdit(&credentialsDialog);
    username->setMaxLength(64);
    auto* password = new QLineEdit(&credentialsDialog);
    password->setEchoMode(QLineEdit::Password);
    password->setMaxLength(256);
    auto* recoveryCode = new QLineEdit(&credentialsDialog);
    recoveryCode->setEchoMode(QLineEdit::Password);
    recoveryCode->setMaxLength(64);
    form->addRow(tr("Account username"), username);
    form->addRow(tr("Account password"), password);
    form->addRow(tr("Unused recovery code"), recoveryCode);
    layout->addLayout(form);
    layout->addWidget(subheading(
        tr("Beginning recovery consumes this code immediately. Continue only if you can finish authenticator setup now. Your remaining recovery codes stay valid."),
        &credentialsDialog));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &credentialsDialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Begin Recovery"));
    connect(buttons, &QDialogButtonBox::accepted, &credentialsDialog, [&] {
        const QString name = username->text().trimmed();
        QByteArray passwordBytes = password->text().toUtf8();
        QString code = recoveryCode->text().trimmed();
        const bool valid = validWireText(name, 64) && !passwordBytes.isEmpty() && passwordBytes.size() <= 256 &&
            validWireText(code, 64);
        passwordBytes.fill('\0');
        code.fill(QChar(u'\0'));
        if (!valid) {
            QMessageBox::warning(&credentialsDialog, tr("Recovery Details Required"),
                                 tr("Enter the account username, password, and one unused recovery code."));
            return;
        }
        credentialsDialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &credentialsDialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (credentialsDialog.exec() != QDialog::Accepted) {
        username->clear();
        password->clear();
        recoveryCode->clear();
        return;
    }

    QString accountName = username->text().trimmed();
    QString accountPassword = password->text();
    QString recovery = recoveryCode->text().trimmed();
    username->clear();
    password->clear();
    recoveryCode->clear();
    QJsonObject beginObject{
        {QStringLiteral("username"), accountName},
        {QStringLiteral("password"), accountPassword},
        {QStringLiteral("recovery_code"), recovery},
    };
    SensitiveBuffer beginBody(QJsonDocument(beginObject).toJson(QJsonDocument::Compact));
    accountPassword.fill(QChar(u'\0'));
    recovery.fill(QChar(u'\0'));
    beginObject.insert(QStringLiteral("password"), QString());
    beginObject.insert(QStringLiteral("recovery_code"), QString());

    QNetworkRequest beginRequest = apiRequest(*beginEndpoint);
    beginRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply* beginReply = network_.post(beginRequest, beginBody.bytes());
    beginReply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, beginReply, [beginReply] { beginReply->abort(); });
    connect(beginReply, &QNetworkReply::downloadProgress, this,
            [beginReply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) beginReply->abort();
            });
    connect(beginReply, &QNetworkReply::finished, this,
            [this, beginReply, profileId, profile = *profile, accountName] {
        const int statusCode = beginReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        SensitiveBuffer beginResponse(beginReply->readAll());
        if (beginReply->error() != QNetworkReply::NoError || statusCode != 201 ||
            beginResponse.bytes().size() > maxHealthResponseBytes) {
            const QString detail = statusCode == 401
                ? tr("The username, password, or recovery code was not accepted.")
                : replyFailure(beginReply);
            statusBar()->showMessage(tr("MFA recovery could not start: %1").arg(detail), 10000);
            beginReply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(beginResponse.bytes(), &parseError);
        const QJsonObject result = document.object();
        QString accountId = result.value(QStringLiteral("account_id")).toString();
        QString enrollmentId = result.value(QStringLiteral("mfa_enrollment_id")).toString();
        QString totpSecret = result.value(QStringLiteral("totp_secret_base32")).toString();
        const QString purpose = result.value(QStringLiteral("purpose")).toString();
        const bool valid = parseError.error == QJsonParseError::NoError && document.isObject() &&
            validWireText(accountId) && validWireText(enrollmentId) &&
            validWireText(totpSecret, 128) && totpSecret.size() >= 16 &&
            purpose == QStringLiteral("totp_factor_recovery_only");
        if (!valid) {
            accountId.fill(QChar(u'\0'));
            enrollmentId.fill(QChar(u'\0'));
            totpSecret.fill(QChar(u'\0'));
            statusBar()->showMessage(tr("The server returned invalid MFA recovery material."), 8000);
            beginReply->deleteLater();
            return;
        }

        QDialog enrollmentDialog(this);
        enrollmentDialog.setWindowTitle(tr("Set Up the New Authenticator"));
        enrollmentDialog.setMinimumSize(580, 440);
        auto* enrollmentLayout = new QVBoxLayout(&enrollmentDialog);
        enrollmentLayout->addWidget(subheading(
            tr("Configure this TOTP secret now. Your existing unused recovery codes remain valid; store the secret safely before confirming."),
            &enrollmentDialog));
        auto* secret = new QPlainTextEdit(&enrollmentDialog);
        secret->setReadOnly(true);
        secret->setMaximumHeight(75);
        secret->setPlainText(totpSecret);
        enrollmentLayout->addWidget(secret);
        auto* confirmationForm = new QFormLayout;
        auto* totpCode = new QLineEdit(&enrollmentDialog);
        totpCode->setMaxLength(6);
        totpCode->setEchoMode(QLineEdit::Password);
        auto* recoveryStored = new QCheckBox(
            tr("I stored the new authenticator setup and still have my remaining recovery codes."),
            &enrollmentDialog);
        confirmationForm->addRow(tr("Six-digit authenticator code"), totpCode);
        confirmationForm->addRow(QString(), recoveryStored);
        enrollmentLayout->addLayout(confirmationForm);
        enrollmentLayout->addWidget(subheading(
            tr("The recovery code was already consumed. If you cancel now, this enrollment expires after 15 minutes."),
            &enrollmentDialog));
        auto* enrollmentButtons = new QDialogButtonBox(
            QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &enrollmentDialog);
        enrollmentButtons->button(QDialogButtonBox::Ok)->setText(tr("Confirm New Authenticator"));
        connect(enrollmentButtons, &QDialogButtonBox::accepted, &enrollmentDialog, [&] {
            const QString code = totpCode->text().trimmed();
            const bool validCode = code.size() == 6 && std::all_of(code.cbegin(), code.cend(), [](QChar c) {
                return c.unicode() >= '0' && c.unicode() <= '9';
            });
            if (!validCode || !recoveryStored->isChecked()) {
                QMessageBox::warning(&enrollmentDialog, tr("Authenticator Confirmation Required"),
                                     tr("Enter a six-digit authenticator code and confirm you stored the setup."));
                return;
            }
            enrollmentDialog.accept();
        });
        connect(enrollmentButtons, &QDialogButtonBox::rejected, &enrollmentDialog, &QDialog::reject);
        enrollmentLayout->addWidget(enrollmentButtons);
        if (enrollmentDialog.exec() != QDialog::Accepted) {
            secret->clear();
            accountId.fill(QChar(u'\0'));
            enrollmentId.fill(QChar(u'\0'));
            totpSecret.fill(QChar(u'\0'));
            statusBar()->showMessage(tr("Recovery code consumed. The unfinished enrollment expires in 15 minutes."), 8000);
            beginReply->deleteLater();
            return;
        }

        QString confirmationCode = totpCode->text().trimmed();
        totpCode->clear();
        QJsonObject completeObject{
            {QStringLiteral("mfa_enrollment_id"), enrollmentId},
            {QStringLiteral("totp_code"), confirmationCode},
            {QStringLiteral("recovery_material_stored"), true},
        };
        SensitiveBuffer completeBody(QJsonDocument(completeObject).toJson(QJsonDocument::Compact));
        confirmationCode.fill(QChar(u'\0'));
        completeObject.insert(QStringLiteral("mfa_enrollment_id"), QString());
        completeObject.insert(QStringLiteral("totp_code"), QString());
        secret->clear();
        totpSecret.fill(QChar(u'\0'));

        const auto completeEndpoint = apiEndpoint(profile, QStringLiteral("/v2/auth/recovery/complete"));
        if (!completeEndpoint) {
            accountId.fill(QChar(u'\0'));
            enrollmentId.fill(QChar(u'\0'));
            statusBar()->showMessage(tr("The selected server address is invalid."), 5000);
            beginReply->deleteLater();
            return;
        }
        QNetworkRequest completeRequest = apiRequest(*completeEndpoint);
        completeRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        QNetworkReply* completeReply = network_.post(completeRequest, completeBody.bytes());
        completeReply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, completeReply, [completeReply] { completeReply->abort(); });
        connect(completeReply, &QNetworkReply::downloadProgress, this,
                [completeReply](qint64 received, qint64) {
                    if (received > maxHealthResponseBytes) completeReply->abort();
                });
        connect(completeReply, &QNetworkReply::finished, this,
                [this, completeReply, profileId, accountId, enrollmentId, accountName]() mutable {
            const int completeStatus = completeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            SensitiveBuffer completeResponse(completeReply->readAll());
            if (completeReply->error() != QNetworkReply::NoError || completeStatus != 200 ||
                completeResponse.bytes().size() > maxHealthResponseBytes) {
                const QString detail = completeStatus == 403
                    ? tr("The authenticator code was not accepted. The recovery code is already consumed; use another unused code to begin again.")
                    : replyFailure(completeReply);
                statusBar()->showMessage(tr("MFA recovery confirmation failed: %1").arg(detail), 11000);
                accountId.fill(QChar(u'\0'));
                enrollmentId.fill(QChar(u'\0'));
                completeReply->deleteLater();
                return;
            }
            QJsonParseError completeParseError;
            const QJsonDocument completeDocument = QJsonDocument::fromJson(completeResponse.bytes(), &completeParseError);
            const QJsonObject completed = completeDocument.object();
            if (completeParseError.error != QJsonParseError::NoError || !completeDocument.isObject() ||
                completed.value(QStringLiteral("account_id")).toString() != accountId ||
                completed.value(QStringLiteral("mfa_status")).toString() != QStringLiteral("active") ||
                completed.value(QStringLiteral("account_state")).toString() != QStringLiteral("active")) {
                statusBar()->showMessage(tr("The server returned an invalid MFA recovery result."), 8000);
                accountId.fill(QChar(u'\0'));
                enrollmentId.fill(QChar(u'\0'));
                completeReply->deleteLater();
                return;
            }
            const auto deploymentEntry = deploymentsByProfile_.find(profileId);
            if (deploymentEntry != deploymentsByProfile_.end()) {
                const DeploymentId deployment = deploymentEntry->second;
                const auto* activeSession = sessions_.session(deployment);
                if (activeSession != nullptr && activeSession->accountId == accountId) {
                    const std::string sessionProfileId = activeSession->profileId;
                    registry_.disconnect(sessionProfileId);
                    invalidateDeploymentSession(deployment);
                    computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
                        [&](const ComputerRow& computer) { return computer.id.deployment == deployment; }), computers_.end());
                    clearDeploymentAccountData(deployment);
                    clearDeploymentAuthentication(deployment, sessionProfileId);
                    refreshServerTable();
                    refreshComputerTable();
                } else if (activeSession != nullptr) {
                    const std::string sessionProfileId = activeSession->profileId;
                    const auto stamp = sessions_.currentStamp(deployment);
                    if (stamp && sessions_.isCurrent(*stamp)) {
                        loadAuthorizedAccountData(sessionProfileId, deployment, *stamp);
                    }
                }
            }
            accountId.fill(QChar(u'\0'));
            enrollmentId.fill(QChar(u'\0'));
            statusBar()->showMessage(tr("MFA recovery completed for %1. Sign in with the new authenticator.")
                                         .arg(accountName), 9000);
            completeReply->deleteLater();
        });
        beginReply->deleteLater();
        accountId.fill(QChar(u'\0'));
        enrollmentId.fill(QChar(u'\0'));
    });
}

void MainWindow::confirmSelectedAccountMfa()
{
    if (accountsServerFilter_ == nullptr || accountTable_ == nullptr) {
        return;
    }
    const QString selectedProfile = accountsServerFilter_->currentData().toString();
    const int row = accountTable_->currentRow();
    if (selectedProfile.isEmpty() || row < 0) {
        statusBar()->showMessage(tr("Choose one server and select its pending account first."), 6000);
        return;
    }
    const auto* serverItem = accountTable_->item(row, 0);
    const auto* idItem = accountTable_->item(row, 2);
    const auto* stateItem = accountTable_->item(row, 3);
    if (serverItem == nullptr || idItem == nullptr || stateItem == nullptr ||
        serverItem->data(Qt::UserRole).toString() != selectedProfile ||
        stateItem->text() != QStringLiteral("pending_mfa")) {
        statusBar()->showMessage(tr("Select a pending MFA account owned by the selected server."), 6000);
        return;
    }
    const std::string profileId = selectedProfile.toStdString();
    const std::string accountId = idItem->text().toStdString();
    const auto deploymentEntry = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (!profile || deploymentEntry == deploymentsByProfile_.end() ||
        !validRouteSegment(QString::fromStdString(accountId))) {
        statusBar()->showMessage(tr("The selected account has an invalid server identity."), 6000);
        return;
    }
    const DeploymentId ownerDeployment = deploymentEntry->second;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Confirm Account MFA"));
    dialog.setMinimumWidth(540);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* enrollmentId = new QLineEdit(&dialog);
    enrollmentId->setMaxLength(128);
    enrollmentId->setObjectName(QStringLiteral("mfaEnrollmentId"));
    auto* totpCode = new QLineEdit(&dialog);
    totpCode->setMaxLength(6);
    totpCode->setEchoMode(QLineEdit::Password);
    totpCode->setObjectName(QStringLiteral("mfaEnrollmentTotpCode"));
    form->addRow(tr("Enrollment ID"), enrollmentId);
    form->addRow(tr("Authenticator code"), totpCode);
    layout->addLayout(form);
    auto* recoveryStored = new QCheckBox(
        tr("The user stored the recovery codes independently of this server."), &dialog);
    layout->addWidget(recoveryStored);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Confirm Enrollment"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const QString id = enrollmentId->text().trimmed();
        const QString code = totpCode->text().trimmed();
        const bool validCode = code.size() == 6 && std::all_of(code.cbegin(), code.cend(), [](QChar c) {
            return c.unicode() >= '0' && c.unicode() <= '9';
        });
        if (!validWireText(id) || !validCode || !recoveryStored->isChecked()) {
            QMessageBox::warning(&dialog, tr("Enrollment Details Required"),
                                 tr("Enter the enrollment ID and six-digit authenticator code, and confirm that recovery codes were stored independently."));
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString enrollment = enrollmentId->text().trimmed();
    QString code = totpCode->text().trimmed();
    enrollmentId->clear();
    totpCode->clear();
    QJsonObject bodyObject{
        {QStringLiteral("mfa_enrollment_id"), enrollment},
        {QStringLiteral("totp_code"), code},
        {QStringLiteral("recovery_material_stored"), true},
    };
    SensitiveBuffer requestBody(QJsonDocument(bodyObject).toJson(QJsonDocument::Compact));
    enrollment.fill(QChar(u'\0'));
    code.fill(QChar(u'\0'));
    bodyObject.insert(QStringLiteral("mfa_enrollment_id"), QString());
    bodyObject.insert(QStringLiteral("totp_code"), QString());
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/mfa/enroll/confirm"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The selected server address is invalid."), 6000);
        return;
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply* reply = network_.post(request, requestBody.bytes());
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, accountId, ownerDeployment] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(tr("MFA enrollment confirmation failed: %1")
                                         .arg(replyFailure(reply)), 10000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject result = document.object();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            result.value(QStringLiteral("account_id")).toString().toStdString() != accountId ||
            result.value(QStringLiteral("mfa_status")).toString() != QStringLiteral("active") ||
            result.value(QStringLiteral("account_state")).toString() != QStringLiteral("active")) {
            statusBar()->showMessage(tr("The server returned an invalid MFA confirmation result."), 8000);
            reply->deleteLater();
            return;
        }
        for (auto& account : accounts_) {
            if (account.id.deployment == ownerDeployment && account.id.serverEntityId == accountId) {
                account.state = "active";
            }
        }
        const auto stamp = sessions_.currentStamp(ownerDeployment);
        if (stamp && sessions_.isCurrent(*stamp)) {
            loadAuthorizedAccountData(profileId, ownerDeployment, *stamp);
        }
        refreshAccountTables();
        statusBar()->showMessage(tr("MFA enrollment confirmed. The account is now active."), 8000);
        reply->deleteLater();
    });
}

void MainWindow::refreshAccountTables()
{
    if (accountTable_ == nullptr || managerTable_ == nullptr || accessTable_ == nullptr || accountsStatus_ == nullptr) {
        return;
    }
    const QString selectedServer = accountsServerFilter_ == nullptr
        ? QString()
        : accountsServerFilter_->currentData().toString();
    accountTable_->setRowCount(static_cast<int>(accounts_.size()));
    for (int row = 0; row < static_cast<int>(accounts_.size()); ++row) {
        const auto& account = accounts_[static_cast<std::size_t>(row)];
        accountTable_->setRowHidden(row, !selectedServer.isEmpty() &&
            selectedServer != QString::fromStdString(account.profileId));
        auto* serverItem = new QTableWidgetItem(QString::fromStdString(account.serverName));
        serverItem->setData(Qt::UserRole, QString::fromStdString(account.profileId));
        accountTable_->setItem(row, 0, serverItem);
        accountTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(account.username)));
        accountTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(account.id.serverEntityId)));
        auto* stateItem = new QTableWidgetItem(QString::fromStdString(account.state));
        if (account.recoveryQuarantined) {
            stateItem->setToolTip(tr("This account came from a restored backup and cannot be reactivated."));
        }
        accountTable_->setItem(row, 3, stateItem);
        accountTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(account.capabilities)));
        accountTable_->setItem(row, 5, new QTableWidgetItem(QString::number(static_cast<qlonglong>(account.revision))));
    }
    managerTable_->setRowCount(static_cast<int>(managers_.size()));
    for (int row = 0; row < static_cast<int>(managers_.size()); ++row) {
        const auto& manager = managers_[static_cast<std::size_t>(row)];
        managerTable_->setRowHidden(row, !selectedServer.isEmpty() &&
            selectedServer != QString::fromStdString(manager.profileId));
        const QString name = QString::fromStdString(manager.serverName);
        const QString friendlyName = QString::fromStdString(manager.friendlyName);
        const QString identity = QString::fromStdString(manager.publicIdentity);
        auto* serverItem = new QTableWidgetItem(name);
        serverItem->setData(Qt::UserRole, QString::fromStdString(manager.profileId));
        managerTable_->setItem(row, 0, serverItem);
        auto* deviceItem = new QTableWidgetItem(friendlyName);
        deviceItem->setData(Qt::UserRole, QString::fromStdString(manager.id.serverEntityId));
        managerTable_->setItem(row, 1, deviceItem);
        managerTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(manager.lifecycleState)));
        auto* identityItem = new QTableWidgetItem(identity);
        identityItem->setToolTip(identity);
        managerTable_->setItem(row, 3, identityItem);
        const QString approved = manager.approvedAtUnix
            ? QDateTime::fromSecsSinceEpoch(*manager.approvedAtUnix, Qt::UTC).toString(Qt::ISODate)
            : tr("—");
        managerTable_->setItem(row, 4, new QTableWidgetItem(approved));
    }

    accessTable_->setRowCount(static_cast<int>(accessAssignments_.size()));
    for (int row = 0; row < static_cast<int>(accessAssignments_.size()); ++row) {
        const auto& assignment = accessAssignments_[static_cast<std::size_t>(row)];
        accessTable_->setRowHidden(row, !selectedServer.isEmpty() &&
            selectedServer != QString::fromStdString(assignment.profileId));
        auto* serverItem = new QTableWidgetItem(QString::fromStdString(assignment.serverName));
        serverItem->setData(Qt::UserRole, QString::fromStdString(assignment.profileId));
        serverItem->setData(Qt::UserRole + 1, QString::fromStdString(assignment.id.serverEntityId));
        serverItem->setData(Qt::UserRole + 2, static_cast<qlonglong>(assignment.revision));
        accessTable_->setItem(row, 0, serverItem);
        accessTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(assignment.accountId.serverEntityId)));
        accessTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(assignment.capability)));
        accessTable_->setItem(row, 3, new QTableWidgetItem(assignment.computerScopeAll
            ? tr("All computers")
            : tr("%1 computers").arg(static_cast<qulonglong>(assignment.computerCount))));
        accessTable_->setItem(row, 4, new QTableWidgetItem(QString::number(static_cast<qlonglong>(assignment.revision))));
    }

    QStringList errors;
    errors.reserve(static_cast<qsizetype>(accountDataErrors_.size()));
    for (const auto& [key, error] : accountDataErrors_) {
        Q_UNUSED(key);
        errors.append(QString::fromStdString(error));
    }
    errors.sort(Qt::CaseInsensitive);
    if (!errors.isEmpty()) {
        QString status = errors.join(QStringLiteral("\n"));
        if (!accountDataPending_.empty()) {
            status.append(tr("\nLoading %1 authorized record collection(s)…")
                              .arg(static_cast<qulonglong>(accountDataPending_.size())));
        }
        accountsStatus_->setText(status);
    } else if (!accountDataPending_.empty()) {
        accountsStatus_->setText(tr("Loading authorized manager and access records from %1 server collection(s)…")
                                     .arg(static_cast<qulonglong>(accountDataPending_.size())));
    } else if (accounts_.empty() && managers_.empty() && accessAssignments_.empty()) {
        accountsStatus_->setText(tr("No authorized account, manager, or access-assignment records are loaded."));
    } else {
        accountsStatus_->setText(tr("Loaded %1 accounts, %2 manager devices, and %3 access assignments across signed-in servers. Access, role, and account-state changes require the owning server's permissions and fresh MFA.")
                                     .arg(static_cast<qulonglong>(accounts_.size()))
                                     .arg(static_cast<qulonglong>(managers_.size()))
                                     .arg(static_cast<qulonglong>(accessAssignments_.size())));
    }
}

void MainWindow::refreshFileJobs()
{
    std::unordered_set<std::string> refreshed;
    for (const auto& [profileId, deployment] : deploymentsByProfile_) {
        if (refreshed.contains(deployment.value)) {
            continue;
        }
        const auto* session = sessions_.session(deployment);
        const auto profile = registry_.profile(profileId);
        const auto stamp = sessions_.currentStamp(deployment);
        if (session == nullptr || !profile || !stamp || session->profileId != profileId ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            continue;
        }
        auto& generation = fileJobRequestGenerations_[deployment.value];
        if (generation == std::numeric_limits<std::uint64_t>::max()) {
            fileJobDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                QStringLiteral(": refresh generation exhausted");
            continue;
        }
        const std::uint64_t requestGeneration = ++generation;
        const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/files/jobs"));
        if (!endpoint) {
            fileJobDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                QStringLiteral(": invalid server address");
            continue;
        }
        QNetworkRequest request = apiRequest(*endpoint);
        request.setRawHeader("Authorization", QByteArray("Bearer ") +
                             QByteArray::fromStdString(session->bearerToken));
        QNetworkReply* reply = network_.get(request);
        reply->setReadBufferSize(maxFileJobResponseBytes);
        QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
        connect(reply, &QNetworkReply::downloadProgress, this,
                [reply](qint64 received, qint64) {
                    if (received > maxFileJobResponseBytes) reply->abort();
                });
        fileJobDataPending_.insert(deployment.value);
        refreshed.insert(deployment.value);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, profileId, deployment, stamp = *stamp, requestGeneration] {
            const auto currentGeneration = fileJobRequestGenerations_.find(deployment.value);
            if (!sessions_.isCurrent(stamp) || currentGeneration == fileJobRequestGenerations_.end() ||
                currentGeneration->second != requestGeneration) {
                reply->deleteLater();
                return;
            }
            fileJobDataPending_.erase(deployment.value);
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
                body.size() > maxFileJobResponseBytes) {
                const auto profile = registry_.profile(profileId);
                fileJobDataErrors_[deployment.value] =
                    (profile ? QString::fromStdString(profile->displayName)
                             : QString::fromStdString(profileId)) +
                    QStringLiteral(": ") + replyFailure(reply);
                if (statusCode == 401) {
                    invalidateProfileSessionLocally(profileId, deployment);
                    clearDeploymentFileJobs(deployment);
                }
                refreshFileJobTable();
                reply->deleteLater();
                return;
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            const QJsonObject response = document.object();
            const QJsonValue jobsValue = response.value(QStringLiteral("jobs"));
            const QJsonArray jobs = jobsValue.toArray();
            const auto profile = registry_.profile(profileId);
            if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
                !jobsValue.isArray() || jobs.size() > 100 || !profile) {
                fileJobDataErrors_[deployment.value] =
                    QString::fromStdString(profileId) + QStringLiteral(": invalid file-job list");
                refreshFileJobTable();
                reply->deleteLater();
                return;
            }
            std::vector<FileJobRow> loaded;
            loaded.reserve(static_cast<std::size_t>(jobs.size()));
            QSet<QString> seenIds;
            for (const QJsonValue& jobValue : jobs) {
                if (!jobValue.isObject()) {
                fileJobDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                        QStringLiteral(": malformed file-job record");
                    refreshFileJobTable();
                    reply->deleteLater();
                    return;
                }
                const QJsonObject job = jobValue.toObject();
                const QString id = job.value(QStringLiteral("id")).toString();
                const QString direction = job.value(QStringLiteral("direction")).toString();
                const QString fileName = job.value(QStringLiteral("file_name")).toString();
                const QString state = job.value(QStringLiteral("state")).toString();
                const QJsonValue sizeValue = job.value(QStringLiteral("file_size_bytes"));
                const double rawSize = sizeValue.toDouble(-1);
                const QJsonValue expiresValue = job.value(QStringLiteral("delivery_expires_at_unix"));
                const double rawExpiresAt = expiresValue.toDouble(-1);
                const double maxDeliveryExpiry = static_cast<double>(
                    QDateTime::currentSecsSinceEpoch() + 7 * 24 * 60 * 60);
                const QJsonValue targetsValue = job.value(QStringLiteral("targets"));
                const QJsonArray targets = targetsValue.toArray();
                const auto scopedId = helicoid::manager::ScopedJobId::create(
                    deployment, id.toStdString());
                const QFileInfo fileNameInfo(fileName);
                const bool validState = state == QStringLiteral("uploading") ||
                    state == QStringLiteral("queued") || state == QStringLiteral("in_progress") ||
                    state == QStringLiteral("completed") || state == QStringLiteral("failed") ||
                    state == QStringLiteral("partial") || state == QStringLiteral("cancelled") ||
                    state == QStringLiteral("expired");
                if (!validUuid(id) || !scopedId || !validWireText(fileName, 255) || direction != QStringLiteral("push") ||
                    fileName == QStringLiteral(".") || fileName == QStringLiteral("..") ||
                    fileName.contains(QLatin1Char('/')) || fileName.contains(QLatin1Char('\\')) ||
                    fileNameInfo.fileName() != fileName ||
                    !validState || !sizeValue.isDouble() || !std::isfinite(rawSize) ||
                    std::floor(rawSize) != rawSize || rawSize < 0 || rawSize > maxManagerFileBytes ||
                    !expiresValue.isDouble() || !std::isfinite(rawExpiresAt) ||
                    std::floor(rawExpiresAt) != rawExpiresAt || rawExpiresAt < 1 ||
                    rawExpiresAt > maxDeliveryExpiry ||
                    !targetsValue.isArray() || targets.size() < 1 || targets.size() > 50 ||
                    seenIds.contains(id)) {
                    fileJobDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                        QStringLiteral(": invalid or duplicate file-job data");
                    refreshFileJobTable();
                    reply->deleteLater();
                    return;
                }
                QStringList targetStates;
                for (const QJsonValue& targetValue : targets) {
                    if (!targetValue.isObject()) {
                        fileJobDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                            QStringLiteral(": malformed target result");
                        refreshFileJobTable();
                        reply->deleteLater();
                        return;
                    }
                    const QJsonObject target = targetValue.toObject();
                    const QString computerId = target.value(QStringLiteral("computer_id")).toString();
                    const QString name = target.value(QStringLiteral("friendly_name")).toString();
                    const QString targetState = target.value(QStringLiteral("state")).toString();
                    const bool validTargetState = targetState == QStringLiteral("awaiting_upload") ||
                        targetState == QStringLiteral("queued") || targetState == QStringLiteral("in_progress") ||
                        targetState == QStringLiteral("waiting_computer") ||
                        targetState == QStringLiteral("waiting_recipient") ||
                        targetState == QStringLiteral("recipient_unavailable") ||
                        targetState == QStringLiteral("completed") || targetState == QStringLiteral("failed") ||
                        targetState == QStringLiteral("cancelled");
                    if (!validUuid(computerId) || !validWireText(name) ||
                        !validTargetState) {
                        fileJobDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                            QStringLiteral(": invalid target result");
                        refreshFileJobTable();
                        reply->deleteLater();
                        return;
                    }
                    QString targetStateLabel = targetState;
                    if (targetState == QStringLiteral("waiting_computer")) {
                        targetStateLabel = tr("Waiting for computer");
                    } else if (targetState == QStringLiteral("waiting_recipient")) {
                        targetStateLabel = tr("Waiting for recipient");
                    } else if (targetState == QStringLiteral("recipient_unavailable")) {
                        targetStateLabel = tr("Recipient unavailable");
                    } else if (targetState == QStringLiteral("completed")) {
                        targetStateLabel = tr("Completed");
                    }
                    const QString recipientName = target.value(QStringLiteral("recipient_display_name")).toString();
                    if (!recipientName.isEmpty() && !validAccountDisplayLabel(recipientName)) {
                        fileJobDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                            QStringLiteral(": invalid recipient label");
                        refreshFileJobTable();
                        reply->deleteLater();
                        return;
                    }
                    const QString targetSummary = recipientName.isEmpty()
                        ? QStringLiteral("%1: %2").arg(name, targetStateLabel)
                        : tr("%1 (For %2): %3").arg(name, recipientName, targetStateLabel);
                    if (targetStates.size() < 5) {
                        targetStates.append(targetSummary);
                    }
                }
                if (targets.size() > targetStates.size()) {
                    targetStates.append(tr("and %1 more").arg(static_cast<qulonglong>(
                        targets.size() - targetStates.size())));
                }
                seenIds.insert(id);
                loaded.push_back(FileJobRow{
                    *scopedId, profileId, profile->displayName, fileName.toStdString(),
                    static_cast<std::uint64_t>(rawSize), static_cast<std::int64_t>(rawExpiresAt),
                    state.toStdString(),
                    targetStates.join(QStringLiteral("; ")).toStdString(),
                });
            }
            fileJobs_.erase(std::remove_if(fileJobs_.begin(), fileJobs_.end(),
                [&](const FileJobRow& row) { return row.id.deployment == deployment; }), fileJobs_.end());
            fileJobs_.insert(fileJobs_.end(), loaded.begin(), loaded.end());
            fileJobDataErrors_.erase(deployment.value);
            refreshFileJobTable();
            reply->deleteLater();
        });
    }
    refreshFileJobTable();
}

void MainWindow::sendFileToSelectedServer()
{
    if (filesServerFilter_ == nullptr) return;
    const QString selectedProfile = filesServerFilter_->currentData().toString();
    if (selectedProfile.isEmpty()) {
        sendFileToAllSignedInServers();
        return;
    }
    const bool fileJobsFeatureGatePassed = fileJobsFeatureGatePassedProfile_ == selectedProfile.toStdString();
    fileJobsFeatureGatePassedProfile_.clear();
    const std::string profileId = selectedProfile.toStdString();
    const auto mapping = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (mapping == deploymentsByProfile_.end() || !profile) {
        statusBar()->showMessage(tr("The selected server is unavailable."), 6000);
        return;
    }
    const DeploymentId deployment = mapping->second;
    const auto* session = sessions_.session(deployment);
    const auto stamp = sessions_.currentStamp(deployment);
    if (!session || !stamp || !sessions_.isCurrent(*stamp) || session->profileId != profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the selected server before sending a file."), 6000);
        return;
    }
    if (!fileJobsFeatureGatePassed) {
        if (fileJobsFeatureGatePending_) return;
        const auto healthEndpoint = apiEndpoint(*profile, QStringLiteral("/health"));
        if (!healthEndpoint) {
            statusBar()->showMessage(tr("The server address is invalid."), 5000);
            return;
        }
        fileJobsFeatureGatePending_ = true;
        const std::string configuredOrigin = profile->endpoint;
        QNetworkReply* healthReply = network_.get(apiRequest(*healthEndpoint));
        healthReply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, healthReply, [healthReply] { healthReply->abort(); });
        connect(healthReply, &QNetworkReply::downloadProgress, this,
                [healthReply](qint64 received, qint64) {
                    if (received > maxHealthResponseBytes) healthReply->abort();
                });
        connect(healthReply, &QNetworkReply::finished, this,
                [this, healthReply, profileId, selectedProfile, configuredOrigin, stamp = *stamp] {
            fileJobsFeatureGatePending_ = false;
            const int statusCode = healthReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = healthReply->readAll();
            const auto currentProfile = registry_.profile(profileId);
            const auto currentSelection = filesServerFilter_ == nullptr
                ? QString{} : filesServerFilter_->currentData().toString();
            if (!sessions_.isCurrent(stamp) || currentSelection != selectedProfile || !currentProfile ||
                currentProfile->endpoint != configuredOrigin) {
                healthReply->deleteLater();
                return;
            }
            if (healthReply->error() != QNetworkReply::NoError || statusCode != 200 ||
                body.size() > maxHealthResponseBytes) {
                statusBar()->showMessage(tr("Could not verify whether file delivery is enabled for this server."), 7000);
                healthReply->deleteLater();
                return;
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            const QJsonObject health = document.object();
            const QJsonObject features = health.value(QStringLiteral("features")).toObject();
            if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
                health.value(QStringLiteral("status")).toString() != QStringLiteral("ok") ||
                health.value(QStringLiteral("schema_version")).toInt(-1) != 1 ||
                !features.value(QStringLiteral("file_jobs")).isBool()) {
                statusBar()->showMessage(tr("The server returned invalid file-delivery availability."), 7000);
                healthReply->deleteLater();
                return;
            }
            if (!features.value(QStringLiteral("file_jobs")).toBool()) {
                statusBar()->showMessage(tr("File delivery is not enabled by this deployment."), 7000);
                healthReply->deleteLater();
                return;
            }
            healthReply->deleteLater();
            fileJobsFeatureGatePassedProfile_ = profileId;
            sendFileToSelectedServer();
        });
        return;
    }
    QStringList computerChoices;
    std::vector<std::string> computerIds;
    for (const ComputerRow& computer : computers_) {
        if (computer.id.deployment == deployment && computer.enrollmentState == "enrolled") {
            computerChoices.append(QStringLiteral("%1 (%2)").arg(
                QString::fromStdString(computer.friendlyName),
                QString::fromStdString(computer.id.serverComputerId)));
            computerIds.push_back(computer.id.serverComputerId);
        }
    }
    if (computerChoices.isEmpty()) {
        statusBar()->showMessage(tr("No enrolled computers are loaded for this server."), 6000);
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose File to Send"));
    if (path.isEmpty()) return;
    auto file = std::make_shared<QFile>(path);
    if (!file->open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Cannot Read File"), tr("The selected file could not be opened."));
        return;
    }
    const qint64 fileSize = file->size();
    if (fileSize < 0 || fileSize > maxManagerFileBytes) {
        QMessageBox::warning(this, tr("File Too Large"),
                             tr("Choose a file no larger than 128 MiB."));
        return;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 hashedBytes = 0;
    while (!file->atEnd()) {
        const QByteArray block = file->read(managerFileChunkBytes);
        if (block.isEmpty() && (file->error() != QFileDevice::NoError || !file->atEnd())) {
            QMessageBox::warning(this, tr("Cannot Read File"), tr("The file could not be read completely."));
            return;
        }
        hashedBytes += block.size();
        if (hashedBytes > maxManagerFileBytes) {
            QMessageBox::warning(this, tr("File Too Large"),
                                 tr("The file grew beyond the 128 MiB transfer limit while it was being read."));
            return;
        }
        hash.addData(block);
    }
    if (hashedBytes != fileSize || file->size() != fileSize) {
        QMessageBox::warning(this, tr("File Changed"),
                             tr("The file changed while its digest was being calculated. Choose it again."));
        return;
    }
    const QByteArray digest = hash.result().toHex();
    if (!file->seek(0)) {
        QMessageBox::warning(this, tr("Cannot Read File"), tr("The file could not be rewound for upload."));
        return;
    }
    QDialog targetDialog(this);
    targetDialog.setWindowTitle(tr("Choose Target Computers"));
    auto* targetLayout = new QVBoxLayout(&targetDialog);
    targetLayout->addWidget(new QLabel(
        tr("Choose up to 50 enrolled computers on this server. The server will recheck operate access."),
        &targetDialog));
    auto* targetList = new QListWidget(&targetDialog);
    targetList->setSelectionMode(QAbstractItemView::NoSelection);
    targetList->setMinimumSize(440, 320);
    for (int index = 0; index < computerChoices.size(); ++index) {
        auto* item = new QListWidgetItem(computerChoices.at(index), targetList);
        item->setData(Qt::UserRole, QString::fromStdString(computerIds[static_cast<std::size_t>(index)]));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    targetLayout->addWidget(targetList);
    auto* targetButtons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok,
                                                &targetDialog);
    targetButtons->button(QDialogButtonBox::Ok)->setText(tr("Review Recipients"));
    connect(targetButtons, &QDialogButtonBox::accepted, &targetDialog, &QDialog::accept);
    connect(targetButtons, &QDialogButtonBox::rejected, &targetDialog, &QDialog::reject);
    targetLayout->addWidget(targetButtons);
    if (targetDialog.exec() != QDialog::Accepted) return;

    std::vector<FileJobRecipient> recipients;
    for (int index = 0; index < targetList->count(); ++index) {
        const QListWidgetItem* item = targetList->item(index);
        if (item->checkState() != Qt::Checked) continue;
        const QString computerId = item->data(Qt::UserRole).toString();
        if (!validUuid(computerId)) {
            statusBar()->showMessage(tr("The selected computer list contains an invalid ID."), 7000);
            return;
        }
        recipients.push_back(FileJobRecipient{
            computerId.toStdString(), item->text(), {}, {}
        });
    }
    if (recipients.empty()) {
        statusBar()->showMessage(tr("Choose at least one enrolled computer."), 6000);
        return;
    }
    if (recipients.size() > maxManagerFileJobTargets) {
        statusBar()->showMessage(tr("Choose no more than 50 computers for one file job."), 7000);
        return;
    }
    const std::string configuredOrigin = profile->endpoint;
    preflightFileJobRecipients(profileId, deployment, *stamp, file, digest, fileSize,
                               configuredOrigin, selectedProfile, std::move(recipients), 0);
}

void MainWindow::sendFileToAllSignedInServers()
{
    if (filesServerFilter_ == nullptr || !filesServerFilter_->currentData().toString().isEmpty()) {
        return;
    }
    auto context = std::make_shared<FileJobSendContext>();

    for (const ConnectionProfile& profile : registry_.profiles()) {
        const auto mapping = deploymentsByProfile_.find(profile.profileId);
        if (mapping == deploymentsByProfile_.end()) continue;
        const DeploymentId deployment = mapping->second;
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        if (!session || !stamp || !sessions_.isCurrent(*stamp) || session->profileId != profile.profileId ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            continue;
        }

        FileJobBatch batch;
        batch.profileId = profile.profileId;
        batch.deployment = deployment;
        batch.stamp = *stamp;
        batch.expectedOrigin = profile.endpoint;
        batch.serverName = QString::fromStdString(profile.displayName);
        for (const ComputerRow& computer : computers_) {
            if (computer.id.deployment != deployment || computer.enrollmentState != "enrolled") continue;
            batch.recipients.push_back(FileJobRecipient{
                computer.id.serverComputerId,
                QStringLiteral("%1 (%2)").arg(QString::fromStdString(computer.friendlyName),
                                                QString::fromStdString(computer.id.serverComputerId)),
                {},
                {},
            });
        }
        if (!batch.recipients.empty()) context->batches.push_back(std::move(batch));
    }

    if (context->batches.empty()) {
        statusBar()->showMessage(
            tr("No enrolled computers are loaded for a currently signed-in server."), 7000);
        return;
    }
    checkFileJobBatchAvailability(context, 0);
}

void MainWindow::checkFileJobBatchAvailability(
    const std::shared_ptr<FileJobSendContext>& context,
    std::size_t batchIndex)
{
    if (!context || batchIndex >= context->batches.size()) {
        selectFileJobBatchRecipients(context);
        return;
    }
    if (filesServerFilter_ == nullptr ||
        filesServerFilter_->currentData().toString() != context->selectedProfile) {
        statusBar()->showMessage(tr("The Files server filter changed before delivery could be reviewed."), 7000);
        return;
    }
    const FileJobBatch& batch = context->batches[batchIndex];
    const auto profile = registry_.profile(batch.profileId);
    const auto* session = sessions_.session(batch.deployment);
    if (!profile || profile->endpoint != batch.expectedOrigin || !session ||
        session->profileId != batch.profileId || !sessions_.isCurrent(batch.stamp) ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        context->skippedSummary += tr("%1: session unavailable").arg(batch.serverName) + QLatin1Char(';');
        context->batches[batchIndex].recipients.clear();
        checkFileJobBatchAvailability(context, batchIndex + 1);
        return;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/health"));
    if (!endpoint) {
        context->skippedSummary += tr("%1: invalid server address").arg(batch.serverName) + QLatin1Char(';');
        context->batches[batchIndex].recipients.clear();
        checkFileJobBatchAvailability(context, batchIndex + 1);
        return;
    }

    statusBar()->showMessage(
        tr("Checking delivery support on %1 (%2 of %3)…")
            .arg(batch.serverName)
            .arg(static_cast<qulonglong>(batchIndex + 1))
            .arg(static_cast<qulonglong>(context->batches.size())), 0);
    QNetworkReply* reply = network_.get(apiRequest(*endpoint));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply, context, batchIndex] {
        const FileJobBatch& currentBatch = context->batches[batchIndex];
        auto skipCurrentDeployment = [this, context, batchIndex](const QString& reason) {
            const QString serverName = context->batches[batchIndex].serverName;
            context->skippedSummary += tr("%1: %2").arg(serverName, reason) + QLatin1Char(';');
            context->batches[batchIndex].recipients.clear();
            checkFileJobBatchAvailability(context, batchIndex + 1);
        };
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const auto profile = registry_.profile(currentBatch.profileId);
        const auto* session = sessions_.session(currentBatch.deployment);
        if (filesServerFilter_ == nullptr ||
            filesServerFilter_->currentData().toString() != context->selectedProfile ||
            !profile || profile->endpoint != currentBatch.expectedOrigin || !session ||
            session->profileId != currentBatch.profileId || !sessions_.isCurrent(currentBatch.stamp) ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            reply->deleteLater();
            skipCurrentDeployment(tr("session or server profile changed"));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            const QString failure = replyFailure(reply);
            reply->deleteLater();
            skipCurrentDeployment(tr("delivery availability check failed (%1)").arg(failure));
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject health = document.object();
        const QJsonObject features = health.value(QStringLiteral("features")).toObject();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            health.value(QStringLiteral("status")).toString() != QStringLiteral("ok") ||
            health.value(QStringLiteral("schema_version")).toInt(-1) != 1 ||
            !features.value(QStringLiteral("file_jobs")).isBool()) {
            reply->deleteLater();
            skipCurrentDeployment(tr("invalid delivery availability"));
            return;
        }
        if (!features.value(QStringLiteral("file_jobs")).toBool()) {
            reply->deleteLater();
            skipCurrentDeployment(tr("file delivery is not enabled"));
            return;
        }
        reply->deleteLater();
        checkFileJobBatchAvailability(context, batchIndex + 1);
    });
}

void MainWindow::selectFileJobBatchRecipients(
    const std::shared_ptr<FileJobSendContext>& context)
{
    if (!context || context->batches.empty() || filesServerFilter_ == nullptr ||
        filesServerFilter_->currentData().toString() != context->selectedProfile) {
        statusBar()->showMessage(tr("The combined server selection changed before delivery could start."), 7000);
        return;
    }
    const bool hasCandidates = std::any_of(context->batches.begin(), context->batches.end(),
        [](const FileJobBatch& batch) { return !batch.recipients.empty(); });
    if (!hasCandidates) {
        statusBar()->showMessage(
            context->skippedSummary.isEmpty()
                ? tr("No servers currently support file delivery.")
                : tr("No server could be included: %1").arg(context->skippedSummary),
            10000);
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose File to Send"));
    if (path.isEmpty()) return;
    context->file = std::make_shared<QFile>(path);
    if (!context->file->open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Cannot Read File"), tr("The selected file could not be opened."));
        return;
    }
    context->fileSize = context->file->size();
    if (context->fileSize < 0 || context->fileSize > maxManagerFileBytes) {
        QMessageBox::warning(this, tr("File Too Large"), tr("Choose a file no larger than 128 MiB."));
        return;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 hashedBytes = 0;
    while (!context->file->atEnd()) {
        const QByteArray block = context->file->read(managerFileChunkBytes);
        if (block.isEmpty() && (context->file->error() != QFileDevice::NoError || !context->file->atEnd())) {
            QMessageBox::warning(this, tr("Cannot Read File"), tr("The file could not be read completely."));
            return;
        }
        hashedBytes += block.size();
        if (hashedBytes > maxManagerFileBytes) {
            QMessageBox::warning(this, tr("File Too Large"),
                                 tr("The file grew beyond the 128 MiB transfer limit while it was being read."));
            return;
        }
        hash.addData(block);
    }
    if (hashedBytes != context->fileSize || context->file->size() != context->fileSize) {
        QMessageBox::warning(this, tr("File Changed"),
                             tr("The file changed while its digest was being calculated. Choose it again."));
        return;
    }
    context->digest = hash.result().toHex();
    if (!context->file->seek(0)) {
        QMessageBox::warning(this, tr("Cannot Read File"),
                             tr("The file could not be rewound for upload."));
        return;
    }

    QDialog targetDialog(this);
    targetDialog.setWindowTitle(tr("Choose Target Computers"));
    auto* targetLayout = new QVBoxLayout(&targetDialog);
    QString selectionInstructions = tr(
        "Choose up to 50 enrolled computers across the signed-in servers. Helicoid creates a separate job on each owning server; those jobs can succeed or fail independently.");
    if (!context->skippedSummary.isEmpty()) {
        selectionInstructions += tr("\n\nSkipped deployments: %1").arg(context->skippedSummary);
    }
    auto* selectionDescription = new QLabel(selectionInstructions, &targetDialog);
    selectionDescription->setWordWrap(true);
    targetLayout->addWidget(selectionDescription);
    auto* targetList = new QListWidget(&targetDialog);
    targetList->setSelectionMode(QAbstractItemView::NoSelection);
    targetList->setMinimumSize(520, 360);
    for (const FileJobBatch& batch : context->batches) {
        for (const FileJobRecipient& recipient : batch.recipients) {
            auto* item = new QListWidgetItem(
                tr("%1 — %2").arg(batch.serverName, recipient.computerLabel), targetList);
            item->setData(Qt::UserRole, QString::fromStdString(batch.profileId));
            item->setData(Qt::UserRole + 1, QString::fromStdString(recipient.computerId));
            item->setData(Qt::UserRole + 2, recipient.computerLabel);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
        }
    }
    targetLayout->addWidget(targetList);
    auto* targetButtons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok,
                                                &targetDialog);
    targetButtons->button(QDialogButtonBox::Ok)->setText(tr("Review Recipients"));
    connect(targetButtons, &QDialogButtonBox::accepted, &targetDialog, &QDialog::accept);
    connect(targetButtons, &QDialogButtonBox::rejected, &targetDialog, &QDialog::reject);
    targetLayout->addWidget(targetButtons);
    if (targetDialog.exec() != QDialog::Accepted) return;

    std::size_t selectedCount = 0;
    for (FileJobBatch& batch : context->batches) batch.recipients.clear();
    for (int index = 0; index < targetList->count(); ++index) {
        const QListWidgetItem* item = targetList->item(index);
        if (item->checkState() != Qt::Checked) continue;
        if (++selectedCount > maxManagerFileJobTargets) {
            statusBar()->showMessage(tr("Choose no more than 50 computers across all servers."), 7000);
            return;
        }
        const QString profileId = item->data(Qt::UserRole).toString();
        const QString computerId = item->data(Qt::UserRole + 1).toString();
        if (profileId.isEmpty() || !validUuid(computerId)) {
            statusBar()->showMessage(tr("The combined computer list contains an invalid target."), 7000);
            return;
        }
        const auto batch = std::find_if(context->batches.begin(), context->batches.end(),
            [&](const FileJobBatch& candidate) { return candidate.profileId == profileId.toStdString(); });
        if (batch == context->batches.end()) {
            statusBar()->showMessage(tr("A selected computer no longer has an owning server."), 7000);
            return;
        }
        batch->recipients.push_back(FileJobRecipient{
            computerId.toStdString(), item->data(Qt::UserRole + 2).toString(), {}, {}
        });
    }
    context->batches.erase(std::remove_if(context->batches.begin(), context->batches.end(),
        [](const FileJobBatch& batch) { return batch.recipients.empty(); }), context->batches.end());
    if (context->batches.empty()) {
        statusBar()->showMessage(
            context->skippedSummary.isEmpty()
                ? tr("Choose at least one enrolled computer.")
                : tr("No recipients were selected from the supported deployments. Skipped: %1")
                    .arg(context->skippedSummary),
            9000);
        return;
    }
    preflightFileJobBatches(context, 0, 0);
}

void MainWindow::preflightFileJobBatches(
    const std::shared_ptr<FileJobSendContext>& context,
    std::size_t batchIndex,
    std::size_t recipientIndex)
{
    if (!context || filesServerFilter_ == nullptr ||
        filesServerFilter_->currentData().toString() != context->selectedProfile) {
        statusBar()->showMessage(tr("Recipient verification stopped because the Files server filter changed."), 7000);
        return;
    }
    if (batchIndex >= context->batches.size()) {
        QStringList recipientLines;
        for (const FileJobBatch& batch : context->batches) {
            for (const FileJobRecipient& recipient : batch.recipients) {
                if (recipient.computerId.empty()) continue;
                recipientLines.append(tr("For %1 on %2 — %3")
                    .arg(recipient.displayName, recipient.computerLabel, batch.serverName));
            }
        }
        if (recipientLines.isEmpty()) {
            statusBar()->showMessage(
                context->skippedSummary.isEmpty()
                    ? tr("No selected recipient could be verified.")
                    : tr("No selected recipient could be verified. Skipped: %1")
                        .arg(context->skippedSummary),
                10000);
            return;
        }
        QMessageBox confirmation(this);
        confirmation.setWindowTitle(tr("Confirm File Recipients"));
        confirmation.setTextFormat(Qt::PlainText);
        QString confirmationText = tr(
            "Queue %1 (%2) in the recipients’ private Received Files folders? The Manager creates one independent job per server. These jobs do not form a cross-server transaction. Each server checks operate access and the signed-in account again.\n\n%3")
            .arg(QFileInfo(context->file->fileName()).fileName(),
                 formatFileSize(static_cast<std::uint64_t>(context->fileSize)),
                 recipientLines.join(QLatin1Char('\n')));
        if (!context->skippedSummary.isEmpty()) {
            confirmationText += tr("\n\nSkipped targets or deployments: %1")
                .arg(context->skippedSummary);
        }
        confirmation.setText(confirmationText);
        confirmation.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        confirmation.setDefaultButton(QMessageBox::Cancel);
        if (confirmation.exec() != QMessageBox::Yes) return;

        for (const FileJobBatch& batch : context->batches) {
            if (batch.recipients.empty()) continue;
            const auto profile = registry_.profile(batch.profileId);
            const auto* session = sessions_.session(batch.deployment);
            if (!profile || profile->endpoint != batch.expectedOrigin || !session ||
                session->profileId != batch.profileId || !sessions_.isCurrent(batch.stamp) ||
                session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
                context->skippedSummary += tr("%1: session or server profile changed before job creation")
                    .arg(batch.serverName) + QLatin1Char(';');
            }
        }
        if (filesServerFilter_->currentData().toString() != context->selectedProfile) {
            statusBar()->showMessage(tr("The Files server filter changed before the jobs were created."), 7000);
            return;
        }
        context->batches.erase(std::remove_if(context->batches.begin(), context->batches.end(),
            [this, &context](const FileJobBatch& batch) {
                const auto profile = registry_.profile(batch.profileId);
                const auto* session = sessions_.session(batch.deployment);
                return batch.recipients.empty() || !profile ||
                    profile->endpoint != batch.expectedOrigin || !session ||
                    session->profileId != batch.profileId || !sessions_.isCurrent(batch.stamp) ||
                    session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch();
            }), context->batches.end());
        if (context->batches.empty()) {
            statusBar()->showMessage(
                tr("No verified server session remained when the jobs were ready to send. Skipped: %1")
                    .arg(context->skippedSummary), 10000);
            return;
        }
        std::size_t submitted = 0;
        for (const FileJobBatch& batch : context->batches) {
            if (createFileJobForConfirmedRecipients(batch.profileId, batch.deployment, batch.stamp,
                context->file, context->digest, context->fileSize, batch.expectedOrigin,
                context->selectedProfile, batch.recipients)) {
                ++submitted;
            }
        }
        if (submitted == 0) return;
        QString submittedSummary = tr(
            "Sent independent file-job requests to %1 servers. Results are shown separately in Files history.")
            .arg(static_cast<qulonglong>(submitted));
        if (!context->skippedSummary.isEmpty()) {
            submittedSummary += tr(" Skipped: %1").arg(context->skippedSummary);
        }
        statusBar()->showMessage(submittedSummary, 10000);
        return;
    }

    FileJobBatch& batch = context->batches[batchIndex];
    if (recipientIndex >= batch.recipients.size()) {
        batch.recipients.erase(std::remove_if(batch.recipients.begin(), batch.recipients.end(),
            [](const FileJobRecipient& recipient) { return recipient.computerId.empty(); }),
            batch.recipients.end());
        preflightFileJobBatches(context, batchIndex + 1, 0);
        return;
    }
    const auto profile = registry_.profile(batch.profileId);
    const auto* session = sessions_.session(batch.deployment);
    if (!profile || profile->endpoint != batch.expectedOrigin || !session ||
        session->profileId != batch.profileId || !sessions_.isCurrent(batch.stamp) ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        context->skippedSummary += tr("%1: session or server profile changed during recipient checks")
            .arg(batch.serverName) + QLatin1Char(';');
        batch.recipients.clear();
        preflightFileJobBatches(context, batchIndex + 1, 0);
        return;
    }

    const FileJobRecipient& recipient = batch.recipients[recipientIndex];
    statusBar()->showMessage(tr("Checking recipient %1 on %2…")
        .arg(recipient.computerLabel, batch.serverName), 0);
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/files/recipients/%1")
        .arg(QString::fromStdString(recipient.computerId)));
    if (!endpoint) {
        context->skippedSummary += tr("%1: invalid recipient endpoint").arg(batch.serverName) + QLatin1Char(';');
        batch.recipients.clear();
        preflightFileJobBatches(context, batchIndex + 1, 0);
        return;
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setRawHeader("Authorization", QByteArray("Bearer ") +
                         QByteArray::fromStdString(session->bearerToken));
    QNetworkReply* reply = network_.get(request);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
        if (received > maxHealthResponseBytes) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, context, batchIndex, recipientIndex] {
        FileJobBatch& currentBatch = context->batches[batchIndex];
        FileJobRecipient& currentRecipient = currentBatch.recipients[recipientIndex];
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const auto profile = registry_.profile(currentBatch.profileId);
        const auto* session = sessions_.session(currentBatch.deployment);
        if (filesServerFilter_ == nullptr ||
            filesServerFilter_->currentData().toString() != context->selectedProfile) {
            statusBar()->showMessage(
                tr("Recipient verification stopped because the Files server filter changed."), 8000);
            reply->deleteLater();
            return;
        }
        if (!sessions_.isCurrent(currentBatch.stamp) || !profile ||
            profile->endpoint != currentBatch.expectedOrigin || !session ||
            session->profileId != currentBatch.profileId ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            context->skippedSummary += tr("%1: session or server profile changed during recipient checks")
                .arg(currentBatch.serverName) + QLatin1Char(';');
            currentBatch.recipients.clear();
            reply->deleteLater();
            preflightFileJobBatches(context, batchIndex + 1, 0);
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            const QString reason = statusCode == 409
                ? tr("no fresh signed-in user for %1").arg(currentRecipient.computerLabel)
                : tr("recipient check failed for %1 (%2)")
                    .arg(currentRecipient.computerLabel, replyFailure(reply));
            context->skippedSummary += tr("%1: %2").arg(currentBatch.serverName, reason) + QLatin1Char(';');
            if (statusCode == 401) {
                invalidateProfileSessionLocally(currentBatch.profileId, currentBatch.deployment);
                currentBatch.recipients.clear();
                reply->deleteLater();
                preflightFileJobBatches(context, batchIndex + 1, 0);
                return;
            }
            if (statusCode != 409) {
                currentBatch.recipients.clear();
                reply->deleteLater();
                preflightFileJobBatches(context, batchIndex + 1, 0);
                return;
            }
            currentRecipient.computerId.clear();
            reply->deleteLater();
            preflightFileJobBatches(context, batchIndex, recipientIndex + 1);
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject response = document.object();
        const QString computerId = response.value(QStringLiteral("computer_id")).toString();
        const QString sessionId = response.value(QStringLiteral("session_id")).toString();
        const QString displayName = response.value(QStringLiteral("display_name")).toString();
        const QJsonValue verifiedValue = response.value(QStringLiteral("verified_at_unix"));
        const double verifiedAt = verifiedValue.toDouble(-1);
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            computerId != QString::fromStdString(currentRecipient.computerId) ||
            !validUuid(sessionId) || !validAccountDisplayLabel(displayName) ||
            !verifiedValue.isDouble() || !std::isfinite(verifiedAt) ||
            std::floor(verifiedAt) != verifiedAt || verifiedAt > now || now - verifiedAt > 180) {
            context->skippedSummary += tr("%1: invalid or stale recipient identity for %2")
                .arg(currentBatch.serverName, currentRecipient.computerLabel) + QLatin1Char(';');
            currentBatch.recipients.clear();
            reply->deleteLater();
            preflightFileJobBatches(context, batchIndex + 1, 0);
            return;
        }
        currentRecipient.sessionId = sessionId.toStdString();
        currentRecipient.displayName = displayName;
        reply->deleteLater();
        preflightFileJobBatches(context, batchIndex, recipientIndex + 1);
    });
}

void MainWindow::preflightFileJobRecipients(
    const std::string& profileId,
    const DeploymentId& deployment,
    const SessionStamp& stamp,
    const std::shared_ptr<QFile>& file,
    const QByteArray& digest,
    qint64 fileSize,
    const std::string& expectedOrigin,
    const QString& selectedProfile,
    std::vector<FileJobRecipient> recipients,
    std::size_t recipientIndex)
{
    if (recipientIndex >= recipients.size()) {
        const auto profile = registry_.profile(profileId);
        const auto* session = sessions_.session(deployment);
        if (!profile || profile->endpoint != expectedOrigin || !session ||
            session->profileId != profileId || !sessions_.isCurrent(stamp) ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() ||
            filesServerFilter_ == nullptr ||
            filesServerFilter_->currentData().toString() != selectedProfile) {
            statusBar()->showMessage(tr("The owning server session or profile changed before recipient confirmation."), 7000);
            return;
        }

        QStringList recipientLines;
        for (const FileJobRecipient& recipient : recipients) {
            recipientLines.append(tr("For %1 on %2")
                .arg(recipient.displayName, recipient.computerLabel));
        }
        QMessageBox confirmation(this);
        confirmation.setWindowTitle(tr("Confirm File Recipients"));
        confirmation.setTextFormat(Qt::PlainText);
        confirmation.setText(
            tr("Queue %1 (%2) for these recipients at %3 in their private Received Files folders?\n\n%4\n\nThe server will reject the job if any signed-in account changes.")
                .arg(QFileInfo(file->fileName()).fileName(),
                     formatFileSize(static_cast<std::uint64_t>(fileSize)),
                     QString::fromStdString(profile->displayName), recipientLines.join(QLatin1Char('\n'))));
        confirmation.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        confirmation.setDefaultButton(QMessageBox::Cancel);
        if (confirmation.exec() != QMessageBox::Yes) return;

        const auto confirmedProfile = registry_.profile(profileId);
        const auto* confirmedSession = sessions_.session(deployment);
        if (!confirmedProfile || confirmedProfile->endpoint != expectedOrigin ||
            !confirmedSession || confirmedSession->profileId != profileId ||
            confirmedSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() ||
            filesServerFilter_ == nullptr ||
            filesServerFilter_->currentData().toString() != selectedProfile ||
            !sessions_.isCurrent(stamp)) {
            statusBar()->showMessage(tr("The owning server changed before the file job was created."), 7000);
            return;
        }
        createFileJobForConfirmedRecipients(profileId, deployment, stamp, file, digest, fileSize,
                                            expectedOrigin, selectedProfile, recipients);
        return;
    }

    const auto profile = registry_.profile(profileId);
    const auto* session = sessions_.session(deployment);
    if (!profile || profile->endpoint != expectedOrigin || !session ||
        session->profileId != profileId || !sessions_.isCurrent(stamp) ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() ||
        filesServerFilter_ == nullptr ||
        filesServerFilter_->currentData().toString() != selectedProfile) {
        statusBar()->showMessage(tr("The owning server session or profile changed during recipient checks."), 7000);
        return;
    }

    statusBar()->showMessage(
        tr("Checking recipient %1 of %2: %3…")
            .arg(static_cast<qulonglong>(recipientIndex + 1))
            .arg(static_cast<qulonglong>(recipients.size()))
            .arg(recipients[recipientIndex].computerLabel), 0);

    const auto recipientEndpoint = apiEndpoint(
        *profile, QStringLiteral("/v2/files/recipients/%1")
            .arg(QString::fromStdString(recipients[recipientIndex].computerId)));
    if (!recipientEndpoint) {
        statusBar()->showMessage(tr("The server address is invalid."), 5000);
        return;
    }
    QNetworkRequest recipientRequest = apiRequest(*recipientEndpoint);
    recipientRequest.setRawHeader("Authorization", QByteArray("Bearer ") +
                                  QByteArray::fromStdString(session->bearerToken));
    QNetworkReply* recipientReply = network_.get(recipientRequest);
    recipientReply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, recipientReply, [recipientReply] { recipientReply->abort(); });
    connect(recipientReply, &QNetworkReply::downloadProgress, this,
            [recipientReply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) recipientReply->abort();
            });
    connect(recipientReply, &QNetworkReply::finished, this,
            [this, recipientReply, profileId, deployment, stamp, file, digest, fileSize,
             expectedOrigin, selectedProfile, recipients = std::move(recipients), recipientIndex] () mutable {
        const int statusCode = recipientReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = recipientReply->readAll();
        const auto currentProfile = registry_.profile(profileId);
        if (!sessions_.isCurrent(stamp) || filesServerFilter_ == nullptr ||
            filesServerFilter_->currentData().toString() != selectedProfile || !currentProfile ||
            currentProfile->endpoint != expectedOrigin) {
            recipientReply->deleteLater();
            return;
        }
        if (recipientReply->error() != QNetworkReply::NoError || statusCode != 200 ||
            responseBody.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(
                statusCode == 409
                    ? tr("No recently verified signed-in user is available on %1. That computer must check in before the job can continue.")
                        .arg(recipients[recipientIndex].computerLabel)
                    : tr("Could not verify the recipient on %1: %2")
                        .arg(recipients[recipientIndex].computerLabel, replyFailure(recipientReply)),
                9000);
            if (statusCode == 401) invalidateProfileSessionLocally(profileId, deployment);
            recipientReply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
        const QJsonObject recipient = document.object();
        const QString responseComputerId = recipient.value(QStringLiteral("computer_id")).toString();
        const QString recipientSessionId = recipient.value(QStringLiteral("session_id")).toString();
        const QString recipientDisplayName = recipient.value(QStringLiteral("display_name")).toString();
        const QJsonValue verifiedValue = recipient.value(QStringLiteral("verified_at_unix"));
        const double verifiedAt = verifiedValue.toDouble(-1);
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            responseComputerId != QString::fromStdString(recipients[recipientIndex].computerId) ||
            !validUuid(recipientSessionId) || !validAccountDisplayLabel(recipientDisplayName) ||
            !verifiedValue.isDouble() || !std::isfinite(verifiedAt) ||
            std::floor(verifiedAt) != verifiedAt || verifiedAt > now || now - verifiedAt > 180) {
            statusBar()->showMessage(
                tr("The server returned an invalid or stale recipient identity for %1.")
                    .arg(recipients[recipientIndex].computerLabel), 9000);
            recipientReply->deleteLater();
            return;
        }
        recipientReply->deleteLater();
        recipients[recipientIndex].sessionId = recipientSessionId.toStdString();
        recipients[recipientIndex].displayName = recipientDisplayName;
        preflightFileJobRecipients(profileId, deployment, stamp, file, digest, fileSize,
                                   expectedOrigin, selectedProfile, std::move(recipients),
                                   recipientIndex + 1);
    });
}

bool MainWindow::createFileJobForConfirmedRecipients(
    const std::string& profileId,
    const DeploymentId& deployment,
    const SessionStamp& stamp,
    const std::shared_ptr<QFile>& file,
    const QByteArray& digest,
    qint64 fileSize,
    const std::string& expectedOrigin,
    const QString& selectedProfile,
    const std::vector<FileJobRecipient>& recipients)
{
    const auto ownerProfile = registry_.profile(profileId);
    const QString ownerName = ownerProfile ? QString::fromStdString(ownerProfile->displayName)
                                           : tr("selected server");
    if (!file || !sessions_.isCurrent(stamp) || file->size() != fileSize || recipients.empty() ||
        recipients.size() > maxManagerFileJobTargets) {
        statusBar()->showMessage(
            tr("The confirmed recipients or file changed before the job was created on %1.").arg(ownerName), 7000);
        return false;
    }
    QJsonArray computerIds;
    QJsonArray expectedRecipients;
    QSet<QString> seenComputerIds;
    for (const FileJobRecipient& recipient : recipients) {
        const QString computerId = QString::fromStdString(recipient.computerId);
        const QString recipientSessionId = QString::fromStdString(recipient.sessionId);
        if (!validUuid(computerId) || !validUuid(recipientSessionId) ||
            !validAccountDisplayLabel(recipient.displayName) || seenComputerIds.contains(computerId)) {
            statusBar()->showMessage(
                tr("A confirmed recipient identity was invalid or duplicated on %1.").arg(ownerName), 7000);
            return false;
        }
        seenComputerIds.insert(computerId);
        computerIds.append(computerId);
        expectedRecipients.append(QJsonObject{
            {QStringLiteral("computer_id"), computerId},
            {QStringLiteral("session_id"), recipientSessionId},
        });
    }
    const auto* currentSession = sessions_.session(deployment);
    const auto profile = registry_.profile(profileId);
    if (!currentSession || !profile || currentSession->profileId != profileId ||
        profile->endpoint != expectedOrigin ||
        filesServerFilter_ == nullptr ||
        filesServerFilter_->currentData().toString() != selectedProfile ||
        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(
            tr("The server session for %1 expired before the file job was created.").arg(ownerName), 6000);
        return false;
    }
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/files/jobs"));
    if (!endpoint) {
        statusBar()->showMessage(tr("The address for %1 is invalid.").arg(ownerName), 5000);
        return false;
    }
    QJsonObject body{
        {QStringLiteral("file_name"), QFileInfo(file->fileName()).fileName()},
        {QStringLiteral("file_size_bytes"), static_cast<double>(fileSize)},
        {QStringLiteral("sha256"), QString::fromLatin1(digest)},
        {QStringLiteral("computer_ids"), computerIds},
        {QStringLiteral("expected_recipients"), expectedRecipients},
    };
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") +
                         QByteArray::fromStdString(currentSession->bearerToken));
    QNetworkReply* reply = network_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp, file, digest, fileSize, recipients,
             expectedOrigin, ownerName] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = reply->readAll();
        const auto currentProfile = registry_.profile(profileId);
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        if (!currentProfile || currentProfile->endpoint != expectedOrigin) {
            statusBar()->showMessage(
                tr("The server profile for %1 changed after job creation. Refresh job history before retrying; no payload was uploaded.")
                    .arg(ownerName),
                10000);
            refreshFileJobs();
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 201 ||
            responseBody.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(
                statusCode == 0
                    ? tr("The job request to %1 did not receive a response. Refresh job history before retrying; the server may have created it.")
                        .arg(ownerName)
                    : statusCode == 409
                        ? tr("A signed-in recipient on %1 is unavailable or changed. Refresh status and confirm again.")
                            .arg(ownerName)
                    : tr("Could not create a file job on %1: %2").arg(ownerName, replyFailure(reply)),
                10000);
            if (statusCode == 401) invalidateProfileSessionLocally(profileId, deployment);
            else refreshFileJobs();
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
        const QJsonObject job = document.object();
        const QString jobId = job.value(QStringLiteral("id")).toString();
        const QString direction = job.value(QStringLiteral("direction")).toString();
        const QString state = job.value(QStringLiteral("state")).toString();
        const QString serverDigest = job.value(QStringLiteral("sha256")).toString();
        const double serverSize = job.value(QStringLiteral("file_size_bytes")).toDouble(-1);
        const double rawChunkSize = job.value(QStringLiteral("chunk_size_bytes")).toDouble(-1);
        const double rawChunkCount = job.value(QStringLiteral("chunk_count")).toDouble(-1);
        const QJsonValue deliveryExpiryValue = job.value(QStringLiteral("delivery_expires_at_unix"));
        const double rawDeliveryExpiry = deliveryExpiryValue.toDouble(-1);
        const qint64 receiptNow = QDateTime::currentSecsSinceEpoch();
        const bool validDeliveryExpiry = deliveryExpiryValue.isDouble() &&
            std::isfinite(rawDeliveryExpiry) && std::floor(rawDeliveryExpiry) == rawDeliveryExpiry &&
            rawDeliveryExpiry >= static_cast<double>(receiptNow) &&
            rawDeliveryExpiry <= static_cast<double>(receiptNow + 7 * 24 * 60 * 60);
        const QJsonArray targets = job.value(QStringLiteral("targets")).toArray();
        const auto scopedJob = helicoid::manager::ScopedJobId::create(deployment, jobId.toStdString());
        const qint64 expectedChunks = fileSize == 0 ? 0 :
            (fileSize + managerFileChunkBytes - 1) / managerFileChunkBytes;
        QSet<QString> returnedComputerIds;
        bool targetReceiptsMatch = targets.size() == static_cast<qsizetype>(recipients.size());
        for (const QJsonValue& targetValue : targets) {
            if (!targetValue.isObject()) {
                targetReceiptsMatch = false;
                continue;
            }
            const QJsonObject target = targetValue.toObject();
            const QString computerId = target.value(QStringLiteral("computer_id")).toString();
            const auto expected = std::find_if(recipients.begin(), recipients.end(),
                [&](const FileJobRecipient& recipient) {
                    return QString::fromStdString(recipient.computerId) == computerId;
                });
            if (expected == recipients.end() || returnedComputerIds.contains(computerId) ||
                target.value(QStringLiteral("recipient_display_name")).toString() != expected->displayName) {
                targetReceiptsMatch = false;
            } else {
                returnedComputerIds.insert(computerId);
            }
        }
        targetReceiptsMatch = targetReceiptsMatch &&
            returnedComputerIds.size() == static_cast<qsizetype>(recipients.size());
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            !validUuid(jobId) || !scopedJob ||
            direction != QStringLiteral("push") ||
            state != (fileSize == 0 ? QStringLiteral("queued") : QStringLiteral("uploading")) ||
            serverDigest != QString::fromLatin1(digest) || serverSize != fileSize ||
            !targetReceiptsMatch || !validDeliveryExpiry ||
            file->size() != fileSize ||
            rawChunkSize != managerFileChunkBytes || rawChunkCount != expectedChunks ||
            rawChunkCount < 0 || rawChunkCount > 512) {
            statusBar()->showMessage(
                tr("%1 returned an invalid file-job receipt; its upload was not started.").arg(ownerName), 9000);
            refreshFileJobs();
            reply->deleteLater();
            return;
        }
        reply->deleteLater();
        refreshFileJobs();
        if (expectedChunks == 0) {
            statusBar()->showMessage(
                tr("The empty file was queued for %1 computers on %2.")
                    .arg(static_cast<qulonglong>(recipients.size())).arg(ownerName), 6000);
            return;
        }
        statusBar()->showMessage(
            tr("Uploading %1 to %2 computers in bounded chunks…")
                .arg(QFileInfo(file->fileName()).fileName())
                .arg(static_cast<qulonglong>(recipients.size())) + tr(" on %1").arg(ownerName), 0);
        uploadFileJobChunk(profileId, deployment, stamp, jobId.toStdString(),
                           digest.toStdString(), file,
                           std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256),
                           expectedOrigin,
                           static_cast<std::uint32_t>(rawChunkSize),
                           static_cast<std::uint32_t>(rawChunkCount), 0);
    });
    return true;
}

void MainWindow::uploadFileJobChunk(
    const std::string& profileId,
    const DeploymentId& deployment,
    const SessionStamp& stamp,
    const std::string& jobId,
    const std::string& expectedSha256,
    const std::shared_ptr<QFile>& file,
    const std::shared_ptr<QCryptographicHash>& uploadedHash,
    const std::string& expectedOrigin,
    std::uint32_t chunkSize,
    std::uint32_t chunkCount,
    std::uint32_t chunkIndex)
{
    if (!sessions_.isCurrent(stamp) || chunkIndex >= chunkCount) return;
    const auto* session = sessions_.session(deployment);
    const auto profile = registry_.profile(profileId);
    const QString ownerName = profile ? QString::fromStdString(profile->displayName) : tr("server");
    if (!session || !profile || session->profileId != profileId ||
        profile->endpoint != expectedOrigin ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(
            tr("Upload stopped because the session or profile for %1 changed.").arg(ownerName), 8000);
        return;
    }
    const qint64 offset = static_cast<qint64>(chunkIndex) * chunkSize;
    const qint64 remaining = file->size() - offset;
    const qint64 expectedLength = std::min<qint64>(chunkSize, remaining);
    if (expectedLength <= 0 || !file->seek(offset)) {
        statusBar()->showMessage(tr("Upload to %1 stopped because the local file could not be read.").arg(ownerName), 8000);
        return;
    }
    const QByteArray chunk = file->read(expectedLength);
    if (chunk.size() != expectedLength) {
        statusBar()->showMessage(tr("Upload to %1 stopped because the local file could not be read.").arg(ownerName), 8000);
        return;
    }
    uploadedHash->addData(chunk);
    const bool finalChunk = chunkIndex + 1 == chunkCount;
    if (finalChunk && uploadedHash->result().toHex() != QByteArray::fromStdString(expectedSha256)) {
        statusBar()->showMessage(
            tr("Upload to %1 stopped because the file changed after its digest was calculated. Cancel the incomplete job or let it expire.")
                .arg(ownerName), 10000);
        refreshFileJobs();
        return;
    }
    const auto endpoint = apiEndpoint(
        *profile, QStringLiteral("/v2/files/jobs/%1/chunks/%2")
            .arg(QString::fromStdString(jobId)).arg(static_cast<qulonglong>(chunkIndex)));
    if (!endpoint) {
        statusBar()->showMessage(tr("Upload stopped because the address for %1 is invalid.").arg(ownerName), 8000);
        return;
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") +
                         QByteArray::fromStdString(session->bearerToken));
    QNetworkReply* reply = network_.put(request, chunk);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp, jobId, expectedSha256,
             file, uploadedHash, expectedOrigin, chunkSize, chunkCount, chunkIndex, finalChunk, ownerName] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (!sessions_.isCurrent(stamp)) {
            reply->deleteLater();
            return;
        }
        const auto currentProfile = registry_.profile(profileId);
        if (!currentProfile || currentProfile->endpoint != expectedOrigin) {
            statusBar()->showMessage(
                tr("Upload to %1 stopped because its server profile changed. Refresh job history before retrying.")
                    .arg(ownerName),
                10000);
            refreshFileJobs();
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            statusBar()->showMessage(
                tr("Upload to %1 stopped at chunk %2: %3. Refresh its state before retrying; any unfinished upload expires automatically.")
                    .arg(ownerName).arg(static_cast<qulonglong>(chunkIndex)).arg(replyFailure(reply)), 10000);
            if (statusCode == 401) invalidateProfileSessionLocally(profileId, deployment);
            refreshFileJobs();
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject receipt = document.object();
        const double receivedIndex = receipt.value(QStringLiteral("chunk_index")).toDouble(-1);
        const double receivedCount = receipt.value(QStringLiteral("received_chunks")).toDouble(-1);
        const double responseChunkCount = receipt.value(QStringLiteral("chunk_count")).toDouble(-1);
        const bool finalized = receipt.value(QStringLiteral("finalized")).toBool(false);
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            receipt.value(QStringLiteral("job_id")).toString() != QString::fromStdString(jobId) ||
            receivedIndex != chunkIndex || receivedCount != chunkIndex + 1 ||
            responseChunkCount != chunkCount || finalized != finalChunk ||
            receipt.value(QStringLiteral("state")).toString() !=
                (finalChunk ? QStringLiteral("queued") : QStringLiteral("uploading"))) {
            statusBar()->showMessage(
                tr("Upload to %1 stopped because the server returned an invalid chunk receipt. Cancel the incomplete job or refresh its state.")
                    .arg(ownerName), 10000);
            refreshFileJobs();
            reply->deleteLater();
            return;
        }
        reply->deleteLater();
        if (finalChunk) {
            statusBar()->showMessage(tr("File payload verified by %1 and queued for delivery.").arg(ownerName), 7000);
            refreshFileJobs();
            return;
        }
        const std::uint32_t nextChunk = chunkIndex + 1;
        statusBar()->showMessage(tr("Uploading to %1: chunk %2 of %3…")
                                     .arg(ownerName)
                                     .arg(static_cast<qulonglong>(nextChunk + 1))
                                     .arg(static_cast<qulonglong>(chunkCount)), 0);
        uploadFileJobChunk(profileId, deployment, stamp, jobId, expectedSha256,
                           file, uploadedHash, expectedOrigin, chunkSize, chunkCount, nextChunk);
    });
}

void MainWindow::cancelSelectedFileJob()
{
    if (!fileJobTable_ || fileJobTable_->currentRow() < 0) {
        statusBar()->showMessage(tr("Select one uploading or queued file job first."), 6000);
        return;
    }
    const int row = fileJobTable_->currentRow();
    const auto* serverItem = fileJobTable_->item(row, 0);
    const auto* idItem = fileJobTable_->item(row, 5);
    const auto* stateItem = fileJobTable_->item(row, 4);
    if (!serverItem || !idItem || !stateItem ||
        (stateItem->text() != QStringLiteral("uploading") && stateItem->text() != QStringLiteral("queued"))) {
        statusBar()->showMessage(tr("Only uploading or not-yet-leased queued jobs can be cancelled."), 6000);
        return;
    }
    const std::string profileId = serverItem->data(Qt::UserRole).toString().toStdString();
    const QString serverName = serverItem->text();
    const std::string jobId = idItem->text().toStdString();
    const std::string rowDeploymentId = idItem->data(Qt::UserRole).toString().toStdString();
    const auto mapping = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    if (mapping == deploymentsByProfile_.end() || !profile ||
        mapping->second.value != rowDeploymentId) {
        statusBar()->showMessage(tr("The selected job no longer matches its owning server."), 6000);
        return;
    }
    const DeploymentId deployment = mapping->second;
    const auto* session = sessions_.session(deployment);
    const auto stamp = sessions_.currentStamp(deployment);
    if (!session || !stamp || !sessions_.isCurrent(*stamp) || session->profileId != profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the owning server before cancelling this job."), 6000);
        return;
    }
    bool accepted = false;
    const QString confirmation = QInputDialog::getText(
        this, tr("Cancel File Job"),
        tr("Type this exact job ID on %1 to confirm cancellation:\n%2")
            .arg(serverName.toHtmlEscaped(), QString::fromStdString(jobId).toHtmlEscaped()),
        QLineEdit::Normal, QString(), &accepted);
    if (!accepted || confirmation.toStdString() != jobId) {
        statusBar()->showMessage(tr("The job was not cancelled because its ID did not match."), 6000);
        return;
    }
    const std::int64_t nowUnix = QDateTime::currentSecsSinceEpoch();
    const auto currentProfile = registry_.profile(profileId);
    const auto currentMapping = deploymentsByProfile_.find(profileId);
    const auto* currentSession = sessions_.session(deployment, nowUnix);
    const auto currentStamp = sessions_.currentStamp(deployment, nowUnix);
    if (!currentProfile || currentProfile->endpoint != profile->endpoint ||
        currentMapping == deploymentsByProfile_.end() || currentMapping->second != deployment ||
        !currentSession || currentSession->profileId != profileId || !currentStamp ||
        currentStamp->generation != stamp->generation || !sessions_.isCurrent(*stamp, nowUnix)) {
        statusBar()->showMessage(tr("The owning server session changed before job cancellation."), 6000);
        return;
    }
    const auto currentJob = std::find_if(fileJobs_.begin(), fileJobs_.end(),
        [&](const FileJobRow& job) {
            return job.id.deployment == deployment && job.id.serverEntityId == jobId &&
                   job.profileId == profileId;
        });
    if (currentJob == fileJobs_.end() ||
        (currentJob->state != "uploading" && currentJob->state != "queued")) {
        statusBar()->showMessage(tr("The selected job is no longer eligible for cancellation."), 6000);
        return;
    }
    const auto endpoint = apiEndpoint(*currentProfile,
        QStringLiteral("/v2/files/jobs/%1").arg(QString::fromStdString(jobId)));
    if (!endpoint) return;
    const std::string expectedOrigin = currentProfile->endpoint;
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") +
                         QByteArray::fromStdString(currentSession->bearerToken));
    const QByteArray body = QJsonDocument(QJsonObject{
        {QStringLiteral("confirm_job_id"), QString::fromStdString(jobId)},
    }).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = network_.post(request, body);
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, expectedOrigin] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto currentProfile = registry_.profile(profileId);
        const auto currentMapping = deploymentsByProfile_.find(profileId);
        if (!sessions_.isCurrent(stamp) || !currentProfile ||
            currentMapping == deploymentsByProfile_.end() || currentMapping->second != deployment ||
            currentProfile->endpoint != expectedOrigin) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200) {
            statusBar()->showMessage(tr("Could not cancel the file job: %1").arg(replyFailure(reply)), 8000);
            if (statusCode == 401) invalidateProfileSessionLocally(profileId, deployment);
        } else {
            statusBar()->showMessage(tr("The file job was cancelled."), 6000);
        }
        reply->deleteLater();
        refreshFileJobs();
    });
}

void MainWindow::refreshFileJobTable()
{
    if (!fileJobTable_ || !filesStatus_) return;
    QString serverFilter = filesServerFilter_ ? filesServerFilter_->currentData().toString() : QString();
    std::vector<const FileJobRow*> visible;
    for (const auto& job : fileJobs_) {
        if (serverFilter.isEmpty() || QString::fromStdString(job.profileId) == serverFilter) {
            visible.push_back(&job);
        }
    }
    std::sort(visible.begin(), visible.end(), [](const FileJobRow* left, const FileJobRow* right) {
        if (left->serverName != right->serverName) return left->serverName < right->serverName;
        if (left->fileName != right->fileName) return left->fileName < right->fileName;
        return left->id.serverEntityId < right->id.serverEntityId;
    });
    fileJobTable_->setRowCount(static_cast<int>(visible.size()));
    for (int index = 0; index < static_cast<int>(visible.size()); ++index) {
        const FileJobRow& job = *visible[static_cast<std::size_t>(index)];
        auto* server = new QTableWidgetItem(QString::fromStdString(job.serverName));
        server->setData(Qt::UserRole, QString::fromStdString(job.profileId));
        fileJobTable_->setItem(index, 0, server);
        fileJobTable_->setItem(index, 1, new QTableWidgetItem(QString::fromStdString(job.fileName)));
        fileJobTable_->setItem(index, 2, new QTableWidgetItem(formatFileSize(job.fileSizeBytes)));
        fileJobTable_->setItem(index, 3, new QTableWidgetItem(QString::fromStdString(job.targetSummary)));
        fileJobTable_->setItem(index, 4, new QTableWidgetItem(QString::fromStdString(job.state)));
        const auto expiry = QDateTime::fromSecsSinceEpoch(job.deliveryExpiresAtUnix, Qt::UTC)
            .toLocalTime().toString(Qt::ISODate);
        fileJobTable_->setItem(index, 5, new QTableWidgetItem(expiry));
        auto* id = new QTableWidgetItem(QString::fromStdString(job.id.serverEntityId));
        id->setData(Qt::UserRole, QString::fromStdString(job.id.deployment.value));
        fileJobTable_->setItem(index, 6, id);
    }
    QStringList pending;
    for (const auto& deployment : fileJobDataPending_) {
        Q_UNUSED(deployment);
        pending.append(tr("Loading jobs"));
    }
    QStringList errors;
    for (const auto& [deployment, error] : fileJobDataErrors_) {
        Q_UNUSED(deployment);
        errors.append(error);
    }
    if (fileTabs_ == nullptr || fileTabs_->currentIndex() == 0) {
        if (!errors.isEmpty()) {
            filesStatus_->setText(tr("Some server job lists could not be refreshed: %1").arg(errors.join(QStringLiteral("; "))));
        } else if (!pending.isEmpty()) {
            filesStatus_->setText(tr("Refreshing durable file transfers."));
        } else if (fileJobs_.empty()) {
            filesStatus_->setText(tr("No file transfers are loaded. Choose a server and sign in to see its history."));
        } else {
            filesStatus_->setText(tr("Showing durable manager-to-computer file transfers."));
        }
    }
}

void MainWindow::clearDeploymentFileJobs(const DeploymentId& deployment)
{
    auto& generation = fileJobRequestGenerations_[deployment.value];
    if (generation < std::numeric_limits<std::uint64_t>::max()) ++generation;
    fileJobs_.erase(std::remove_if(fileJobs_.begin(), fileJobs_.end(),
        [&](const FileJobRow& row) { return row.id.deployment == deployment; }), fileJobs_.end());
    fileJobDataErrors_.erase(deployment.value);
    fileJobDataPending_.erase(deployment.value);
    refreshFileJobTable();
}

void MainWindow::withFileRequestsAvailable(const std::string& profileId,
                                           std::function<void()> continuation)
{
    if (fileRequestAvailabilityPending_.contains(profileId)) {
        statusBar()->showMessage(tr("Checking file-request availability. Try again shortly."), 5000);
        return;
    }
    const auto mapping = deploymentsByProfile_.find(profileId);
    const auto profile = registry_.profile(profileId);
    const QString serverFilter = filesServerFilter_ == nullptr
        ? QString{} : filesServerFilter_->currentData().toString();
    if (mapping == deploymentsByProfile_.end() || !profile || filesServerFilter_ == nullptr) {
        statusBar()->showMessage(tr("The file request's owning server is unavailable."), 6000);
        return;
    }
    if (!serverFilter.isEmpty() && serverFilter != QString::fromStdString(profileId)) {
        statusBar()->showMessage(tr("Select All servers or the file request's owning server."), 6000);
        return;
    }
    const DeploymentId deployment = mapping->second;
    const auto* session = sessions_.session(deployment);
    const auto stamp = sessions_.currentStamp(deployment);
    if (!session || !stamp || !sessions_.isCurrent(*stamp) || session->profileId != profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("Sign in to the file request's owning server first."), 6000);
        return;
    }
    const auto healthEndpoint = apiEndpoint(*profile, QStringLiteral("/health"));
    if (!healthEndpoint) {
        statusBar()->showMessage(tr("The server address is invalid."), 5000);
        return;
    }
    const std::string expectedOrigin = profile->endpoint;
    fileRequestAvailabilityPending_.insert(profileId);
    QNetworkReply* reply = network_.get(apiRequest(*healthEndpoint));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp = *stamp, expectedOrigin,
             continuation = std::move(continuation)]() mutable {
        fileRequestAvailabilityPending_.erase(profileId);
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const auto currentProfile = registry_.profile(profileId);
        const QString currentSelection = filesServerFilter_ == nullptr
            ? QString{} : filesServerFilter_->currentData().toString();
        if (!sessions_.isCurrent(stamp) ||
            (!currentSelection.isEmpty() && currentSelection != QString::fromStdString(profileId)) ||
            !currentProfile || currentProfile->endpoint != expectedOrigin) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            const auto profile = registry_.profile(profileId);
            fileRequestDataErrors_[deployment.value] =
                (profile ? QString::fromStdString(profile->displayName)
                         : QString::fromStdString(profileId)) +
                QStringLiteral(": could not verify file-request availability");
            refreshFileRequestTable();
            statusBar()->showMessage(
                tr("Could not verify file-request availability for this server."), 7000);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject health = document.object();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            health.value(QStringLiteral("status")).toString() != QStringLiteral("ok") ||
            health.value(QStringLiteral("schema_version")).toInt(-1) != 1) {
            fileRequestDataErrors_[deployment.value] =
                QString::fromStdString(currentProfile->displayName) +
                QStringLiteral(": invalid file-request availability");
            refreshFileRequestTable();
            statusBar()->showMessage(tr("The server returned invalid file-request availability."), 7000);
            reply->deleteLater();
            return;
        }
        const auto* currentSession = sessions_.session(deployment);
        if (!currentSession || currentSession->profileId != profileId ||
            currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() ||
            !sessions_.isCurrent(stamp)) {
            statusBar()->showMessage(tr("The selected server session expired."), 6000);
            reply->deleteLater();
            return;
        }
        reply->deleteLater();
        continuation();
    });
}

void MainWindow::refreshFileRequests()
{
    if (filesServerFilter_ == nullptr) return;
    const QString selected = filesServerFilter_->currentData().toString();
    std::vector<std::string> profileIds;
    const auto addSignedInProfile = [this, &profileIds](const ConnectionProfile& profile) {
        const auto mapping = deploymentsByProfile_.find(profile.profileId);
        if (mapping == deploymentsByProfile_.end()) return;
        const DeploymentId deployment = mapping->second;
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        if (!session || !stamp || session->profileId != profile.profileId ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() || !sessions_.isCurrent(*stamp)) {
            fileRequestDataErrors_[deployment.value] = QString::fromStdString(profile.displayName) +
                QStringLiteral(": sign in to refresh file requests");
            return;
        }
        fileRequestDataErrors_.erase(deployment.value);
        profileIds.push_back(profile.profileId);
    };
    if (selected.isEmpty()) {
        for (const ConnectionProfile& profile : registry_.profiles()) addSignedInProfile(profile);
    } else if (const auto profile = registry_.profile(selected.toStdString())) {
        addSignedInProfile(*profile);
    }
    refreshFileRequestTable();
    if (profileIds.empty()) {
        if (registry_.profiles().empty()) {
            statusBar()->showMessage(tr("Add a server profile before refreshing file requests."), 6000);
        }
        return;
    }
    for (const std::string& profileId : profileIds) {
        const auto mapping = deploymentsByProfile_.find(profileId);
        const auto profile = registry_.profile(profileId);
        if (mapping == deploymentsByProfile_.end() || !profile) continue;
        const DeploymentId deployment = mapping->second;
        withFileRequestsAvailable(profileId, [this, profileId, deployment] {
            const auto profile = registry_.profile(profileId);
            if (!profile) return;
            const auto* session = sessions_.session(deployment);
            const auto stamp = sessions_.currentStamp(deployment);
            if (!session || !stamp || session->profileId != profileId || !sessions_.isCurrent(*stamp)) return;
            auto& generation = fileRequestRequestGenerations_[deployment.value];
            if (generation == std::numeric_limits<std::uint64_t>::max()) {
                fileRequestDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                    QStringLiteral(": request refresh generation exhausted");
                refreshFileRequestTable();
                return;
            }
            const std::uint64_t requestGeneration = ++generation;
            const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/files/requests"));
            if (!endpoint) {
                fileRequestDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                    QStringLiteral(": invalid server address");
                refreshFileRequestTable();
                return;
            }
            QNetworkRequest request = apiRequest(*endpoint);
            request.setRawHeader("Authorization", QByteArray("Bearer ") +
                                 QByteArray::fromStdString(session->bearerToken));
            QNetworkReply* reply = network_.get(request);
            reply->setReadBufferSize(maxFileJobResponseBytes);
            QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
            connect(reply, &QNetworkReply::downloadProgress, this,
                    [reply](qint64 received, qint64) {
                        if (received > maxFileJobResponseBytes) reply->abort();
                    });
            fileRequestDataPending_.insert(deployment.value);
            refreshFileRequestTable();
            connect(reply, &QNetworkReply::finished, this,
                    [this, reply, profileId, deployment, stamp = *stamp, requestGeneration] {
                const auto currentGeneration = fileRequestRequestGenerations_.find(deployment.value);
                if (!sessions_.isCurrent(stamp) || currentGeneration == fileRequestRequestGenerations_.end() ||
                    currentGeneration->second != requestGeneration) {
                    reply->deleteLater();
                    return;
                }
                fileRequestDataPending_.erase(deployment.value);
                const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray body = reply->readAll();
                const auto profile = registry_.profile(profileId);
                if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
                    body.size() > maxFileJobResponseBytes || !profile) {
                    fileRequestDataErrors_[deployment.value] =
                        (profile ? QString::fromStdString(profile->displayName)
                                 : QString::fromStdString(profileId)) +
                        QStringLiteral(": ") + replyFailure(reply);
                    if (statusCode == 401) invalidateProfileSessionLocally(profileId, deployment);
                    refreshFileRequestTable();
                    reply->deleteLater();
                    return;
                }
                QJsonParseError parseError;
                const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
                const QJsonObject response = document.object();
                const QJsonValue requestsValue = response.value(QStringLiteral("requests"));
                const QJsonArray requests = requestsValue.toArray();
                if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
                    !requestsValue.isArray() || requests.size() > 100) {
                    fileRequestDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                        QStringLiteral(": invalid file-request list");
                    refreshFileRequestTable();
                    reply->deleteLater();
                    return;
                }
                std::vector<FileRequestRow> loaded;
                loaded.reserve(static_cast<std::size_t>(requests.size()));
                QSet<QString> seenIds;
                for (const QJsonValue& value : requests) {
                    if (!value.isObject()) {
                        fileRequestDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                            QStringLiteral(": malformed file-request record");
                        refreshFileRequestTable();
                        reply->deleteLater();
                        return;
                    }
                    const QJsonObject request = value.toObject();
                    const QString id = request.value(QStringLiteral("id")).toString();
                    const QString title = request.value(QStringLiteral("title")).toString();
                    const QString state = request.value(QStringLiteral("state")).toString();
                    const QJsonValue revisionValue = request.value(QStringLiteral("revision"));
                    const QJsonValue closesAtValue = request.value(QStringLiteral("closes_at_unix"));
                    const QJsonValue computersValue = request.value(QStringLiteral("computer_ids"));
                    const double revision = revisionValue.toDouble(-1);
                    const double closesAt = closesAtValue.toDouble(-1);
                    const QJsonArray computers = computersValue.toArray();
                    bool validTargets = computersValue.isArray() && computers.size() > 0 && computers.size() <= 50;
                    QSet<QString> seenComputers;
                    for (const QJsonValue& computerValue : computers) {
                        if (!computerValue.isString() || !validUuid(computerValue.toString()) ||
                            seenComputers.contains(computerValue.toString())) {
                            validTargets = false;
                        }
                        seenComputers.insert(computerValue.toString());
                    }
                    if (!validUuid(id) || seenIds.contains(id) || !validWireText(title, 120) ||
                        !validTargets || (state != QStringLiteral("draft") && state != QStringLiteral("open") &&
                                          state != QStringLiteral("closed")) ||
                        !revisionValue.isDouble() || !std::isfinite(revision) || std::floor(revision) != revision ||
                        revision <= 0 || !closesAtValue.isDouble() || !std::isfinite(closesAt) ||
                        std::floor(closesAt) != closesAt || closesAt <= 0) {
                        fileRequestDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                            QStringLiteral(": invalid file-request fields");
                        refreshFileRequestTable();
                        reply->deleteLater();
                        return;
                    }
                    seenIds.insert(id);
                    const auto scopedId = helicoid::manager::ScopedFileRequestId::create(
                        deployment, id.toStdString());
                    if (!scopedId) {
                        fileRequestDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                            QStringLiteral(": invalid file-request identity");
                        refreshFileRequestTable();
                        reply->deleteLater();
                        return;
                    }
                    loaded.push_back(FileRequestRow{
                        *scopedId, profileId,
                        profile->displayName, title.toStdString(), state.toStdString(),
                        static_cast<std::int64_t>(revision), static_cast<std::int64_t>(closesAt),
                        static_cast<std::size_t>(computers.size())
                    });
                }
                fileRequests_.erase(std::remove_if(fileRequests_.begin(), fileRequests_.end(),
                    [&](const FileRequestRow& row) { return row.id.deployment == deployment; }), fileRequests_.end());
                fileRequests_.insert(fileRequests_.end(), loaded.begin(), loaded.end());
                fileRequestDataErrors_.erase(deployment.value);
                refreshFileRequestTable();
                reply->deleteLater();
            });
        });
    }
}

void MainWindow::refreshFileStorage()
{
    if (filesServerFilter_ == nullptr || filesQuotaStatus_ == nullptr) return;
    const QString selected = filesServerFilter_->currentData().toString();
    std::vector<std::string> profileIds;
    const auto addSignedInProfile = [this, &profileIds](const ConnectionProfile& profile) {
        const auto mapping = deploymentsByProfile_.find(profile.profileId);
        if (mapping == deploymentsByProfile_.end()) return;
        const DeploymentId deployment = mapping->second;
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        if (!session || !stamp || session->profileId != profile.profileId ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() || !sessions_.isCurrent(*stamp)) {
            fileStorageDataErrors_[deployment.value] = QString::fromStdString(profile.displayName) +
                QStringLiteral(": sign in to refresh the file quota");
            return;
        }
        fileStorageDataErrors_.erase(deployment.value);
        profileIds.push_back(profile.profileId);
    };
    if (selected.isEmpty()) {
        for (const ConnectionProfile& profile : registry_.profiles()) addSignedInProfile(profile);
    } else if (const auto profile = registry_.profile(selected.toStdString())) {
        addSignedInProfile(*profile);
    }

    for (const std::string& profileId : profileIds) {
        const auto mapping = deploymentsByProfile_.find(profileId);
        const auto profile = registry_.profile(profileId);
        if (mapping == deploymentsByProfile_.end() || !profile) continue;
        const DeploymentId deployment = mapping->second;
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        if (!session || !stamp || session->profileId != profileId || !sessions_.isCurrent(*stamp)) continue;
        auto& generation = fileStorageRequestGenerations_[deployment.value];
        if (generation == std::numeric_limits<std::uint64_t>::max()) {
            fileStorageDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                QStringLiteral(": quota refresh generation exhausted");
            continue;
        }
        const std::uint64_t requestGeneration = ++generation;
        const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/files/storage"));
        if (!endpoint) {
            fileStorageDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                QStringLiteral(": invalid server address");
            continue;
        }
        QNetworkRequest request = apiRequest(*endpoint);
        request.setRawHeader("Authorization", QByteArray("Bearer ") +
                             QByteArray::fromStdString(session->bearerToken));
        QNetworkReply* reply = network_.get(request);
        reply->setReadBufferSize(maxFileJobResponseBytes);
        QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
        connect(reply, &QNetworkReply::downloadProgress, this,
                [reply](qint64 received, qint64) {
            if (received > maxFileJobResponseBytes) reply->abort();
        });
        fileStorageDataPending_.insert(deployment.value);
        refreshFileStorageStatus();
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, profileId, deployment, stamp = *stamp, requestGeneration] {
            const auto currentGeneration = fileStorageRequestGenerations_.find(deployment.value);
            if (!sessions_.isCurrent(stamp) || currentGeneration == fileStorageRequestGenerations_.end() ||
                currentGeneration->second != requestGeneration) {
                reply->deleteLater();
                return;
            }
            fileStorageDataPending_.erase(deployment.value);
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            const auto profile = registry_.profile(profileId);
            if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
                body.size() > maxFileJobResponseBytes || !profile) {
                fileStorageDataErrors_[deployment.value] =
                    (profile ? QString::fromStdString(profile->displayName)
                             : QString::fromStdString(profileId)) +
                    QStringLiteral(": ") + replyFailure(reply);
                if (statusCode == 401) invalidateProfileSessionLocally(profileId, deployment);
                refreshFileStorageStatus();
                reply->deleteLater();
                return;
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            const QJsonObject object = document.object();
            const auto readUnsigned = [&object](const QString& key, std::uint64_t& output) {
                const QJsonValue value = object.value(key);
                if (!value.isDouble()) return false;
                const double number = value.toDouble(-1);
                if (!std::isfinite(number) || std::floor(number) != number || number < 0 ||
                    number > 9007199254740991.0) return false;
                output = static_cast<std::uint64_t>(number);
                return true;
            };
            FileStorageSummaryRow summary;
            std::uint64_t deliveryBytes = 0;
            std::uint64_t requestBytes = 0;
            std::uint64_t activeFileJobLimit = 0;
            std::uint64_t activeRequestUploadLimit = 0;
            const bool valid = parseError.error == QJsonParseError::NoError && document.isObject() &&
                readUnsigned(QStringLiteral("payload_limit_bytes"), summary.payloadLimitBytes) &&
                readUnsigned(QStringLiteral("delivery_payload_bytes"), deliveryBytes) &&
                readUnsigned(QStringLiteral("request_payload_bytes"), requestBytes) &&
                readUnsigned(QStringLiteral("reserved_payload_bytes"), summary.reservedPayloadBytes) &&
                readUnsigned(QStringLiteral("available_payload_bytes"), summary.availablePayloadBytes) &&
                readUnsigned(QStringLiteral("active_file_jobs"), summary.activeFileJobs) &&
                readUnsigned(QStringLiteral("active_file_job_limit"), activeFileJobLimit) &&
                readUnsigned(QStringLiteral("active_request_uploads"), summary.activeRequestUploads) &&
                readUnsigned(QStringLiteral("active_request_upload_limit"), activeRequestUploadLimit) &&
                readUnsigned(QStringLiteral("retained_received_files"), summary.retainedReceivedFiles) &&
                readUnsigned(QStringLiteral("retained_received_bytes"), summary.retainedReceivedBytes) &&
                summary.payloadLimitBytes > 0 &&
                deliveryBytes <= std::numeric_limits<std::uint64_t>::max() - requestBytes &&
                summary.reservedPayloadBytes == deliveryBytes + requestBytes &&
                summary.availablePayloadBytes == (summary.reservedPayloadBytes >= summary.payloadLimitBytes
                    ? 0 : summary.payloadLimitBytes - summary.reservedPayloadBytes) &&
                summary.retainedReceivedBytes <= requestBytes &&
                activeFileJobLimit > 0 && activeRequestUploadLimit > 0;
            if (!valid) {
                fileStorageDataErrors_[deployment.value] = QString::fromStdString(profile->displayName) +
                    QStringLiteral(": invalid file quota summary");
                refreshFileStorageStatus();
                reply->deleteLater();
                return;
            }
            summary.activeFileJobLimit = activeFileJobLimit;
            summary.activeRequestUploadLimit = activeRequestUploadLimit;
            fileStorageByDeployment_[deployment.value] = summary;
            fileStorageDataErrors_.erase(deployment.value);
            refreshFileStorageStatus();
            reply->deleteLater();
        });
    }
    refreshFileStorageStatus();
}

void MainWindow::refreshFileStorageStatus()
{
    if (filesQuotaStatus_ == nullptr || filesServerFilter_ == nullptr) return;
    const QString selected = filesServerFilter_->currentData().toString();
    QStringList lines;
    bool pending = false;
    for (const ConnectionProfile& profile : registry_.profiles()) {
        if (!selected.isEmpty() && selected != QString::fromStdString(profile.profileId)) continue;
        const auto mapping = deploymentsByProfile_.find(profile.profileId);
        if (mapping == deploymentsByProfile_.end()) continue;
        const std::string& deployment = mapping->second.value;
        const auto summary = fileStorageByDeployment_.find(deployment);
        if (summary != fileStorageByDeployment_.end()) {
            const auto& value = summary->second;
            QString line = tr("%1: %2 / %3 reserved; %4 available; %5 / %6 active jobs; %7 / %8 request uploads; %9 retained files (%10)");
            line = line.arg(profile.displayName)
                .arg(formatFileSize(value.reservedPayloadBytes))
                .arg(formatFileSize(value.payloadLimitBytes))
                .arg(formatFileSize(value.availablePayloadBytes))
                .arg(QString::number(value.activeFileJobs))
                .arg(QString::number(value.activeFileJobLimit))
                .arg(QString::number(value.activeRequestUploads))
                .arg(QString::number(value.activeRequestUploadLimit))
                .arg(QString::number(value.retainedReceivedFiles))
                .arg(formatFileSize(value.retainedReceivedBytes));
            lines.append(line);
            const auto error = fileStorageDataErrors_.find(deployment);
            if (error != fileStorageDataErrors_.end()) lines.append(error->second);
            if (fileStorageDataPending_.contains(deployment)) pending = true;
            continue;
        }
        const auto error = fileStorageDataErrors_.find(deployment);
        if (error != fileStorageDataErrors_.end()) lines.append(error->second);
        if (fileStorageDataPending_.contains(deployment)) pending = true;
    }
    if (lines.isEmpty()) {
        filesQuotaStatus_->setText(pending
            ? tr("Refreshing logical file payload quota…")
            : tr("Sign in to a server to view its logical file payload quota."));
    } else {
        if (pending) lines.append(tr("Refreshing selected server quota…"));
        lines.append(tr("Logical payload only; database pages, backups, and filesystem usage are excluded."));
        filesQuotaStatus_->setText(lines.join(QStringLiteral("\n")));
    }
}

void MainWindow::clearDeploymentFileStorage(const DeploymentId& deployment)
{
    auto& generation = fileStorageRequestGenerations_[deployment.value];
    if (generation < std::numeric_limits<std::uint64_t>::max()) ++generation;
    fileStorageByDeployment_.erase(deployment.value);
    fileStorageDataErrors_.erase(deployment.value);
    fileStorageDataPending_.erase(deployment.value);
    refreshFileStorageStatus();
}

void MainWindow::refreshFileRequestTable()
{
    if (fileRequestTable_ == nullptr || filesServerFilter_ == nullptr) return;
    const QString selectedProfile = filesServerFilter_->currentData().toString();
    std::vector<const FileRequestRow*> visible;
    for (const FileRequestRow& row : fileRequests_) {
        if (selectedProfile.isEmpty() || row.profileId == selectedProfile.toStdString()) visible.push_back(&row);
    }
    std::sort(visible.begin(), visible.end(), [](const FileRequestRow* left, const FileRequestRow* right) {
        if (left->closesAtUnix != right->closesAtUnix) return left->closesAtUnix < right->closesAtUnix;
        if (left->serverName != right->serverName) return left->serverName < right->serverName;
        return left->id.serverEntityId < right->id.serverEntityId;
    });
    fileRequestTable_->setRowCount(static_cast<int>(visible.size()));
    for (std::size_t index = 0; index < visible.size(); ++index) {
        const FileRequestRow& row = *visible[index];
        const int tableRow = static_cast<int>(index);
        auto* server = new QTableWidgetItem(QString::fromStdString(row.serverName));
        server->setData(Qt::UserRole, QString::fromStdString(row.profileId));
        server->setData(Qt::UserRole + 1, QString::fromStdString(row.id.deployment.value));
        fileRequestTable_->setItem(tableRow, 0, server);
        fileRequestTable_->setItem(tableRow, 1, new QTableWidgetItem(QString::fromStdString(row.title)));
        fileRequestTable_->setItem(tableRow, 2, new QTableWidgetItem(QString::fromStdString(row.state)));
        fileRequestTable_->setItem(tableRow, 3, new QTableWidgetItem(QString::number(row.computerCount)));
        fileRequestTable_->setItem(tableRow, 4, new QTableWidgetItem(
            QDateTime::fromSecsSinceEpoch(row.closesAtUnix).toLocalTime().toString(Qt::ISODate)));
        fileRequestTable_->setItem(tableRow, 5, new QTableWidgetItem(QString::number(row.revision)));
        fileRequestTable_->setItem(tableRow, 6, new QTableWidgetItem(
            QString::fromStdString(row.id.serverEntityId)));
    }
    QStringList notes;
    std::optional<DeploymentId> selectedDeployment;
    if (!selectedProfile.isEmpty()) {
        const auto mapping = deploymentsByProfile_.find(selectedProfile.toStdString());
        if (mapping != deploymentsByProfile_.end()) selectedDeployment = mapping->second;
    }
    for (const auto& [deployment, error] : fileRequestDataErrors_) {
        if (!selectedProfile.isEmpty() && (!selectedDeployment || selectedDeployment->value != deployment)) continue;
        notes.append(error);
    }
    const bool pendingForSelection = selectedProfile.isEmpty()
        ? !fileRequestDataPending_.empty()
        : selectedDeployment && fileRequestDataPending_.contains(selectedDeployment->value);
    if (fileTabs_ == nullptr || fileTabs_->currentIndex() == 1) {
        if (!notes.isEmpty()) {
            filesStatus_->setText(
                tr("File request refresh needs attention: %1").arg(notes.join(QStringLiteral("; "))));
        } else if (pendingForSelection) {
            filesStatus_->setText(tr("Refreshing file requests from signed-in servers."));
        } else if (visible.empty()) {
            filesStatus_->setText(tr("No file requests are loaded for this selection."));
        } else {
            filesStatus_->setText(tr("Showing combined file requests. Each change is sent to its owning server."));
        }
    }
}

void MainWindow::saveFileRequestDraft()
{
    if (filesServerFilter_ == nullptr) return;
    const QString selected = filesServerFilter_->currentData().toString();
    if (selected.isEmpty()) {
        statusBar()->showMessage(tr("Choose one owning server before saving a file request."), 6000);
        return;
    }
    const std::string profileId = selected.toStdString();
    const auto mapping = deploymentsByProfile_.find(profileId);
    if (mapping == deploymentsByProfile_.end()) {
        statusBar()->showMessage(tr("The selected server is unavailable."), 6000);
        return;
    }
    const DeploymentId deployment = mapping->second;
    QStringList targetChoices;
    std::vector<std::string> targetIds;
    for (const ComputerRow& computer : computers_) {
        if (computer.id.deployment != deployment || computer.enrollmentState != "enrolled") continue;
        targetChoices.append(QStringLiteral("%1 (%2)").arg(
            QString::fromStdString(computer.friendlyName),
            QString::fromStdString(computer.id.serverComputerId)));
        targetIds.push_back(computer.id.serverComputerId);
    }
    if (targetChoices.isEmpty()) {
        statusBar()->showMessage(tr("No enrolled computers are loaded for this server."), 6000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Save File Request Draft"));
    dialog.setMinimumSize(600, 620);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* title = new QLineEdit(&dialog);
    title->setObjectName(QStringLiteral("fileRequestTitle"));
    title->setMaxLength(120);
    auto* instructions = new QPlainTextEdit(&dialog);
    instructions->setObjectName(QStringLiteral("fileRequestInstructions"));
    instructions->setMaximumHeight(100);
    auto* extensions = new QLineEdit(QStringLiteral("pdf, docx, txt"), &dialog);
    extensions->setObjectName(QStringLiteral("fileRequestExtensions"));
    auto* fileCount = new QSpinBox(&dialog);
    fileCount->setObjectName(QStringLiteral("fileRequestMaxFileCount"));
    fileCount->setRange(1, 50);
    auto* totalMiB = new QSpinBox(&dialog);
    totalMiB->setObjectName(QStringLiteral("fileRequestMaxTotalMiB"));
    totalMiB->setRange(1, 128);
    totalMiB->setValue(32);
    auto* closesAt = new QDateTimeEdit(QDateTime::currentDateTime().addDays(7), &dialog);
    closesAt->setObjectName(QStringLiteral("fileRequestClosesAt"));
    closesAt->setCalendarPopup(true);
    closesAt->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    closesAt->setDateTimeRange(QDateTime::currentDateTime().addSecs(60),
                               QDateTime::currentDateTime().addDays(30));
    auto* replacement = new QCheckBox(tr("Allow a user to replace their own Received submission"), &dialog);
    replacement->setObjectName(QStringLiteral("fileRequestAllowReplacement"));
    form->addRow(tr("Title"), title);
    form->addRow(tr("Instructions"), instructions);
    form->addRow(tr("Allowed extensions"), extensions);
    form->addRow(tr("Maximum files per user"), fileCount);
    form->addRow(tr("Maximum total size (MiB)"), totalMiB);
    form->addRow(tr("Close deadline (local time)"), closesAt);
    form->addRow(QString(), replacement);
    layout->addLayout(form);
    layout->addWidget(subheading(
        tr("Choose enrolled computers on this server. Saving a draft sends nothing to endpoints."), &dialog));
    auto* targets = new QListWidget(&dialog);
    targets->setObjectName(QStringLiteral("fileRequestTargets"));
    targets->setSelectionMode(QAbstractItemView::NoSelection);
    for (int index = 0; index < targetChoices.size(); ++index) {
        auto* item = new QListWidgetItem(targetChoices.at(index), targets);
        item->setData(Qt::UserRole, QString::fromStdString(targetIds[static_cast<std::size_t>(index)]));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    layout->addWidget(targets, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save Draft"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, title, instructions, extensions, targets] {
        const QString requestTitle = title->text().trimmed();
        const QString ruleText = extensions->text().trimmed();
        const QStringList requestedExtensions = ruleText.split(QLatin1Char(','), Qt::KeepEmptyParts);
        QSet<QString> uniqueExtensions;
        bool validExtensions = !requestedExtensions.isEmpty();
        for (QString extension : requestedExtensions) {
            extension = extension.trimmed().toLower();
            if (extension.startsWith(QLatin1Char('.'))) extension.remove(0, 1);
            if (extension.isEmpty() || uniqueExtensions.contains(extension)) validExtensions = false;
            uniqueExtensions.insert(extension);
        }
        if (!validWireText(requestTitle, 120) || requestTitle.isEmpty()) {
            QMessageBox::warning(&dialog, QObject::tr("Check Request"),
                                 QObject::tr("Enter a title of at most 120 characters."));
            return;
        }
        if (instructions->toPlainText().size() > 2000 || !validExtensions) {
            QMessageBox::warning(&dialog, QObject::tr("Check Request"),
                                 QObject::tr("Instructions must be at most 2000 characters and at least one extension is required."));
            return;
        }
        bool hasTarget = false;
        for (int row = 0; row < targets->count(); ++row) {
            hasTarget = hasTarget || targets->item(row)->checkState() == Qt::Checked;
        }
        if (!hasTarget) {
            QMessageBox::warning(&dialog, QObject::tr("Choose Targets"),
                                 QObject::tr("Choose at least one enrolled computer."));
            return;
        }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return;

    const QString requestTitle = title->text().trimmed();
    const QString requestInstructions = instructions->toPlainText();
    QStringList extensionList = extensions->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    QJsonArray extensionValues;
    for (QString extension : extensionList) {
        extension = extension.trimmed().toLower();
        if (extension.startsWith(QLatin1Char('.'))) extension.remove(0, 1);
        extensionValues.append(extension);
    }
    QJsonArray computerValues;
    std::size_t selectedCount = 0;
    for (int row = 0; row < targets->count(); ++row) {
        const QListWidgetItem* item = targets->item(row);
        if (item->checkState() != Qt::Checked) continue;
        const QString computerId = item->data(Qt::UserRole).toString();
        if (!validUuid(computerId)) {
            statusBar()->showMessage(tr("The selected computer list contains an invalid ID."), 7000);
            return;
        }
        computerValues.append(computerId);
        ++selectedCount;
    }
    const qint64 closesAtUnix = closesAt->dateTime().toSecsSinceEpoch();
    const QJsonObject body{
        {QStringLiteral("title"), requestTitle},
        {QStringLiteral("instructions"), requestInstructions},
        {QStringLiteral("computer_ids"), computerValues},
        {QStringLiteral("allowed_extensions"), extensionValues},
        {QStringLiteral("max_file_count"), fileCount->value()},
        {QStringLiteral("max_total_size_bytes"), static_cast<qint64>(totalMiB->value()) * 1024 * 1024},
        {QStringLiteral("closes_at_unix"), static_cast<qint64>(closesAtUnix)},
        {QStringLiteral("replacement_allowed"), replacement->isChecked()},
    };
    const QString expectedTitle = requestTitle;
    statusBar()->showMessage(tr("Saving a draft for %1 computers on this server…")
                                 .arg(static_cast<qulonglong>(selectedCount)), 0);
    withFileRequestsAvailable(profileId, [this, profileId, body, expectedTitle] {
        const auto mapping = deploymentsByProfile_.find(profileId);
        const auto profile = registry_.profile(profileId);
        if (mapping == deploymentsByProfile_.end() || !profile) return;
        const DeploymentId deployment = mapping->second;
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/files/requests"));
        if (!session || !stamp || !sessions_.isCurrent(*stamp) || !endpoint) return;
        const std::string expectedOrigin = profile->endpoint;
        QNetworkRequest request = apiRequest(*endpoint);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Authorization", QByteArray("Bearer ") +
                             QByteArray::fromStdString(session->bearerToken));
        QNetworkReply* reply = network_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        reply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
        connect(reply, &QNetworkReply::downloadProgress, this,
                [reply](qint64 received, qint64) { if (received > maxHealthResponseBytes) reply->abort(); });
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, profileId, deployment, stamp = *stamp, expectedOrigin, expectedTitle] {
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray responseBody = reply->readAll();
            const auto currentProfile = registry_.profile(profileId);
            const QString currentSelection = filesServerFilter_ == nullptr
                ? QString{} : filesServerFilter_->currentData().toString();
            if (!sessions_.isCurrent(stamp) || filesServerFilter_ == nullptr ||
                (!currentSelection.isEmpty() && currentSelection != QString::fromStdString(profileId)) ||
                !currentProfile || currentProfile->endpoint != expectedOrigin) {
                reply->deleteLater();
                return;
            }
            if (reply->error() != QNetworkReply::NoError || statusCode != 201 ||
                responseBody.size() > maxHealthResponseBytes) {
                if (statusCode == 401) invalidateProfileSessionLocally(profileId, deployment);
                statusBar()->showMessage(tr("Could not save file request draft: %1").arg(replyFailure(reply)), 9000);
                reply->deleteLater();
                return;
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
            const QJsonObject draft = document.object();
            const QString id = draft.value(QStringLiteral("id")).toString();
            const bool validReceipt = parseError.error == QJsonParseError::NoError && document.isObject() &&
                validUuid(id) && draft.value(QStringLiteral("state")).toString() == QStringLiteral("draft") &&
                draft.value(QStringLiteral("title")).toString() == expectedTitle &&
                draft.value(QStringLiteral("revision")).toInt(-1) == 1;
            reply->deleteLater();
            if (!validReceipt) {
                statusBar()->showMessage(tr("The server returned an invalid draft receipt."), 8000);
                refreshFileRequests();
                return;
            }
            statusBar()->showMessage(tr("Draft saved. Review it, then publish when ready."), 6000);
            refreshFileRequests();
        });
    });
}

void MainWindow::publishSelectedFileRequest()
{
    if (fileRequestTable_ == nullptr || fileRequestTable_->currentRow() < 0) {
        statusBar()->showMessage(tr("Select a file request draft first."), 5000);
        return;
    }
    const int rowIndex = fileRequestTable_->currentRow();
    const QString profileId = fileRequestTable_->item(rowIndex, 0)->data(Qt::UserRole).toString();
    const QString deploymentId = fileRequestTable_->item(rowIndex, 0)->data(Qt::UserRole + 1).toString();
    const QString requestId = fileRequestTable_->item(rowIndex, 6)->text();
    const auto request = std::find_if(fileRequests_.begin(), fileRequests_.end(),
        [&](const FileRequestRow& item) {
            return item.id.serverEntityId == requestId.toStdString() &&
                   item.id.deployment.value == deploymentId.toStdString() &&
                   item.profileId == profileId.toStdString();
        });
    if (request == fileRequests_.end() || request->state != "draft") {
        statusBar()->showMessage(tr("Only a saved draft can be published."), 5000);
        return;
    }
    const std::string selectedProfile = request->profileId;
    const std::string id = request->id.serverEntityId;
    const std::int64_t expectedRevision = request->revision;
    const QString title = QString::fromStdString(request->title);
    const QString serverName = QString::fromStdString(request->serverName);
    const qulonglong computerCount = static_cast<qulonglong>(request->computerCount);
    QMessageBox confirmation(this);
    confirmation.setWindowTitle(tr("Publish File Request"));
    confirmation.setTextFormat(Qt::PlainText);
    confirmation.setText(tr("Publish “%1” to %2 computers on %3? Eligible users can submit files until the request closes.")
        .arg(title)
        .arg(computerCount)
        .arg(serverName));
    confirmation.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    confirmation.setDefaultButton(QMessageBox::Cancel);
    if (confirmation.exec() != QMessageBox::Yes) return;

    withFileRequestsAvailable(selectedProfile, [this, selectedProfile, id, expectedRevision] {
        const auto mapping = deploymentsByProfile_.find(selectedProfile);
        const auto profile = registry_.profile(selectedProfile);
        if (mapping == deploymentsByProfile_.end() || !profile) return;
        const DeploymentId deployment = mapping->second;
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        const auto endpoint = apiEndpoint(*profile,
            QStringLiteral("/v2/files/requests/%1/publish").arg(QString::fromStdString(id)));
        if (!session || !stamp || !sessions_.isCurrent(*stamp) || !endpoint) return;
        const std::string expectedOrigin = profile->endpoint;
        QNetworkRequest request = apiRequest(*endpoint);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Authorization", QByteArray("Bearer ") +
                             QByteArray::fromStdString(session->bearerToken));
        const QByteArray payload = QJsonDocument(QJsonObject{
            {QStringLiteral("expected_revision"), static_cast<qint64>(expectedRevision)},
        }).toJson(QJsonDocument::Compact);
        QNetworkReply* reply = network_.post(request, payload);
        reply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, selectedProfile, deployment, stamp = *stamp, expectedOrigin, id, expectedRevision] {
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            const auto currentProfile = registry_.profile(selectedProfile);
            const QString currentSelection = filesServerFilter_ == nullptr
                ? QString{} : filesServerFilter_->currentData().toString();
            if (!sessions_.isCurrent(stamp) || filesServerFilter_ == nullptr ||
                (!currentSelection.isEmpty() && currentSelection != QString::fromStdString(selectedProfile)) ||
                !currentProfile || currentProfile->endpoint != expectedOrigin) {
                reply->deleteLater();
                return;
            }
            if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
                body.size() > maxHealthResponseBytes) {
                if (statusCode == 401) invalidateProfileSessionLocally(selectedProfile, deployment);
                statusBar()->showMessage(tr("Could not publish file request: %1").arg(replyFailure(reply)), 9000);
                reply->deleteLater();
                refreshFileRequests();
                return;
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            const QJsonObject published = document.object();
            const bool validReceipt = parseError.error == QJsonParseError::NoError && document.isObject() &&
                published.value(QStringLiteral("id")).toString() == QString::fromStdString(id) &&
                published.value(QStringLiteral("state")).toString() == QStringLiteral("open") &&
                published.value(QStringLiteral("revision")).toInt(-1) == expectedRevision + 1;
            reply->deleteLater();
            if (!validReceipt) {
                statusBar()->showMessage(tr("The server returned an invalid publication receipt."), 8000);
            } else {
                statusBar()->showMessage(tr("File request published."), 5000);
            }
            refreshFileRequests();
        });
    });
}

void MainWindow::closeSelectedFileRequest()
{
    if (fileRequestTable_ == nullptr || fileRequestTable_->currentRow() < 0) {
        statusBar()->showMessage(tr("Select an open file request first."), 5000);
        return;
    }
    const int rowIndex = fileRequestTable_->currentRow();
    const QString profileId = fileRequestTable_->item(rowIndex, 0)->data(Qt::UserRole).toString();
    const QString deploymentId = fileRequestTable_->item(rowIndex, 0)->data(Qt::UserRole + 1).toString();
    const QString requestId = fileRequestTable_->item(rowIndex, 6)->text();
    const auto request = std::find_if(fileRequests_.begin(), fileRequests_.end(),
        [&](const FileRequestRow& item) {
            return item.id.serverEntityId == requestId.toStdString() &&
                   item.id.deployment.value == deploymentId.toStdString() &&
                   item.profileId == profileId.toStdString();
        });
    if (request == fileRequests_.end() || request->state != "open") {
        statusBar()->showMessage(tr("Only an open file request can be closed."), 5000);
        return;
    }
    const std::string selectedProfile = request->profileId;
    const std::string id = request->id.serverEntityId;
    const std::int64_t expectedRevision = request->revision;
    const QString title = QString::fromStdString(request->title);
    const QString serverName = QString::fromStdString(request->serverName);
    bool accepted = false;
    const QString typedId = QInputDialog::getText(
        this, tr("Confirm Request Closure"),
        tr("Type the exact request ID to close it:\n%1").arg(requestId),
        QLineEdit::Normal, QString(), &accepted);
    if (!accepted || typedId != requestId) return;
    QMessageBox confirmation(this);
    confirmation.setWindowTitle(tr("Close File Request"));
    confirmation.setTextFormat(Qt::PlainText);
    confirmation.setText(tr("Close “%1” on %2? New submissions will stop. Received files remain available for review.")
        .arg(title, serverName));
    confirmation.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    confirmation.setDefaultButton(QMessageBox::Cancel);
    if (confirmation.exec() != QMessageBox::Yes) return;

    withFileRequestsAvailable(selectedProfile, [this, selectedProfile, id, expectedRevision] {
        const auto mapping = deploymentsByProfile_.find(selectedProfile);
        const auto profile = registry_.profile(selectedProfile);
        if (mapping == deploymentsByProfile_.end() || !profile) return;
        const DeploymentId deployment = mapping->second;
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        const auto endpoint = apiEndpoint(*profile,
            QStringLiteral("/v2/files/requests/%1/close").arg(QString::fromStdString(id)));
        if (!session || !stamp || !sessions_.isCurrent(*stamp) || !endpoint) return;
        const std::string expectedOrigin = profile->endpoint;
        QNetworkRequest request = apiRequest(*endpoint);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Authorization", QByteArray("Bearer ") +
                             QByteArray::fromStdString(session->bearerToken));
        const QByteArray payload = QJsonDocument(QJsonObject{
            {QStringLiteral("expected_revision"), static_cast<qint64>(expectedRevision)},
            {QStringLiteral("confirm_request_id"), QString::fromStdString(id)},
        }).toJson(QJsonDocument::Compact);
        QNetworkReply* reply = network_.post(request, payload);
        reply->setReadBufferSize(maxHealthResponseBytes);
        QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, selectedProfile, deployment, stamp = *stamp, expectedOrigin, id, expectedRevision] {
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            const auto currentProfile = registry_.profile(selectedProfile);
            const QString currentSelection = filesServerFilter_ == nullptr
                ? QString{} : filesServerFilter_->currentData().toString();
            if (!sessions_.isCurrent(stamp) || filesServerFilter_ == nullptr ||
                (!currentSelection.isEmpty() && currentSelection != QString::fromStdString(selectedProfile)) ||
                !currentProfile || currentProfile->endpoint != expectedOrigin) {
                reply->deleteLater();
                return;
            }
            if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
                body.size() > maxHealthResponseBytes) {
                if (statusCode == 401) invalidateProfileSessionLocally(selectedProfile, deployment);
                statusBar()->showMessage(tr("Could not close file request: %1").arg(replyFailure(reply)), 9000);
                reply->deleteLater();
                refreshFileRequests();
                return;
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            const QJsonObject closed = document.object();
            const bool validReceipt = parseError.error == QJsonParseError::NoError && document.isObject() &&
                closed.value(QStringLiteral("id")).toString() == QString::fromStdString(id) &&
                closed.value(QStringLiteral("state")).toString() == QStringLiteral("closed") &&
                closed.value(QStringLiteral("revision")).toInt(-1) == expectedRevision + 1;
            reply->deleteLater();
            if (!validReceipt) {
                statusBar()->showMessage(tr("The server returned an invalid closure receipt."), 8000);
            } else {
                statusBar()->showMessage(tr("File request closed. Received files remain retained."), 6000);
            }
            refreshFileRequests();
        });
    });
}

void MainWindow::reviewFileRequestSubmissions()
{
    if (fileRequestTable_ == nullptr || fileRequestTable_->currentRow() < 0) {
        statusBar()->showMessage(tr("Select a file request first."), 5000);
        return;
    }
    const int rowIndex = fileRequestTable_->currentRow();
    const QString profileId = fileRequestTable_->item(rowIndex, 0)->data(Qt::UserRole).toString();
    const QString deploymentId = fileRequestTable_->item(rowIndex, 0)->data(Qt::UserRole + 1).toString();
    const QString requestId = fileRequestTable_->item(rowIndex, 6)->text();
    const auto request = std::find_if(fileRequests_.begin(), fileRequests_.end(),
        [&](const FileRequestRow& item) {
            return item.id.serverEntityId == requestId.toStdString() &&
                   item.id.deployment.value == deploymentId.toStdString() &&
                   item.profileId == profileId.toStdString();
        });
    if (request == fileRequests_.end()) {
        statusBar()->showMessage(tr("The selected file request is no longer loaded."), 5000);
        return;
    }
    const std::string selectedProfile = request->profileId;
    const std::string requestIdValue = request->id.serverEntityId;
    withFileRequestsAvailable(selectedProfile, [this, selectedProfile, requestIdValue] {
        const auto mapping = deploymentsByProfile_.find(selectedProfile);
        const auto profile = registry_.profile(selectedProfile);
        if (mapping == deploymentsByProfile_.end() || !profile) return;
        const DeploymentId deployment = mapping->second;
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        const auto endpoint = apiEndpoint(*profile,
            QStringLiteral("/v2/files/requests/%1/submissions").arg(QString::fromStdString(requestIdValue)));
        if (!session || !stamp || !sessions_.isCurrent(*stamp) || !endpoint) return;
        const std::string expectedOrigin = profile->endpoint;
        QNetworkRequest networkRequest = apiRequest(*endpoint);
        networkRequest.setRawHeader("Authorization", QByteArray("Bearer ") +
                                    QByteArray::fromStdString(session->bearerToken));
        QNetworkReply* reply = network_.get(networkRequest);
        reply->setReadBufferSize(maxFileJobResponseBytes);
        QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
        connect(reply, &QNetworkReply::downloadProgress, this,
                [reply](qint64 received, qint64) {
                    if (received > maxFileJobResponseBytes) reply->abort();
                });
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, selectedProfile, requestIdValue, deployment,
                 stamp = *stamp, expectedOrigin] {
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            const auto currentProfile = registry_.profile(selectedProfile);
            const QString currentSelection = filesServerFilter_ == nullptr
                ? QString{} : filesServerFilter_->currentData().toString();
            if (!sessions_.isCurrent(stamp) || filesServerFilter_ == nullptr ||
                (!currentSelection.isEmpty() && currentSelection != QString::fromStdString(selectedProfile)) ||
                !currentProfile || currentProfile->endpoint != expectedOrigin) {
                reply->deleteLater();
                return;
            }
            if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
                body.size() > maxFileJobResponseBytes) {
                if (statusCode == 401) invalidateProfileSessionLocally(selectedProfile, deployment);
                statusBar()->showMessage(tr("Could not load received files: %1").arg(replyFailure(reply)), 9000);
                reply->deleteLater();
                return;
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isArray() || document.array().size() > 100) {
                statusBar()->showMessage(tr("The server returned an invalid submission list."), 8000);
                reply->deleteLater();
                return;
            }
            struct DownloadChoice {
                QString label;
                std::string submissionId;
                std::string fileId;
                std::string fileName;
                std::string sha256;
                std::uint64_t size = 0;
                std::uint32_t chunks = 0;
            };
            std::vector<DownloadChoice> choices;
            for (const QJsonValue& submissionValue : document.array()) {
                if (!submissionValue.isObject()) continue;
                const QJsonObject submission = submissionValue.toObject();
                const QString submissionId = submission.value(QStringLiteral("id")).toString();
                const QString returnedRequestId = submission.value(QStringLiteral("request_id")).toString();
                const QString computerId = submission.value(QStringLiteral("computer_id")).toString();
                const QString userName = submission.value(QStringLiteral("local_user_display_name")).toString();
                const QString state = submission.value(QStringLiteral("state")).toString();
                const QJsonValue filesValue = submission.value(QStringLiteral("files"));
                if (!validUuid(submissionId) || returnedRequestId != QString::fromStdString(requestIdValue) ||
                    !validUuid(computerId) ||
                    !validAccountDisplayLabel(userName) || !filesValue.isArray() ||
                    filesValue.toArray().size() > 50 ||
                    (state != QStringLiteral("uploading") && state != QStringLiteral("received") &&
                     state != QStringLiteral("failed") && state != QStringLiteral("expired") &&
                     state != QStringLiteral("superseded"))) {
                    statusBar()->showMessage(tr("The server returned invalid submission metadata."), 8000);
                    reply->deleteLater();
                    return;
                }
                const auto computer = std::find_if(computers_.begin(), computers_.end(),
                    [&](const ComputerRow& item) {
                        return item.id.deployment == deployment &&
                            item.id.serverComputerId == computerId.toStdString();
                    });
                const QString computerLabel = computer == computers_.end()
                    ? computerId : QString::fromStdString(computer->friendlyName);
                for (const QJsonValue& fileValue : filesValue.toArray()) {
                    if (!fileValue.isObject()) continue;
                    const QJsonObject file = fileValue.toObject();
                    const QString fileId = file.value(QStringLiteral("id")).toString();
                    const QString fileName = file.value(QStringLiteral("file_name")).toString();
                    const QString fileState = file.value(QStringLiteral("state")).toString();
                    const QString digest = file.value(QStringLiteral("sha256")).toString();
                    const QJsonValue sizeValue = file.value(QStringLiteral("file_size_bytes"));
                    const QJsonValue chunkSizeValue = file.value(QStringLiteral("chunk_size_bytes"));
                    const QJsonValue chunkCountValue = file.value(QStringLiteral("chunk_count"));
                    const double rawSize = sizeValue.toDouble(-1);
                    const double rawChunkSize = chunkSizeValue.toDouble(-1);
                    const double rawChunkCount = chunkCountValue.toDouble(-1);
                    const QByteArray digestBytes = digest.toLatin1();
                    const QFileInfo fileNameInfo(fileName);
                    QString deviceStem = fileName.section(QLatin1Char('.'), 0, 0);
                    while (deviceStem.endsWith(QLatin1Char(' ')) ||
                           deviceStem.endsWith(QLatin1Char('.'))) {
                        deviceStem.chop(1);
                    }
                    deviceStem = deviceStem.toUpper();
                    const bool reservedWindowsName = deviceStem == QStringLiteral("CON") ||
                        deviceStem == QStringLiteral("PRN") || deviceStem == QStringLiteral("AUX") ||
                        deviceStem == QStringLiteral("NUL") ||
                        (deviceStem.size() == 4 &&
                         (deviceStem.startsWith(QStringLiteral("COM")) ||
                          deviceStem.startsWith(QStringLiteral("LPT"))) &&
                         deviceStem.at(3) >= QLatin1Char('1') && deviceStem.at(3) <= QLatin1Char('9'));
                    const bool validDigest = digestBytes.size() == 64 && std::all_of(
                        digestBytes.cbegin(), digestBytes.cend(), [](char byte) {
                            return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
                        });
                    const bool validSize = sizeValue.isDouble() && std::isfinite(rawSize) &&
                        std::floor(rawSize) == rawSize && rawSize >= 0 && rawSize <= maxManagerFileBytes;
                    const qint64 expectedChunks = validSize && rawSize > 0
                        ? (static_cast<qint64>(rawSize) + managerFileChunkBytes - 1) / managerFileChunkBytes
                        : 0;
                    const bool safeFileName = validWireText(fileName, 255) &&
                        fileName.trimmed() == fileName && fileName != QStringLiteral(".") &&
                        fileName != QStringLiteral("..") && !fileName.endsWith(QLatin1Char('.')) &&
                        !fileName.endsWith(QLatin1Char(' ')) && !fileName.contains(QLatin1Char('/')) &&
                        !fileName.contains(QLatin1Char('\\')) && !fileName.contains(QLatin1Char(':')) &&
                        fileNameInfo.fileName() == fileName && !reservedWindowsName;
                    if (!validUuid(fileId) || !safeFileName || !validDigest || !validSize ||
                        !chunkSizeValue.isDouble() || !std::isfinite(rawChunkSize) ||
                        rawChunkSize != managerFileChunkBytes ||
                        !chunkCountValue.isDouble() || !std::isfinite(rawChunkCount) ||
                        std::floor(rawChunkCount) != rawChunkCount || rawChunkCount != expectedChunks ||
                        rawChunkCount > 512 || (fileState != QStringLiteral("uploading") &&
                                               fileState != QStringLiteral("received") &&
                                               fileState != QStringLiteral("deleted"))) {
                        statusBar()->showMessage(tr("The server returned invalid received-file metadata."), 8000);
                        reply->deleteLater();
                        return;
                    }
                    if (state == QStringLiteral("received") && fileState == QStringLiteral("received")) {
                        choices.push_back(DownloadChoice{
                            tr("%1 · %2 · %3 · %4")
                                .arg(fileName, formatFileSize(static_cast<std::uint64_t>(rawSize)),
                                     userName, computerLabel),
                            submissionId.toStdString(), fileId.toStdString(), fileName.toStdString(),
                            digest.toStdString(), static_cast<std::uint64_t>(rawSize),
                            static_cast<std::uint32_t>(rawChunkCount)
                        });
                    }
                }
            }
            reply->deleteLater();
            if (choices.empty()) {
                statusBar()->showMessage(tr("No Received files are available for this request yet."), 6000);
                return;
            }
            QDialog chooser(this);
            chooser.setWindowTitle(tr("Choose a Received File"));
            auto* chooserLayout = new QVBoxLayout(&chooser);
            chooserLayout->addWidget(subheading(
                tr("Choose a received file to save. Files are downloaded only after this explicit selection."),
                &chooser));
            auto* list = new QListWidget(&chooser);
            list->setObjectName(QStringLiteral("receivedFileChoices"));
            for (std::size_t index = 0; index < choices.size(); ++index) {
                auto* item = new QListWidgetItem(choices[index].label, list);
                item->setData(Qt::UserRole, static_cast<qulonglong>(index));
            }
            list->setCurrentRow(0);
            chooserLayout->addWidget(list, 1);
            auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &chooser);
            buttons->button(QDialogButtonBox::Save)->setText(tr("Choose Save Location"));
            chooserLayout->addWidget(buttons);
            connect(buttons, &QDialogButtonBox::accepted, &chooser, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &chooser, &QDialog::reject);
            if (chooser.exec() != QDialog::Accepted || list->currentRow() < 0) return;
            const std::size_t choiceIndex = static_cast<std::size_t>(
                list->currentItem()->data(Qt::UserRole).toULongLong());
            if (choiceIndex >= choices.size()) return;
            const DownloadChoice choice = choices[choiceIndex];
            const QString path = QFileDialog::getSaveFileName(
                this, tr("Save Received File"), QString::fromStdString(choice.fileName));
            if (path.isEmpty()) return;
            auto output = std::make_shared<QSaveFile>(path);
            output->setDirectWriteFallback(false);
            if (!output->open(QIODevice::WriteOnly)) {
                QMessageBox::warning(this, tr("Cannot Save File"),
                                     tr("The selected destination could not be opened safely."));
                return;
            }
            const auto verifiedDownloadProfile = registry_.profile(selectedProfile);
            const auto* currentSession = sessions_.session(deployment);
            const auto currentStamp = sessions_.currentStamp(deployment);
            if (!verifiedDownloadProfile || !currentSession || !currentStamp ||
                currentSession->profileId != selectedProfile ||
                currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() ||
                verifiedDownloadProfile->endpoint != expectedOrigin || !sessions_.isCurrent(*currentStamp)) {
                output->cancelWriting();
                statusBar()->showMessage(tr("The server session changed before file download."), 6000);
                return;
            }
            if (currentStamp->deployment != stamp.deployment || currentStamp->generation != stamp.generation) {
                output->cancelWriting();
                statusBar()->showMessage(tr("The server session changed before file download."), 6000);
                return;
            }
            const std::string downloadOrigin = expectedOrigin;
            auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
            statusBar()->showMessage(tr("Downloading %1…").arg(QString::fromStdString(choice.fileName)), 0);
            downloadFileRequestChunk(selectedProfile, deployment, *currentStamp,
                requestIdValue, choice.submissionId, choice.fileId, downloadOrigin, output, hash,
                choice.size, choice.sha256, choice.chunks, 0);
        });
    });
}

void MainWindow::deleteSelectedFileRequestSubmissionContent()
{
    if (fileRequestTable_ == nullptr || fileRequestTable_->currentRow() < 0) {
        statusBar()->showMessage(tr("Select a file request first."), 5000);
        return;
    }
    const int rowIndex = fileRequestTable_->currentRow();
    const QString profileId = fileRequestTable_->item(rowIndex, 0)->data(Qt::UserRole).toString();
    const QString deploymentId = fileRequestTable_->item(rowIndex, 0)->data(Qt::UserRole + 1).toString();
    const QString requestId = fileRequestTable_->item(rowIndex, 6)->text();
    const auto selectedRequest = std::find_if(fileRequests_.begin(), fileRequests_.end(),
        [&](const FileRequestRow& item) {
            return item.id.serverEntityId == requestId.toStdString() &&
                   item.id.deployment.value == deploymentId.toStdString() &&
                   item.profileId == profileId.toStdString();
        });
    if (selectedRequest == fileRequests_.end()) {
        statusBar()->showMessage(tr("The selected file request is no longer loaded."), 5000);
        return;
    }

    const std::string selectedProfile = selectedRequest->profileId;
    const std::string requestIdValue = selectedRequest->id.serverEntityId;
    withFileRequestsAvailable(selectedProfile, [this, selectedProfile, requestIdValue] {
        const auto mapping = deploymentsByProfile_.find(selectedProfile);
        const auto profile = registry_.profile(selectedProfile);
        if (mapping == deploymentsByProfile_.end() || !profile) return;
        const DeploymentId deployment = mapping->second;
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        const auto endpoint = apiEndpoint(*profile,
            QStringLiteral("/v2/files/requests/%1/submissions")
                .arg(QString::fromStdString(requestIdValue)));
        if (!session || !stamp || !sessions_.isCurrent(*stamp) || !endpoint) return;

        const std::string expectedOrigin = profile->endpoint;
        QNetworkRequest request = apiRequest(*endpoint);
        request.setRawHeader("Authorization", QByteArray("Bearer ") +
                             QByteArray::fromStdString(session->bearerToken));
        QNetworkReply* reply = network_.get(request);
        reply->setReadBufferSize(maxFileJobResponseBytes);
        QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
        connect(reply, &QNetworkReply::downloadProgress, this,
                [reply](qint64 received, qint64) {
                    if (received > maxFileJobResponseBytes) reply->abort();
                });
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, selectedProfile, deployment, stamp = *stamp, expectedOrigin,
                 requestIdValue] {
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            const auto currentProfile = registry_.profile(selectedProfile);
            const QString currentSelection = filesServerFilter_ == nullptr
                ? QString{} : filesServerFilter_->currentData().toString();
            if (!sessions_.isCurrent(stamp) || filesServerFilter_ == nullptr ||
                (!currentSelection.isEmpty() && currentSelection != QString::fromStdString(selectedProfile)) ||
                !currentProfile || currentProfile->endpoint != expectedOrigin) {
                reply->deleteLater();
                return;
            }
            if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
                body.size() > maxFileJobResponseBytes) {
                if (statusCode == 401) invalidateProfileSessionLocally(selectedProfile, deployment);
                statusBar()->showMessage(tr("Could not load submissions: %1").arg(replyFailure(reply)), 9000);
                reply->deleteLater();
                return;
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isArray() ||
                document.array().size() > 100) {
                statusBar()->showMessage(tr("The server returned invalid submission metadata."), 8000);
                reply->deleteLater();
                return;
            }
            struct SubmissionChoice {
                QString label;
                QString id;
            };
            std::vector<SubmissionChoice> choices;
            for (const QJsonValue& value : document.array()) {
                if (!value.isObject()) {
                    statusBar()->showMessage(tr("The server returned invalid submission metadata."), 8000);
                    reply->deleteLater();
                    return;
                }
                const QJsonObject submission = value.toObject();
                const QString submissionId = submission.value(QStringLiteral("id")).toString();
                const QString returnedRequestId = submission.value(QStringLiteral("request_id")).toString();
                const QString localUser = submission.value(QStringLiteral("local_user_display_name")).toString();
                const QString state = submission.value(QStringLiteral("state")).toString();
                const QJsonValue filesValue = submission.value(QStringLiteral("files"));
                if (!validUuid(submissionId) || returnedRequestId != QString::fromStdString(requestIdValue) ||
                    !validAccountDisplayLabel(localUser) || !filesValue.isArray() ||
                    filesValue.toArray().size() > 50 ||
                    (state != QStringLiteral("uploading") && state != QStringLiteral("received") &&
                     state != QStringLiteral("failed") && state != QStringLiteral("expired") &&
                     state != QStringLiteral("superseded"))) {
                    statusBar()->showMessage(tr("The server returned invalid submission metadata."), 8000);
                    reply->deleteLater();
                    return;
                }
                std::size_t fileCount = 0;
                for (const QJsonValue& fileValue : filesValue.toArray()) {
                    if (!fileValue.isObject()) {
                        statusBar()->showMessage(tr("The server returned invalid file metadata."), 8000);
                        reply->deleteLater();
                        return;
                    }
                    const QString fileState = fileValue.toObject().value(QStringLiteral("state")).toString();
                    if (fileState != QStringLiteral("uploading") &&
                        fileState != QStringLiteral("received") &&
                        fileState != QStringLiteral("deleted")) {
                        statusBar()->showMessage(tr("The server returned invalid file metadata."), 8000);
                        reply->deleteLater();
                        return;
                    }
                    if (fileState == QStringLiteral("received")) ++fileCount;
                }
                if (state == QStringLiteral("received")) {
                    const QString contentLabel = fileCount == 0
                        ? tr("content already removed")
                        : tr("%1 file(s)").arg(static_cast<qulonglong>(fileCount));
                    choices.push_back({
                        tr("%1 · %2 · %3")
                            .arg(localUser, contentLabel, submissionId),
                        submissionId,
                    });
                }
            }
            reply->deleteLater();
            if (choices.empty()) {
                statusBar()->showMessage(tr("This request has no received submissions to clean up."), 6000);
                return;
            }

            QStringList labels;
            for (const SubmissionChoice& choice : choices) labels.push_back(choice.label);
            bool selected = false;
            const QString label = QInputDialog::getItem(
                this, tr("Choose Submission"),
                tr("Select the received submission whose retained content should be removed:"),
                labels, 0, false, &selected);
            if (!selected) return;
            const auto choice = std::find_if(choices.begin(), choices.end(),
                [&](const SubmissionChoice& item) { return item.label == label; });
            if (choice == choices.end()) return;
            const QString submissionId = choice->id;
            bool accepted = false;
            const QString typedId = QInputDialog::getText(
                this, tr("Confirm Content Removal"),
                tr("Type the exact submission ID to remove its retained content:\n%1")
                    .arg(submissionId),
                QLineEdit::Normal, QString(), &accepted);
            if (!accepted || typedId != submissionId) return;

            QMessageBox confirmation(this);
            confirmation.setWindowTitle(tr("Remove Retained Content"));
            confirmation.setTextFormat(Qt::PlainText);
            confirmation.setText(tr("Remove the stored file content for this submission on %1? "
                                    "Submission and file metadata will remain. SQLite free pages and prior "
                                    "database backups may still retain the bytes.")
                                     .arg(currentProfile->displayName));
            confirmation.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
            confirmation.setDefaultButton(QMessageBox::Cancel);
            if (confirmation.exec() != QMessageBox::Yes) return;

            const auto latestProfile = registry_.profile(selectedProfile);
            const auto* currentSession = sessions_.session(deployment);
            const auto currentStamp = sessions_.currentStamp(deployment);
            std::optional<QUrl> currentEndpoint;
            if (latestProfile) {
                currentEndpoint = apiEndpoint(*latestProfile,
                    QStringLiteral("/v2/files/requests/%1/submissions/%2")
                        .arg(QString::fromStdString(requestIdValue), submissionId));
            }
            if (!latestProfile || latestProfile->endpoint != expectedOrigin ||
                !currentSession || !currentStamp || !sessions_.isCurrent(*currentStamp) ||
                currentSession->profileId != selectedProfile ||
                currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() ||
                !currentEndpoint ||
                currentStamp->generation != stamp.generation) {
                statusBar()->showMessage(tr("The server session changed before content removal."), 6000);
                return;
            }
            QNetworkRequest deleteRequest = apiRequest(*currentEndpoint);
            deleteRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            deleteRequest.setRawHeader("Authorization", QByteArray("Bearer ") +
                                       QByteArray::fromStdString(currentSession->bearerToken));
            const QByteArray payload = QJsonDocument(QJsonObject{
                {QStringLiteral("confirm_submission_id"), submissionId},
            }).toJson(QJsonDocument::Compact);
            QNetworkReply* deleteReply = network_.sendCustomRequest(
                deleteRequest, QByteArrayLiteral("DELETE"), payload);
            deleteReply->setReadBufferSize(maxFileJobResponseBytes);
            QTimer::singleShot(requestTimeoutMs, deleteReply, [deleteReply] { deleteReply->abort(); });
            connect(deleteReply, &QNetworkReply::downloadProgress, this,
                    [deleteReply](qint64 received, qint64) {
                        if (received > maxFileJobResponseBytes) deleteReply->abort();
                    });
            connect(deleteReply, &QNetworkReply::finished, this,
                    [this, deleteReply, selectedProfile, deployment, stamp = *currentStamp,
                     expectedOrigin, requestIdValue, submissionId] {
                const int deleteStatus = deleteReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray deleteBody = deleteReply->readAll();
                const auto latestProfile = registry_.profile(selectedProfile);
                const QString latestSelection = filesServerFilter_ == nullptr
                    ? QString{} : filesServerFilter_->currentData().toString();
                if (!sessions_.isCurrent(stamp) || filesServerFilter_ == nullptr ||
                    (!latestSelection.isEmpty() && latestSelection != QString::fromStdString(selectedProfile)) ||
                    !latestProfile || latestProfile->endpoint != expectedOrigin) {
                    deleteReply->deleteLater();
                    return;
                }
                if (deleteReply->error() != QNetworkReply::NoError || deleteStatus != 200 ||
                    deleteBody.size() > maxFileJobResponseBytes) {
                    if (deleteStatus == 401) invalidateProfileSessionLocally(selectedProfile, deployment);
                    statusBar()->showMessage(
                        tr("Could not remove retained content: %1").arg(replyFailure(deleteReply)), 9000);
                    deleteReply->deleteLater();
                    refreshFileRequests();
                    return;
                }
                QJsonParseError deleteParseError;
                const QJsonDocument receipt = QJsonDocument::fromJson(deleteBody, &deleteParseError);
                const QJsonObject result = receipt.object();
                const QJsonValue filesValue = result.value(QStringLiteral("files"));
                const QJsonValue deletedAtValue = result.value(QStringLiteral("content_deleted_at_unix"));
                const QJsonValue deletedByValue = result.value(QStringLiteral("content_deleted_by_account_id"));
                const double deletedAt = deletedAtValue.toDouble(-1);
                const bool validDeletionAudit = deletedAtValue.isDouble() && std::isfinite(deletedAt) &&
                    std::floor(deletedAt) == deletedAt && deletedAt > 0 &&
                    (deletedByValue.isNull() ||
                     (deletedByValue.isString() && validUuid(deletedByValue.toString())));
                bool filesDeleted = filesValue.isArray() && filesValue.toArray().size() <= 50;
                if (filesDeleted) {
                    for (const QJsonValue& fileValue : filesValue.toArray()) {
                        if (!fileValue.isObject() ||
                            fileValue.toObject().value(QStringLiteral("state")).toString() != QStringLiteral("deleted")) {
                            filesDeleted = false;
                            break;
                        }
                    }
                }
                const bool validReceipt = deleteParseError.error == QJsonParseError::NoError &&
                    receipt.isObject() && result.value(QStringLiteral("id")).toString() == submissionId &&
                    result.value(QStringLiteral("request_id")).toString() == QString::fromStdString(requestIdValue) &&
                    result.value(QStringLiteral("state")).toString() == QStringLiteral("received") &&
                    validDeletionAudit && filesDeleted;
                deleteReply->deleteLater();
                if (!validReceipt) {
                    statusBar()->showMessage(tr("The server returned an invalid content-removal receipt."), 8000);
                } else {
                    const QString actor = deletedByValue.isString()
                        ? deletedByValue.toString() : tr("unknown manager account");
                    const QString deletedAtText = QDateTime::fromSecsSinceEpoch(
                        static_cast<qint64>(deletedAt), Qt::UTC).toString(Qt::ISODate);
                    statusBar()->showMessage(
                        tr("Submission content removed at %1 by %2; metadata remains available.")
                            .arg(deletedAtText, actor), 9000);
                }
                refreshFileRequests();
            });
        });
    });
}

void MainWindow::downloadFileRequestChunk(
    const std::string& profileId,
    const DeploymentId& deployment,
    const SessionStamp& stamp,
    const std::string& requestId,
    const std::string& submissionId,
    const std::string& fileId,
    const std::string& expectedOrigin,
    const std::shared_ptr<QSaveFile>& output,
    const std::shared_ptr<QCryptographicHash>& hash,
    std::uint64_t expectedSize,
    const std::string& expectedDigest,
    std::uint32_t chunkCount,
    std::uint32_t chunkIndex)
{
    const auto profile = registry_.profile(profileId);
    const auto* session = sessions_.session(deployment);
    if (!profile || profile->endpoint != expectedOrigin || !session ||
        session->profileId != profileId || !sessions_.isCurrent(stamp) ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        output->cancelWriting();
        statusBar()->showMessage(tr("The owning server session changed during file download."), 7000);
        return;
    }
    if (chunkIndex >= chunkCount) {
        const QByteArray actualDigest = hash->result().toHex();
        if (static_cast<std::uint64_t>(output->size()) != expectedSize ||
            actualDigest != QByteArray::fromStdString(expectedDigest)) {
            output->cancelWriting();
            statusBar()->showMessage(tr("The downloaded file failed its size or SHA-256 check."), 9000);
            return;
        }
        if (!output->commit()) {
            output->cancelWriting();
            statusBar()->showMessage(tr("The received file could not be committed to the chosen path."), 9000);
            return;
        }
        statusBar()->showMessage(tr("Received file saved and verified."), 6000);
        return;
    }
    const auto endpoint = apiEndpoint(*profile,
        QStringLiteral("/v2/files/requests/%1/submissions/%2/files/%3/chunks/%4")
            .arg(QString::fromStdString(requestId), QString::fromStdString(submissionId),
                 QString::fromStdString(fileId), QString::number(chunkIndex)));
    if (!endpoint) {
        output->cancelWriting();
        statusBar()->showMessage(tr("The file download route is invalid."), 6000);
        return;
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setRawHeader("Authorization", QByteArray("Bearer ") +
                         QByteArray::fromStdString(session->bearerToken));
    QNetworkReply* reply = network_.get(request);
    reply->setReadBufferSize(managerFileChunkBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > managerFileChunkBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, stamp, requestId, submissionId, fileId,
             expectedOrigin, output, hash, expectedSize, expectedDigest, chunkCount, chunkIndex] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const auto currentProfile = registry_.profile(profileId);
        if (!sessions_.isCurrent(stamp) || !currentProfile ||
            currentProfile->endpoint != expectedOrigin) {
            output->cancelWriting();
            reply->deleteLater();
            return;
        }
        const qint64 chunkStart = static_cast<qint64>(chunkIndex) * managerFileChunkBytes;
        const qint64 expectedChunkSize = std::min<qint64>(
            static_cast<qint64>(expectedSize) - chunkStart, managerFileChunkBytes);
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() != expectedChunkSize || body.size() > managerFileChunkBytes) {
            if (statusCode == 401) invalidateProfileSessionLocally(profileId, deployment);
            output->cancelWriting();
            statusBar()->showMessage(tr("Could not retrieve received-file chunk %1: %2")
                                         .arg(chunkIndex).arg(replyFailure(reply)), 9000);
            reply->deleteLater();
            return;
        }
        if (output->write(body) != body.size()) {
            output->cancelWriting();
            statusBar()->showMessage(tr("The selected destination stopped accepting the received file."), 9000);
            reply->deleteLater();
            return;
        }
        hash->addData(body);
        reply->deleteLater();
        downloadFileRequestChunk(profileId, deployment, stamp, requestId, submissionId, fileId,
                                 expectedOrigin, output, hash, expectedSize, expectedDigest,
                                 chunkCount, chunkIndex + 1);
    });
}

void MainWindow::clearDeploymentFileRequests(const DeploymentId& deployment)
{
    auto& generation = fileRequestRequestGenerations_[deployment.value];
    if (generation < std::numeric_limits<std::uint64_t>::max()) ++generation;
    fileRequests_.erase(std::remove_if(fileRequests_.begin(), fileRequests_.end(),
        [&](const FileRequestRow& row) { return row.id.deployment == deployment; }), fileRequests_.end());
    fileRequestDataErrors_.erase(deployment.value);
    fileRequestDataPending_.erase(deployment.value);
    refreshFileRequestTable();
}

void MainWindow::clearDeploymentAccountData(const DeploymentId& deployment)
{
    auto& generation = accountRequestGenerations_[deployment.value];
    if (generation < std::numeric_limits<std::uint64_t>::max()) {
        ++generation;
    }
    managers_.erase(std::remove_if(managers_.begin(), managers_.end(),
        [&](const ManagerRow& row) { return row.id.deployment == deployment; }), managers_.end());
    accounts_.erase(std::remove_if(accounts_.begin(), accounts_.end(),
        [&](const AccountRow& row) { return row.id.deployment == deployment; }), accounts_.end());
    accessAssignments_.erase(std::remove_if(accessAssignments_.begin(), accessAssignments_.end(),
        [&](const AccessAssignmentRow& row) { return row.id.deployment == deployment; }), accessAssignments_.end());
    const std::string prefix = deployment.value + "/";
    for (auto iterator = accountDataPending_.begin(); iterator != accountDataPending_.end();) {
        if (iterator->starts_with(prefix)) {
            iterator = accountDataPending_.erase(iterator);
        } else {
            ++iterator;
        }
    }
    for (auto iterator = accountDataErrors_.begin(); iterator != accountDataErrors_.end();) {
        if (iterator->first.starts_with(prefix)) {
            iterator = accountDataErrors_.erase(iterator);
        } else {
            ++iterator;
        }
    }
    if (accountsServerFilter_ != nullptr && auditTable_ != nullptr) {
        const auto selected = deploymentsByProfile_.find(
            accountsServerFilter_->currentData().toString().toStdString());
        if (selected != deploymentsByProfile_.end() && selected->second == deployment) {
            auditNextCursorId_.clear();
            auditNextCursorUnix_ = 0;
            auditLoading_ = false;
            if (auditRequestGeneration_ < std::numeric_limits<std::uint64_t>::max()) {
                ++auditRequestGeneration_;
            }
            auditTable_->setRowCount(0);
            if (auditOlderButton_ != nullptr) auditOlderButton_->setEnabled(false);
            if (auditStatus_ != nullptr) {
                auditStatus_->setText(tr("Sign in with fresh MFA to load this server's audit metadata."));
            }
        }
    }
    refreshAccountTables();
}

void MainWindow::closeViewersForDeployment(const DeploymentId& deployment)
{
    for (auto viewer = viewerWindows_.begin(); viewer != viewerWindows_.end();) {
        if (viewer->first.deployment != deployment) {
            ++viewer;
            continue;
        }
        QPointer<QMainWindow> window = viewer->second;
        viewer = viewerWindows_.erase(viewer);
        if (!window.isNull()) {
            window->close();
        }
    }
}

void MainWindow::retainEnrolledDeploymentViewers(
    const DeploymentId& deployment,
    const std::vector<std::string>& enrolledComputerIds)
{
    const std::unordered_set<std::string> retained(
        enrolledComputerIds.begin(), enrolledComputerIds.end());
    for (auto viewer = viewerWindows_.begin(); viewer != viewerWindows_.end();) {
        if (viewer->first.deployment != deployment ||
            retained.contains(viewer->first.serverEntityId)) {
            ++viewer;
            continue;
        }
        QPointer<QMainWindow> window = viewer->second;
        viewer = viewerWindows_.erase(viewer);
        if (!window.isNull()) {
            window->close();
        }
    }
}

void MainWindow::closeAllViewers()
{
    auto viewers = std::move(viewerWindows_);
    viewerWindows_.clear();
    for (auto& [identity, viewer] : viewers) {
        Q_UNUSED(identity);
        if (!viewer.isNull()) {
            viewer->close();
        }
    }
}

void MainWindow::invalidateDeploymentSession(const DeploymentId& deployment)
{
    closeViewersForDeployment(deployment);
    computerRequestGenerations_.erase(deployment.value);
    observationScopeRequestGenerations_.erase(deployment.value);
    observationScopePending_.erase(deployment.value);
    observationScopes_.invalidate(deployment);
    fileJobRequestGenerations_.erase(deployment.value);
    sessions_.invalidate(deployment);
    clearDeploymentFileJobs(deployment);
    clearDeploymentFileRequests(deployment);
    clearDeploymentFileStorage(deployment);
}

void MainWindow::expireSessions()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    std::unordered_set<std::string> expiredDeployments;
    for (const auto& profileDeployment : deploymentsByProfile_) {
        if (sessions_.isExpired(profileDeployment.second, now)) {
            expiredDeployments.insert(profileDeployment.second.value);
            invalidateDeploymentSession(profileDeployment.second);
        }
    }
    if (expiredDeployments.empty()) {
        return;
    }
    computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
        [&](const ComputerRow& row) {
            return expiredDeployments.contains(row.id.deployment.value);
        }), computers_.end());
    for (const auto& deployment : expiredDeployments) {
        clearDeploymentAccountData(DeploymentId{deployment});
        clearDeploymentFileJobs(DeploymentId{deployment});
        clearDeploymentFileRequests(DeploymentId{deployment});
    }
    refreshServerTable();
    refreshComputerTable();
    statusBar()->showMessage(tr("A server session expired; its inventory was cleared."), 5000);
}

void MainWindow::clearDeploymentAuthentication(const DeploymentId& deployment,
                                               const std::string& profileId)
{
    const auto authentication = authenticationProfileByDeployment_.find(deployment.value);
    if (authentication != authenticationProfileByDeployment_.end() &&
        authentication->second == profileId) {
        authenticationProfileByDeployment_.erase(authentication);
    }
}

void MainWindow::invalidateProfileSessionLocally(const std::string& profileId,
                                                  const DeploymentId& deployment)
{
    const auto* session = sessions_.session(deployment);
    if (session != nullptr && session->profileId == profileId) {
        invalidateDeploymentSession(deployment);
        computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
            [&](const ComputerRow& row) { return row.id.deployment == deployment; }), computers_.end());
        clearDeploymentAccountData(deployment);
    }
    clearDeploymentAuthentication(deployment, profileId);
    const auto mappedDeployment = deploymentsByProfile_.find(profileId);
    if (mappedDeployment != deploymentsByProfile_.end() && mappedDeployment->second == deployment) {
        deploymentsByProfile_.erase(mappedDeployment);
    }
    registry_.disconnect(profileId);
    signInsInProgress_.erase(profileId);
    refreshServerTable();
    refreshComputerTable();
}

void MainWindow::signOutSelectedServer()
{
    const auto selectedId = selectedProfileId();
    if (!selectedId) {
        statusBar()->showMessage(tr("Select a server profile first."), 3000);
        return;
    }
    const auto deploymentEntry = deploymentsByProfile_.find(*selectedId);
    const std::optional<DeploymentId> deployment = deploymentEntry == deploymentsByProfile_.end()
        ? std::nullopt
        : std::optional<DeploymentId>(deploymentEntry->second);
    const auto* session = deployment ? sessions_.session(*deployment) : nullptr;
    const auto profile = registry_.profile(*selectedId);
    QByteArray bearerToken = session == nullptr ? QByteArray() :
        QByteArray::fromStdString(session->bearerToken);
    registry_.disconnect(*selectedId);
    signInsInProgress_.erase(*selectedId);
    if (deployment) {
        const auto pendingAuthentication = authenticationProfileByDeployment_.find(deployment->value);
        if (pendingAuthentication != authenticationProfileByDeployment_.end()) {
            const std::string authenticationProfile = pendingAuthentication->second;
            if (authenticationProfile != *selectedId) {
                registry_.disconnect(authenticationProfile);
            }
            signInsInProgress_.erase(authenticationProfile);
        }
        invalidateDeploymentSession(*deployment);
        computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
            [&](const ComputerRow& row) { return row.id.deployment == *deployment; }), computers_.end());
        clearDeploymentAccountData(*deployment);
    }
    refreshServerTable();
    if (bearerToken.isEmpty()) {
        statusBar()->showMessage(tr("Disconnected locally. Any in-flight sign-in was invalidated."), 5000);
        return;
    }
    if (!profile || !deployment) {
        statusBar()->showMessage(tr("Disconnected locally; the server revocation request could not be sent."), 6000);
        return;
    }
    sendLogoutRequestForVerifiedProfile(*profile, *deployment, std::move(bearerToken), true);
}

void MainWindow::forgetSelectedServerProfile()
{
    const auto selectedId = selectedProfileId();
    if (!selectedId) {
        statusBar()->showMessage(tr("Select a server profile first."), 3000);
        return;
    }
    const auto profile = registry_.profile(*selectedId);
    if (!profile) {
        refreshServerTable();
        return;
    }
    const auto answer = QMessageBox::question(
        this, tr("Forget Server Profile"),
        tr("Remove %1 from this manager? This removes only the saved connection profile and local session. It does not remove the server or its data.")
            .arg(QString::fromStdString(profile->displayName)),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const auto deploymentEntry = deploymentsByProfile_.find(*selectedId);
    const std::optional<DeploymentId> deployment = deploymentEntry == deploymentsByProfile_.end()
        ? std::nullopt
        : std::optional<DeploymentId>(deploymentEntry->second);
    const auto* session = deployment ? sessions_.session(*deployment) : nullptr;
    QByteArray bearerToken = session == nullptr ? QByteArray() :
        QByteArray::fromStdString(session->bearerToken);

    auto remainingProfiles = registry_.profiles();
    remainingProfiles.erase(std::remove_if(remainingProfiles.begin(), remainingProfiles.end(),
        [&](const ConnectionProfile& saved) { return saved.profileId == *selectedId; }),
        remainingProfiles.end());
    QSettings settings;
    settings.beginGroup(QStringLiteral("serverProfiles"));
    settings.setValue(QStringLiteral("profiles_v1"), encodeProfiles(remainingProfiles));
    settings.endGroup();
    settings.beginGroup(QStringLiteral("serverProfileTrust"));
    settings.remove(QString::fromStdString(*selectedId));
    settings.endGroup();
    settings.beginGroup(QStringLiteral("serverAuthorityKeyTrust"));
    settings.remove(QString::fromStdString(*selectedId));
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        saveProfiles();
        QMessageBox::warning(this, tr("Unable to Forget Profile"),
                             tr("The saved profile could not be removed from this user's application settings."));
        return;
    }
    registry_.disconnect(*selectedId);
    if (!registry_.removeProfile(*selectedId)) {
        saveProfiles();
        QMessageBox::warning(this, tr("Unable to Forget Profile"),
                             tr("The selected profile could not be removed from the current session."));
        return;
    }

    serverHealthByProfile_.erase(*selectedId);
    serverAuthorityMismatches_.erase(*selectedId);
    signInsInProgress_.erase(*selectedId);
    if (deployment) {
        const auto pendingAuthentication = authenticationProfileByDeployment_.find(deployment->value);
        if (pendingAuthentication != authenticationProfileByDeployment_.end()) {
            const std::string authenticationProfile = pendingAuthentication->second;
            if (authenticationProfile != *selectedId) {
                registry_.disconnect(authenticationProfile);
            }
            signInsInProgress_.erase(authenticationProfile);
        }
        invalidateDeploymentSession(*deployment);
        computers_.erase(std::remove_if(computers_.begin(), computers_.end(),
            [&](const ComputerRow& row) { return row.id.deployment == *deployment; }), computers_.end());
        clearDeploymentAccountData(*deployment);
    }
    deploymentsByProfile_.erase(*selectedId);
    refreshServerTable();
    if (!bearerToken.isEmpty() && profile && deployment) {
        sendLogoutRequestForVerifiedProfile(*profile, *deployment, std::move(bearerToken), false);
    }
    showWorkspace(1);
    statusBar()->showMessage(tr("Saved server profile removed. The server and its data were left untouched."), 5000);
}

void MainWindow::sendLogoutRequestForVerifiedProfile(const ConnectionProfile& profile,
                                                      const DeploymentId& deployment,
                                                      QByteArray bearerToken,
                                                      bool reportResult,
                                                      std::function<void()> completion)
{
    if (shutdownInProgress_ && !completion) {
        ++pendingShutdownLogoutRequests_;
        completion = [this] {
            QTimer::singleShot(0, this, [this] { completeShutdownOperation(); });
        };
    }
    const auto finish = [completion = std::move(completion)] {
        if (completion) {
            completion();
        }
    };
    SensitiveBuffer token(std::move(bearerToken));
    QString tokenText = QString::fromLatin1(token.bytes());
    const bool validToken = validBearerToken(tokenText);
    tokenText.fill(QChar(u'\0'));
    if (!deployment.isValid() || !hasTrustedDeploymentAuthority(profile) ||
        profile.trustedDeploymentId != deployment.value || !validToken) {
        if (reportResult) {
            statusBar()->showMessage(tr("Signed out locally; the saved server identity did not permit a remote logout request."), 8000);
        }
        finish();
        return;
    }
    const auto deploymentEndpoint = apiEndpoint(
        profile, QStringLiteral("/v2/deployment/authority"));
    const auto logoutEndpoint = apiEndpoint(profile, QStringLiteral("/v2/auth/logout"));
    if (!deploymentEndpoint || !logoutEndpoint) {
        if (reportResult) {
            statusBar()->showMessage(tr("Signed out locally; the server address could not be verified for remote logout."), 8000);
        }
        finish();
        return;
    }

    const std::string profileId = profile.profileId;
    const QString endpointSnapshot = QString::fromStdString(profile.endpoint);
    const std::string trustedIdentity = profile.trustedDeploymentId;
    const std::string trustedPublicKey = profile.trustedGrantSigningPublicKey;
    const std::uint64_t trustedGeneration = profile.trustedAuthorityGeneration;
    auto protectedToken = std::make_shared<SensitiveBuffer>(std::move(token));
    QNetworkReply* reply = network_.get(apiRequest(*deploymentEndpoint));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, endpointSnapshot, trustedIdentity, trustedPublicKey,
             trustedGeneration, deployment,
             logoutEndpoint = *logoutEndpoint, protectedToken, reportResult, finish] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const auto currentProfile = registry_.profile(profileId);
        if (currentProfile &&
            (QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
             currentProfile->trustedDeploymentId != trustedIdentity ||
             currentProfile->trustedGrantSigningPublicKey != trustedPublicKey ||
             currentProfile->trustedAuthorityGeneration != trustedGeneration)) {
            if (reportResult) {
                statusBar()->showMessage(tr("Signed out locally; the saved profile changed, so the bearer token was not sent."), 8000);
            }
            reply->deleteLater();
            finish();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 200 ||
            body.size() > maxHealthResponseBytes) {
            if (reportResult) {
                statusBar()->showMessage(tr("Signed out locally; the current server identity could not be confirmed, so remote logout was skipped."), 8000);
            }
            reply->deleteLater();
            finish();
            return;
        }

        const auto authority = parseDeploymentAuthority(body);
        if (!authority || !currentProfile ||
            authority->deploymentId.toStdString() != deployment.value ||
            !authorityMatchesProfile(*currentProfile, *authority)) {
            serverAuthorityMismatches_.insert(profileId);
            refreshServerTable();
            if (reportResult) {
                statusBar()->showMessage(tr("Signed out locally; the server identity changed, so the old bearer token was not sent."), 9000);
            }
            reply->deleteLater();
            finish();
            return;
        }
        if (serverAuthorityMismatches_.erase(profileId) != 0) {
            refreshServerTable();
        }
        reply->deleteLater();
        sendLogoutRequest(logoutEndpoint, protectedToken->take(), reportResult, finish);
    });
}

void MainWindow::sendLogoutRequest(const QUrl& endpoint,
                                   QByteArray bearerToken,
                                   bool reportResult,
                                   std::function<void()> completion)
{
    SensitiveBuffer bearer(QByteArray("Bearer ") + bearerToken);
    bearerToken.fill('\0');
    bearerToken.clear();
    QNetworkRequest request = apiRequest(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", bearer.bytes());
    QNetworkReply* reply = network_.post(request, QByteArray("{}"));
    reply->setReadBufferSize(maxHealthResponseBytes);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > maxHealthResponseBytes) {
                    reply->abort();
                }
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply, reportResult, completion = std::move(completion)] {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && statusCode == 204) {
            if (reportResult) {
                statusBar()->showMessage(tr("Signed out of the selected server."), 4000);
            }
        } else if (reportResult) {
            statusBar()->showMessage(tr("Signed out locally; server revocation failed: %1")
                                         .arg(replyFailure(reply)), 8000);
        }
        reply->deleteLater();
        if (completion) {
            completion();
        }
    });
}

void MainWindow::requestObservationPreview(
    const QString& profileId,
    const DeploymentId& deployment,
    const std::string& computerId,
    const SessionStamp& stamp,
    const QPointer<QLabel>& statusLabel,
    const QPointer<QPushButton>& requestButton)
{
    if (statusLabel.isNull()) return;
    if (statusLabel->property("observationPreviewOccupied").toBool()) return;
    const auto setStatus = [statusLabel, requestButton](const QString& message) {
        setObservationPreviewFailure(statusLabel, requestButton, message);
    };
    if (!sessions_.isCurrent(stamp) || !validDeploymentIdentity(QString::fromStdString(deployment.value)) ||
        !validWireText(QString::fromStdString(computerId))) {
        setStatus(tr("The server session changed. Reopen the computer viewer and try again."));
        return;
    }
    const auto profile = registry_.profile(profileId.toStdString());
    const auto* session = sessions_.session(deployment);
    if (!profile || !session || session->profileId != profileId.toStdString() ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() ||
        profile->trustedDeploymentId != deployment.value ||
        !hasTrustedDeploymentAuthority(*profile) ||
        serverAuthorityMismatches_.contains(profileId.toStdString())) {
        setStatus(tr("Reconnect to the verified owning server before requesting a preview."));
        return;
    }
    const auto computer = std::find_if(computers_.begin(), computers_.end(),
        [&](const ComputerRow& row) {
            return row.profileId == profileId.toStdString() &&
                   row.id.deployment == deployment && row.id.serverComputerId == computerId &&
                   row.enrollmentState == "enrolled";
        });
    if (computer == computers_.end() ||
        !observationScopes_.allows(stamp, computerId,
            helicoid::manager::ObservationScopeRegistry::Clock::now())) {
        setStatus(tr("Current observe permission or endpoint enrollment could not be confirmed."));
        return;
    }

    const auto nativeRuntime = helicoid::manager::NativeWebRtcRuntime::loadBundled();
    if (!nativeRuntime.runtime || !nativeRuntime.runtime->supportsObservation()) {
        setStatus(tr("This Manager's bundled native runtime cannot receive screen observation; no endpoint grant was requested."));
        statusLabel->setProperty("observationPreviewRetryAfterUnixMs",
                                 std::numeric_limits<qint64>::max());
        return;
    }

    auto identity = std::make_shared<helicoid::manager::ManagerSigningIdentity>();
    QString identityError;
    if (!identity->loadOrCreate(identityError)) {
        setStatus(tr("Manager device identity unavailable: %1").arg(identityError));
        return;
    }
    const QByteArray managerPublicKey = identity->publicKeyHex(identityError);
    if (managerPublicKey.size() != 64 || session->managerId.empty() || session->accountId.empty()) {
        setStatus(tr("The signed-in Manager identity is incomplete."));
        return;
    }

    const QString endpointSnapshot = QString::fromStdString(profile->endpoint);
    const std::string trustedDeployment = profile->trustedDeploymentId;
    const std::string trustedGrantKey = profile->trustedGrantSigningPublicKey;
    const std::uint64_t trustedGeneration = profile->trustedAuthorityGeneration;
    const auto endpoint = apiEndpoint(*profile, QStringLiteral("/v2/admissions/observation"));
    if (!endpoint) {
        setStatus(tr("The owning server address is invalid."));
        return;
    }
    const QString cooldownKey = observationPreviewCooldownKey(deployment, computerId);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const std::string cooldownMapKey = cooldownKey.toStdString();
    const auto cooldown = observationPreviewCooldowns_.find(cooldownMapKey);
    if (cooldown != observationPreviewCooldowns_.end() && cooldown->second > nowMs) {
        setStatus(tr("A previous short-lived preview request is still resolving. Try again shortly."));
        return;
    } else if (cooldown != observationPreviewCooldowns_.end()) {
        observationPreviewCooldowns_.erase(cooldown);
    }
    if (activeObservationPreviewCount() >= maxConcurrentObservationPreviews) {
        setStatus(tr("The shared preview capacity is full. Try again when another preview closes."));
        return;
    }

    statusLabel->setProperty("observationPreviewOccupied", true);
    statusLabel->setProperty("observationPreviewDeployment", QString::fromStdString(deployment.value));
    statusLabel->setProperty("observationPreviewCooldownKey", cooldownKey);
    statusLabel->setProperty("observationPreviewRetryAfterUnixMs", 0);
    observationPreviewCooldowns_.insert_or_assign(
        cooldownMapKey, nowMs + requestTimeoutMs + 1000);
    if (!requestButton.isNull()) {
        requestButton->setEnabled(false);
        if (statusLabel->property("overviewPreviewTile").toBool()) {
            requestButton->setVisible(false);
        }
    }
    QNetworkRequest request = apiRequest(*endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(session->bearerToken));
    const QByteArray requestBody = QJsonDocument(QJsonObject{
        {QStringLiteral("computer_id"), QString::fromStdString(computerId)},
    }).toJson(QJsonDocument::Compact);
    statusLabel->setText(tr("Requesting a short-lived, server-authorized observation grant…"));
    QNetworkReply* reply = network_.post(request, requestBody);
    reply->setReadBufferSize(16 * 1024);
    QTimer::singleShot(requestTimeoutMs, reply, [reply] { reply->abort(); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [reply](qint64 received, qint64) {
                if (received > 16 * 1024) reply->abort();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, profileId, deployment, computerId, stamp, statusLabel,
             requestButton,
             managerPublicKey, identity, endpointSnapshot, trustedDeployment,
             trustedGrantKey, trustedGeneration, cooldownKey] {
        const auto setCurrentStatus = [statusLabel, requestButton](const QString& message) {
            setObservationPreviewFailure(statusLabel, requestButton, message);
        };
        const auto currentProfile = registry_.profile(profileId.toStdString());
        const auto* currentSession = sessions_.session(deployment);
        const bool currentContext = sessions_.isCurrent(stamp) && currentProfile && currentSession &&
            currentSession->profileId == profileId.toStdString() &&
            currentSession->expiresAtUnix > QDateTime::currentSecsSinceEpoch() &&
            !serverAuthorityMismatches_.contains(profileId.toStdString()) &&
            QString::fromStdString(currentProfile->endpoint) == endpointSnapshot &&
            currentProfile->trustedDeploymentId == trustedDeployment &&
            currentProfile->trustedGrantSigningPublicKey == trustedGrantKey &&
            currentProfile->trustedAuthorityGeneration == trustedGeneration;
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const qint64 responseAtMs = QDateTime::currentMSecsSinceEpoch();
        observationPreviewCooldowns_.insert_or_assign(
            cooldownKey.toStdString(), responseAtMs + observationPreviewGrantLifetimeMs);
        SensitiveBuffer responseBuffer(reply->readAll());
        if (!currentContext) {
            setCurrentStatus(tr("The server profile or session changed; this request was discarded."));
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError || statusCode != 201 ||
            responseBuffer.bytes().size() > 16 * 1024) {
            setCurrentStatus(tr("The server denied the preview request: %1").arg(replyFailure(reply)));
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(responseBuffer.bytes(), &parseError);
        QJsonObject response = document.object();
        const qsizetype responseFieldCount = response.size();
        const QString grantId = response.value(QStringLiteral("grant_id")).toString();
        const QString responseComputer = response.value(QStringLiteral("computer_id")).toString();
        QString grantToken = response.value(QStringLiteral("grant_token")).toString();
        response.remove(QStringLiteral("grant_token"));
        document.setObject(response);
        const QJsonValue expiresValue = response.value(QStringLiteral("expires_at_unix"));
        const double rawExpires = expiresValue.toDouble(0);
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const bool validExpiry = expiresValue.isDouble() && std::isfinite(rawExpires) &&
            std::floor(rawExpires) == rawExpires && rawExpires > static_cast<double>(now) &&
            rawExpires <= static_cast<double>(now + 10);
        if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
            responseFieldCount != 4 || !validWireText(grantId) ||
            responseComputer.toStdString() != computerId || !validExpiry ||
            grantToken.isEmpty() || grantToken.size() > 8 * 1024 ||
            grantToken.toLatin1().size() != grantToken.size()) {
            grantToken.fill(QChar(u'\0'));
            setCurrentStatus(tr("The server returned an invalid observation grant response."));
            reply->deleteLater();
            return;
        }
        observationPreviewCooldowns_.insert_or_assign(
            cooldownKey.toStdString(), static_cast<qint64>(rawExpires) * 1000);

        QString verifyError;
        QByteArray tokenBytes = grantToken.toLatin1();
        grantToken.fill(QChar(u'\0'));
        auto verifiedUnique = helicoid::manager::VerifiedObservationGrant::verify(
            std::move(tokenBytes), QByteArray::fromStdString(trustedGrantKey),
            QByteArray::fromStdString(deployment.value),
            QByteArray::fromStdString(currentSession->accountId),
            QByteArray::fromStdString(computerId),
            QByteArray::fromStdString(currentSession->managerId),
            managerPublicKey, now, verifyError);
        if (!verifiedUnique || verifiedUnique->grantId() != grantId.toLatin1() ||
            verifiedUnique->computerId() != responseComputer.toLatin1() ||
            verifiedUnique->expiresAtUnix() != static_cast<qint64>(rawExpires)) {
            setCurrentStatus(tr("The signed observation grant could not be verified: %1")
                                 .arg(verifyError));
            reply->deleteLater();
            return;
        }
        const qint64 grantRemainingMs = static_cast<qint64>(rawExpires) * 1000 - responseAtMs;
        if (grantRemainingMs <= 0) {
            setCurrentStatus(tr("The observation grant expired before preview setup completed."));
            reply->deleteLater();
            return;
        }
        statusLabel->setProperty("observationPreviewExpiryMonotonicMs",
                                 monotonicMilliseconds() + grantRemainingMs);
        reply->deleteLater();

        auto grant = std::shared_ptr<helicoid::manager::VerifiedObservationGrant>(
            std::move(verifiedUnique));
        const auto signalsEndpoint = apiEndpoint(
            *currentProfile,
            QStringLiteral("/v2/admissions/observation/%1/signals").arg(grantId));
        if (!signalsEndpoint) {
            setCurrentStatus(tr("The observation signaling address is invalid."));
            return;
        }

        if (statusLabel.isNull()) return;
        statusLabel->setText(tr("Waiting for the endpoint's short-lived admission challenge…"));
        auto* pollTimer = new QTimer(statusLabel.data());
        pollTimer->setInterval(300);
        auto elapsedTimer = std::make_shared<QElapsedTimer>();
        elapsedTimer->start();
        auto inFlight = std::make_shared<bool>(false);
        auto cursor = std::make_shared<quint64>(0);
        QPointer<QTimer> timerGuard(pollTimer);
        connect(pollTimer, &QTimer::timeout, this,
                [this, pollTimer, elapsedTimer, inFlight, cursor, timerGuard,
                 signalsEndpoint = *signalsEndpoint, profileId, deployment, computerId,
                 stamp, statusLabel, requestButton, identity, endpointSnapshot, trustedDeployment, trustedGrantKey,
                 trustedGeneration, grant] {
            const auto setPollStatus = [statusLabel, requestButton](const QString& message) {
                setObservationPreviewFailure(statusLabel, requestButton, message);
            };
            if (!timerGuard || !sessions_.isCurrent(stamp) ||
                elapsedTimer->elapsed() >= 8500) {
                pollTimer->stop();
                pollTimer->deleteLater();
                setPollStatus(tr("No endpoint challenge arrived before the observation grant expired."));
                return;
            }
            if (*inFlight) return;
            const auto currentProfile = registry_.profile(profileId.toStdString());
            const auto* currentSession = sessions_.session(deployment);
            if (!currentProfile || !currentSession ||
                serverAuthorityMismatches_.contains(profileId.toStdString()) ||
                currentSession->profileId != profileId.toStdString() ||
                QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
                currentProfile->trustedDeploymentId != trustedDeployment ||
                currentProfile->trustedGrantSigningPublicKey != trustedGrantKey ||
                currentProfile->trustedAuthorityGeneration != trustedGeneration ||
                currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
                pollTimer->stop();
                pollTimer->deleteLater();
                setPollStatus(tr("The server session changed; the preview request was stopped."));
                return;
            }
            QUrl pollUrl(signalsEndpoint);
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("after"), QString::number(*cursor));
            pollUrl.setQuery(query);
            QNetworkRequest pollRequest = apiRequest(pollUrl);
            pollRequest.setRawHeader(
                "Authorization", QByteArray("Bearer ") +
                    QByteArray::fromStdString(currentSession->bearerToken));
            *inFlight = true;
            QNetworkReply* pollReply = network_.get(pollRequest);
            pollReply->setReadBufferSize(16 * 1024);
            QTimer::singleShot(requestTimeoutMs, pollReply, [pollReply] { pollReply->abort(); });
            connect(pollReply, &QNetworkReply::downloadProgress, this,
                    [pollReply](qint64 received, qint64) {
                        if (received > 16 * 1024) pollReply->abort();
                    });
            connect(pollReply, &QNetworkReply::finished, this,
                    [this, pollReply, pollTimer, elapsedTimer, inFlight, cursor,
                     timerGuard, stamp, deployment, profileId, computerId, statusLabel,
                     requestButton, identity,
                     signalsEndpoint, endpointSnapshot, trustedDeployment, trustedGrantKey,
                     trustedGeneration, grant] {
                *inFlight = false;
                const auto setReplyStatus = [statusLabel, requestButton](const QString& message) {
                    setObservationPreviewFailure(statusLabel, requestButton, message);
                };
                const int replyStatus = pollReply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
                SensitiveBuffer batchBuffer(pollReply->readAll());
                if (!timerGuard) {
                    pollReply->deleteLater();
                    return;
                }
                if (!sessions_.isCurrent(stamp)) {
                    pollTimer->stop();
                    pollTimer->deleteLater();
                    setReplyStatus(tr("The Manager session ended; the observation challenge was discarded."));
                    pollReply->deleteLater();
                    return;
                }
                const auto currentProfile = registry_.profile(profileId.toStdString());
                const auto* currentSession = sessions_.session(deployment);
                if (!currentProfile || !currentSession ||
                    serverAuthorityMismatches_.contains(profileId.toStdString()) ||
                    currentSession->profileId != profileId.toStdString() ||
                    QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
                    currentProfile->trustedDeploymentId != trustedDeployment ||
                    currentProfile->trustedGrantSigningPublicKey != trustedGrantKey ||
                    currentProfile->trustedAuthorityGeneration != trustedGeneration ||
                    currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
                    pollTimer->stop();
                    pollTimer->deleteLater();
                    setReplyStatus(tr("The server profile or session changed; the challenge was discarded."));
                    pollReply->deleteLater();
                    return;
                }
                if (pollReply->error() != QNetworkReply::NoError || replyStatus != 200 ||
                    batchBuffer.bytes().size() > 16 * 1024) {
                    pollTimer->stop();
                    pollTimer->deleteLater();
                    setReplyStatus(tr("Endpoint signaling failed: %1").arg(replyFailure(pollReply)));
                    pollReply->deleteLater();
                    return;
                }
                QJsonParseError batchError;
                const QJsonDocument batchDocument = QJsonDocument::fromJson(
                    batchBuffer.bytes(), &batchError);
                const QJsonObject batch = batchDocument.object();
                const QJsonValue cursorValue = batch.value(QStringLiteral("cursor"));
                const QJsonValue messagesValue = batch.value(QStringLiteral("messages"));
                const double rawCursor = cursorValue.toDouble(-1);
                if (batchError.error != QJsonParseError::NoError ||
                    !batchDocument.isObject() || batch.size() != 2 ||
                    !cursorValue.isDouble() || std::floor(rawCursor) != rawCursor ||
                    rawCursor < 0 || rawCursor > maxObservationSignalsPerGrant ||
                    !messagesValue.isArray() || messagesValue.toArray().size() > 16) {
                    pollTimer->stop();
                    pollTimer->deleteLater();
                    setReplyStatus(tr("The observation signaling response was invalid."));
                    pollReply->deleteLater();
                    return;
                }
                *cursor = static_cast<quint64>(rawCursor);
                quint64 lastSequence = 0;
                for (const QJsonValue& messageValue : messagesValue.toArray()) {
                    const QJsonObject message = messageValue.toObject();
                    const QJsonValue sequenceValue = message.value(QStringLiteral("sequence"));
                    const double rawSequence = sequenceValue.toDouble(-1);
                    if (!messageValue.isObject() || message.size() != 4 ||
                        !sequenceValue.isDouble() || std::floor(rawSequence) != rawSequence ||
                        rawSequence <= static_cast<double>(lastSequence) || rawSequence > rawCursor ||
                        rawSequence > maxObservationSignalsPerGrant ||
                        message.value(QStringLiteral("sender")).toString() != QStringLiteral("endpoint") ||
                        message.value(QStringLiteral("kind")).toString() != QStringLiteral("challenge")) {
                        pollTimer->stop();
                        pollTimer->deleteLater();
                        setReplyStatus(tr("The endpoint sent an invalid observation challenge signal."));
                        pollReply->deleteLater();
                        return;
                    }
                    lastSequence = static_cast<quint64>(rawSequence);
                    const QString payload = message.value(QStringLiteral("payload")).toString();
                    QJsonParseError challengeError;
                    const QJsonDocument challengeDocument = QJsonDocument::fromJson(
                        payload.toUtf8(), &challengeError);
                    const QJsonObject challenge = challengeDocument.object();
                    const QString challengeComputer = challenge.value(QStringLiteral("computer_id")).toString();
                    const QString endpointSession = challenge.value(QStringLiteral("endpoint_session_id")).toString();
                    const QString nonce = challenge.value(QStringLiteral("endpoint_nonce")).toString();
                    const QJsonValue expiryValue = challenge.value(QStringLiteral("expires_at_unix"));
                    const QJsonValue certificateValue = challenge.value(QStringLiteral("endpoint_certificate_pem"));
                    const QJsonValue certificateSignatureValue = challenge.value(
                        QStringLiteral("endpoint_certificate_signature"));
                    const double rawExpiry = expiryValue.toDouble(0);
                    const bool validNonce = nonce.size() == 64 && std::all_of(
                        nonce.cbegin(), nonce.cend(), [](QChar value) {
                            return (value.unicode() >= '0' && value.unicode() <= '9') ||
                                   (value.unicode() >= 'a' && value.unicode() <= 'f');
                        });
                    if (challengeError.error != QJsonParseError::NoError ||
                        payload.size() > 4096 ||
                        !challengeDocument.isObject() ||
                        (challenge.size() != 4 && challenge.size() != 6) ||
                        challengeComputer.toStdString() != computerId ||
                        endpointSession.toLatin1() != grant->endpointSessionId() ||
                        !validNonce || !expiryValue.isDouble() ||
                        std::floor(rawExpiry) != rawExpiry ||
                        rawExpiry != static_cast<double>(grant->expiresAtUnix())) {
                        pollTimer->stop();
                        pollTimer->deleteLater();
                        setReplyStatus(tr("The endpoint challenge did not match this signed grant."));
                        pollReply->deleteLater();
                        return;
                    }
                    if (challenge.size() == 4) {
                        pollTimer->stop();
                        pollTimer->deleteLater();
                        setReplyStatus(tr("The endpoint challenge has no signed DTLS certificate; peer setup stopped before media."));
                        pollReply->deleteLater();
                        return;
                    }
                    const QByteArray certificatePem = certificateValue.toString().toUtf8();
                    const QByteArray certificateSignature = certificateSignatureValue.toString().toLatin1();
                    QString certificateError;
                    if (!certificateValue.isString() || !certificateSignatureValue.isString() ||
                        !grant->verifyPeerCertificateAttestation(
                            certificatePem,
                            nonce.toLatin1(),
                            helicoid::manager::ObservationPeerRole::Endpoint,
                            certificateSignature,
                            certificateError)) {
                        pollTimer->stop();
                        pollTimer->deleteLater();
                        setReplyStatus(tr("The endpoint DTLS certificate attestation failed: %1")
                                           .arg(certificateError));
                        pollReply->deleteLater();
                        return;
                    }
                    pollTimer->stop();
                    pollTimer->deleteLater();

                    QWidget* previewOwner = statusLabel.data();
                    if (previewOwner->findChild<QObject*>(QStringLiteral("observationPeerOwner")) != nullptr) {
                        setReplyStatus(tr("An observation peer already exists for this preview. Close and reopen it to start another request."));
                        pollReply->deleteLater();
                        return;
                    }
                    auto* peerOwner = new ObservationPeerOwner(previewOwner);
                    QString peerError;
                    if (!peerOwner->initialize(certificatePem, peerError)) {
                        peerOwner->deleteLater();
                        setReplyStatus(tr("The endpoint identity was verified, but native peer setup failed: %1")
                                           .arg(peerError));
                        pollReply->deleteLater();
                        return;
                    }

                    std::string localCertificate;
                    const std::uint32_t certificateStatus =
                        peerOwner->peer()->exportLocalCertificate(localCertificate);
                    if (certificateStatus != HS_STATUS_OK || localCertificate.empty()) {
                        peerOwner->deleteLater();
                        setReplyStatus(tr("The native Manager certificate could not be exported (status %1).")
                                           .arg(static_cast<quint32>(certificateStatus)));
                        pollReply->deleteLater();
                        return;
                    }
                    const QByteArray managerCertificate(
                        localCertificate.data(), static_cast<qsizetype>(localCertificate.size()));
                    QString managerCertificateError;
                    const QByteArray managerCertificateSignature =
                        grant->signPeerCertificateAttestation(
                            *identity, managerCertificate, nonce.toLatin1(),
                            managerCertificateError);
                    if (managerCertificateSignature.isEmpty()) {
                        peerOwner->deleteLater();
                        setReplyStatus(tr("The Manager certificate could not be attested: %1")
                                           .arg(managerCertificateError));
                        pollReply->deleteLater();
                        return;
                    }

                    QPointer<ObservationPeerOwner> peerOwnerGuard(peerOwner);
                    QPointer<QLabel> statusGuard(statusLabel);
                    QPointer<QPushButton> requestButtonGuard(requestButton);
                    using OfferCreation = std::pair<std::uint32_t, std::string>;
                    auto* offerWatcher = new QFutureWatcher<OfferCreation>(peerOwner);
                    connect(offerWatcher, &QFutureWatcher<OfferCreation>::finished, this,
                            [this, offerWatcher, peerOwnerGuard, statusGuard,
                             requestButtonGuard, managerCertificate, managerCertificateSignature,
                             identity, nonce, computerId,
                             signalsEndpoint, stamp, deployment, profileId, endpointSnapshot,
                             trustedDeployment, trustedGrantKey, trustedGeneration, grant] {
                        const OfferCreation result = offerWatcher->result();
                        offerWatcher->deleteLater();
                        if (peerOwnerGuard.isNull() || statusGuard.isNull()) return;
                        peerOwnerGuard->setProperty("observationOfferCreating", false);
                        const auto currentProfile = registry_.profile(profileId.toStdString());
                        const auto* currentSession = sessions_.session(deployment);
                        if (!sessions_.isCurrent(stamp) || !currentProfile || !currentSession ||
                            serverAuthorityMismatches_.contains(profileId.toStdString()) ||
                            currentSession->profileId != profileId.toStdString() ||
                            QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
                            currentProfile->trustedDeploymentId != trustedDeployment ||
                            currentProfile->trustedGrantSigningPublicKey != trustedGrantKey ||
                            currentProfile->trustedAuthorityGeneration != trustedGeneration ||
                            currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() ||
                            grant->expiresAtUnix() <= QDateTime::currentSecsSinceEpoch()) {
                            peerOwnerGuard->deleteLater();
                            setObservationPreviewFailure(
                                statusGuard, requestButtonGuard,
                                tr("The verified server authority changed before the observation offer could be sent."));
                            return;
                        }
                        if (result.first != HS_STATUS_OK || result.second.empty()) {
                            peerOwnerGuard->deleteLater();
                            setObservationPreviewFailure(
                                statusGuard, requestButtonGuard,
                                tr("The native Manager offer could not be created (status %1).")
                                    .arg(static_cast<quint32>(result.first)));
                            return;
                        }
                        QString attestedOfferError;
                        const QByteArray attestedOffer =
                            helicoid::manager::appendManagerCertificateAttestationToSdp(
                                QByteArray(result.second.data(),
                                           static_cast<qsizetype>(result.second.size())),
                                managerCertificate,
                                managerCertificateSignature,
                                attestedOfferError);
                        if (attestedOffer.isEmpty()) {
                            peerOwnerGuard->deleteLater();
                            setObservationPreviewFailure(
                                statusGuard, requestButtonGuard,
                                tr("The Manager offer could not carry its signed certificate: %1")
                                    .arg(attestedOfferError));
                            return;
                        }

                        QNetworkRequest offerRequest = apiRequest(signalsEndpoint);
                        offerRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                                               QStringLiteral("application/json"));
                        offerRequest.setRawHeader(
                            "Authorization", QByteArray("Bearer ") +
                                QByteArray::fromStdString(currentSession->bearerToken));
                        const QByteArray offerBody = QJsonDocument(QJsonObject{
                            {QStringLiteral("kind"), QStringLiteral("offer")},
                            {QStringLiteral("payload"), QString::fromLatin1(attestedOffer)},
                        }).toJson(QJsonDocument::Compact);
                        QNetworkReply* offerReply = network_.post(offerRequest, offerBody);
                        offerReply->setReadBufferSize(4 * 1024);
                        QTimer::singleShot(requestTimeoutMs, offerReply,
                                           [offerReply] { offerReply->abort(); });
                        connect(offerReply, &QNetworkReply::finished, this,
                                [this, offerReply, peerOwnerGuard, statusGuard,
                                 requestButtonGuard, identity, nonce, signalsEndpoint,
                                 stamp, deployment, profileId, computerId,
                                 endpointSnapshot, trustedDeployment, trustedGrantKey,
                                 trustedGeneration, grant] {
                            const int status = offerReply->attribute(
                                QNetworkRequest::HttpStatusCodeAttribute).toInt();
                            const auto currentProfile = registry_.profile(profileId.toStdString());
                            const auto* currentSession = sessions_.session(deployment);
                            const bool currentContext = sessions_.isCurrent(stamp) &&
                                currentProfile && currentSession &&
                                !serverAuthorityMismatches_.contains(profileId.toStdString()) &&
                                currentSession->profileId == profileId.toStdString() &&
                                QString::fromStdString(currentProfile->endpoint) == endpointSnapshot &&
                                currentProfile->trustedDeploymentId == trustedDeployment &&
                                currentProfile->trustedGrantSigningPublicKey == trustedGrantKey &&
                                currentProfile->trustedAuthorityGeneration == trustedGeneration &&
                                currentSession->expiresAtUnix > QDateTime::currentSecsSinceEpoch() &&
                                grant->expiresAtUnix() > QDateTime::currentSecsSinceEpoch();
                            if (!currentContext || offerReply->error() != QNetworkReply::NoError ||
                                status != 204) {
                                if (!peerOwnerGuard.isNull()) peerOwnerGuard->deleteLater();
                                setObservationPreviewFailure(
                                    statusGuard, requestButtonGuard,
                                    currentContext
                                        ? tr("The server did not accept the attested offer: %1")
                                              .arg(replyFailure(offerReply))
                                        : tr("The server session or observation grant changed before offer relay."));
                            } else {
                                if (!statusGuard.isNull()) {
                                    statusGuard->setText(tr("The signed offer was relayed. Negotiating the verified media channel…"));
                                }
                                if (!requestButtonGuard.isNull()) requestButtonGuard->setEnabled(false);
                                auto* sessionTimer = new QTimer(peerOwnerGuard.data());
                                sessionTimer->setInterval(100);
                                QPointer<QTimer> sessionTimerGuard(sessionTimer);
                                const auto inFlight = std::make_shared<bool>(false);
                                const auto pollCursor = std::make_shared<quint64>(2);
                                const auto hasRemoteAnswer = std::make_shared<bool>(false);
                                const auto localAdmissionStarted = std::make_shared<bool>(false);
                                const auto receivedFrame = std::make_shared<bool>(false);
                                const auto pendingRemoteIce =
                                    std::make_shared<std::vector<std::string>>();
                                const auto stopSession = std::make_shared<std::function<void(QString)>>();
                                *stopSession = [peerOwnerGuard, statusGuard, requestButtonGuard,
                                                sessionTimerGuard](QString message) {
                                    if (!sessionTimerGuard.isNull()) {
                                        sessionTimerGuard->stop();
                                        sessionTimerGuard->deleteLater();
                                    }
                                    if (!peerOwnerGuard.isNull()) {
                                        if (peerOwnerGuard->peer() != nullptr) {
                                            (void)peerOwnerGuard->peer()->endObservation();
                                        }
                                        peerOwnerGuard->deleteLater();
                                    }
                                    setObservationPreviewFailure(statusGuard, requestButtonGuard, message);
                                };

                                connect(sessionTimer, &QTimer::timeout, this,
                                        [this, peerOwnerGuard, statusGuard, requestButtonGuard,
                                         sessionTimerGuard, inFlight, pollCursor, hasRemoteAnswer,
                                         localAdmissionStarted, receivedFrame, pendingRemoteIce, stopSession,
                                         identity, nonce, signalsEndpoint, stamp, deployment,
                                         profileId, computerId, endpointSnapshot, trustedDeployment,
                                         trustedGrantKey, trustedGeneration, grant] {
                                    if (peerOwnerGuard.isNull() || statusGuard.isNull() ||
                                        sessionTimerGuard.isNull()) {
                                        return;
                                    }
                                    if (!sessions_.isCurrent(stamp)) {
                                        (*stopSession)(tr("The Manager session ended; observation was stopped."));
                                        return;
                                    }
                                    const auto currentProfile = registry_.profile(profileId.toStdString());
                                    const auto* currentSession = sessions_.session(deployment);
                                    if (!currentProfile || !currentSession ||
                                        serverAuthorityMismatches_.contains(profileId.toStdString()) ||
                                        currentSession->profileId != profileId.toStdString() ||
                                        QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
                                        currentProfile->trustedDeploymentId != trustedDeployment ||
                                        currentProfile->trustedGrantSigningPublicKey != trustedGrantKey ||
                                        currentProfile->trustedAuthorityGeneration != trustedGeneration ||
                                        currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
                                        (*stopSession)(tr("The verified server session changed; observation was stopped."));
                                        return;
                                    }
                                    if (!observationScopes_.allows(
                                            stamp, computerId,
                                            helicoid::manager::ObservationScopeRegistry::Clock::now())) {
                                        (*stopSession)(tr("Current observe permission ended; observation was stopped."));
                                        return;
                                    }
                                    if (grant->expiresAtUnix() <= QDateTime::currentSecsSinceEpoch()) {
                                        if (*receivedFrame) {
                                            (*stopSession)(tr("The short preview lease ended. Requesting fresh observation authority…"));
                                            if (!statusGuard.isNull()) {
                                                statusGuard->setProperty("observationPreviewRetryAfterUnixMs", 0);
                                            }
                                            if (!requestButtonGuard.isNull()) {
                                                requestButtonGuard->setEnabled(false);
                                                if (!statusGuard.isNull() &&
                                                    statusGuard->property("overviewPreviewTile").toBool()) {
                                                    requestButtonGuard->setVisible(false);
                                                }
                                            }
                                            QTimer::singleShot(100, this,
                                                [this, statusGuard, requestButtonGuard, profileId,
                                                 deployment, computerId, stamp] {
                                                    if (statusGuard.isNull() || requestButtonGuard.isNull()) return;
                                                    requestObservationPreview(
                                                        profileId, deployment, computerId, stamp,
                                                        statusGuard, requestButtonGuard);
                                                });
                                        } else {
                                            (*stopSession)(tr("The preview grant expired before the first frame arrived. Request a new preview to try again."));
                                        }
                                        return;
                                    }

                                    std::string localCandidate;
                                    const std::uint32_t iceStatus =
                                        peerOwnerGuard->peer()->pollIceCandidate(localCandidate);
                                    if (iceStatus == HS_STATUS_OK) {
                                        const QUrl signalEndpoint(signalsEndpoint);
                                        QNetworkRequest iceRequest = apiRequest(signalEndpoint);
                                        iceRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                                                             QStringLiteral("application/json"));
                                        iceRequest.setRawHeader(
                                            "Authorization", QByteArray("Bearer ") +
                                                QByteArray::fromStdString(currentSession->bearerToken));
                                        const QByteArray iceBody = QJsonDocument(QJsonObject{
                                            {QStringLiteral("kind"), QStringLiteral("ice")},
                                            {QStringLiteral("payload"), QString::fromLatin1(
                                                localCandidate.data(),
                                                static_cast<qsizetype>(localCandidate.size()))},
                                        }).toJson(QJsonDocument::Compact);
                                        QNetworkReply* iceReply = network_.post(iceRequest, iceBody);
                                        iceReply->setReadBufferSize(1024);
                                        QTimer::singleShot(requestTimeoutMs, iceReply,
                                                           [iceReply] { iceReply->abort(); });
                                        connect(iceReply, &QNetworkReply::finished, this,
                                                [iceReply, stopSession] {
                                            const int status = iceReply->attribute(
                                                QNetworkRequest::HttpStatusCodeAttribute).toInt();
                                            if (iceReply->error() != QNetworkReply::NoError || status != 204) {
                                                (*stopSession)(tr("The server rejected an ICE candidate; observation was stopped."));
                                            }
                                            iceReply->deleteLater();
                                        });
                                    } else if (iceStatus != HS_STATUS_TIMEOUT) {
                                        (*stopSession)(tr("The native Manager could not read its ICE candidates."));
                                        return;
                                    }

                                    if (*inFlight) return;
                                    QUrl pollUrl(signalsEndpoint);
                                    QUrlQuery query;
                                    query.addQueryItem(QStringLiteral("after"),
                                                       QString::number(*pollCursor));
                                    pollUrl.setQuery(query);
                                    QNetworkRequest pollRequest = apiRequest(pollUrl);
                                    pollRequest.setRawHeader(
                                        "Authorization", QByteArray("Bearer ") +
                                            QByteArray::fromStdString(currentSession->bearerToken));
                                    *inFlight = true;
                                    QNetworkReply* signalingReply = network_.get(pollRequest);
                                    signalingReply->setReadBufferSize(256 * 1024);
                                    QTimer::singleShot(requestTimeoutMs, signalingReply,
                                                       [signalingReply] { signalingReply->abort(); });
                                    connect(signalingReply, &QNetworkReply::downloadProgress, this,
                                            [signalingReply](qint64 received, qint64) {
                                        if (received > 256 * 1024) signalingReply->abort();
                                    });
                                    connect(signalingReply, &QNetworkReply::finished, this,
                                            [this, signalingReply, peerOwnerGuard, statusGuard,
                                             requestButtonGuard, inFlight, pollCursor,
                                             hasRemoteAnswer, localAdmissionStarted,
                                             receivedFrame, pendingRemoteIce, stopSession, identity, nonce,
                                             stamp, deployment, profileId, endpointSnapshot,
                                             trustedDeployment, trustedGrantKey, trustedGeneration, grant] {
                                        *inFlight = false;
                                        const int status = signalingReply->attribute(
                                            QNetworkRequest::HttpStatusCodeAttribute).toInt();
                                        SensitiveBuffer batchBuffer(signalingReply->readAll());
                                        if (peerOwnerGuard.isNull() || statusGuard.isNull()) {
                                            signalingReply->deleteLater();
                                            return;
                                        }
                                        if (!sessions_.isCurrent(stamp)) {
                                            (*stopSession)(tr("The Manager session ended; observation was stopped."));
                                            signalingReply->deleteLater();
                                            return;
                                        }
                                        const auto currentProfile = registry_.profile(profileId.toStdString());
                                        const auto* currentSession = sessions_.session(deployment);
                                        if (!currentProfile || !currentSession ||
                                            serverAuthorityMismatches_.contains(profileId.toStdString()) ||
                                            currentSession->profileId != profileId.toStdString() ||
                                            QString::fromStdString(currentProfile->endpoint) != endpointSnapshot ||
                                            currentProfile->trustedDeploymentId != trustedDeployment ||
                                            currentProfile->trustedGrantSigningPublicKey != trustedGrantKey ||
                                            currentProfile->trustedAuthorityGeneration != trustedGeneration ||
                                            currentSession->expiresAtUnix <= QDateTime::currentSecsSinceEpoch() ||
                                            grant->expiresAtUnix() <= QDateTime::currentSecsSinceEpoch()) {
                                            (*stopSession)(tr("The verified server authority changed; observation was stopped."));
                                            signalingReply->deleteLater();
                                            return;
                                        }
                                        if (signalingReply->error() != QNetworkReply::NoError ||
                                            status != 200 || batchBuffer.bytes().size() > 256 * 1024) {
                                            (*stopSession)(tr("The server denied the current observation signal poll."));
                                            signalingReply->deleteLater();
                                            return;
                                        }
                                        QJsonParseError parseError;
                                        const QJsonDocument batchDocument = QJsonDocument::fromJson(
                                            batchBuffer.bytes(), &parseError);
                                        const QJsonObject batch = batchDocument.object();
                                        const QJsonValue cursorValue = batch.value(QStringLiteral("cursor"));
                                        const QJsonValue messagesValue = batch.value(QStringLiteral("messages"));
                                        const double rawCursor = cursorValue.toDouble(-1);
                                        if (parseError.error != QJsonParseError::NoError ||
                                            !batchDocument.isObject() || batch.size() != 2 ||
                                            !cursorValue.isDouble() || std::floor(rawCursor) != rawCursor ||
                                            rawCursor < static_cast<double>(*pollCursor) ||
                                            rawCursor > maxObservationSignalsPerGrant ||
                                            !messagesValue.isArray() || messagesValue.toArray().size() > 64) {
                                            (*stopSession)(tr("The server returned an invalid observation signal batch."));
                                            signalingReply->deleteLater();
                                            return;
                                        }
                                        const quint64 priorCursor = *pollCursor;
                                        *pollCursor = static_cast<quint64>(rawCursor);
                                        quint64 lastSequence = priorCursor;
                                        for (const QJsonValue& signalValue : messagesValue.toArray()) {
                                            const QJsonObject signal = signalValue.toObject();
                                            const QJsonValue sequenceValue = signal.value(QStringLiteral("sequence"));
                                            const double rawSequence = sequenceValue.toDouble(-1);
                                            const QString kind = signal.value(QStringLiteral("kind")).toString();
                                            const QString sender = signal.value(QStringLiteral("sender")).toString();
                                            const QString payload = signal.value(QStringLiteral("payload")).toString();
                                            if (!signalValue.isObject() || signal.size() != 4 ||
                                                !sequenceValue.isDouble() ||
                                                std::floor(rawSequence) != rawSequence ||
                                                rawSequence <= static_cast<double>(lastSequence) ||
                                                rawSequence > rawCursor ||
                                                rawSequence > maxObservationSignalsPerGrant ||
                                                sender != QStringLiteral("endpoint")) {
                                                (*stopSession)(tr("The server returned an invalid observation signal."));
                                                signalingReply->deleteLater();
                                                return;
                                            }
                                            lastSequence = static_cast<quint64>(rawSequence);
                                            if (kind == QStringLiteral("close")) {
                                                if (!payload.isEmpty()) {
                                                    (*stopSession)(tr("The endpoint sent a malformed close signal."));
                                                    signalingReply->deleteLater();
                                                    return;
                                                }
                                                (*stopSession)(tr("The endpoint closed the observation session."));
                                                signalingReply->deleteLater();
                                                return;
                                            }
                                            const QByteArray payloadBytes = payload.toLatin1();
                                            if (payloadBytes.size() != payload.size() ||
                                                payloadBytes.isEmpty() || payloadBytes.size() > 65'536) {
                                                (*stopSession)(tr("The endpoint sent an invalid negotiation value."));
                                                signalingReply->deleteLater();
                                                return;
                                            }
                                            if (kind == QStringLiteral("answer")) {
                                                if (*hasRemoteAnswer || !payload.startsWith(QStringLiteral("v=0")) ||
                                                    peerOwnerGuard->peer()->setRemoteDescription(
                                                        "answer", std::string_view(
                                                            payloadBytes.constData(),
                                                            static_cast<std::size_t>(payloadBytes.size()))) != HS_STATUS_OK) {
                                                    (*stopSession)(tr("The endpoint answer failed native SDP validation."));
                                                    signalingReply->deleteLater();
                                                    return;
                                                }
                                                *hasRemoteAnswer = true;
                                                for (const std::string& candidate : *pendingRemoteIce) {
                                                    if (peerOwnerGuard->peer()->addIceCandidate(candidate) != HS_STATUS_OK) {
                                                        (*stopSession)(tr("An endpoint ICE candidate failed native validation."));
                                                        signalingReply->deleteLater();
                                                        return;
                                                    }
                                                }
                                                pendingRemoteIce->clear();
                                            } else if (kind == QStringLiteral("ice")) {
                                                if (payloadBytes.size() > 4 * 1024 ||
                                                    (!*hasRemoteAnswer && pendingRemoteIce->size() >= HS_MAX_ICE_QUEUE)) {
                                                    (*stopSession)(tr("The endpoint ICE candidate queue exceeded its bound."));
                                                    signalingReply->deleteLater();
                                                    return;
                                                }
                                                if (*hasRemoteAnswer) {
                                                    if (peerOwnerGuard->peer()->addIceCandidate(
                                                            std::string_view(payloadBytes.constData(),
                                                                static_cast<std::size_t>(payloadBytes.size()))) != HS_STATUS_OK) {
                                                        (*stopSession)(tr("An endpoint ICE candidate failed native validation."));
                                                        signalingReply->deleteLater();
                                                        return;
                                                    }
                                                } else {
                                                    pendingRemoteIce->emplace_back(
                                                        payloadBytes.constData(),
                                                        static_cast<std::size_t>(payloadBytes.size()));
                                                }
                                            } else {
                                                (*stopSession)(tr("The endpoint sent an unsupported observation signal."));
                                                signalingReply->deleteLater();
                                                return;
                                            }
                                        }

                                        std::uint32_t peerState = HS_PEER_STATE_CLOSED;
                                        if (peerOwnerGuard->peer()->state(peerState) != HS_STATUS_OK ||
                                            peerState == HS_PEER_STATE_FAILED ||
                                            peerState == HS_PEER_STATE_CLOSED) {
                                            (*stopSession)(tr("The verified native WebRTC peer closed or failed."));
                                            signalingReply->deleteLater();
                                            return;
                                        }
                                        if (!*localAdmissionStarted &&
                                            peerState == HS_PEER_STATE_CHANNEL_OPEN) {
                                            std::vector<std::uint8_t> binding;
                                            if (peerOwnerGuard->peer()->exportVerifiedChannelBinding(binding) != HS_STATUS_OK ||
                                                binding.empty()) {
                                                (*stopSession)(tr("The native DTLS channel binding could not be verified."));
                                                signalingReply->deleteLater();
                                                return;
                                            }
                                            const QByteArray bindingBytes(
                                                reinterpret_cast<const char*>(binding.data()),
                                                static_cast<qsizetype>(binding.size()));
                                            QString proofError;
                                            const QByteArray proof = grant->signEndpointNonce(
                                                *identity, nonce.toLatin1(), bindingBytes, proofError);
                                            const qint64 remainingMs = std::min<qint64>(
                                                10'000, grant->expiresAtUnix() * 1000 -
                                                    QDateTime::currentMSecsSinceEpoch());
                                            if (proof.isEmpty() || remainingMs <= 0 ||
                                                peerOwnerGuard->peer()->beginObservation(
                                                    std::span<const std::uint8_t>(binding),
                                                    static_cast<std::uint32_t>(remainingMs)) != HS_STATUS_OK) {
                                                (*stopSession)(proofError.isEmpty()
                                                    ? tr("The native observation lease could not start.")
                                                    : proofError);
                                                signalingReply->deleteLater();
                                                return;
                                            }
                                            const QByteArray proofMessage = QJsonDocument(QJsonObject{
                                                {QStringLiteral("type"), QStringLiteral("observation_admission_proof")},
                                                {QStringLiteral("grant_id"), QString::fromLatin1(grant->grantId())},
                                                {QStringLiteral("endpoint_nonce"), nonce},
                                                {QStringLiteral("channel_binding"), QString::fromLatin1(bindingBytes.toHex())},
                                                {QStringLiteral("manager_proof"), QString::fromLatin1(proof)},
                                            }).toJson(QJsonDocument::Compact);
                                            if (peerOwnerGuard->peer()->sendProductMessage(
                                                    std::span<const std::uint8_t>(
                                                        reinterpret_cast<const std::uint8_t*>(proofMessage.constData()),
                                                        static_cast<std::size_t>(proofMessage.size()))) != HS_STATUS_OK) {
                                                (*stopSession)(tr("The Manager admission proof could not be sent over the verified data channel."));
                                                signalingReply->deleteLater();
                                                return;
                                            }
                                            *localAdmissionStarted = true;
                                            statusGuard->setText(tr("Verified channel established. Waiting for endpoint redemption and screen frames…"));
                                        }
                                        if (*localAdmissionStarted) {
                                            helicoid::manager::NativeScreenFrame frame;
                                            const std::uint32_t frameStatus =
                                                peerOwnerGuard->peer()->acquireScreenFrame(frame);
                                            if (frameStatus == HS_STATUS_OK) {
                                                const QImage image(
                                                    frame.bgra.data(),
                                                    static_cast<int>(frame.width),
                                                    static_cast<int>(frame.height),
                                                    static_cast<qsizetype>(frame.width * 4U),
                                                    QImage::Format_ARGB32);
                                                const QImage retainedImage = image.copy();
                                                if (retainedImage.isNull()) {
                                                    (*stopSession)(tr("The admitted screen frame could not be retained for display."));
                                                    signalingReply->deleteLater();
                                                    return;
                                                }
                                                const bool screenshotControl = statusGuard->property(
                                                    "observationScreenshotControl").toBool();
                                                if (screenshotControl) {
                                                    statusGuard->setProperty(
                                                        "observationPreviewImage", QVariant::fromValue(retainedImage));
                                                    statusGuard->setProperty(
                                                        "observationPreviewFrameAtMonotonicMs",
                                                        monotonicMilliseconds());
                                                }
                                                statusGuard->setPixmap(QPixmap::fromImage(retainedImage).scaled(
                                                    statusGuard->size(), Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation));
                                                if (screenshotControl) {
                                                    if (auto* saveButton = statusGuard->window()->findChild<QPushButton*>(
                                                            QStringLiteral("saveObservationScreenshot"))) {
                                                        saveButton->setEnabled(true);
                                                    }
                                                }
                                                statusGuard->setToolTip(tr("View-only screen preview"));
                                                const QString cooldownKey = statusGuard->property(
                                                    "observationPreviewCooldownKey").toString();
                                                if (!cooldownKey.isEmpty()) {
                                                    observationPreviewCooldowns_.erase(
                                                        cooldownKey.toStdString());
                                                }
                                                *receivedFrame = true;
                                            } else if (frameStatus != HS_STATUS_TIMEOUT &&
                                                       frameStatus != HS_STATUS_UNAVAILABLE) {
                                                (*stopSession)(tr("Native screen-frame delivery stopped because the observation lease or peer became invalid."));
                                                signalingReply->deleteLater();
                                                return;
                                            }
                                        }
                                        signalingReply->deleteLater();
                                    });
                                });
                                sessionTimer->start();
                            }
                            offerReply->deleteLater();
                        });
                    });
                    const auto peerState = peerOwner->state();
                    peerOwner->setProperty("observationOfferCreating", true);
                    offerWatcher->setFuture(QtConcurrent::run([peerState] {
                        std::string offer;
                        const std::uint32_t status = peerState && peerState->peer
                            ? peerState->peer->createOffer(offer)
                            : static_cast<std::uint32_t>(HS_STATUS_UNAVAILABLE);
                        return OfferCreation{status, std::move(offer)};
                    }));
                    pollReply->deleteLater();
                    return;
                }
                pollReply->deleteLater();
            });
        });
        pollTimer->start();
    });
}

void MainWindow::openSelectedComputerViewer()
{
    QString profileId;
    QString deploymentId;
    QString computerId;
    if (computerLayoutStack_ != nullptr && computerLayoutStack_->currentIndex() == 1 &&
        computerTable_ != nullptr) {
        const int row = computerTable_->currentRow();
        const auto* server = row >= 0 ? computerTable_->item(row, 0) : nullptr;
        const auto* computer = row >= 0 ? computerTable_->item(row, 1) : nullptr;
        if (server != nullptr && computer != nullptr && !computerTable_->isRowHidden(row)) {
            profileId = server->data(Qt::UserRole).toString();
            deploymentId = server->data(Qt::UserRole + 1).toString();
            computerId = computer->data(Qt::UserRole + 2).toString();
        }
    } else if (computerGrid_ != nullptr) {
        const auto* computer = computerGrid_->currentItem();
        if (computer != nullptr && !computer->isHidden()) {
            profileId = computer->data(Qt::UserRole).toString();
            deploymentId = computer->data(Qt::UserRole + 1).toString();
            computerId = computer->data(Qt::UserRole + 2).toString();
        }
    }
    if (profileId.isEmpty() || !validDeploymentIdentity(deploymentId) || !validWireText(computerId)) {
        statusBar()->showMessage(tr("Select one authorized computer first."), 5000);
        return;
    }

    const auto target = std::find_if(computers_.begin(), computers_.end(),
        [&](const ComputerRow& row) {
            return row.profileId == profileId.toStdString() &&
                   row.id.deployment.value == deploymentId.toStdString() &&
                   row.id.serverComputerId == computerId.toStdString();
        });
    if (target == computers_.end()) {
        statusBar()->showMessage(tr("The selected computer is no longer in the current authorized inventory. Refresh it before opening a view."), 7000);
        return;
    }
    const DeploymentId deployment{deploymentId.toStdString()};
    const std::string owningProfileId = profileId.toStdString();
    const auto owningProfile = registry_.profile(owningProfileId);
    const auto mappedDeployment = deploymentsByProfile_.find(owningProfileId);
    const auto* session = sessions_.session(deployment);
    const auto stamp = sessions_.currentStamp(deployment);
    if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
        session->profileId != target->profileId || !owningProfile ||
        serverAuthorityMismatches_.contains(owningProfileId) ||
        !hasTrustedDeploymentAuthority(*owningProfile) ||
        owningProfile->trustedDeploymentId != deployment.value ||
        mappedDeployment == deploymentsByProfile_.end() || mappedDeployment->second != deployment ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        statusBar()->showMessage(tr("The computer's owning server is no longer verified and signed in. Reconnect before opening its view."), 7000);
        return;
    }
    if (target->enrollmentState != "enrolled") {
        statusBar()->showMessage(tr("This computer is not enrolled and cannot provide an endpoint view."), 6000);
        return;
    }

    const auto scopeNow = helicoid::manager::ObservationScopeRegistry::Clock::now();
    if (!observationScopes_.hasCurrent(*stamp, scopeNow)) {
        loadObservationScope(owningProfileId, deployment, *stamp);
        statusBar()->showMessage(tr("Checking current observe access for this computer. Select it again after the check completes."), 7000);
        return;
    }
    if (!observationScopes_.allows(*stamp, target->id.serverComputerId, scopeNow)) {
        statusBar()->showMessage(tr("Your current account has no observe access for this computer."), 7000);
        return;
    }

    const auto viewerKey = ScopedViewerId::create(
        target->id.deployment, target->id.serverComputerId);
    if (!viewerKey) {
        statusBar()->showMessage(tr("This computer cannot be assigned a valid viewer identity."), 6000);
        return;
    }
    const auto existing = viewerWindows_.find(*viewerKey);
    if (existing != viewerWindows_.end() && !existing->second.isNull()) {
        existing->second->show();
        existing->second->raise();
        existing->second->activateWindow();
        return;
    }
    if (computerGrid_ != nullptr) {
        for (int index = 0; index < computerGrid_->count(); ++index) {
            QListWidgetItem* item = computerGrid_->item(index);
            if (item->data(Qt::UserRole).toString() != profileId ||
                item->data(Qt::UserRole + 1).toString() != deploymentId ||
                item->data(Qt::UserRole + 2).toString() != computerId) {
                continue;
            }
            if (QWidget* tileWidget = computerGrid_->itemWidget(item)) {
                stopOverviewPreview(tileWidget);
            }
            break;
        }
    }
    const std::string viewerComputerId = target->id.serverComputerId;
    const SessionStamp viewerSessionStamp = *stamp;

    auto* viewer = new QMainWindow(this, Qt::Window);
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    viewer->setObjectName(QStringLiteral("computerViewerWindow"));
    viewer->setWindowTitle(tr("%1 — %2 — View Screen")
                               .arg(QString::fromStdString(target->friendlyName),
                                    QString::fromStdString(target->serverName)));
    viewer->setMinimumSize(560, 360);
    viewer->resize(760, 520);

    auto* page = new QWidget(viewer);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(14);
    layout->addWidget(heading(QString::fromStdString(target->friendlyName), page));
    layout->addWidget(subheading(
        tr("Server: %1\nComputer ID: %2\nPlatform: %3")
            .arg(QString::fromStdString(target->serverName),
                 QString::fromStdString(target->id.serverComputerId),
                 QString::fromStdString(target->platform)), page));

    auto* preview = new QFrame(page);
    preview->setObjectName(QStringLiteral("viewerPreviewUnavailable"));
    preview->setMinimumHeight(180);
    preview->setFrameShape(QFrame::StyledPanel);
    preview->setFrameShadow(QFrame::Sunken);
    auto* previewLayout = new QVBoxLayout(preview);
    auto* previewStatus = heading(tr("View-only preview"), preview);
    previewStatus->setObjectName(QStringLiteral("viewerPreviewStatus"));
    previewStatus->setAlignment(Qt::AlignCenter);
    previewStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    previewStatus->setText(tr("Starting authorized view-only preview…"));
    previewStatus->setProperty("observationScreenshotControl", true);
    previewLayout->addWidget(previewStatus);
    layout->addWidget(preview, 1);
    layout->addWidget(subheading(
        tr("A preview request checks current server authority and endpoint admission. Video transport is enabled only after the peer identity is verified."), page));
    auto* saveScreenshot = new QPushButton(tr("Save Screenshot…"), page);
    saveScreenshot->setObjectName(QStringLiteral("saveObservationScreenshot"));
    saveScreenshot->setToolTip(tr("Save the latest fresh, admitted screen frame to a local PNG file."));
    saveScreenshot->setEnabled(false);
    layout->addWidget(saveScreenshot, 0, Qt::AlignLeft);
    connect(saveScreenshot, &QPushButton::clicked, viewer,
            [this, viewer, previewStatus, saveScreenshot] {
        const auto currentFrame = [previewStatus]() -> QImage {
            if (previewStatus->property("observationPreviewOccupied").toBool() == false) {
                return {};
            }
            const qint64 nowMs = monotonicMilliseconds();
            const qint64 frameAtMs = previewStatus->property(
                "observationPreviewFrameAtMonotonicMs").toLongLong();
            const qint64 expiresAtMs = previewStatus->property(
                "observationPreviewExpiryMonotonicMs").toLongLong();
            if (frameAtMs <= 0 || expiresAtMs <= nowMs || nowMs < frameAtMs ||
                nowMs - frameAtMs > 1500) {
                return {};
            }
            return previewStatus->property("observationPreviewImage").value<QImage>();
        };
        if (currentFrame().isNull()) {
            saveScreenshot->setEnabled(false);
            QMessageBox::information(
                viewer, tr("No Current Screenshot"),
                tr("A fresh admitted screen frame is not available. Wait for the authorized preview to resume."));
            return;
        }

        const QString suggestedName = tr("Helicoid Screenshot %1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
        QString path = QFileDialog::getSaveFileName(
            viewer, tr("Save Screenshot"), suggestedName,
            tr("PNG image (*.png)"));
        if (path.isEmpty()) return;
        const QFileInfo targetInfo(path);
        if (targetInfo.suffix().isEmpty()) {
            path += QStringLiteral(".png");
        } else if (targetInfo.suffix().compare(QStringLiteral("png"), Qt::CaseInsensitive) != 0) {
            QMessageBox::warning(viewer, tr("Save Screenshot"), tr("Choose a PNG file name."));
            return;
        }

        const QImage image = currentFrame();
        if (image.isNull()) {
            saveScreenshot->setEnabled(false);
            QMessageBox::information(
                viewer, tr("Screenshot Expired"),
                tr("The observation frame expired while the save dialog was open. Wait for a fresh admitted frame and try again."));
            return;
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit()) {
            file.cancelWriting();
            QMessageBox::warning(
                viewer, tr("Save Screenshot Failed"),
                tr("The screenshot could not be written to the selected file."));
            return;
        }
        statusBar()->showMessage(tr("Screenshot saved locally."), 5000);
    });
    auto* requestPreview = new QPushButton(tr("Request preview"), page);
    requestPreview->setObjectName(QStringLiteral("requestObservationPreview"));
    layout->addWidget(requestPreview, 0, Qt::AlignLeft);
    connect(requestPreview, &QPushButton::clicked, this,
            [this, requestPreview, profileId, deployment,
             computerId = viewerComputerId, stamp = viewerSessionStamp, previewStatus] {
        requestPreview->setEnabled(false);
        requestObservationPreview(
            profileId, deployment, computerId, stamp, QPointer<QLabel>(previewStatus),
            QPointer<QPushButton>(requestPreview));
    });
    viewer->setCentralWidget(page);

    viewerWindows_[*viewerKey] = viewer;
    connect(viewer, &QObject::destroyed, this, [this, viewerKey = *viewerKey] {
        const auto current = viewerWindows_.find(viewerKey);
        if (current != viewerWindows_.end() && current->second.isNull()) {
            viewerWindows_.erase(current);
        }
    });
    viewer->show();
    requestPreview->setEnabled(false);
    const QPointer<QLabel> previewStatusGuard(previewStatus);
    const QPointer<QPushButton> requestPreviewGuard(requestPreview);
    QTimer::singleShot(0, viewer,
        [this, profileId, deployment, computerId = viewerComputerId,
         stamp = viewerSessionStamp, previewStatusGuard, requestPreviewGuard] {
            if (previewStatusGuard.isNull() || requestPreviewGuard.isNull()) return;
            requestObservationPreview(profileId, deployment, computerId, stamp,
                                      previewStatusGuard, requestPreviewGuard);
        });
}

MainWindow::ComputerInclusionPreference& MainWindow::computerInclusionPreference(
    const DeploymentId& deployment,
    const std::string& accountId)
{
    const QString key = computerInclusionScopeKey(deployment, accountId);
    const std::string mapKey = key.toStdString();
    const auto existing = computerInclusionByScope_.find(mapKey);
    if (existing != computerInclusionByScope_.end()) {
        return existing->second;
    }

    ComputerInclusionPreference preference;
    QSettings settings;
    settings.beginGroup(QStringLiteral("monitoringInclusionV1"));
    const QVariantMap stored = settings.value(key).toMap();
    settings.endGroup();
    if (stored.contains(QStringLiteral("allComputers"))) {
        preference.allComputers = stored.value(QStringLiteral("allComputers")).toBool();
        const QStringList ids = stored.value(QStringLiteral("computerIds")).toStringList();
        for (const QString& id : ids) {
            const std::string value = id.toStdString();
            if (helicoid::manager::validScopedEntityId(value)) {
                preference.computerIds.insert(value);
            }
        }
        if (preference.allComputers) {
            preference.computerIds.clear();
        }
    }
    if (stored.contains(QStringLiteral("panelVisible"))) {
        preference.panelVisible = stored.value(QStringLiteral("panelVisible")).toBool();
        preference.panelVisibilityConfigured = true;
    }
    const QString savedView = stored.value(QStringLiteral("computerView")).toString();
    if (savedView == QStringLiteral("grid") || savedView == QStringLiteral("list")) {
        preference.computerViewIndex = savedView == QStringLiteral("list") ? 1 : 0;
        preference.computerViewConfigured = true;
    }
    const int storedPreviewSizeIndex = computerPreviewSizeIndex(
        stored.value(QStringLiteral("previewSize")).toString());
    if (storedPreviewSizeIndex >= 0) {
        preference.previewSizeIndex = storedPreviewSizeIndex;
        preference.previewSizeConfigured = true;
    }
    return computerInclusionByScope_.emplace(mapKey, std::move(preference)).first->second;
}

bool MainWindow::saveComputerInclusionPreferences(const std::vector<std::string>& scopeKeys)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("monitoringInclusionV1"));
    for (const std::string& scopeKey : scopeKeys) {
        const auto preference = computerInclusionByScope_.find(scopeKey);
        if (preference == computerInclusionByScope_.end()) {
            continue;
        }
        QStringList computerIds;
        computerIds.reserve(static_cast<qsizetype>(preference->second.computerIds.size()));
        for (const std::string& id : preference->second.computerIds) {
            computerIds.append(QString::fromStdString(id));
        }
        computerIds.sort(Qt::CaseSensitive);
        QVariantMap stored;
        stored.insert(QStringLiteral("allComputers"), preference->second.allComputers);
        stored.insert(QStringLiteral("computerIds"), computerIds);
        if (preference->second.panelVisibilityConfigured) {
            stored.insert(QStringLiteral("panelVisible"), preference->second.panelVisible);
        }
        if (preference->second.computerViewConfigured) {
            stored.insert(QStringLiteral("computerView"),
                          preference->second.computerViewIndex == 1
                              ? QStringLiteral("list") : QStringLiteral("grid"));
        }
        if (preference->second.previewSizeConfigured) {
            stored.insert(QStringLiteral("previewSize"),
                          computerPreviewSizeName(preference->second.previewSizeIndex));
        }
        settings.setValue(QString::fromStdString(scopeKey), stored);
    }
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

void MainWindow::setComputerInclusionPanelVisible(bool visible)
{
    const bool previousVisible = computerInclusionPanelVisible_;
    computerInclusionPanelVisible_ = visible;
    if (computerInclusionPanel_ != nullptr) {
        computerInclusionPanel_->setVisible(visible);
    }
    std::unordered_set<std::string> seenScopes;
    std::vector<std::string> changedScopes;
    std::unordered_map<std::string, ComputerInclusionPreference> previous;
    for (const auto& [profileId, deployment] : deploymentsByProfile_) {
        Q_UNUSED(profileId);
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            continue;
        }
        const std::string scopeKey = computerInclusionScopeKey(deployment, session->accountId).toStdString();
        if (!seenScopes.insert(scopeKey).second) {
            continue;
        }
        ComputerInclusionPreference& preference = computerInclusionPreference(deployment, session->accountId);
        if (preference.panelVisibilityConfigured && preference.panelVisible == visible) {
            continue;
        }
        previous.emplace(scopeKey, preference);
        preference.panelVisible = visible;
        preference.panelVisibilityConfigured = true;
        changedScopes.push_back(scopeKey);
    }
    if (!changedScopes.empty() && !saveComputerInclusionPreferences(changedScopes)) {
        for (auto& [scopeKey, preference] : previous) {
            computerInclusionByScope_.insert_or_assign(scopeKey, std::move(preference));
        }
        computerInclusionPanelVisible_ = previousVisible;
        if (computerInclusionToggle_ != nullptr) {
            const QSignalBlocker toggleSignals(computerInclusionToggle_);
            computerInclusionToggle_->setChecked(previousVisible);
            computerInclusionToggle_->setArrowType(previousVisible ? Qt::DownArrow : Qt::RightArrow);
        }
        if (computerInclusionPanel_ != nullptr) {
            computerInclusionPanel_->setVisible(previousVisible);
        }
        statusBar()->showMessage(tr("The Computers panel preference could not be saved."), 7000);
    }
}

bool MainWindow::computerIsIncluded(const ComputerRow& computer)
{
    const auto* session = sessions_.session(computer.id.deployment);
    const auto stamp = sessions_.currentStamp(computer.id.deployment);
    if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
        session->profileId != computer.profileId ||
        session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
        return false;
    }
    const ComputerInclusionPreference& preference =
        computerInclusionPreference(computer.id.deployment, session->accountId);
    return preference.allComputers || preference.computerIds.contains(computer.id.serverComputerId);
}

void MainWindow::refreshComputerInclusionPanel()
{
    if (allComputersIncluded_ == nullptr || computerInclusionTree_ == nullptr) {
        return;
    }
    rebuildingComputerInclusionPanel_ = true;
    const QSignalBlocker allSignals(allComputersIncluded_);
    const QSignalBlocker treeSignals(computerInclusionTree_);
    computerInclusionTree_->clear();
    computerInclusionItemTargets_.clear();

    std::unordered_map<std::string, std::vector<const ComputerRow*>> currentByScope;
    std::unordered_map<std::string, std::unordered_set<std::string>> currentIdsByScope;
    for (const ComputerRow& computer : computers_) {
        const auto* session = sessions_.session(computer.id.deployment);
        const auto stamp = sessions_.currentStamp(computer.id.deployment);
        if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
            session->profileId != computer.profileId ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            continue;
        }
        const std::string scopeKey =
            computerInclusionScopeKey(computer.id.deployment, session->accountId).toStdString();
        currentByScope[scopeKey].push_back(&computer);
        currentIdsByScope[scopeKey].insert(computer.id.serverComputerId);
    }

    std::vector<std::string> prunedScopeKeys;
    bool panelShouldShow = false;
    for (const auto& [scopeKey, rows] : currentByScope) {
        const auto mapKey = computerInclusionByScope_.find(scopeKey);
        if (mapKey == computerInclusionByScope_.end()) {
            // Load this preference before pruning so the first inventory refresh
            // cannot mistake an explicit selection for the default All mode.
            const ComputerRow& computer = *currentByScope.at(scopeKey).front();
            const auto* session = sessions_.session(computer.id.deployment);
            if (session != nullptr) {
                computerInclusionPreference(computer.id.deployment, session->accountId);
            }
        }
        auto preference = computerInclusionByScope_.find(scopeKey);
        if (preference == computerInclusionByScope_.end()) {
            continue;
        }
        if (!preference->second.allComputers) {
            const std::size_t oldSize = preference->second.computerIds.size();
            const auto& currentIds = currentIdsByScope.at(scopeKey);
            std::erase_if(preference->second.computerIds, [&](const std::string& id) {
                return !currentIds.contains(id);
            });
            if (preference->second.computerIds.size() != oldSize) {
                prunedScopeKeys.push_back(scopeKey);
            }
        }
        const bool hasGroups = std::any_of(rows.begin(), rows.end(), [](const ComputerRow* computer) {
            return !computer->groups.empty();
        });
        if (!preference->second.panelVisibilityConfigured && hasGroups) {
            preference->second.panelVisible = true;
            preference->second.panelVisibilityConfigured = true;
            prunedScopeKeys.push_back(scopeKey);
        }
        panelShouldShow = panelShouldShow || preference->second.panelVisible;
    }
    if (!prunedScopeKeys.empty() && !saveComputerInclusionPreferences(prunedScopeKeys)) {
        statusBar()->showMessage(
            tr("Some saved computer inclusions could not be updated after the inventory changed."), 7000);
    }
    if (!currentByScope.empty()) {
        computerInclusionPanelVisible_ = panelShouldShow;
        const QSignalBlocker toggleSignals(computerInclusionToggle_);
        computerInclusionToggle_->setChecked(panelShouldShow);
        computerInclusionToggle_->setArrowType(panelShouldShow ? Qt::DownArrow : Qt::RightArrow);
        computerInclusionPanel_->setVisible(panelShouldShow);
    }

    struct DisplayGroup {
        std::string key;
        QString label;
        std::vector<const ComputerRow*> computers;
    };
    std::vector<DisplayGroup> displayGroups;
    std::unordered_map<std::string, std::size_t> displayGroupIndex;
    const auto addToGroup = [&](std::string key, QString label, const ComputerRow& computer) {
        auto index = displayGroupIndex.find(key);
        if (index == displayGroupIndex.end()) {
            const std::size_t next = displayGroups.size();
            displayGroupIndex.emplace(key, next);
            displayGroups.push_back(DisplayGroup{std::move(key), std::move(label), {}});
            index = displayGroupIndex.find(displayGroups.back().key);
        }
        displayGroups[index->second].computers.push_back(&computer);
    };
    for (const ComputerRow& computer : computers_) {
        const auto* session = sessions_.session(computer.id.deployment);
        const auto stamp = sessions_.currentStamp(computer.id.deployment);
        if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
            session->profileId != computer.profileId ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            continue;
        }
        const QString server = QString::fromStdString(computer.serverName);
        if (computer.groups.empty()) {
            const std::string key = "u:" + std::to_string(computer.id.deployment.value.size()) + ":" +
                computer.id.deployment.value;
            addToGroup(key, tr("Ungrouped Computers (%1)").arg(server), computer);
            continue;
        }
        for (const ComputerGroupRow& group : computer.groups) {
            const std::string deploymentId = group.id.deployment.value;
            const std::string groupId = group.id.serverEntityId;
            const std::string key = "g:" + std::to_string(deploymentId.size()) + ":" + deploymentId +
                std::to_string(groupId.size()) + ":" + groupId;
            addToGroup(key,
                       tr("%1 (%2)").arg(QString::fromStdString(group.name), server),
                       computer);
        }
    }
    std::sort(displayGroups.begin(), displayGroups.end(), [](const DisplayGroup& left,
                                                              const DisplayGroup& right) {
        const int order = QString::compare(left.label, right.label, Qt::CaseInsensitive);
        return order == 0 ? left.key < right.key : order < 0;
    });

    std::size_t authorizedCount = 0;
    std::size_t includedCount = 0;
    for (const auto& [scopeKey, rows] : currentByScope) {
        Q_UNUSED(scopeKey);
        for (const ComputerRow* computer : rows) {
            ++authorizedCount;
            if (computerIsIncluded(*computer)) {
                ++includedCount;
            }
        }
    }
    if (authorizedCount == 0) {
        allComputersIncluded_->setCheckState(Qt::Unchecked);
        allComputersIncluded_->setEnabled(false);
    } else {
        allComputersIncluded_->setEnabled(true);
        allComputersIncluded_->setCheckState(includedCount == 0
                                                  ? Qt::Unchecked
                                                  : includedCount == authorizedCount
                                                        ? Qt::Checked
                                                        : Qt::PartiallyChecked);
    }

    for (DisplayGroup& group : displayGroups) {
        std::sort(group.computers.begin(), group.computers.end(), [](const ComputerRow* left,
                                                                    const ComputerRow* right) {
            const int order = QString::compare(QString::fromStdString(left->friendlyName),
                                               QString::fromStdString(right->friendlyName),
                                               Qt::CaseInsensitive);
            return order == 0 ? left->id.serverComputerId < right->id.serverComputerId : order < 0;
        });
        auto* groupItem = new QTreeWidgetItem(computerInclusionTree_);
        groupItem->setText(0, group.label);
        groupItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        std::vector<helicoid::manager::ScopedComputerId> targets;
        std::size_t groupIncludedCount = 0;
        for (const ComputerRow* computer : group.computers) {
            auto* computerItem = new QTreeWidgetItem(groupItem);
            computerItem->setText(0, QString::fromStdString(computer->friendlyName));
            computerItem->setToolTip(0, tr("%1 — %2")
                                           .arg(QString::fromStdString(computer->serverName),
                                                QString::fromStdString(computer->id.serverComputerId)));
            computerItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            const bool included = computerIsIncluded(*computer);
            computerItem->setCheckState(0, included ? Qt::Checked : Qt::Unchecked);
            if (included) {
                ++groupIncludedCount;
            }
            computerInclusionItemTargets_[computerItem].push_back(computer->id);
            targets.push_back(computer->id);
        }
        const Qt::CheckState groupState = groupIncludedCount == 0
            ? Qt::Unchecked
            : groupIncludedCount == group.computers.size() ? Qt::Checked : Qt::PartiallyChecked;
        groupItem->setCheckState(0, groupState);
        computerInclusionItemTargets_.emplace(groupItem, std::move(targets));
    }
    computerInclusionTree_->expandAll();
    rebuildingComputerInclusionPanel_ = false;
}

void MainWindow::setAllComputerInclusion(bool included)
{
    std::unordered_set<std::string> seenScopes;
    std::vector<std::string> changedScopes;
    std::unordered_map<std::string, ComputerInclusionPreference> previous;
    for (const auto& [profileId, deployment] : deploymentsByProfile_) {
        Q_UNUSED(profileId);
        const auto* session = sessions_.session(deployment);
        const auto stamp = sessions_.currentStamp(deployment);
        if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            continue;
        }
        const std::string scopeKey = computerInclusionScopeKey(deployment, session->accountId).toStdString();
        if (!seenScopes.insert(scopeKey).second) {
            continue;
        }
        ComputerInclusionPreference& preference = computerInclusionPreference(deployment, session->accountId);
        previous.emplace(scopeKey, preference);
        preference.allComputers = included;
        preference.computerIds.clear();
        changedScopes.push_back(scopeKey);
    }
    if (!saveComputerInclusionPreferences(changedScopes)) {
        for (auto& [scopeKey, preference] : previous) {
            computerInclusionByScope_.insert_or_assign(scopeKey, std::move(preference));
        }
        statusBar()->showMessage(tr("Computer inclusion could not be saved; the previous selection was kept."), 7000);
    }
    refreshComputerTable();
}

void MainWindow::setComputerInclusionItems(QTreeWidgetItem* item)
{
    if (item == nullptr) {
        return;
    }
    const auto itemTargets = computerInclusionItemTargets_.find(item);
    if (itemTargets == computerInclusionItemTargets_.end() || itemTargets->second.empty()) {
        refreshComputerInclusionPanel();
        return;
    }
    const bool included = item->checkState(0) != Qt::Unchecked;
    std::unordered_map<std::string, std::vector<std::string>> idsByScope;
    std::unordered_map<std::string, std::pair<DeploymentId, std::string>> ownersByScope;
    for (const auto& target : itemTargets->second) {
        const auto* session = sessions_.session(target.deployment);
        const auto stamp = sessions_.currentStamp(target.deployment);
        if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
            session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
            continue;
        }
        const bool stillAuthorized = std::any_of(computers_.begin(), computers_.end(),
            [&](const ComputerRow& computer) {
                return computer.id == target && computer.profileId == session->profileId;
            });
        if (!stillAuthorized) {
            continue;
        }
        const std::string scopeKey = computerInclusionScopeKey(target.deployment, session->accountId).toStdString();
        ownersByScope.insert_or_assign(scopeKey, std::make_pair(target.deployment, session->accountId));
        idsByScope[scopeKey].push_back(target.serverComputerId);
    }

    std::unordered_map<std::string, ComputerInclusionPreference> previous;
    std::vector<std::string> changedScopes;
    for (auto& [scopeKey, computerIds] : idsByScope) {
        const auto owner = ownersByScope.find(scopeKey);
        if (owner == ownersByScope.end()) {
            continue;
        }
        ComputerInclusionPreference& preference =
            computerInclusionPreference(owner->second.first, owner->second.second);
        previous.emplace(scopeKey, preference);
        if (preference.allComputers) {
            if (included) {
                continue;
            }
            preference.allComputers = false;
            for (const ComputerRow& computer : computers_) {
                if (computer.id.deployment == owner->second.first &&
                    computer.profileId == sessions_.session(owner->second.first)->profileId) {
                    preference.computerIds.insert(computer.id.serverComputerId);
                }
            }
        }
        for (const std::string& computerId : computerIds) {
            if (included) {
                preference.computerIds.insert(computerId);
            } else {
                preference.computerIds.erase(computerId);
            }
        }
        if (preference.allComputers != previous.at(scopeKey).allComputers ||
            preference.computerIds != previous.at(scopeKey).computerIds) {
            changedScopes.push_back(scopeKey);
        }
    }
    if (!changedScopes.empty() && !saveComputerInclusionPreferences(changedScopes)) {
        for (auto& [scopeKey, preference] : previous) {
            computerInclusionByScope_.insert_or_assign(scopeKey, std::move(preference));
        }
        statusBar()->showMessage(tr("Computer inclusion could not be saved; the previous selection was kept."), 7000);
    }
    refreshComputerTable();
}

void MainWindow::refreshComputerTable()
{
    if (computerTable_ == nullptr || computerGrid_ == nullptr) {
        return;
    }
    const QString serverFilter = computerServerFilter_ == nullptr
        ? QString()
        : computerServerFilter_->currentData().toString();
    std::optional<int> scopedComputerView;
    std::optional<int> scopedPreviewSize;
    if (!serverFilter.isEmpty()) {
        for (const auto& [profileId, deployment] : deploymentsByProfile_) {
            if (profileId != serverFilter.toStdString()) {
                continue;
            }
            const auto* session = sessions_.session(deployment);
            const auto stamp = sessions_.currentStamp(deployment);
            if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
                session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
                continue;
            }
            const ComputerInclusionPreference& preference =
                computerInclusionPreference(deployment, session->accountId);
            scopedComputerView = preference.computerViewConfigured
                ? preference.computerViewIndex
                : (QSettings().value(QStringLiteral("computerViewV1")).toString() ==
                       QStringLiteral("list") ? 1 : 0);
            if (preference.previewSizeConfigured) {
                scopedPreviewSize = preference.previewSizeIndex;
            } else {
                const int legacySize = computerPreviewSizeIndex(
                    QSettings().value(QStringLiteral("computerPreviewSizeV1")).toString());
                scopedPreviewSize = legacySize >= 0 ? legacySize : 1;
            }
            break;
        }
    } else {
        std::optional<int> commonSavedView;
        bool conflictingSavedViews = false;
        std::optional<int> commonSavedSize;
        std::size_t activeScopeCount = 0;
        bool conflictingSavedSizes = false;
        std::unordered_set<std::string> seenScopes;
        for (const auto& [profileId, deployment] : deploymentsByProfile_) {
            Q_UNUSED(profileId);
            const auto* session = sessions_.session(deployment);
            const auto stamp = sessions_.currentStamp(deployment);
            if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
                session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
                continue;
            }
            const std::string scopeKey = computerInclusionScopeKey(
                deployment, session->accountId).toStdString();
            if (!seenScopes.insert(scopeKey).second) {
                continue;
            }
            ++activeScopeCount;
            const ComputerInclusionPreference& preference =
                computerInclusionPreference(deployment, session->accountId);
            if (preference.computerViewConfigured) {
                if (!commonSavedView) {
                    commonSavedView = preference.computerViewIndex;
                } else if (*commonSavedView != preference.computerViewIndex) {
                    conflictingSavedViews = true;
                }
            }
            if (!preference.previewSizeConfigured) {
                continue;
            }
            if (!commonSavedSize) {
                commonSavedSize = preference.previewSizeIndex;
            } else if (*commonSavedSize != preference.previewSizeIndex) {
                conflictingSavedSizes = true;
            }
        }
        if (commonSavedSize && !conflictingSavedSizes) {
            scopedPreviewSize = commonSavedSize;
        } else if (activeScopeCount == 1 && !commonSavedSize) {
            scopedPreviewSize = computerPreviewSizeIndex(
                QSettings().value(QStringLiteral("computerPreviewSizeV1")).toString());
            if (scopedPreviewSize && *scopedPreviewSize < 0) {
                scopedPreviewSize.reset();
            }
            if (!scopedPreviewSize) {
                scopedPreviewSize = 1;
            }
        }
        if (commonSavedView && !conflictingSavedViews) {
            scopedComputerView = commonSavedView;
        } else if (activeScopeCount == 1 && !commonSavedView) {
            scopedComputerView = QSettings().value(QStringLiteral("computerViewV1")).toString() ==
                QStringLiteral("list") ? 1 : 0;
        }
    }
    if (scopedComputerView && *scopedComputerView != computerLayoutStack_->currentIndex()) {
        applyComputerView(*scopedComputerView, false);
    }
    if (scopedPreviewSize && *scopedPreviewSize != computerPreviewSizeIndex_) {
        applyComputerPreviewSize(*scopedPreviewSize, false);
    }
    refreshComputerInclusionPanel();
    computerTable_->setRowCount(static_cast<int>(computers_.size()));
    for (int index = 0; index < computerGrid_->count(); ++index) {
        if (QWidget* tileWidget = computerGrid_->itemWidget(computerGrid_->item(index))) {
            stopOverviewPreview(tileWidget);
        }
    }
    computerGrid_->clear();
    const QString search = computerSearch_ == nullptr ? QString() : computerSearch_->text().trimmed();
    const QString statusFilter = computerStatusFilter_ == nullptr
        ? QString()
        : computerStatusFilter_->currentData().toString();
    const QString groupFilter = computerGroupFilter_ == nullptr
        ? QString()
        : computerGroupFilter_->currentData().toString();
    for (int row = 0; row < static_cast<int>(computers_.size()); ++row) {
        const auto& computer = computers_[static_cast<std::size_t>(row)];
        const QString serverName = QString::fromStdString(computer.serverName);
        const QString friendlyName = QString::fromStdString(computer.friendlyName);
        const QString platform = QString::fromStdString(computer.platform);
        const QString enrollmentState = QString::fromStdString(computer.enrollmentState);
        const QString profileId = QString::fromStdString(computer.profileId);
        const QString deploymentId = QString::fromStdString(computer.id.deployment.value);
        QString endpointSessionText = tr("Not available");
        if (computer.endpointSessionReported && computer.endpointSession) {
            const auto& endpointSession = *computer.endpointSession;
            if (!endpointSession.lastAuthenticatedAtUnix) {
                endpointSessionText = tr("No endpoint proof");
            } else {
                const QString lastProof = QDateTime::fromSecsSinceEpoch(
                    *endpointSession.lastAuthenticatedAtUnix).toLocalTime()
                    .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                if (endpointSession.currentSessionExpiresAtUnix && endpointSession.consoleUserReported) {
                    const QString expiry = QDateTime::fromSecsSinceEpoch(
                        *endpointSession.currentSessionExpiresAtUnix).toLocalTime()
                        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                endpointSessionText = tr("Last key proof %1; session valid until %2; console user reported %3")
                        .arg(lastProof, expiry,
                             *endpointSession.consoleUserReported ? tr("present") : tr("absent"));
                } else {
                    endpointSessionText = tr("Last key proof %1; no current session").arg(lastProof);
                }
            }
        }
        QStringList groupNames;
        groupNames.reserve(static_cast<qsizetype>(computer.groups.size()));
        for (const ComputerGroupRow& group : computer.groups) {
            groupNames.append(QString::fromStdString(group.name));
        }
        const QString groupNamesText = groupNames.isEmpty() ? tr("None") : groupNames.join(QStringLiteral(", "));
        const bool matchesGroup = groupFilter.isEmpty() ||
            std::any_of(computer.groups.begin(), computer.groups.end(), [&](const ComputerGroupRow& group) {
                const QString groupKey = QString::fromStdString(group.id.deployment.value) +
                    QStringLiteral("/") + QString::fromStdString(group.id.serverEntityId);
                return groupKey == groupFilter;
            });
        auto* server = new QTableWidgetItem(serverName);
        server->setData(Qt::UserRole, profileId);
        server->setData(Qt::UserRole + 1, deploymentId);
        computerTable_->setItem(row, 0, server);
        auto* computerItem = new QTableWidgetItem(friendlyName);
        computerItem->setData(Qt::UserRole + 2,
                              QString::fromStdString(computer.id.serverComputerId));
        computerTable_->setItem(row, 1, computerItem);
        computerTable_->setItem(row, 2, new QTableWidgetItem(platform));
        computerTable_->setItem(row, 3, new QTableWidgetItem(enrollmentState));
        computerTable_->setItem(row, 4, new QTableWidgetItem(groupNamesText));
        computerTable_->setItem(row, 5, new QTableWidgetItem(endpointSessionText));
        auto* tile = new QListWidgetItem(friendlyName, computerGrid_);
        const auto metrics = computerPreviewMetrics(computerPreviewSizeIndex_);
        tile->setSizeHint(QSize(metrics.gridCell.width, metrics.gridCell.height));
        tile->setToolTip(tr("Groups: %1\nEndpoint session: %2")
                             .arg(groupNamesText, endpointSessionText));
        tile->setData(Qt::UserRole, profileId);
        tile->setData(Qt::UserRole + 1, deploymentId);
        tile->setData(Qt::UserRole + 2,
                      QString::fromStdString(computer.id.serverComputerId));
        const bool matchesServer = serverFilter.isEmpty() || serverFilter == profileId;
        const bool matchesSearch = search.isEmpty() ||
            friendlyName.contains(search, Qt::CaseInsensitive) ||
            serverName.contains(search, Qt::CaseInsensitive) ||
            platform.contains(search, Qt::CaseInsensitive) ||
            enrollmentState.contains(search, Qt::CaseInsensitive) ||
            groupNamesText.contains(search, Qt::CaseInsensitive);
        const bool matchesStatus = statusFilter.isEmpty() || statusFilter == enrollmentState;
        const bool visible = computerIsIncluded(computer) && matchesServer && matchesSearch &&
            matchesStatus && matchesGroup;
        computerTable_->setRowHidden(row, !visible);
        tile->setHidden(!visible);
    }
}

void MainWindow::applyComputerView(int index, bool persist)
{
    if ((index != 0 && index != 1) || computerLayoutStack_ == nullptr) {
        return;
    }

    const int previousIndex = computerLayoutStack_->currentIndex();
    if (persist) {
        std::vector<std::pair<DeploymentId, std::string>> targetScopes;
        const QString selectedProfile = computerServerFilter_ == nullptr
            ? QString()
            : computerServerFilter_->currentData().toString();
        for (const auto& [profileId, deployment] : deploymentsByProfile_) {
            if (!selectedProfile.isEmpty() && selectedProfile.toStdString() != profileId) {
                continue;
            }
            const auto* session = sessions_.session(deployment);
            const auto stamp = sessions_.currentStamp(deployment);
            if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
                session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
                continue;
            }
            targetScopes.emplace_back(deployment, session->accountId);
        }

        std::unordered_map<std::string, ComputerInclusionPreference> previousPreferences;
        std::vector<std::string> changedScopes;
        changedScopes.reserve(targetScopes.size());
        for (const auto& [deployment, accountId] : targetScopes) {
            const std::string scopeKey = computerInclusionScopeKey(deployment, accountId).toStdString();
            if (previousPreferences.contains(scopeKey)) {
                continue;
            }
            ComputerInclusionPreference& preference =
                computerInclusionPreference(deployment, accountId);
            previousPreferences.emplace(scopeKey, preference);
            preference.computerViewIndex = index;
            preference.computerViewConfigured = true;
            changedScopes.push_back(scopeKey);
        }

        if (changedScopes.empty() || !saveComputerInclusionPreferences(changedScopes)) {
            for (auto& [scopeKey, preference] : previousPreferences) {
                computerInclusionByScope_.insert_or_assign(scopeKey, std::move(preference));
            }
            index = previousIndex;
            statusBar()->showMessage(
                changedScopes.empty()
                    ? tr("Sign in to a server before saving a scoped computer view.")
                    : tr("The computer view could not be saved to this user's application settings."),
                6000);
        }
    }

    computerLayoutStack_->setCurrentIndex(index);
    if (auto* grid = findChild<QAction*>(QStringLiteral("actionComputerGrid"))) {
        grid->setChecked(index == 0);
    }
    if (auto* list = findChild<QAction*>(QStringLiteral("actionComputerList"))) {
        list->setChecked(index == 1);
    }
}

void MainWindow::applyComputerPreviewSize(int index, bool persist)
{
    if (index < 0 || index > 3) {
        return;
    }

    const int previousIndex = computerPreviewSizeIndex_;
    if (persist) {
        std::vector<std::pair<DeploymentId, std::string>> targetScopes;
        const QString selectedProfile = computerServerFilter_ == nullptr
            ? QString()
            : computerServerFilter_->currentData().toString();
        for (const auto& [profileId, deployment] : deploymentsByProfile_) {
            if (!selectedProfile.isEmpty() &&
                selectedProfile.toStdString() != profileId) {
                continue;
            }
            const auto* session = sessions_.session(deployment);
            const auto stamp = sessions_.currentStamp(deployment);
            if (session == nullptr || !stamp || !sessions_.isCurrent(*stamp) ||
                session->expiresAtUnix <= QDateTime::currentSecsSinceEpoch()) {
                continue;
            }
            targetScopes.emplace_back(deployment, session->accountId);
        }

        std::unordered_map<std::string, ComputerInclusionPreference> previousPreferences;
        std::vector<std::string> changedScopes;
        changedScopes.reserve(targetScopes.size());
        for (const auto& [deployment, accountId] : targetScopes) {
            const std::string scopeKey = computerInclusionScopeKey(deployment, accountId).toStdString();
            if (previousPreferences.contains(scopeKey)) {
                continue;
            }
            ComputerInclusionPreference& preference =
                computerInclusionPreference(deployment, accountId);
            previousPreferences.emplace(scopeKey, preference);
            preference.previewSizeIndex = index;
            preference.previewSizeConfigured = true;
            changedScopes.push_back(scopeKey);
        }

        if (changedScopes.empty() || !saveComputerInclusionPreferences(changedScopes)) {
            for (auto& [scopeKey, preference] : previousPreferences) {
                computerInclusionByScope_.insert_or_assign(scopeKey, std::move(preference));
            }
            index = previousIndex;
            statusBar()->showMessage(
                changedScopes.empty()
                    ? tr("Sign in to a server before saving a scoped preview size.")
                    : tr("The computer preview size could not be saved to this user's application settings."),
                6000);
        }
    }

    const bool sizeChanged = index != computerPreviewSizeIndex_;
    computerPreviewSizeIndex_ = index;
    const auto metrics = computerPreviewMetrics(index);
    if (sizeChanged && computerGrid_ != nullptr) {
        for (int itemIndex = 0; itemIndex < computerGrid_->count(); ++itemIndex) {
            QListWidgetItem* item = computerGrid_->item(itemIndex);
            if (QWidget* tileWidget = computerGrid_->itemWidget(item)) {
                stopOverviewPreview(tileWidget);
            }
            item->setSizeHint(QSize(metrics.gridCell.width, metrics.gridCell.height));
        }
        computerGrid_->setGridSize(QSize(metrics.gridCell.width, metrics.gridCell.height));
    }
    if (auto* compact = findChild<QAction*>(QStringLiteral("actionPreviewSizeCompact"))) {
        compact->setChecked(index == 0);
    }
    if (auto* standard = findChild<QAction*>(QStringLiteral("actionPreviewSizeStandard"))) {
        standard->setChecked(index == 1);
    }
    if (auto* large = findChild<QAction*>(QStringLiteral("actionPreviewSizeLarge"))) {
        large->setChecked(index == 2);
    }
    if (auto* extraLarge = findChild<QAction*>(QStringLiteral("actionPreviewSizeExtraLarge"))) {
        extraLarge->setChecked(index == 3);
    }
}

void MainWindow::stopOverviewPreview(QWidget* tileWidget)
{
    if (tileWidget == nullptr) {
        return;
    }
    const auto statusLabels = tileWidget->findChildren<QLabel*>(QStringLiteral("overviewPreviewStatus"));
    for (QLabel* statusLabel : statusLabels) {
        statusLabel->setProperty("observationPreviewOccupied", false);
        for (QTimer* timer : statusLabel->findChildren<QTimer*>()) {
            timer->stop();
            timer->deleteLater();
        }
        for (QObject* child : statusLabel->children()) {
            auto* owner = dynamic_cast<ObservationPeerOwner*>(child);
            if (owner == nullptr) continue;
            if (owner->peer() != nullptr &&
                !owner->property("observationOfferCreating").toBool()) {
                (void)owner->peer()->endObservation();
            }
            owner->deleteLater();
        }
    }

    if (computerGrid_ != nullptr) {
        for (int index = 0; index < computerGrid_->count(); ++index) {
            QListWidgetItem* item = computerGrid_->item(index);
            if (computerGrid_->itemWidget(item) == tileWidget) {
                computerGrid_->removeItemWidget(item);
                break;
            }
        }
    }
    delete tileWidget;
}

std::size_t MainWindow::activeObservationPreviewCount() const
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    std::unordered_set<std::string> activeCooldownKeys;
    std::size_t count = 0;
    for (QLabel* label : findChildren<QLabel*>()) {
        if (!label->property("observationPreviewOccupied").toBool()) {
            continue;
        }
        ++count;
        const QString key = label->property("observationPreviewCooldownKey").toString();
        if (!key.isEmpty()) {
            activeCooldownKeys.insert(key.toStdString());
        }
    }
    for (const auto& [key, expiresAtMs] : observationPreviewCooldowns_) {
        if (expiresAtMs > nowMs && !activeCooldownKeys.contains(key)) {
            ++count;
        }
    }
    return count;
}

void MainWindow::scheduleOverviewPreviews()
{
    if (computerGrid_ == nullptr) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (auto iterator = observationPreviewCooldowns_.begin();
         iterator != observationPreviewCooldowns_.end();) {
        if (iterator->second <= nowMs) {
            iterator = observationPreviewCooldowns_.erase(iterator);
        } else {
            ++iterator;
        }
    }

    const bool overviewVisible = isVisible() && !isMinimized() && workspaceStack_ != nullptr &&
        workspaceStack_->currentIndex() == 0 && computerContentStack_ != nullptr &&
        computerContentStack_->currentWidget() == computerLayoutStack_ &&
        computerLayoutStack_ != nullptr && computerLayoutStack_->currentWidget() == computerGrid_ &&
        computerGrid_->isVisible();
    if (!overviewVisible) {
        for (int index = 0; index < computerGrid_->count(); ++index) {
            if (QWidget* tileWidget = computerGrid_->itemWidget(computerGrid_->item(index))) {
                stopOverviewPreview(tileWidget);
            }
        }
        return;
    }

    std::size_t activeCount = activeObservationPreviewCount();

    const QRect viewportRect = computerGrid_->viewport()->rect();
    for (int index = 0; index < computerGrid_->count(); ++index) {
        QListWidgetItem* item = computerGrid_->item(index);
        const QRect itemRect = computerGrid_->visualItemRect(item);
        const bool itemVisible = !item->isHidden() && itemRect.isValid() &&
            itemRect.intersects(viewportRect);
        QWidget* tileWidget = computerGrid_->itemWidget(item);
        if (!itemVisible) {
            if (tileWidget != nullptr) {
                stopOverviewPreview(tileWidget);
                activeCount = activeObservationPreviewCount();
            }
            continue;
        }

        const QString profileId = item->data(Qt::UserRole).toString();
        const DeploymentId deployment{item->data(Qt::UserRole + 1).toString().toStdString()};
        const std::string computerId = item->data(Qt::UserRole + 2).toString().toStdString();
        const auto viewerKey = ScopedViewerId::create(deployment, computerId);
        if (viewerKey) {
            const auto viewer = viewerWindows_.find(*viewerKey);
            if (viewer != viewerWindows_.end() && !viewer->second.isNull()) {
                if (tileWidget != nullptr) {
                    stopOverviewPreview(tileWidget);
                    activeCount = activeObservationPreviewCount();
                }
                continue;
            }
        }
        const auto computer = std::find_if(computers_.begin(), computers_.end(),
            [&](const ComputerRow& row) {
                return row.profileId == profileId.toStdString() &&
                    row.id.deployment == deployment && row.id.serverComputerId == computerId;
            });

        if (tileWidget == nullptr) {
            const ComputerPreviewMetrics metrics = computerPreviewMetrics(computerPreviewSizeIndex_);
            tileWidget = new QWidget(computerGrid_->viewport());
            tileWidget->setObjectName(QStringLiteral("computerOverviewTile"));
            auto* tileLayout = new QVBoxLayout(tileWidget);
            tileLayout->setContentsMargins(6, 6, 6, 6);
            tileLayout->setSpacing(5);
            auto* previewStatus = new QLabel(tr("Preview starts when authorized and visible."), tileWidget);
            previewStatus->setObjectName(QStringLiteral("overviewPreviewStatus"));
            previewStatus->setAlignment(Qt::AlignCenter);
            previewStatus->setWordWrap(true);
            previewStatus->setFixedHeight(metrics.previewHeight);
            previewStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            previewStatus->setStyleSheet(QStringLiteral(
                "border: 1px solid palette(mid); border-radius: 4px; background: palette(base);"));
            previewStatus->setProperty("overviewPreviewTile", true);
            previewStatus->setProperty("overviewProfileId", profileId);
            previewStatus->setProperty("overviewDeploymentId", QString::fromStdString(deployment.value));
            previewStatus->setProperty("overviewComputerId", QString::fromStdString(computerId));
            tileLayout->addWidget(previewStatus);

            auto* details = new QLabel(tileWidget);
            details->setObjectName(QStringLiteral("computerOverviewDetails"));
            details->setWordWrap(true);
            details->setMaximumHeight(metrics.detailsHeight);
            if (computer != computers_.end()) {
                details->setText(tr("%1 · %2\n%3 · %4")
                    .arg(QString::fromStdString(computer->friendlyName),
                         QString::fromStdString(computer->serverName),
                         QString::fromStdString(computer->platform),
                         QString::fromStdString(computer->enrollmentState)));
            } else {
                details->setText(tr("Computer is no longer in the authorized inventory."));
            }
            tileLayout->addWidget(details);

            auto* actions = new QWidget(tileWidget);
            auto* actionLayout = new QHBoxLayout(actions);
            actionLayout->setContentsMargins(0, 0, 0, 0);
            actionLayout->setSpacing(5);
            auto* openViewer = new QPushButton(tr("Open view"), actions);
            openViewer->setObjectName(QStringLiteral("openComputerViewer"));
            auto* retryPreview = new QPushButton(tr("Retry preview"), actions);
            retryPreview->setObjectName(QStringLiteral("retryObservationPreview"));
            retryPreview->setVisible(false);
            actionLayout->addWidget(openViewer);
            actionLayout->addWidget(retryPreview);
            tileLayout->addWidget(actions);
            tileWidget->setFixedSize(QSize(metrics.tile.width, metrics.tile.height));
            computerGrid_->setItemWidget(item, tileWidget);

            connect(openViewer, &QPushButton::clicked, this,
                    [this, profileId, deploymentId = QString::fromStdString(deployment.value),
                     computerIdentity = QString::fromStdString(computerId)] {
                QTimer::singleShot(0, this, [this, profileId, deploymentId, computerIdentity] {
                    if (computerGrid_ == nullptr) {
                        return;
                    }
                    for (int itemIndex = 0; itemIndex < computerGrid_->count(); ++itemIndex) {
                        QListWidgetItem* candidate = computerGrid_->item(itemIndex);
                        if (!candidate->isHidden() &&
                            candidate->data(Qt::UserRole).toString() == profileId &&
                            candidate->data(Qt::UserRole + 1).toString() == deploymentId &&
                            candidate->data(Qt::UserRole + 2).toString() == computerIdentity) {
                            computerGrid_->setCurrentItem(candidate);
                            openSelectedComputerViewer();
                            return;
                        }
                    }
                });
            });
            connect(retryPreview, &QPushButton::clicked, this,
                    [this, retryPreview, previewStatus, profileId, deployment, computerId] {
                const auto stamp = sessions_.currentStamp(deployment);
                if (!stamp) {
                    setObservationPreviewFailure(
                        QPointer<QLabel>(previewStatus), QPointer<QPushButton>(retryPreview),
                        tr("Sign in to the owning server before requesting a preview."));
                    return;
                }
                previewStatus->setProperty("observationPreviewRetryAfterUnixMs", 0);
                requestObservationPreview(profileId, deployment, computerId, *stamp,
                                          QPointer<QLabel>(previewStatus),
                                          QPointer<QPushButton>(retryPreview));
            });
        }

        auto* previewStatus = tileWidget->findChild<QLabel*>(QStringLiteral("overviewPreviewStatus"));
        auto* retryPreview = tileWidget->findChild<QPushButton*>(QStringLiteral("retryObservationPreview"));
        if (previewStatus == nullptr) {
            continue;
        }

        const auto stamp = sessions_.currentStamp(deployment);
        const auto profile = registry_.profile(profileId.toStdString());
        const auto* session = sessions_.session(deployment);
        const bool currentSession = stamp && sessions_.isCurrent(*stamp) && profile && session &&
            session->profileId == profileId.toStdString() &&
            session->expiresAtUnix > QDateTime::currentSecsSinceEpoch() &&
            profile->trustedDeploymentId == deployment.value &&
            hasTrustedDeploymentAuthority(*profile) &&
            !serverAuthorityMismatches_.contains(profileId.toStdString());
        const auto scopeNow = helicoid::manager::ObservationScopeRegistry::Clock::now();
        const bool currentScope = currentSession && observationScopes_.hasCurrent(*stamp, scopeNow);
        if (currentScope) {
            observationScopeRefreshRetryAfterMs_.erase(deployment.value);
        }
        const bool authorized = computer != computers_.end() &&
            computer->enrollmentState == "enrolled" && computerIsIncluded(*computer) &&
            currentScope && observationScopes_.allows(*stamp, computerId, scopeNow);

        if (!currentSession || !authorized) {
            if (previewStatus->property("observationPreviewOccupied").toBool()) {
                stopOverviewPreview(tileWidget);
                activeCount = activeObservationPreviewCount();
                continue;
            }
            if (retryPreview != nullptr) {
                retryPreview->setVisible(false);
            }
            if (computer == computers_.end()) {
                previewStatus->setText(tr("Computer is no longer in the authorized inventory."));
            } else if (computer->enrollmentState != "enrolled") {
                previewStatus->setText(tr("This computer is not enrolled for a live preview."));
            } else if (!currentSession) {
                previewStatus->setText(tr("Sign in to the owning server to start a preview."));
            } else if (!currentScope) {
                const auto retryAfter = observationScopeRefreshRetryAfterMs_.find(deployment.value);
                if (retryAfter == observationScopeRefreshRetryAfterMs_.end() ||
                    retryAfter->second <= nowMs) {
                    observationScopeRefreshRetryAfterMs_.insert_or_assign(
                        deployment.value, nowMs + 5000);
                    loadObservationScope(profileId.toStdString(), deployment, *stamp, false);
                }
                previewStatus->setText(tr("Checking current observe permission…"));
            } else {
                previewStatus->setText(tr("Your account has no current observe permission."));
            }
            continue;
        }

        if (previewStatus->property("observationPreviewOccupied").toBool()) {
            continue;
        }
        const QString cooldownKey = observationPreviewCooldownKey(deployment, computerId);
        const auto cooldown = observationPreviewCooldowns_.find(cooldownKey.toStdString());
        if (cooldown != observationPreviewCooldowns_.end() && cooldown->second > nowMs) {
            previewStatus->setText(tr("Waiting for the previous short-lived preview request to expire…"));
            if (retryPreview != nullptr) {
                retryPreview->setEnabled(false);
            }
            continue;
        }
        if (retryPreview != nullptr) {
            retryPreview->setEnabled(true);
        }
        if (nowMs < previewStatus->property("observationPreviewRetryAfterUnixMs").toLongLong()) {
            continue;
        }
        if (activeCount >= maxConcurrentObservationPreviews) {
            previewStatus->setText(tr("Waiting for an available preview slot…"));
            continue;
        }

        requestObservationPreview(profileId, deployment, computerId, *stamp,
                                  QPointer<QLabel>(previewStatus),
                                  QPointer<QPushButton>(retryPreview));
        ++activeCount;
    }
}

void MainWindow::loadProfiles()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("serverProfiles"));
    const QByteArray data = settings.value(QStringLiteral("profiles_v1")).toByteArray();
    settings.endGroup();
    if (data.isEmpty()) {
        return;
    }

    std::vector<ConnectionProfile> profiles;
    QString error;
    if (!parseProfiles(data, profiles, error)) {
        profileLoadWarning_ = tr("Saved server profiles could not be loaded: %1").arg(error);
        return;
    }
    settings.beginGroup(QStringLiteral("serverProfileTrust"));
    for (auto& profile : profiles) {
        const QString trustedId = settings.value(QString::fromStdString(profile.profileId)).toString();
        if (validDeploymentIdentity(trustedId)) {
            profile.trustedDeploymentId = trustedId.toStdString();
        }
    }
    settings.endGroup();
    settings.beginGroup(QStringLiteral("serverAuthorityKeyTrust"));
    for (auto& profile : profiles) {
        if (!profile.trustedDeploymentId.empty()) {
            const QString trustedKey = settings.value(QString::fromStdString(profile.profileId)).toString();
            if (helicoid::manager::validGrantSigningPublicKey(trustedKey.toStdString())) {
                profile.trustedGrantSigningPublicKey = trustedKey.toStdString();
            }
        }
    }
    settings.endGroup();
    settings.beginGroup(QStringLiteral("serverAuthorityGenerationTrust"));
    for (auto& profile : profiles) {
        if (helicoid::manager::validGrantSigningPublicKey(
                profile.trustedGrantSigningPublicKey)) {
            bool generationOk = false;
            const qulonglong generation = settings
                .value(QString::fromStdString(profile.profileId))
                .toULongLong(&generationOk);
            if (generationOk && generation > 0) {
                profile.trustedAuthorityGeneration = generation;
            }
        }
    }
    settings.endGroup();

    for (auto& profile : profiles) {
        std::string addError;
        if (!registry_.addProfile(std::move(profile), addError)) {
            profileLoadWarning_ = tr("Saved server profiles could not be loaded: %1")
                                      .arg(QString::fromStdString(addError));
            registry_.disconnectAll();
            return;
        }
    }
}

bool MainWindow::saveProfiles() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("serverProfiles"));
    settings.setValue(QStringLiteral("profiles_v1"), encodeProfiles(registry_.profiles()));
    settings.endGroup();
    const auto profiles = registry_.profiles();
    settings.beginGroup(QStringLiteral("serverProfileTrust"));
    settings.remove(QString());
    for (const auto& profile : profiles) {
        if (!profile.trustedDeploymentId.empty()) {
            settings.setValue(QString::fromStdString(profile.profileId),
                              QString::fromStdString(profile.trustedDeploymentId));
        }
    }
    settings.endGroup();
    settings.beginGroup(QStringLiteral("serverAuthorityKeyTrust"));
    settings.remove(QString());
    for (const auto& profile : profiles) {
        if (hasTrustedDeploymentAuthority(profile)) {
            settings.setValue(QString::fromStdString(profile.profileId),
                              QString::fromStdString(profile.trustedGrantSigningPublicKey));
        }
    }
    settings.endGroup();
    settings.beginGroup(QStringLiteral("serverAuthorityGenerationTrust"));
    settings.remove(QString());
    for (const auto& profile : profiles) {
        if (hasTrustedDeploymentAuthority(profile)) {
            settings.setValue(QString::fromStdString(profile.profileId),
                              QVariant::fromValue<qulonglong>(profile.trustedAuthorityGeneration));
        }
    }
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

void MainWindow::refreshServerTable()
{
    if (serverTable_ == nullptr || serverContentStack_ == nullptr || computerContentStack_ == nullptr) {
        return;
    }
    const auto selectedId = selectedProfileId();
    const QSignalBlocker tableSignals(serverTable_);
    const auto profiles = registry_.profiles();
    serverContentStack_->setCurrentIndex(profiles.empty() ? 0 : 1);
    bool hasSession = false;
    for (const auto& sessionProfile : deploymentsByProfile_) {
        const auto* session = sessions_.session(sessionProfile.second);
        if (session != nullptr && session->expiresAtUnix > QDateTime::currentSecsSinceEpoch()) {
            hasSession = true;
            break;
        }
    }
    computerContentStack_->setCurrentIndex(profiles.empty() ? 0 : (hasSession ? 2 : 1));
    serverTable_->setRowCount(static_cast<int>(profiles.size()));
    for (int row = 0; row < static_cast<int>(profiles.size()); ++row) {
        const auto& profile = profiles[static_cast<std::size_t>(row)];
        auto* name = new QTableWidgetItem(QString::fromStdString(profile.displayName));
        name->setData(Qt::UserRole, QString::fromStdString(profile.profileId));
        serverTable_->setItem(row, 0, name);
        const auto deployment = deploymentsByProfile_.find(profile.profileId);
        const bool authorityMismatch = serverAuthorityMismatches_.contains(profile.profileId);
        const QString identityState = authorityMismatch
            ? tr("Authority mismatch; verify")
            : (deployment == deploymentsByProfile_.end()
                   ? (!hasTrustedDeploymentAuthority(profile)
                          ? tr("Verify identity and key")
                          : QString::fromStdString(profile.trustedDeploymentId))
                   : QString::fromStdString(deployment->second.value));
        auto* identityItem = new QTableWidgetItem(identityState);
        if (hasTrustedDeploymentAuthority(profile)) {
            identityItem->setToolTip(
                tr("Locally saved authority checkpoint: generation %1. This Manager profile adopted the value after independent verification.")
                    .arg(profile.trustedAuthorityGeneration));
        }
        serverTable_->setItem(row, 1, identityItem);
        serverTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(profile.endpoint)));
        QString state = availabilityText(profile.availability);
        if (deployment != deploymentsByProfile_.end()) {
            if (const auto* session = sessions_.session(deployment->second);
                session != nullptr && session->expiresAtUnix > QDateTime::currentSecsSinceEpoch()) {
                state = tr("Signed in as %1").arg(QString::fromStdString(session->username));
            }
        }
        serverTable_->setItem(row, 3, new QTableWidgetItem(state));
        if (selectedId && *selectedId == profile.profileId) {
            serverTable_->selectRow(row);
        }
    }
    refreshServerDetails();

    if (computerServerFilter_ != nullptr) {
        const QString selectedFilter = computerServerFilter_->currentData().toString();
        const QSignalBlocker blocker(computerServerFilter_);
        computerServerFilter_->clear();
        computerServerFilter_->addItem(tr("All servers"), QString());
        for (const auto& profile : profiles) {
            computerServerFilter_->addItem(QString::fromStdString(profile.displayName),
                                           QString::fromStdString(profile.profileId));
        }
        const int index = computerServerFilter_->findData(selectedFilter);
        computerServerFilter_->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (accountsServerFilter_ != nullptr) {
        const QString selectedFilter = accountsServerFilter_->currentData().toString();
        const QSignalBlocker blocker(accountsServerFilter_);
        accountsServerFilter_->clear();
        accountsServerFilter_->addItem(tr("All servers"), QString());
        for (const auto& profile : profiles) {
            accountsServerFilter_->addItem(QString::fromStdString(profile.displayName),
                                           QString::fromStdString(profile.profileId));
        }
        const int index = accountsServerFilter_->findData(selectedFilter);
        accountsServerFilter_->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (filesServerFilter_ != nullptr) {
        const QString selectedFilter = filesServerFilter_->currentData().toString();
        const QSignalBlocker blocker(filesServerFilter_);
        filesServerFilter_->clear();
        filesServerFilter_->addItem(tr("All servers"), QString());
        for (const auto& profile : profiles) {
            filesServerFilter_->addItem(QString::fromStdString(profile.displayName),
                                        QString::fromStdString(profile.profileId));
        }
        const int index = filesServerFilter_->findData(selectedFilter);
        filesServerFilter_->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (computerStatusFilter_ != nullptr) {
        const QString selectedStatus = computerStatusFilter_->currentData().toString();
        const QSignalBlocker blocker(computerStatusFilter_);
        QStringList states;
        for (const auto& computer : computers_) {
            const QString state = QString::fromStdString(computer.enrollmentState);
            if (!states.contains(state)) {
                states.append(state);
            }
        }
        states.sort(Qt::CaseInsensitive);
        computerStatusFilter_->clear();
        computerStatusFilter_->addItem(tr("All enrollment states"), QString());
        for (const QString& state : states) {
            computerStatusFilter_->addItem(state, state);
        }
        const int index = computerStatusFilter_->findData(selectedStatus);
        computerStatusFilter_->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (computerGroupFilter_ != nullptr) {
        const QString selectedGroup = computerGroupFilter_->currentData().toString();
        const QSignalBlocker blocker(computerGroupFilter_);
        std::vector<std::pair<QString, QString>> groupOptions;
        QSet<QString> seenGroups;
        for (const auto& computer : computers_) {
            const QString serverName = QString::fromStdString(computer.serverName);
            for (const auto& group : computer.groups) {
                const QString key = QString::fromStdString(group.id.deployment.value) +
                    QStringLiteral("/") + QString::fromStdString(group.id.serverEntityId);
                if (seenGroups.contains(key)) {
                    continue;
                }
                seenGroups.insert(key);
                groupOptions.emplace_back(
                    tr("%1 (%2)").arg(QString::fromStdString(group.name), serverName), key);
            }
        }
        std::sort(groupOptions.begin(), groupOptions.end(), [](const auto& left, const auto& right) {
            const int nameOrder = QString::compare(left.first, right.first, Qt::CaseInsensitive);
            return nameOrder == 0 ? left.second < right.second : nameOrder < 0;
        });
        computerGroupFilter_->clear();
        computerGroupFilter_->addItem(tr("All groups"), QString());
        for (const auto& [label, key] : groupOptions) {
            computerGroupFilter_->addItem(label, key);
        }
        computerGroupFilter_->setEnabled(!groupOptions.empty());
        const int index = computerGroupFilter_->findData(selectedGroup);
        computerGroupFilter_->setCurrentIndex(index >= 0 ? index : 0);
    }
    refreshComputerTable();
    refreshAccountTables();
}

void MainWindow::refreshServerDetails()
{
    if (serverDetailName_ == nullptr || serverDetailEndpoint_ == nullptr ||
        serverDetailDeployment_ == nullptr || serverDetailState_ == nullptr ||
        serverDetailHealth_ == nullptr) {
        return;
    }
    const auto selectedId = selectedProfileId();
    std::optional<ConnectionProfile> profile;
    if (selectedId) {
        profile = registry_.profile(*selectedId);
    }
    if (!profile) {
        serverDetailName_->setText(tr("Select a server profile."));
        serverDetailEndpoint_->clear();
        serverDetailDeployment_->clear();
        serverDetailState_->clear();
        serverDetailHealth_->clear();
        return;
    }
    serverDetailName_->setText(tr("Server: %1").arg(QString::fromStdString(profile->displayName)));
    serverDetailEndpoint_->setText(tr("Address: %1").arg(QString::fromStdString(profile->endpoint)));
    const bool authorityMismatch = serverAuthorityMismatches_.contains(profile->profileId);
    serverDetailDeployment_->setText(
        authorityMismatch
            ? tr("Server authority mismatch. The saved pin is not verified against the current server.\nSaved deployment: %1\nSaved authority fingerprint: %2\nVerify both values through your organization's trusted channel.")
                  .arg(QString::fromStdString(profile->trustedDeploymentId),
                       hasTrustedDeploymentAuthority(*profile)
                           ? grantSigningKeyFingerprint(QString::fromStdString(
                                 profile->trustedGrantSigningPublicKey))
                           : tr("not pinned"))
            : (hasTrustedDeploymentAuthority(*profile)
            ? tr("Verified deployment: %1\nGrant authority fingerprint: %2")
                  .arg(QString::fromStdString(profile->trustedDeploymentId),
                       grantSigningKeyFingerprint(
                           QString::fromStdString(profile->trustedGrantSigningPublicKey)))
            : tr("Deployment identity and grant authority key: not verified")));
    const auto deployment = deploymentsByProfile_.find(profile->profileId);
    QString state = availabilityText(profile->availability);
    if (deployment != deploymentsByProfile_.end()) {
        if (const auto* session = sessions_.session(deployment->second);
            session != nullptr && session->expiresAtUnix > QDateTime::currentSecsSinceEpoch()) {
            state = tr("Signed in as %1").arg(QString::fromStdString(session->username));
        }
    }
    serverDetailState_->setText(authorityMismatch
        ? tr("Session / availability: authority mismatch; sign-in and management are blocked")
        : tr("Session / availability: %1").arg(state));
    const auto health = serverHealthByProfile_.find(profile->profileId);
    serverDetailHealth_->setText(health == serverHealthByProfile_.end()
        ? tr("No public health report loaded. Use Check Availability.")
        : health->second);
}

void MainWindow::showWorkspace(int index)
{
    if (index >= 0 && index < 4) {
        workspaceStack_->setCurrentIndex(index);
        if (index == 2) {
            refreshAuthorizedAccountData();
        } else if (index == 3) {
            refreshFileJobs();
            refreshFileStorage();
        }
        return;
    }
    if (index == 4) {
        workspaceStack_->setCurrentIndex(4);
    }
}

std::optional<std::string> MainWindow::selectedProfileId() const
{
    if (serverTable_ == nullptr || serverTable_->currentRow() < 0) {
        return std::nullopt;
    }
    const auto* item = serverTable_->item(serverTable_->currentRow(), 0);
    if (item == nullptr) {
        return std::nullopt;
    }
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty()) {
        return std::nullopt;
    }
    return id.toStdString();
}

void MainWindow::showPreferences()
{
    QMessageBox::information(this, tr("Preferences"),
                             tr("Server profiles, the computer Grid/List choice, and scoped monitoring preferences are stored in this user's application settings. "
                                "Use File > Export Profiles & Preferences to create a versioned migration bundle, then import it on the other Manager. "
                                "Passwords, sessions, server trust pins, and private keys are not included. "
                                "The setup draft remains local because its host and filesystem paths may not apply on another computer."));
}
