#include "deployment_registry.h"

#include <algorithm>
#include <chrono>
#include <limits>

namespace helicoid::manager {
namespace {

std::int64_t currentUnixTime() noexcept
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

bool DeploymentId::isValid() const noexcept
{
    if (value.size() != 36 || value.rfind("dep_", 0) != 0) {
        return false;
    }
    return std::all_of(value.begin() + 4, value.end(), [](char character) {
        return (character >= '0' && character <= '9') ||
               (character >= 'a' && character <= 'f') ||
               (character >= 'A' && character <= 'F');
    });
}

bool validGrantSigningPublicKey(const std::string& publicKeyHex) noexcept
{
    return publicKeyHex.size() == 64 &&
           std::all_of(publicKeyHex.begin(), publicKeyHex.end(), [](char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

std::optional<ScopedComputerId> ScopedComputerId::create(DeploymentId deployment,
                                                          std::string serverComputerId)
{
    if (!deployment.isValid() || !validScopedEntityId(serverComputerId)) {
        return std::nullopt;
    }
    return ScopedComputerId{std::move(deployment), std::move(serverComputerId)};
}

bool DeploymentRegistry::addProfile(ConnectionProfile profile, std::string& error)
{
    if (profile.profileId.empty() || profile.profileId.size() > 128) {
        error = "Invalid connection profile ID.";
        return false;
    }
    if (profile.displayName.empty() || profile.displayName.size() > 400) {
        error = "Connection profile name must contain 1 to 400 bytes.";
        return false;
    }
    if (profile.endpoint.empty() || profile.endpoint.size() > 2048) {
        error = "Connection endpoint must contain 1 to 2048 bytes.";
        return false;
    }
    if (!profile.trustedDeploymentId.empty() &&
        !DeploymentId{profile.trustedDeploymentId}.isValid()) {
        error = "Connection profile has an invalid deployment trust pin.";
        return false;
    }
    if (!profile.trustedGrantSigningPublicKey.empty() &&
        (profile.trustedDeploymentId.empty() ||
         !validGrantSigningPublicKey(profile.trustedGrantSigningPublicKey))) {
        error = "Connection profile has an invalid authority-key trust pin.";
        return false;
    }
    if (entries_.size() >= maxProfiles) {
        error = "The maximum number of saved server profiles has been reached.";
        return false;
    }
    const auto duplicate = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == profile.profileId;
    });
    if (duplicate != entries_.end()) {
        error = "A profile with this ID already exists.";
        return false;
    }
    if (incarnationsExhausted_ || nextIncarnation_ == std::numeric_limits<std::uint64_t>::max()) {
        incarnationsExhausted_ = true;
        error = "Connection profile identity space is exhausted.";
        return false;
    }

    profile.availability = Availability::Unknown;
    entries_.push_back(Entry{std::move(profile), 0, false, 0, false, ++nextIncarnation_});
    error.clear();
    return true;
}

bool DeploymentRegistry::setTrustedDeploymentId(const std::string& profileId,
                                                 std::string deploymentId)
{
    if (!DeploymentId{deploymentId}.isValid()) {
        return false;
    }
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == profileId;
    });
    if (existing == entries_.end()) {
        return false;
    }
    if (existing->profile.trustedAuthorityGeneration > 0) {
        return false;
    }
    existing->profile.trustedDeploymentId = std::move(deploymentId);
    existing->profile.trustedGrantSigningPublicKey.clear();
    existing->profile.trustedAuthorityGeneration = 0;
    if (existing->trustGeneration == std::numeric_limits<std::uint64_t>::max()) {
        existing->trustExhausted = true;
    } else {
        ++existing->trustGeneration;
    }
    return true;
}

bool DeploymentRegistry::setTrustedAuthority(const std::string& profileId,
                                              std::string deploymentId,
                                              std::string grantSigningPublicKey,
                                              std::uint64_t authorityGeneration)
{
    if (!DeploymentId{deploymentId}.isValid() ||
        !validGrantSigningPublicKey(grantSigningPublicKey) || authorityGeneration == 0) {
        return false;
    }
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == profileId;
    });
    if (existing == entries_.end()) {
        return false;
    }
    if (existing->profile.trustedDeploymentId == deploymentId &&
        existing->profile.trustedAuthorityGeneration > 0 &&
        (authorityGeneration < existing->profile.trustedAuthorityGeneration ||
         (authorityGeneration == existing->profile.trustedAuthorityGeneration &&
          existing->profile.trustedGrantSigningPublicKey != grantSigningPublicKey))) {
        return false;
    }
    existing->profile.trustedDeploymentId = std::move(deploymentId);
    existing->profile.trustedGrantSigningPublicKey = std::move(grantSigningPublicKey);
    existing->profile.trustedAuthorityGeneration = authorityGeneration;
    if (existing->trustGeneration == std::numeric_limits<std::uint64_t>::max()) {
        existing->trustExhausted = true;
    } else {
        ++existing->trustGeneration;
    }
    return true;
}

bool DeploymentRegistry::restoreTrustedAuthorityState(const std::string& profileId,
                                                        std::string deploymentId,
                                                        std::string grantSigningPublicKey,
                                                        std::uint64_t authorityGeneration)
{
    const bool validLegacyState = authorityGeneration == 0 &&
        (grantSigningPublicKey.empty() || validGrantSigningPublicKey(grantSigningPublicKey));
    if (!DeploymentId{deploymentId}.isValid() ||
        (!validGrantSigningPublicKey(grantSigningPublicKey) && !validLegacyState)) {
        return false;
    }
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == profileId;
    });
    if (existing == entries_.end()) {
        return false;
    }
    existing->profile.trustedDeploymentId = std::move(deploymentId);
    existing->profile.trustedGrantSigningPublicKey = std::move(grantSigningPublicKey);
    existing->profile.trustedAuthorityGeneration = authorityGeneration;
    if (existing->trustGeneration == std::numeric_limits<std::uint64_t>::max()) {
        existing->trustExhausted = true;
    } else {
        ++existing->trustGeneration;
    }
    return true;
}

bool DeploymentRegistry::clearTrustedDeploymentId(const std::string& profileId) noexcept
{
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == profileId;
    });
    if (existing == entries_.end()) {
        return false;
    }
    existing->profile.trustedDeploymentId.clear();
    existing->profile.trustedGrantSigningPublicKey.clear();
    existing->profile.trustedAuthorityGeneration = 0;
    if (existing->trustGeneration == std::numeric_limits<std::uint64_t>::max()) {
        existing->trustExhausted = true;
    } else {
        ++existing->trustGeneration;
    }
    return true;
}

bool DeploymentRegistry::removeProfile(const std::string& profileId)
{
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == profileId;
    });
    if (existing == entries_.end()) {
        return false;
    }
    entries_.erase(existing);
    return true;
}

std::optional<ConnectionProfile> DeploymentRegistry::profile(const std::string& profileId) const
{
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == profileId;
    });
    if (existing == entries_.end()) {
        return std::nullopt;
    }
    return existing->profile;
}

std::vector<ConnectionProfile> DeploymentRegistry::profiles() const
{
    std::vector<ConnectionProfile> result;
    result.reserve(entries_.size());
    for (const auto& entry : entries_) {
        result.push_back(entry.profile);
    }
    return result;
}

std::optional<AvailabilityRequest>
DeploymentRegistry::beginAvailabilityCheck(const std::string& profileId)
{
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == profileId;
    });
    if (existing == entries_.end() || existing->exhausted) {
        return std::nullopt;
    }
    if (existing->generation == std::numeric_limits<std::uint64_t>::max()) {
        existing->exhausted = true;
        return std::nullopt;
    }

    ++existing->generation;
    existing->profile.availability = Availability::Checking;
    return AvailabilityRequest{profileId, existing->generation, existing->incarnation};
}

bool DeploymentRegistry::finishAvailabilityCheck(const AvailabilityRequest& request,
                                                   Availability availability)
{
    if (availability == Availability::Unknown || availability == Availability::Checking) {
        return false;
    }
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == request.profileId;
    });
    if (existing == entries_.end() || existing->exhausted ||
        existing->incarnation != request.profileIncarnation ||
        existing->generation != request.generation) {
        return false;
    }
    existing->profile.availability = availability;
    return true;
}

bool DeploymentRegistry::isCurrent(const AvailabilityRequest& request) const noexcept
{
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == request.profileId;
    });
    return existing != entries_.end() && !existing->exhausted &&
           existing->incarnation == request.profileIncarnation &&
           existing->generation == request.generation;
}

std::optional<TrustVerificationRequest>
DeploymentRegistry::beginTrustVerification(const std::string& profileId)
{
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == profileId;
    });
    if (existing == entries_.end() || existing->trustExhausted) {
        return std::nullopt;
    }
    if (existing->trustGeneration == std::numeric_limits<std::uint64_t>::max()) {
        existing->trustExhausted = true;
        return std::nullopt;
    }

    ++existing->trustGeneration;
    return TrustVerificationRequest{profileId, existing->trustGeneration, existing->incarnation};
}

bool DeploymentRegistry::isCurrent(const TrustVerificationRequest& request) const noexcept
{
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == request.profileId;
    });
    return existing != entries_.end() && !existing->trustExhausted &&
           existing->incarnation == request.profileIncarnation &&
           existing->trustGeneration == request.generation;
}

bool DeploymentRegistry::disconnect(const std::string& profileId)
{
    const auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.profile.profileId == profileId;
    });
    if (existing == entries_.end()) {
        return false;
    }
    if (existing->generation == std::numeric_limits<std::uint64_t>::max()) {
        existing->exhausted = true;
    } else {
        ++existing->generation;
    }
    if (existing->trustGeneration == std::numeric_limits<std::uint64_t>::max()) {
        existing->trustExhausted = true;
    } else {
        ++existing->trustGeneration;
    }
    existing->profile.availability = Availability::Unknown;
    return true;
}

void DeploymentRegistry::disconnectAll() noexcept
{
    for (auto& entry : entries_) {
        if (entry.generation != std::numeric_limits<std::uint64_t>::max()) {
            ++entry.generation;
        } else {
            entry.exhausted = true;
        }
        if (entry.trustGeneration != std::numeric_limits<std::uint64_t>::max()) {
            ++entry.trustGeneration;
        } else {
            entry.trustExhausted = true;
        }
        entry.profile.availability = Availability::Unknown;
    }
}

std::optional<SessionStamp> SessionGenerationRegistry::activate(const DeploymentId& deployment)
{
    if (!deployment.isValid()) {
        return std::nullopt;
    }

    auto& generation = generations_[deployment.value];
    if (generation.exhausted || generation.value == std::numeric_limits<std::uint64_t>::max()) {
        generation.active = false;
        generation.exhausted = true;
        return std::nullopt;
    }
    ++generation.value;
    generation.active = true;
    return SessionStamp{deployment, generation.value};
}

bool SessionGenerationRegistry::invalidate(const DeploymentId& deployment) noexcept
{
    if (!deployment.isValid()) {
        return false;
    }
    const auto existing = generations_.find(deployment.value);
    if (existing == generations_.end()) {
        return false;
    }
    existing->second.active = false;
    if (existing->second.value == std::numeric_limits<std::uint64_t>::max()) {
        existing->second.exhausted = true;
    } else {
        ++existing->second.value;
    }
    return true;
}

bool SessionGenerationRegistry::isCurrent(const SessionStamp& stamp) const noexcept
{
    const auto existing = generations_.find(stamp.deployment.value);
    return stamp.deployment.isValid() && existing != generations_.end() &&
           existing->second.active && !existing->second.exhausted &&
           existing->second.value == stamp.generation;
}

bool ObservationScopeRegistry::replace(const SessionStamp& stamp,
                                        bool allComputers,
                                        std::unordered_set<std::string> computerIds,
                                        TimePoint expiresAt,
                                        TimePoint now)
{
    if (!stamp.deployment.isValid() || stamp.generation == 0 || expiresAt <= now ||
        (allComputers && !computerIds.empty()) ||
        std::any_of(computerIds.begin(), computerIds.end(), [](const std::string& id) {
            return !validScopedEntityId(id);
        })) {
        return false;
    }

    const auto existing = entries_.find(stamp.deployment.value);
    if (existing != entries_.end() && existing->second.stamp.generation > stamp.generation) {
        return false;
    }
    entries_.insert_or_assign(stamp.deployment.value,
                               Entry{stamp, allComputers, std::move(computerIds), expiresAt});
    return true;
}

bool ObservationScopeRegistry::hasCurrent(const SessionStamp& stamp, TimePoint now) const noexcept
{
    if (!stamp.deployment.isValid() || stamp.generation == 0) {
        return false;
    }
    const auto existing = entries_.find(stamp.deployment.value);
    return existing != entries_.end() && existing->second.stamp == stamp &&
           existing->second.expiresAt > now;
}

bool ObservationScopeRegistry::allows(const SessionStamp& stamp,
                                      const std::string& computerId,
                                      TimePoint now) const noexcept
{
    if (!hasCurrent(stamp, now)) {
        return false;
    }
    const auto existing = entries_.find(stamp.deployment.value);
    return existing != entries_.end() &&
           (existing->second.allComputers || existing->second.computerIds.contains(computerId));
}

bool ObservationScopeRegistry::expireIfCurrent(const SessionStamp& stamp,
                                               TimePoint now) noexcept
{
    if (!stamp.deployment.isValid()) {
        return false;
    }
    const auto existing = entries_.find(stamp.deployment.value);
    if (existing == entries_.end() || existing->second.stamp != stamp ||
        existing->second.expiresAt > now) {
        return false;
    }
    entries_.erase(existing);
    return true;
}

bool ObservationScopeRegistry::invalidate(const DeploymentId& deployment) noexcept
{
    if (!deployment.isValid()) {
        return false;
    }
    return entries_.erase(deployment.value) != 0;
}

void ObservationScopeRegistry::clear() noexcept
{
    entries_.clear();
}

std::optional<SessionStamp> ServerSessionRegistry::activate(AuthenticatedSession session)
{
    if (!session.deployment.isValid() || session.profileId.empty() || session.profileId.size() > 128 ||
        session.accountId.empty() || session.accountId.size() > 128 || session.managerId.empty() ||
        session.managerId.size() > 128 || session.username.empty() || session.username.size() > 128 ||
        session.bearerToken.empty() || session.bearerToken.size() > 4096 || session.expiresAtUnix <= 0) {
        return std::nullopt;
    }

    auto& entry = sessions_[session.deployment.value];
    if (entry.exhausted || entry.generation == std::numeric_limits<std::uint64_t>::max()) {
        entry.active = false;
        entry.exhausted = true;
        std::fill(entry.session.bearerToken.begin(), entry.session.bearerToken.end(), '\0');
        entry.session.bearerToken.clear();
        return std::nullopt;
    }
    std::fill(entry.session.bearerToken.begin(), entry.session.bearerToken.end(), '\0');
    ++entry.generation;
    entry.active = true;
    entry.session = std::move(session);
    return SessionStamp{entry.session.deployment, entry.generation};
}

const AuthenticatedSession* ServerSessionRegistry::session(const DeploymentId& deployment) const noexcept
{
    return session(deployment, currentUnixTime());
}

const AuthenticatedSession* ServerSessionRegistry::session(const DeploymentId& deployment,
                                                           std::int64_t nowUnix) const noexcept
{
    if (!deployment.isValid()) {
        return nullptr;
    }
    const auto entry = sessions_.find(deployment.value);
    if (entry == sessions_.end() || !entry->second.active || entry->second.exhausted ||
        entry->second.session.bearerToken.empty() ||
        entry->second.session.expiresAtUnix <= nowUnix) {
        return nullptr;
    }
    return &entry->second.session;
}

std::optional<SessionStamp>
ServerSessionRegistry::currentStamp(const DeploymentId& deployment) const noexcept
{
    return currentStamp(deployment, currentUnixTime());
}

std::optional<SessionStamp>
ServerSessionRegistry::currentStamp(const DeploymentId& deployment,
                                    std::int64_t nowUnix) const noexcept
{
    if (!deployment.isValid()) {
        return std::nullopt;
    }
    const auto entry = sessions_.find(deployment.value);
    if (entry == sessions_.end() || !entry->second.active || entry->second.exhausted ||
        entry->second.session.bearerToken.empty() ||
        entry->second.session.expiresAtUnix <= nowUnix) {
        return std::nullopt;
    }
    return SessionStamp{deployment, entry->second.generation};
}

bool ServerSessionRegistry::isExpired(const DeploymentId& deployment,
                                      std::int64_t nowUnix) const noexcept
{
    if (!deployment.isValid()) {
        return false;
    }
    const auto entry = sessions_.find(deployment.value);
    return entry != sessions_.end() && entry->second.active && !entry->second.exhausted &&
           !entry->second.session.bearerToken.empty() &&
           entry->second.session.expiresAtUnix <= nowUnix;
}

bool ServerSessionRegistry::invalidate(const DeploymentId& deployment) noexcept
{
    if (!deployment.isValid()) {
        return false;
    }
    const auto entry = sessions_.find(deployment.value);
    if (entry == sessions_.end()) {
        return false;
    }
    entry->second.active = false;
    std::fill(entry->second.session.bearerToken.begin(), entry->second.session.bearerToken.end(), '\0');
    entry->second.session.bearerToken.clear();
    if (entry->second.generation == std::numeric_limits<std::uint64_t>::max()) {
        entry->second.exhausted = true;
    } else {
        ++entry->second.generation;
    }
    return true;
}

bool ServerSessionRegistry::isCurrent(const SessionStamp& stamp) const noexcept
{
    return isCurrent(stamp, currentUnixTime());
}

bool ServerSessionRegistry::isCurrent(const SessionStamp& stamp,
                                      std::int64_t nowUnix) const noexcept
{
    const auto entry = sessions_.find(stamp.deployment.value);
    return stamp.deployment.isValid() && entry != sessions_.end() && entry->second.active &&
           !entry->second.exhausted && !entry->second.session.bearerToken.empty() &&
           entry->second.session.expiresAtUnix > nowUnix &&
           entry->second.generation == stamp.generation;
}

void ServerSessionRegistry::invalidateAll() noexcept
{
    for (auto& sessionEntry : sessions_) {
        auto& entry = sessionEntry.second;
        entry.active = false;
        std::fill(entry.session.bearerToken.begin(), entry.session.bearerToken.end(), '\0');
        entry.session.bearerToken.clear();
        if (entry.generation == std::numeric_limits<std::uint64_t>::max()) {
            entry.exhausted = true;
        } else {
            ++entry.generation;
        }
    }
}

} // namespace helicoid::manager
