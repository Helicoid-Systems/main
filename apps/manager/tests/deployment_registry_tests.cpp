#include "deployment_registry.h"

#include <cstdlib>
#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

using namespace helicoid::manager;

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

AuthenticatedSession makeSession(const DeploymentId& deployment,
                                 std::string profileId,
                                 std::string token,
                                 std::string managerId = "manager-device",
                                 std::int64_t expiresAtUnix = std::numeric_limits<std::int64_t>::max())
{
    return AuthenticatedSession{
        deployment,
        std::move(profileId),
        "account-1",
        std::move(managerId),
        "operator",
        std::move(token),
        expiresAtUnix,
    };
}

void scopedComputerIdentityIncludesDeployment()
{
    const DeploymentId first{"dep_00000000000000000000000000000001"};
    const DeploymentId second{"dep_00000000000000000000000000000002"};
    const auto firstComputer = ScopedComputerId::create(first, "computer-1");
    const auto sameServerComputer = ScopedComputerId::create(first, "computer-1");
    const auto secondComputer = ScopedComputerId::create(second, "computer-1");
    const auto controlCharacterComputer = ScopedComputerId::create(first, "computer\n1");

    require(firstComputer.has_value() && sameServerComputer.has_value() && secondComputer.has_value(),
            "valid deployment-scoped computer identities must be accepted");
    require(!controlCharacterComputer,
            "deployment-scoped computer identities must reject control characters");
    require(*firstComputer == *sameServerComputer,
            "the same server-local ID on one deployment must identify the same computer");
    require(!(*firstComputer == *secondComputer),
            "duplicate server-local IDs on separate deployments must remain distinct");

    std::unordered_map<ScopedComputerId, int, ScopedComputerIdHash> scopedItems;
    scopedItems.emplace(*firstComputer, 1);
    scopedItems.emplace(*secondComputer, 2);
    require(scopedItems.size() == 2 && scopedItems.at(*firstComputer) == 1 &&
                scopedItems.at(*secondComputer) == 2,
            "deployment-scoped identities must be safe as application registry keys");
}

void scopedGroupIdentityIncludesDeployment()
{
    const DeploymentId first{"dep_00000000000000000000000000000001"};
    const DeploymentId second{"dep_00000000000000000000000000000002"};
    const auto firstGroup = ScopedGroupId::create(first, "group-1");
    const auto sameServerGroup = ScopedGroupId::create(first, "group-1");
    const auto secondGroup = ScopedGroupId::create(second, "group-1");

    require(firstGroup.has_value() && sameServerGroup.has_value() && secondGroup.has_value(),
            "valid deployment-scoped group identities must be accepted");
    require(*firstGroup == *sameServerGroup,
            "the same server-local group ID on one deployment must identify the same group");
    require(!(*firstGroup == *secondGroup),
            "duplicate server-local group IDs on separate deployments must remain distinct");

    require(!ScopedAccountId::create(first, "account\t1"),
            "deployment-scoped entity identities must reject control characters");
}

template <typename ScopedId, typename Kind>
void requireScopedEntityMapSeparatesDeployments(const char* serverEntityId,
                                                const char* description)
{
    const DeploymentId first{"dep_00000000000000000000000000000001"};
    const DeploymentId second{"dep_00000000000000000000000000000002"};
    const auto firstEntity = ScopedId::create(first, serverEntityId);
    const auto secondEntity = ScopedId::create(second, serverEntityId);
    require(firstEntity && secondEntity, "valid scoped entity identities must be accepted");

    std::unordered_map<ScopedId, int, ScopedEntityIdHash<Kind>> entities;
    entities.emplace(*firstEntity, 1);
    entities.emplace(*secondEntity, 2);
    require(entities.size() == 2 && entities.at(*firstEntity) == 1 &&
                entities.at(*secondEntity) == 2,
            description);
}

void scopedEntityIdentitiesAreHashable()
{
    requireScopedEntityMapSeparatesDeployments<ScopedAccountId, AccountKind>(
        "account-1", "duplicate account IDs must remain distinct registry keys");
    requireScopedEntityMapSeparatesDeployments<ScopedGroupId, GroupKind>(
        "group-1", "duplicate group IDs must remain distinct registry keys");
    requireScopedEntityMapSeparatesDeployments<ScopedManagerId, ManagerKind>(
        "manager-1", "duplicate manager IDs must remain distinct registry keys");
    requireScopedEntityMapSeparatesDeployments<ScopedAccessAssignmentId, AccessAssignmentKind>(
        "assignment-1", "duplicate access-assignment IDs must remain distinct registry keys");
    requireScopedEntityMapSeparatesDeployments<ScopedJobId, JobKind>(
        "job-1", "duplicate job IDs must remain distinct registry keys");
    requireScopedEntityMapSeparatesDeployments<ScopedFileRequestId, FileRequestKind>(
        "request-1", "duplicate file-request IDs must remain distinct registry keys");
    requireScopedEntityMapSeparatesDeployments<ScopedPolicyId, PolicyKind>(
        "policy-1", "duplicate policy IDs must remain distinct registry keys");
    requireScopedEntityMapSeparatesDeployments<ScopedViewerId, ViewerKind>(
        "viewer-1", "duplicate viewer IDs must remain distinct registry keys");
}

void availabilityChecksAreFencedPerProfile()
{
    DeploymentRegistry profiles;
    std::string error;
    require(profiles.addProfile(ConnectionProfile{"profile-a", "Server A", "https://a.example",
                                                 Availability::Unknown, "", ""}, error),
            "the first server profile must be accepted");
    require(profiles.addProfile(ConnectionProfile{"profile-b", "Server B", "https://b.example",
                                                 Availability::Unknown, "", ""}, error),
            "the second server profile must be accepted");

    const auto firstRequest = profiles.beginAvailabilityCheck("profile-a");
    const auto secondRequest = profiles.beginAvailabilityCheck("profile-b");
    require(firstRequest.has_value() && secondRequest.has_value(),
            "availability checks must begin independently");
    require(profiles.disconnect("profile-a"), "the first server profile must disconnect");
    require(!profiles.finishAvailabilityCheck(*firstRequest, Availability::Reachable),
            "a late response after disconnect must be ignored");
    require(profiles.finishAvailabilityCheck(*secondRequest, Availability::Reachable),
            "the other server response must still be accepted");

    const auto firstProfile = profiles.profile("profile-a");
    const auto secondProfile = profiles.profile("profile-b");
    require(firstProfile && firstProfile->availability == Availability::Unknown,
            "disconnecting one server must leave its profile disconnected");
    require(secondProfile && secondProfile->availability == Availability::Reachable,
            "disconnecting one server must not alter another server's availability");
}

void trustVerificationGenerationsAreFencedPerProfile()
{
    DeploymentRegistry profiles;
    std::string error;
    require(profiles.addProfile(ConnectionProfile{"profile-a", "Server A", "https://a.example",
                                                 Availability::Unknown, "", ""}, error),
            "the first server profile must be accepted");
    require(profiles.addProfile(ConnectionProfile{"profile-b", "Server B", "https://b.example",
                                                 Availability::Unknown, "", ""}, error),
            "the second server profile must be accepted");

    const auto oldVerification = profiles.beginTrustVerification("profile-a");
    const auto currentVerification = profiles.beginTrustVerification("profile-a");
    const auto otherServerVerification = profiles.beginTrustVerification("profile-b");
    require(oldVerification && currentVerification && otherServerVerification,
            "identity verification must begin independently for saved profiles");
    require(!profiles.isCurrent(*oldVerification),
            "starting a replacement verification must fence the older response");
    require(profiles.isCurrent(*currentVerification) && profiles.isCurrent(*otherServerVerification),
            "the current response for each server must remain valid");

    require(!profiles.setTrustedAuthority(
                "profile-a", "dep_00000000000000000000000000000001", "not-a-key", 1),
            "malformed public authority keys must not be saved");
    require(!profiles.setTrustedAuthority(
                "profile-a", "dep_00000000000000000000000000000001",
                std::string(64, 'a'), 0),
            "a zero authority checkpoint must not be trusted");
    require(profiles.setTrustedAuthority(
                "profile-a", "dep_00000000000000000000000000000001",
                std::string(64, 'a'), 7),
            "a validated deployment identity, authority key, and checkpoint must be saved together");
    require(!profiles.isCurrent(*currentVerification),
            "changing a trust pin must fence outstanding verification responses for that profile");
    const auto trustedProfile = profiles.profile("profile-a");
    require(trustedProfile &&
                trustedProfile->trustedGrantSigningPublicKey == std::string(64, 'a') &&
                trustedProfile->trustedAuthorityGeneration == 7,
            "the authority key and checkpoint must remain paired with the deployment identity");
    require(!profiles.setTrustedAuthority(
                "profile-a", "dep_00000000000000000000000000000001",
                std::string(64, 'a'), 6),
            "a lower authority checkpoint must not replace a saved checkpoint");
    require(!profiles.setTrustedAuthority(
                "profile-a", "dep_00000000000000000000000000000001",
                std::string(64, 'b'), 7),
            "the signing key must not change without an authority generation increase");
    require(profiles.setTrustedAuthority(
                "profile-a", "dep_00000000000000000000000000000001",
                std::string(64, 'b'), 8),
            "a newer authority generation may establish its corresponding signing key");
    const auto advancedTrust = profiles.profile("profile-a");
    require(advancedTrust && advancedTrust->trustedGrantSigningPublicKey == std::string(64, 'b') &&
                advancedTrust->trustedAuthorityGeneration == 8,
            "the newer authority generation and signing key must be saved together");
    require(!profiles.setTrustedAuthority(
                "profile-a", "dep_00000000000000000000000000000002",
                std::string(64, 'A'), 8),
            "noncanonical uppercase authority-key encodings must be refused");
    const auto unchangedTrust = profiles.profile("profile-a");
    require(unchangedTrust &&
                unchangedTrust->trustedDeploymentId ==
                    "dep_00000000000000000000000000000001" &&
                unchangedTrust->trustedGrantSigningPublicKey == std::string(64, 'b') &&
                unchangedTrust->trustedAuthorityGeneration == 8,
            "a rejected replacement must leave the previously trusted authority intact");
    require(!profiles.setTrustedDeploymentId(
                "profile-a", "dep_00000000000000000000000000000002"),
            "the legacy identity-only setter cannot erase a durable authority checkpoint");
    require(profiles.restoreTrustedAuthorityState(
                "profile-a", "dep_00000000000000000000000000000001", "", 0),
            "a failed settings save can restore a legacy identity-only trust snapshot");
    const auto restoredLegacyTrust = profiles.profile("profile-a");
    require(restoredLegacyTrust &&
                restoredLegacyTrust->trustedDeploymentId ==
                    "dep_00000000000000000000000000000001" &&
                restoredLegacyTrust->trustedGrantSigningPublicKey.empty() &&
                restoredLegacyTrust->trustedAuthorityGeneration == 0,
            "rollback of an upgrade must restore the exact legacy trust state");
    require(profiles.isCurrent(*otherServerVerification),
            "changing one server's trust pin must not affect another server");

    const auto disconnectBoundVerification = profiles.beginTrustVerification("profile-a");
    require(disconnectBoundVerification.has_value(),
            "a profile can start a new identity check after saving its trust pin");
    require(profiles.disconnect("profile-a"), "the first profile must disconnect");
    require(!profiles.isCurrent(*disconnectBoundVerification),
            "disconnecting a profile must fence its pending identity verification");
    require(profiles.clearTrustedDeploymentId("profile-a"),
            "the first profile's trust can be cleared");
    const auto clearedProfile = profiles.profile("profile-a");
    require(clearedProfile && clearedProfile->trustedDeploymentId.empty() &&
                clearedProfile->trustedGrantSigningPublicKey.empty() &&
                clearedProfile->trustedAuthorityGeneration == 0,
            "clearing deployment trust must also clear its key and checkpoint");
    require(profiles.isCurrent(*otherServerVerification),
            "disconnecting one profile must not fence another profile's identity verification");
}

void sessionGenerationsAreIsolatedPerDeployment()
{
    const DeploymentId first{"dep_00000000000000000000000000000001"};
    const DeploymentId second{"dep_00000000000000000000000000000002"};
    ServerSessionRegistry sessions;

    const auto firstStamp = sessions.activate(makeSession(first, "profile-a", "token-a"));
    const auto secondStamp = sessions.activate(makeSession(second, "profile-b", "token-b"));
    require(firstStamp.has_value() && secondStamp.has_value(),
            "valid sessions for separate deployments must be accepted");

    const auto replacementStamp = sessions.activate(makeSession(first, "profile-a", "token-a2"));
    require(replacementStamp.has_value(), "a deployment session can be replaced");
    require(!sessions.isCurrent(*firstStamp), "replaced session callbacks must become stale");
    require(sessions.isCurrent(*replacementStamp), "the replacement session must be current");
    require(sessions.isCurrent(*secondStamp), "replacing one deployment must not affect another");

    const auto* firstSession = sessions.session(first);
    require(firstSession != nullptr && firstSession->bearerToken == "token-a2",
            "the first deployment must retain its own replacement credentials");
    require(sessions.invalidate(first), "the first deployment session must invalidate");
    require(firstSession->bearerToken.empty(), "invalidation must erase its bearer token");
    require(!sessions.isCurrent(*replacementStamp), "invalidated callbacks must become stale");
    require(sessions.isCurrent(*secondStamp), "invalidating one deployment must preserve another");
}

void expiredSessionsFenceCredentialsAndCallbacksPerDeployment()
{
    const DeploymentId first{"dep_00000000000000000000000000000001"};
    const DeploymentId second{"dep_00000000000000000000000000000002"};
    ServerSessionRegistry sessions;

    const auto firstStamp = sessions.activate(
        makeSession(first, "profile-a", "token-a", "manager-device", 100));
    const auto secondStamp = sessions.activate(
        makeSession(second, "profile-b", "token-b", "manager-device", 200));
    require(firstStamp && secondStamp, "sessions with positive expiries must be accepted");
    require(sessions.session(first, 99) != nullptr && sessions.currentStamp(first, 99),
            "a session is available immediately before its expiry");
    require(sessions.isCurrent(*firstStamp, 99),
            "callbacks remain current immediately before expiry");

    require(sessions.isExpired(first, 100), "a session expires at its exact deadline");
    require(sessions.session(first, 100) == nullptr,
            "expired credentials must not be exposed by the registry");
    require(!sessions.currentStamp(first, 100) && !sessions.isCurrent(*firstStamp, 100),
            "expired sessions must reject new work and late callbacks");
    require(sessions.session(first) == nullptr && !sessions.currentStamp(first) &&
                !sessions.isCurrent(*firstStamp),
            "ordinary registry access must enforce wall-clock session expiry");

    require(!sessions.isExpired(second, 100) && sessions.session(second, 100) != nullptr,
            "expiry on one deployment must not invalidate another session");
    require(sessions.isCurrent(*secondStamp, 100),
            "callbacks for the other deployment remain current");
}

void observationScopesFencePermissionsPerServerAndSession()
{
    const DeploymentId first{"dep_00000000000000000000000000000001"};
    const DeploymentId second{"dep_00000000000000000000000000000002"};
    const SessionStamp firstSession{first, 1};
    const SessionStamp secondSession{second, 1};
    ObservationScopeRegistry scopes;
    const auto start = ObservationScopeRegistry::TimePoint{} + std::chrono::seconds(100);
    const auto expiry = start + std::chrono::seconds(30);
    const auto secondExpiry = start + std::chrono::seconds(60);

    require(scopes.replace(firstSession, false, {"computer-1"}, expiry, start),
            "an explicit observe scope must be stored for its current session");
    require(scopes.replace(secondSession, true, {}, secondExpiry, start),
            "an all-computers scope must be stored independently for another server");
    require(scopes.allows(firstSession, "computer-1", start),
            "an explicit permission must allow its listed computer");
    require(!scopes.allows(firstSession, "computer-2", start),
            "an explicit permission must deny unlisted computers");
    require(scopes.allows(secondSession, "computer-1", start),
            "duplicate server-local computer IDs must use their owning deployment scope");
    require(!scopes.replace(firstSession, true, {"computer-1"}, expiry, start),
            "contradictory all-computers scopes must be rejected");

    const auto expired = start + std::chrono::seconds(31);
    require(!scopes.hasCurrent(firstSession, expired),
            "an expired observe scope must not authorize new viewer work");
    require(scopes.expireIfCurrent(firstSession, expired),
            "the matching expired scope must be removable");
    require(scopes.allows(secondSession, "computer-1", expired),
            "expiring one server scope must leave another server authorized");

    const SessionStamp replacementSession{first, 2};
    require(scopes.replace(replacementSession, true, {}, expired + std::chrono::seconds(30), expired),
            "a refreshed session may install a fresh observe scope");
    require(!scopes.hasCurrent(firstSession, expired),
            "a replaced session stamp must not reuse its previous observe scope");
    require(!scopes.replace(firstSession, false, {"computer-1"},
                            expired + std::chrono::seconds(60), expired),
            "a late response from an older session must not replace its successor's scope");
    require(!scopes.expireIfCurrent(firstSession, expired + std::chrono::seconds(31)),
            "an old expiry timer must not erase the replacement session scope");
    require(scopes.allows(replacementSession, "computer-2", expired),
            "the replacement session's current all-computers scope must remain usable");
    require(scopes.invalidate(second), "a server scope can be invalidated independently");
    require(!scopes.hasCurrent(secondSession, expired) &&
                scopes.allows(replacementSession, "computer-2", expired),
            "invalidating one server's scope must preserve another server's access");
}

void authenticatedSessionRequiresManagerIdentity()
{
    const DeploymentId deployment{"dep_00000000000000000000000000000001"};
    ServerSessionRegistry sessions;
    require(!sessions.activate(makeSession(deployment, "profile-a", "token", "")),
            "an authenticated session without its manager-device identity must be rejected");
}

} // namespace

int main()
{
    scopedComputerIdentityIncludesDeployment();
    scopedGroupIdentityIncludesDeployment();
    scopedEntityIdentitiesAreHashable();
    availabilityChecksAreFencedPerProfile();
    trustVerificationGenerationsAreFencedPerProfile();
    sessionGenerationsAreIsolatedPerDeployment();
    expiredSessionsFenceCredentialsAndCallbacksPerDeployment();
    observationScopesFencePermissionsPerServerAndSession();
    authenticatedSessionRequiresManagerIdentity();
    std::cout << "deployment registry tests passed\n";
    return EXIT_SUCCESS;
}
