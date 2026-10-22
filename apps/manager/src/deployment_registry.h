#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace helicoid::manager {

inline bool validScopedEntityId(const std::string& value) noexcept
{
    if (value.empty() || value.size() > 128) {
        return false;
    }
    return std::none_of(value.begin(), value.end(), [](unsigned char character) {
        return character < 0x20U || character == 0x7fU;
    });
}

struct DeploymentId {
    // Populated only from the authenticated server contract, never from a profile name.
    std::string value;

    bool isValid() const noexcept;
    friend bool operator==(const DeploymentId&, const DeploymentId&) = default;
};

struct ScopedComputerId {
    DeploymentId deployment;
    std::string serverComputerId;

    static std::optional<ScopedComputerId> create(DeploymentId deployment,
                                                   std::string serverComputerId);
    friend bool operator==(const ScopedComputerId&, const ScopedComputerId&) = default;
};

struct ScopedComputerIdHash {
    std::size_t operator()(const ScopedComputerId& identity) const noexcept
    {
        const std::size_t deploymentHash = std::hash<std::string>{}(identity.deployment.value);
        const std::size_t computerHash = std::hash<std::string>{}(identity.serverComputerId);
        return deploymentHash ^ (computerHash + static_cast<std::size_t>(0x9e3779b9U) +
                                 (deploymentHash << 6U) + (deploymentHash >> 2U));
    }
};

template <typename Kind>
struct ScopedEntityId {
    DeploymentId deployment;
    std::string serverEntityId;

    static std::optional<ScopedEntityId> create(DeploymentId deployment,
                                                 std::string serverEntityId)
    {
        if (!deployment.isValid() || !validScopedEntityId(serverEntityId)) {
            return std::nullopt;
        }
        return ScopedEntityId{std::move(deployment), std::move(serverEntityId)};
    }

    friend bool operator==(const ScopedEntityId&, const ScopedEntityId&) = default;
};

template <typename Kind>
struct ScopedEntityIdHash {
    std::size_t operator()(const ScopedEntityId<Kind>& identity) const noexcept
    {
        const std::size_t deploymentHash = std::hash<std::string>{}(identity.deployment.value);
        const std::size_t entityHash = std::hash<std::string>{}(identity.serverEntityId);
        return deploymentHash ^ (entityHash + static_cast<std::size_t>(0x9e3779b9U) +
                                 (deploymentHash << 6U) + (deploymentHash >> 2U));
    }
};

struct AccountKind;
struct GroupKind;
struct ManagerKind;
struct AccessAssignmentKind;
struct JobKind;
struct FileRequestKind;
struct PolicyKind;
struct ViewerKind;

using ScopedAccountId = ScopedEntityId<AccountKind>;
using ScopedGroupId = ScopedEntityId<GroupKind>;
using ScopedManagerId = ScopedEntityId<ManagerKind>;
using ScopedAccessAssignmentId = ScopedEntityId<AccessAssignmentKind>;
using ScopedJobId = ScopedEntityId<JobKind>;
using ScopedFileRequestId = ScopedEntityId<FileRequestKind>;
using ScopedPolicyId = ScopedEntityId<PolicyKind>;
// Core v1 keeps one viewer window per deployment/computer identity.
using ScopedViewerId = ScopedEntityId<ViewerKind>;

enum class Availability {
    Unknown,
    Checking,
    Reachable,
    Degraded,
    Unreachable,
};

struct ConnectionProfile {
    std::string profileId;
    std::string displayName;
    std::string endpoint;
    Availability availability = Availability::Unknown;
    // Local trust pins; connection-profile imports and exports never carry them.
    std::string trustedDeploymentId;
    // Public authority key pinned through the same offline verification step.
    // It is local trust state and is never included in profile exports.
    std::string trustedGrantSigningPublicKey;
    // Monotonic server authority checkpoint from the same independent review.
    // Zero represents a legacy pin that must be reverified before use.
    std::uint64_t trustedAuthorityGeneration = 0;
};

bool validGrantSigningPublicKey(const std::string& publicKeyHex) noexcept;

// Pre-authentication requests are fenced by profile ID, incarnation, and
// request generation. Authenticated session generations use the server-owned
// DeploymentId.
struct AvailabilityRequest {
    std::string profileId;
    std::uint64_t generation = 0;
    // Distinguishes a saved profile from a later profile reusing the same ID.
    std::uint64_t profileIncarnation = 0;
};

struct TrustVerificationRequest {
    std::string profileId;
    std::uint64_t generation = 0;
    std::uint64_t profileIncarnation = 0;
};

struct SessionStamp {
    DeploymentId deployment;
    std::uint64_t generation = 0;
    friend bool operator==(const SessionStamp&, const SessionStamp&) = default;
};

/// A short-lived observe permission snapshot scoped to one authenticated
/// deployment session. It is advisory UI state; endpoints still require fresh
/// server and endpoint admission before delivering frames.
class ObservationScopeRegistry {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    bool replace(const SessionStamp& stamp,
                 bool allComputers,
                 std::unordered_set<std::string> computerIds,
                 TimePoint expiresAt,
                 TimePoint now);
    bool hasCurrent(const SessionStamp& stamp, TimePoint now) const noexcept;
    bool allows(const SessionStamp& stamp,
                const std::string& computerId,
                TimePoint now) const noexcept;
    bool expireIfCurrent(const SessionStamp& stamp, TimePoint now) noexcept;
    bool invalidate(const DeploymentId& deployment) noexcept;
    void clear() noexcept;

private:
    struct Entry {
        SessionStamp stamp;
        bool allComputers = false;
        std::unordered_set<std::string> computerIds;
        TimePoint expiresAt;
    };

    std::unordered_map<std::string, Entry> entries_;
};

struct AuthenticatedSession {
    DeploymentId deployment;
    std::string profileId;
    std::string accountId;
    std::string managerId;
    std::string username;
    std::string bearerToken;
    std::int64_t expiresAtUnix = 0;
    /// Local sign-in observation; privileged endpoint enrollment still relies
    /// on the server's persisted MFA assurance and freshness check.
    std::int64_t mfaAssuredAtUnix = 0;
};

class DeploymentRegistry {
public:
    static constexpr std::size_t maxProfiles = 64;

    bool addProfile(ConnectionProfile profile, std::string& error);
    bool setTrustedDeploymentId(const std::string& profileId, std::string deploymentId);
    bool setTrustedAuthority(const std::string& profileId,
                             std::string deploymentId,
                             std::string grantSigningPublicKey,
                             std::uint64_t authorityGeneration);
    bool restoreTrustedAuthorityState(const std::string& profileId,
                                      std::string deploymentId,
                                      std::string grantSigningPublicKey,
                                      std::uint64_t authorityGeneration);
    bool clearTrustedDeploymentId(const std::string& profileId) noexcept;
    bool removeProfile(const std::string& profileId);
    std::optional<ConnectionProfile> profile(const std::string& profileId) const;
    std::vector<ConnectionProfile> profiles() const;

    std::optional<AvailabilityRequest> beginAvailabilityCheck(const std::string& profileId);
    bool finishAvailabilityCheck(const AvailabilityRequest& request, Availability availability);
    bool isCurrent(const AvailabilityRequest& request) const noexcept;
    std::optional<TrustVerificationRequest> beginTrustVerification(const std::string& profileId);
    bool isCurrent(const TrustVerificationRequest& request) const noexcept;
    bool disconnect(const std::string& profileId);
    void disconnectAll() noexcept;

private:
    struct Entry {
        ConnectionProfile profile;
        std::uint64_t generation = 0;
        bool exhausted = false;
        std::uint64_t trustGeneration = 0;
        bool trustExhausted = false;
        std::uint64_t incarnation = 0;
    };

    std::vector<Entry> entries_;
    std::uint64_t nextIncarnation_ = 0;
    bool incarnationsExhausted_ = false;
};

class SessionGenerationRegistry {
public:
    std::optional<SessionStamp> activate(const DeploymentId& deployment);
    bool invalidate(const DeploymentId& deployment) noexcept;
    bool isCurrent(const SessionStamp& stamp) const noexcept;

private:
    struct Generation {
        std::uint64_t value = 0;
        bool active = false;
        bool exhausted = false;
    };

    std::unordered_map<std::string, Generation> generations_;
};

class ServerSessionRegistry {
public:
    std::optional<SessionStamp> activate(AuthenticatedSession session);
    const AuthenticatedSession* session(const DeploymentId& deployment) const noexcept;
    const AuthenticatedSession* session(const DeploymentId& deployment,
                                        std::int64_t nowUnix) const noexcept;
    std::optional<SessionStamp> currentStamp(const DeploymentId& deployment) const noexcept;
    std::optional<SessionStamp> currentStamp(const DeploymentId& deployment,
                                             std::int64_t nowUnix) const noexcept;
    bool isExpired(const DeploymentId& deployment, std::int64_t nowUnix) const noexcept;
    bool invalidate(const DeploymentId& deployment) noexcept;
    bool isCurrent(const SessionStamp& stamp) const noexcept;
    bool isCurrent(const SessionStamp& stamp, std::int64_t nowUnix) const noexcept;
    void invalidateAll() noexcept;

private:
    struct Entry {
        AuthenticatedSession session;
        std::uint64_t generation = 0;
        bool active = false;
        bool exhausted = false;
    };

    std::unordered_map<std::string, Entry> sessions_;
};

} // namespace helicoid::manager
