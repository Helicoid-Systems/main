#pragma once

#include <QString>

#include <cstdint>
#include <functional>
#include <optional>

class QObject;

namespace helicoid::manager {

struct SSHSetupAddress {
    QString destination;
    std::uint16_t port = 22;
};

struct SSHSetupCheckResult {
    bool succeeded = false;
    QString report;
};

enum class SSHSetupDeploymentKind {
    Systemd,
    Compose,
};

std::optional<SSHSetupAddress> parseSSHSetupAddress(const QString& input,
                                                     QString* error = nullptr);
bool validSSHSetupDataDirectory(const QString& input);

/// Perform a read-only remote host preflight through the user's OpenSSH
/// configuration. SSH must already trust the server key in known_hosts.
void runSSHSetupPreflight(QObject* owner,
                          const SSHSetupAddress& address,
                          std::uint16_t authorityPort,
                          SSHSetupDeploymentKind deploymentKind,
                          const QString& bindAddress,
                          const QString& dataDirectory,
                          std::function<void(SSHSetupCheckResult)> completion);

} // namespace helicoid::manager
