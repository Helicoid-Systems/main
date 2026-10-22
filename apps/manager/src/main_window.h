#pragma once

#include "deployment_registry.h"
#include "SARibbon.h"

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QByteArray>

#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <memory>
#include <vector>

class QAction;
class QCheckBox;
class QLabel;
class QLineEdit;
class QComboBox;
class QCloseEvent;
class QFile;
class QCryptographicHash;
class QListWidget;
class QStackedWidget;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QToolButton;
class QPushButton;
class QWidget;
class QTimer;
class QSaveFile;

class MainWindow final : public SARibbonMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    struct ComputerRow;
    struct ComputerInclusionPreference;

    struct FileJobRecipient {
        std::string computerId;
        QString computerLabel;
        std::string sessionId;
        QString displayName;
    };

    struct FileJobBatch {
        std::string profileId;
        helicoid::manager::DeploymentId deployment;
        helicoid::manager::SessionStamp stamp;
        std::string expectedOrigin;
        QString serverName;
        std::vector<FileJobRecipient> recipients;
    };

    struct FileJobSendContext {
        QString selectedProfile;
        QString skippedSummary;
        std::shared_ptr<QFile> file;
        QByteArray digest;
        qint64 fileSize = 0;
        std::vector<FileJobBatch> batches;
    };

    void createRibbon();
    QWidget* createComputersWorkspace();
    QWidget* createServersWorkspace();
    QWidget* createAccountsWorkspace();
    QWidget* createFilesWorkspace();
    QWidget* createSetupWorkspace();
    void addServerProfile();
    void importProfiles();
    void exportProfiles();
    void exportAuthorizedInventory();
    void openSetupDraft();
    void saveSetupDraft();
    void checkSetupHost();
    void checkSelectedServerAvailability();
    void verifySelectedServerIdentity();
    void manageAuthorityCutover();
    void checkPinnedDeploymentIdentity(const std::string& profileId,
                                       std::function<void()> continuation);
    void bootstrapSelectedServer();
    void confirmBootstrapMfa();
    void signInSelectedServer();
    void signOutSelectedServer();
    void enrollCurrentManagerDevice();
    void approveSelectedManager();
    void revokeSelectedManager();
    void createServerAccount();
    void editSelectedAccountCapabilities();
    void updateSelectedAccountState();
    void editOperatorMfaPolicy();
    void recoverAccountMfa();
    void beginAccountMfaRecovery(const std::string& profileId);
    void assignAccountAccess();
    void revokeSelectedAccountAccess();
    void refreshAuditEvents();
    void loadOlderAuditEvents();
    void requestAuditEvents(bool olderPage);
    void editAuditRetention();
    void beginSelfMfaEnrollment();
    void confirmSelectedAccountMfa();
    void confirmMfaEnrollment(const std::string& profileId,
                              const QString& initialEnrollmentId = QString());
    void showMfaEnrollmentConfirmation(const std::string& profileId,
                                       const QString& initialEnrollmentId);
    void forgetSelectedServerProfile();
    void sendLogoutRequestForVerifiedProfile(const helicoid::manager::ConnectionProfile& profile,
                                             const helicoid::manager::DeploymentId& deployment,
                                             QByteArray bearerToken,
                                             bool reportResult,
                                             std::function<void()> completion = {});
    void sendLogoutRequest(const QUrl& endpoint,
                           QByteArray bearerToken,
                           bool reportResult,
                           std::function<void()> completion = {});
    void authenticateSelectedServer(const std::string& profileId,
                                    const helicoid::manager::AvailabilityRequest& request,
                                    const helicoid::manager::DeploymentId& deployment,
                                    QByteArray loginBody);
    void acceptV2Session(const std::string& profileId,
                         const helicoid::manager::AvailabilityRequest& request,
                         const helicoid::manager::DeploymentId& deployment,
                         const QString& serverName,
                         const std::optional<QUrl>& logoutEndpoint,
                         QByteArray responseBody,
                         int statusCode,
                         const QByteArray& expectedManagerPublicKey,
                         bool mfaAssured);
    void loadAuthorizedComputers(const std::string& profileId,
                                 const helicoid::manager::DeploymentId& deployment,
                                 const helicoid::manager::SessionStamp& stamp);
    void loadObservationScope(const std::string& profileId,
                             const helicoid::manager::DeploymentId& deployment,
                             const helicoid::manager::SessionStamp& stamp,
                             bool force = false);
    void refreshComputerInclusionPanel();
    ComputerInclusionPreference& computerInclusionPreference(
        const helicoid::manager::DeploymentId& deployment,
        const std::string& accountId);
    void setComputerInclusionPanelVisible(bool visible);
    void setAllComputerInclusion(bool included);
    void setComputerInclusionItems(QTreeWidgetItem* item);
    bool computerIsIncluded(const ComputerRow& computer);
    bool saveComputerInclusionPreferences(const std::vector<std::string>& scopeKeys);
    void refreshAuthorizedComputers();
    void manageComputerGroups();
    void loadAuthorizedAccountData(const std::string& profileId,
                                   const helicoid::manager::DeploymentId& deployment,
                                   const helicoid::manager::SessionStamp& stamp);
    void refreshAuthorizedAccountData();
    void refreshAccountTables();
    void clearDeploymentAccountData(const helicoid::manager::DeploymentId& deployment);
    void refreshFileJobs();
    void sendFileToSelectedServer();
    void sendFileToAllSignedInServers();
    void checkFileJobBatchAvailability(const std::shared_ptr<FileJobSendContext>& context,
                                       std::size_t batchIndex);
    void selectFileJobBatchRecipients(const std::shared_ptr<FileJobSendContext>& context);
    void preflightFileJobBatches(const std::shared_ptr<FileJobSendContext>& context,
                                 std::size_t batchIndex,
                                 std::size_t recipientIndex);
    void preflightFileJobRecipients(const std::string& profileId,
                                    const helicoid::manager::DeploymentId& deployment,
                                    const helicoid::manager::SessionStamp& stamp,
                                    const std::shared_ptr<QFile>& file,
                                    const QByteArray& digest,
                                    qint64 fileSize,
                                    const std::string& expectedOrigin,
                                    const QString& selectedProfile,
                                    std::vector<FileJobRecipient> recipients,
                                    std::size_t recipientIndex);
    bool createFileJobForConfirmedRecipients(const std::string& profileId,
                                              const helicoid::manager::DeploymentId& deployment,
                                              const helicoid::manager::SessionStamp& stamp,
                                              const std::shared_ptr<QFile>& file,
                                              const QByteArray& digest,
                                              qint64 fileSize,
                                              const std::string& expectedOrigin,
                                              const QString& selectedProfile,
                                              const std::vector<FileJobRecipient>& recipients);
    void uploadFileJobChunk(const std::string& profileId,
                            const helicoid::manager::DeploymentId& deployment,
                            const helicoid::manager::SessionStamp& stamp,
                            const std::string& jobId,
                            const std::string& expectedSha256,
                            const std::shared_ptr<QFile>& file,
                            const std::shared_ptr<QCryptographicHash>& uploadedHash,
                            const std::string& expectedOrigin,
                            std::uint32_t chunkSize,
                            std::uint32_t chunkCount,
                            std::uint32_t chunkIndex);
    void cancelSelectedFileJob();
    void refreshFileJobTable();
    void clearDeploymentFileJobs(const helicoid::manager::DeploymentId& deployment);
    void withFileRequestsAvailable(const std::string& profileId, std::function<void()> continuation);
    void refreshFileStorage();
    void refreshFileStorageStatus();
    void clearDeploymentFileStorage(const helicoid::manager::DeploymentId& deployment);
    void refreshFileRequests();
    void saveFileRequestDraft();
    void publishSelectedFileRequest();
    void closeSelectedFileRequest();
    void reviewFileRequestSubmissions();
    void deleteSelectedFileRequestSubmissionContent();
    void downloadFileRequestChunk(const std::string& profileId,
                                  const helicoid::manager::DeploymentId& deployment,
                                  const helicoid::manager::SessionStamp& stamp,
                                  const std::string& requestId,
                                  const std::string& submissionId,
                                  const std::string& fileId,
                                  const std::string& expectedOrigin,
                                  const std::shared_ptr<QSaveFile>& output,
                                  const std::shared_ptr<QCryptographicHash>& hash,
                                  std::uint64_t expectedSize,
                                  const std::string& expectedDigest,
                                  std::uint32_t chunkCount,
                                  std::uint32_t chunkIndex);
    void refreshFileRequestTable();
    void clearDeploymentFileRequests(const helicoid::manager::DeploymentId& deployment);
    void expireSessions();
    void refreshComputerTable();
    void applyComputerView(int index, bool persist);
    void applyComputerPreviewSize(int index, bool persist);
    void scheduleOverviewPreviews();
    void stopOverviewPreview(QWidget* tileWidget);
    std::size_t activeObservationPreviewCount() const;
    void refreshServerDetails();
    void clearDeploymentAuthentication(const helicoid::manager::DeploymentId& deployment,
                                       const std::string& profileId);
    void invalidateDeploymentSession(const helicoid::manager::DeploymentId& deployment);
    void closeViewersForDeployment(const helicoid::manager::DeploymentId& deployment);
    void retainEnrolledDeploymentViewers(const helicoid::manager::DeploymentId& deployment,
                                         const std::vector<std::string>& enrolledComputerIds);
    void closeAllViewers();
    void openSelectedComputerViewer();
    void requestObservationPreview(const QString& profileId,
                                  const helicoid::manager::DeploymentId& deployment,
                                  const std::string& computerId,
                                  const helicoid::manager::SessionStamp& stamp,
                                  const QPointer<QLabel>& statusLabel,
                                  const QPointer<QPushButton>& requestButton);
    void invalidateProfileSessionLocally(const std::string& profileId,
                                         const helicoid::manager::DeploymentId& deployment);
    void loadProfiles();
    bool saveProfiles() const;
    void refreshServerTable();
    void showWorkspace(int index);
    std::optional<std::string> selectedProfileId() const;
    void showPreferences();
    void completeShutdownOperation();

protected:
    void closeEvent(QCloseEvent* event) override;

private:

    QStackedWidget* workspaceStack_ = nullptr;
    QStackedWidget* computerContentStack_ = nullptr;
    QStackedWidget* serverContentStack_ = nullptr;
    QTableWidget* serverTable_ = nullptr;
    QTableWidget* computerTable_ = nullptr;
    QTableWidget* accountTable_ = nullptr;
    QTableWidget* managerTable_ = nullptr;
    QTableWidget* accessTable_ = nullptr;
    QTableWidget* auditTable_ = nullptr;
    QPushButton* auditOlderButton_ = nullptr;
    QTableWidget* fileJobTable_ = nullptr;
    QTableWidget* fileRequestTable_ = nullptr;
    QTabWidget* fileTabs_ = nullptr;
    QListWidget* computerGrid_ = nullptr;
    QStackedWidget* computerLayoutStack_ = nullptr;
    int computerPreviewSizeIndex_ = 1;
    QLineEdit* computerSearch_ = nullptr;
    QComboBox* computerServerFilter_ = nullptr;
    QComboBox* computerGroupFilter_ = nullptr;
    QComboBox* computerStatusFilter_ = nullptr;
    QToolButton* computerInclusionToggle_ = nullptr;
    QWidget* computerInclusionPanel_ = nullptr;
    QCheckBox* allComputersIncluded_ = nullptr;
    QTreeWidget* computerInclusionTree_ = nullptr;
    QComboBox* accountsServerFilter_ = nullptr;
    QComboBox* filesServerFilter_ = nullptr;
    QLabel* filesStatus_ = nullptr;
    QLabel* filesQuotaStatus_ = nullptr;
    QLineEdit* setupName_ = nullptr;
    QLineEdit* setupHost_ = nullptr;
    QLineEdit* setupDataDirectory_ = nullptr;
    QLineEdit* setupBackupDirectory_ = nullptr;
    QComboBox* setupNetworkScope_ = nullptr;
    QLineEdit* setupBindAddress_ = nullptr;
    QLineEdit* setupClientHostname_ = nullptr;
    QSpinBox* setupAuthorityPort_ = nullptr;
    QComboBox* setupMode_ = nullptr;
    QLabel* setupNetworkHint_ = nullptr;
    QLabel* setupDraftStatus_ = nullptr;
    QLabel* setupHostCheckStatus_ = nullptr;
    QPushButton* setupCheckHostButton_ = nullptr;
    QLabel* serverDetailName_ = nullptr;
    QLabel* serverDetailEndpoint_ = nullptr;
    QLabel* serverDetailDeployment_ = nullptr;
    QLabel* serverDetailState_ = nullptr;
    QLabel* serverDetailHealth_ = nullptr;
    std::unordered_map<std::string, QString> serverHealthByProfile_;
    std::unordered_set<std::string> serverAuthorityMismatches_;
    QLabel* accountsStatus_ = nullptr;
    QLabel* auditStatus_ = nullptr;
    QString auditNextCursorId_;
    qint64 auditNextCursorUnix_ = 0;
    bool auditLoading_ = false;
    std::uint64_t auditRequestGeneration_ = 0;
    QAction* openViewAction_ = nullptr;
    QString profileLoadWarning_;
    QNetworkAccessManager network_;
    QTimer* sessionExpiryTimer_ = nullptr;
    QTimer* overviewPreviewTimer_ = nullptr;
    std::unordered_map<std::string, qint64> observationPreviewCooldowns_;
    std::unordered_map<std::string, qint64> observationScopeRefreshRetryAfterMs_;
    bool shutdownInProgress_ = false;
    bool shutdownComplete_ = false;
    std::size_t pendingShutdownLogoutRequests_ = 0;
    helicoid::manager::DeploymentRegistry registry_;
    helicoid::manager::ServerSessionRegistry sessions_;
    std::unordered_map<std::string, helicoid::manager::DeploymentId> deploymentsByProfile_;
    std::unordered_map<std::string, std::uint64_t> computerRequestGenerations_;
    std::unordered_map<std::string, std::uint64_t> observationScopeRequestGenerations_;
    std::unordered_set<std::string> observationScopePending_;
    helicoid::manager::ObservationScopeRegistry observationScopes_;
    std::unordered_set<std::string> signInsInProgress_;
    std::unordered_map<std::string, std::string> authenticationProfileByDeployment_;
    std::unordered_map<helicoid::manager::ScopedViewerId,
                       QPointer<QMainWindow>,
                       helicoid::manager::ScopedEntityIdHash<helicoid::manager::ViewerKind>> viewerWindows_;
    struct ComputerGroupRow {
        helicoid::manager::ScopedGroupId id;
        std::string name;
    };
    struct EndpointSessionRow {
        std::optional<std::int64_t> lastAuthenticatedAtUnix;
        std::optional<std::int64_t> currentSessionExpiresAtUnix;
        std::optional<std::string> currentSessionId;
        std::optional<bool> consoleUserReported;
    };
    struct ComputerRow {
        helicoid::manager::ScopedComputerId id;
        std::string profileId;
        std::string serverName;
        std::string friendlyName;
        std::string platform;
        std::string enrollmentState;
        std::vector<ComputerGroupRow> groups;
        bool endpointSessionReported = false;
        std::optional<EndpointSessionRow> endpointSession;
    };
    struct ComputerInclusionPreference {
        bool allComputers = true;
        std::unordered_set<std::string> computerIds;
        bool panelVisible = false;
        bool panelVisibilityConfigured = false;
        int computerViewIndex = 0;
        bool computerViewConfigured = false;
        int previewSizeIndex = 1;
        bool previewSizeConfigured = false;
    };
    std::unordered_map<std::string, ComputerInclusionPreference> computerInclusionByScope_;
    std::unordered_map<QTreeWidgetItem*, std::vector<helicoid::manager::ScopedComputerId>>
        computerInclusionItemTargets_;
    bool rebuildingComputerInclusionPanel_ = false;
    bool computerInclusionPanelVisible_ = false;
    struct ManagerRow {
        helicoid::manager::ScopedManagerId id;
        std::string profileId;
        std::string serverName;
        std::string friendlyName;
        std::string publicIdentity;
        std::string lifecycleState;
        std::optional<std::int64_t> approvedAtUnix;
    };
    struct AccountRow {
        helicoid::manager::ScopedAccountId id;
        std::string profileId;
        std::string serverName;
        std::string username;
        std::string state;
        bool recoveryQuarantined = false;
        bool canOperate = false;
        bool canManageEnrollment = false;
        bool canManageAccess = false;
        bool canManageAccounts = false;
        bool canManageSecurity = false;
        std::string capabilities;
        std::int64_t revision = 0;
    };
    struct AccessAssignmentRow {
        helicoid::manager::ScopedAccessAssignmentId id;
        helicoid::manager::ScopedAccountId accountId;
        std::string profileId;
        std::string serverName;
        std::string capability;
        bool computerScopeAll = false;
        std::size_t computerCount = 0;
        std::int64_t revision = 0;
    };
    std::vector<ComputerRow> computers_;
    std::vector<AccountRow> accounts_;
    std::vector<ManagerRow> managers_;
    std::vector<AccessAssignmentRow> accessAssignments_;
    std::unordered_map<std::string, std::string> accountDataErrors_;
    std::unordered_map<std::string, std::uint64_t> accountRequestGenerations_;
    std::unordered_set<std::string> accountDataPending_;
    std::unordered_map<std::string, std::uint64_t> fileJobRequestGenerations_;
    std::unordered_map<std::string, QString> fileJobDataErrors_;
    std::unordered_set<std::string> fileJobDataPending_;
    bool fileJobsFeatureGatePending_ = false;
    std::string fileJobsFeatureGatePassedProfile_;
    struct FileJobRow {
        helicoid::manager::ScopedJobId id;
        std::string profileId;
        std::string serverName;
        std::string fileName;
        std::uint64_t fileSizeBytes = 0;
        std::int64_t deliveryExpiresAtUnix = 0;
        std::string state;
        std::string targetSummary;
    };
    std::vector<FileJobRow> fileJobs_;
    struct FileRequestRow {
        helicoid::manager::ScopedFileRequestId id;
        std::string profileId;
        std::string serverName;
        std::string title;
        std::string state;
        std::int64_t revision = 0;
        std::int64_t closesAtUnix = 0;
        std::size_t computerCount = 0;
    };
    std::unordered_map<std::string, std::uint64_t> fileRequestRequestGenerations_;
    std::unordered_map<std::string, QString> fileRequestDataErrors_;
    std::unordered_set<std::string> fileRequestDataPending_;
    std::unordered_set<std::string> fileRequestAvailabilityPending_;
    std::vector<FileRequestRow> fileRequests_;
    struct FileStorageSummaryRow {
        std::uint64_t payloadLimitBytes = 0;
        std::uint64_t reservedPayloadBytes = 0;
        std::uint64_t availablePayloadBytes = 0;
        std::uint64_t activeFileJobs = 0;
        std::uint64_t activeFileJobLimit = 0;
        std::uint64_t activeRequestUploads = 0;
        std::uint64_t activeRequestUploadLimit = 0;
        std::uint64_t retainedReceivedFiles = 0;
        std::uint64_t retainedReceivedBytes = 0;
    };
    std::unordered_map<std::string, FileStorageSummaryRow> fileStorageByDeployment_;
    std::unordered_map<std::string, QString> fileStorageDataErrors_;
    std::unordered_map<std::string, std::uint64_t> fileStorageRequestGenerations_;
    std::unordered_set<std::string> fileStorageDataPending_;
};
