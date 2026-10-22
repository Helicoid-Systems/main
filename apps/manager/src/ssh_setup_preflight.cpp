#include "ssh_setup_preflight.h"

#include <QAbstractSocket>
#include <QDir>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace helicoid::manager {
namespace {

constexpr qsizetype kMaximumOutputBytes = 64 * 1024;
constexpr int kTimeoutMilliseconds = 45'000;

const QByteArray kPreflightScript = R"PY(import ipaddress, json, os, platform, shutil, stat, subprocess
data_path = bytes.fromhex('{{DATA_PATH_HEX}}').decode('utf-8')
bind_address = bytes.fromhex('{{BIND_ADDRESS_HEX}}').decode('utf-8')
storage_probe_path = data_path
while not os.path.isdir(storage_probe_path):
    parent = os.path.dirname(storage_probe_path)
    if parent == storage_probe_path:
        raise RuntimeError("cannot locate storage path")
    storage_probe_path = parent
data_path_exists = os.path.lexists(data_path)
def command(argv, input_bytes=None):
    result = subprocess.run(argv, input=input_bytes, capture_output=True, timeout=5, check=False)
    if len(result.stdout) + len(result.stderr) > 65536:
        raise RuntimeError("preflight output exceeds bound")
    return result
values = {}
with open('/etc/os-release', encoding='utf-8') as stream:
    for line in stream:
        key, sep, value = line.strip().partition('=')
        if sep:
            values[key] = value.strip('"')
def ports(flag):
    result = command(['/usr/bin/ss', '-H', '-l', '-n', flag])
    if result.returncode:
        raise RuntimeError("cannot inspect listeners")
    found = set()
    for row in result.stdout.decode('utf-8', errors='strict').splitlines():
        fields = row.split()
        if len(fields) < 5:
            raise RuntimeError("invalid listener row")
        found.add(int(fields[3].rsplit(':', 1)[1]))
    return sorted(found)
sudo = command(['/usr/bin/sudo', '-n', 'true'])
docker_server_version = ''
compose_version = ''
bind_address_check_available = False
bind_address_available = False
if bind_address:
    try:
        result = command(['/usr/sbin/ip', '-j', 'address', 'show'])
        if result.returncode == 0:
            interfaces = json.loads(result.stdout.decode('utf-8', errors='strict'))
            expected_address = ipaddress.ip_address(bind_address)
            if isinstance(interfaces, list):
                bind_address_check_available = True
                for interface in interfaces:
                    if not isinstance(interface, dict):
                        continue
                    address_rows = interface.get('addr_info', [])
                    if not isinstance(address_rows, list):
                        continue
                    for address in address_rows:
                        if isinstance(address, dict) and isinstance(address.get('local'), str):
                            try:
                                if ipaddress.ip_address(address['local']) == expected_address:
                                    bind_address_available = True
                            except ValueError:
                                continue
    except (FileNotFoundError, OSError, RuntimeError, subprocess.TimeoutExpired, UnicodeDecodeError, ValueError):
        pass
if {{CHECK_COMPOSE}}:
    def docker_output(arguments):
        try:
            result = command(['/usr/bin/sudo', '-n', '/usr/bin/docker', '--host',
                'unix:///var/run/docker.sock'] + arguments)
        except (FileNotFoundError, OSError, subprocess.TimeoutExpired):
            return ''
        if result.returncode:
            return ''
        output = result.stdout.decode('utf-8', errors='strict').strip()
        return output if len(output) <= 128 and '\n' not in output else ''
    docker_server_version = docker_output(['info', '--format', '{{.ServerVersion}}'])
    compose_version = docker_output(['compose', 'version', '--short'])
def optional_json_command(argv, payload=None):
    try:
        result = command(argv, payload)
        if result.returncode or len(result.stdout) > 65536:
            return (False, None)
        value = json.loads(result.stdout.decode('utf-8', errors='strict'))
        if isinstance(value, (dict, list)) or value is None:
            return (True, value)
        return (False, None)
    except (FileNotFoundError, OSError, RuntimeError, subprocess.TimeoutExpired,
            UnicodeDecodeError, ValueError):
        return (False, None)
lifecycle = {'backend_available': False, 'state': 'unavailable'}
server_cli = '/opt/helicoid/bin/hs-server'
try:
    cli_is_trusted = True
    for path in ['/opt', '/opt/helicoid', '/opt/helicoid/bin', server_cli]:
        cli_metadata = os.lstat(path)
        if stat.S_ISLNK(cli_metadata.st_mode) or cli_metadata.st_uid != 0 or \
            (cli_metadata.st_mode & 0o022) != 0:
            cli_is_trusted = False
            break
    cli_is_trusted = cli_is_trusted and stat.S_ISREG(cli_metadata.st_mode) and \
        os.access(server_cli, os.X_OK)
except (FileNotFoundError, OSError):
    cli_is_trusted = False
if sudo.returncode == 0 and cli_is_trusted:
    cli_prefix = ['/usr/bin/sudo', '-n', server_cli, 'installer']
    capabilities_ok, capabilities = optional_json_command(cli_prefix + ['capabilities-v2'])
    if capabilities_ok and isinstance(capabilities, dict) and capabilities.get('contract_version') == 2:
        lifecycle['backend_available'] = True
        lifecycle['capabilities'] = capabilities
        configuration_ok, configuration = optional_json_command(cli_prefix + ['configuration-v2'])
        if not configuration_ok:
            lifecycle['state'] = 'unavailable'
        elif configuration is None:
            lifecycle['state'] = 'not_installed'
        elif isinstance(configuration, dict):
            status_input = json.dumps({'contract_version': 2,
                'configuration': configuration}, separators=(',', ':')).encode('utf-8')
            status_ok, status = optional_json_command(cli_prefix + ['status-v2'], status_input)
            if status_ok and isinstance(status, dict) and status.get('contract_version') == 2:
                lifecycle['state'] = status.get('state', 'unavailable')
                lifecycle['action'] = status.get('action')
                lifecycle['current_phase'] = status.get('current_phase')
                lifecycle['service_installed'] = status.get('service_installed')
                lifecycle['bootstrap_ready'] = status.get('bootstrap_ready')
                lifecycle['server_ready_for_enrollment'] = status.get('server_ready_for_enrollment')
            else:
                lifecycle['state'] = 'unavailable'
print(json.dumps({'version': 1, 'os_id': values.get('ID', ''),
    'os_version': values.get('VERSION_ID', ''), 'architecture': platform.machine(),
    'systemd_available': shutil.which('systemctl') is not None and os.path.isdir('/run/systemd/system'),
    'sudo_available': sudo.returncode == 0,
    'free_bytes': shutil.disk_usage(storage_probe_path).free,
    'storage_probe_path': storage_probe_path,
    'data_path_exists': data_path_exists,
    'existing_installation': os.path.lexists('/opt/helicoid'),
    'docker_server_version': docker_server_version,
    'compose_version': compose_version,
    'bind_address_check_available': bind_address_check_available,
    'bind_address_available': bind_address_available,
    'tcp_ports': ports('-t'), 'udp_ports': ports('-u'), 'lifecycle': lifecycle}))
)PY";

bool asciiHostName(const QString& host)
{
    if (host.isEmpty() || host.size() > 253 || host.startsWith(QLatin1Char('-'))) {
        return false;
    }
    for (const QChar character : host) {
        const ushort value = character.unicode();
        if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') || value == '.' || value == '-')) {
            return false;
        }
    }
    const QStringList labels = host.split(QLatin1Char('.'), Qt::KeepEmptyParts);
    return std::all_of(labels.cbegin(), labels.cend(), [](const QString& label) {
        if (label.isEmpty() || label.size() > 63 || label.startsWith(QLatin1Char('-')) ||
            label.endsWith(QLatin1Char('-'))) {
            return false;
        }
        return true;
    });
}

bool parsePort(const QString& value, std::uint16_t& result)
{
    bool ok = false;
    const uint port = value.toUInt(&ok, 10);
    if (!ok || port == 0 || port > 65535) {
        return false;
    }
    result = static_cast<std::uint16_t>(port);
    return true;
}

bool boundedString(const QJsonValue& value, QString& output, int maximum)
{
    if (!value.isString()) {
        return false;
    }
    output = value.toString();
    return output.toUtf8().size() <= maximum;
}

bool safeVersionString(const QJsonValue& value, QString& output, int maximum)
{
    if (!boundedString(value, output, maximum)) {
        return false;
    }
    return std::all_of(output.cbegin(), output.cend(), [](QChar character) {
        const ushort code = character.unicode();
        return (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z') ||
               (code >= '0' && code <= '9') || code == '.' || code == '+' ||
               code == '_' || code == '-';
    });
}

bool supportsComposeVersion(const QString& version)
{
    const QString numeric = version.startsWith(QLatin1Char('v')) ? version.mid(1) : version;
    const QStringList components = numeric.split(QLatin1Char('.'), Qt::KeepEmptyParts);
    if (components.size() != 3) {
        return false;
    }
    std::uint64_t values[3]{};
    for (qsizetype index = 0; index < components.size(); ++index) {
        if (components[index].isEmpty() ||
            !std::all_of(components[index].cbegin(), components[index].cend(), [](QChar c) {
                return c >= QLatin1Char('0') && c <= QLatin1Char('9');
            })) {
            return false;
        }
        bool ok = false;
        values[index] = components[index].toULongLong(&ok, 10);
        if (!ok) {
            return false;
        }
    }
    return values[0] > 2 || (values[0] == 2 && values[1] >= 17);
}

bool parsePorts(const QJsonValue& value, QSet<int>& ports)
{
    if (!value.isArray()) {
        return false;
    }
    const QJsonArray array = value.toArray();
    if (array.size() > 1024) {
        return false;
    }
    for (const QJsonValue& entry : array) {
        if (!entry.isDouble()) {
            return false;
        }
        const double raw = entry.toDouble();
        if (!std::isfinite(raw) || std::floor(raw) != raw || raw < 1 || raw > 65535) {
            return false;
        }
        const int port = static_cast<int>(raw);
        if (ports.contains(port)) {
            return false;
        }
        ports.insert(port);
    }
    return true;
}

bool safeAbsolutePathText(const QString& path)
{
    if (path.isEmpty() || path.size() > 512 || !path.startsWith(QLatin1Char('/')) ||
        path.contains(QStringLiteral("//")) || QDir::cleanPath(path) != path) {
        return false;
    }
    for (const QChar character : path) {
        const ushort value = character.unicode();
        if (!(value == '/' || (value >= 'a' && value <= 'z') ||
              (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
              value == '-' || value == '_' || value == '.')) {
            return false;
        }
    }
    return true;
}

std::optional<QString> safeLifecycleToken(const QJsonValue& value, qsizetype maximum)
{
    if (!value.isString()) {
        return std::nullopt;
    }
    const QString token = value.toString();
    if (token.isEmpty() || token.size() > maximum ||
        !std::all_of(token.cbegin(), token.cend(), [](QChar character) {
            const ushort code = character.unicode();
            return (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9') ||
                   code == '_';
        })) {
        return std::nullopt;
    }
    return token;
}

QString safeLifecycleReason(const QJsonValue& value)
{
    if (!value.isString()) {
        return {};
    }
    const QString reason = value.toString();
    if (reason.toUtf8().size() > 1024) {
        return {};
    }
    QString display;
    display.reserve(reason.size());
    for (const QChar character : reason) {
        const ushort code = character.unicode();
        display.append(code >= 0x20 && code <= 0x7e ? character : QLatin1Char(' '));
    }
    return display.simplified();
}

std::optional<QString> parseFingerprint(const QByteArray& output)
{
    const QList<QByteArray> lines = output.split('\n');
    QString fingerprint;
    int matches = 0;
    for (const QByteArray& line : lines) {
        const QByteArray prefix("debug1: Server host key: ");
        if (!line.startsWith(prefix)) {
            continue;
        }
        const QList<QByteArray> parts = line.mid(prefix.size()).trimmed().split(' ');
        if (parts.size() != 2 || !parts[1].startsWith("SHA256:") || parts[1].size() != 50) {
            return std::nullopt;
        }
        const QByteArray encoded = parts[1].mid(7);
        if (!std::all_of(encoded.cbegin(), encoded.cend(), [](char value) {
                return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
                       (value >= '0' && value <= '9') || value == '+' || value == '/';
            })) {
            return std::nullopt;
        }
        fingerprint = QString::fromLatin1(parts[1]);
        ++matches;
    }
    if (matches != 1) {
        return std::nullopt;
    }
    return fingerprint;
}

std::optional<QString> makeReport(const QByteArray& stdoutBytes,
                                  const QByteArray& stderrBytes,
                                  std::uint16_t authorityPort,
                                  SSHSetupDeploymentKind deploymentKind,
                                  const QString& bindAddress,
                                  QString& error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(stdoutBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("The host returned an invalid preflight response.");
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    const QJsonValue version = object.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toInt(-1) != 1) {
        error = QStringLiteral("The host returned an unsupported preflight version.");
        return std::nullopt;
    }

    QString osId;
    QString osVersion;
    QString architecture;
    QString storageProbePath;
    QString dockerServerVersion;
    QString composeVersion;
    const QJsonValue freeBytesValue = object.value(QStringLiteral("free_bytes"));
    const double freeBytes = freeBytesValue.toDouble(-1);
    if (!boundedString(object.value(QStringLiteral("os_id")), osId, 32) ||
        !boundedString(object.value(QStringLiteral("os_version")), osVersion, 32) ||
        !boundedString(object.value(QStringLiteral("architecture")), architecture, 32) ||
        !boundedString(object.value(QStringLiteral("storage_probe_path")), storageProbePath, 512) ||
        !safeVersionString(object.value(QStringLiteral("docker_server_version")), dockerServerVersion, 128) ||
        !safeVersionString(object.value(QStringLiteral("compose_version")), composeVersion, 64) ||
        !safeAbsolutePathText(storageProbePath) ||
        !freeBytesValue.isDouble() || !std::isfinite(freeBytes) || std::floor(freeBytes) != freeBytes ||
        freeBytes < 0 || freeBytes > 9'007'199'254'740'991.0 ||
        !object.value(QStringLiteral("systemd_available")).isBool() ||
        !object.value(QStringLiteral("sudo_available")).isBool() ||
        !object.value(QStringLiteral("existing_installation")).isBool() ||
        !object.value(QStringLiteral("data_path_exists")).isBool() ||
        !object.value(QStringLiteral("bind_address_check_available")).isBool() ||
        !object.value(QStringLiteral("bind_address_available")).isBool()) {
        error = QStringLiteral("The host returned invalid or out-of-range preflight facts.");
        return std::nullopt;
    }

    QSet<int> tcpPorts;
    QSet<int> udpPorts;
    if (!parsePorts(object.value(QStringLiteral("tcp_ports")), tcpPorts) ||
        !parsePorts(object.value(QStringLiteral("udp_ports")), udpPorts)) {
        error = QStringLiteral("The host returned an invalid listener list.");
        return std::nullopt;
    }
    const auto fingerprint = parseFingerprint(stderrBytes);
    if (!fingerprint) {
        error = QStringLiteral("SSH did not provide one verified host-key fingerprint.");
        return std::nullopt;
    }

    const bool supportedHost = osId == QStringLiteral("ubuntu") &&
                               (osVersion == QStringLiteral("24.04") ||
                                osVersion == QStringLiteral("26.04")) &&
                               architecture == QStringLiteral("x86_64");
    const bool systemd = object.value(QStringLiteral("systemd_available")).toBool();
    const bool sudo = object.value(QStringLiteral("sudo_available")).toBool();
    const bool disk = freeBytes >= 2'147'483'648.0;
    const bool portFree = !tcpPorts.contains(static_cast<int>(authorityPort));
    const bool existing = object.value(QStringLiteral("existing_installation")).toBool();
    const bool dataPathExists = object.value(QStringLiteral("data_path_exists")).toBool();
    const bool composeDeployment = deploymentKind == SSHSetupDeploymentKind::Compose;
    const bool composeRuntimeAvailable = !dockerServerVersion.isEmpty() &&
                                         supportsComposeVersion(composeVersion);
    const bool bindCheckAvailable = object.value(
        QStringLiteral("bind_address_check_available")).toBool();
    const bool bindAddressAvailable = object.value(
        QStringLiteral("bind_address_available")).toBool();

    QStringList lines{
        QStringLiteral("SSH host key verified: %1").arg(*fingerprint),
        QStringLiteral("Host checks (read-only):"),
        QStringLiteral("  Ubuntu 24.04/26.04 x86-64: %1 (%2 %3, %4)")
            .arg(supportedHost ? QStringLiteral("pass") : QStringLiteral("review"), osId, osVersion, architecture),
        QStringLiteral("  systemd available: %1").arg(systemd ? QStringLiteral("pass") : QStringLiteral("fail")),
        QStringLiteral("  noninteractive sudo: %1").arg(sudo ? QStringLiteral("pass") : QStringLiteral("fail")),
        QStringLiteral("  free space on %3 (2 GiB minimum): %1 (%2 MiB)")
            .arg(disk ? QStringLiteral("pass") : QStringLiteral("fail"))
            .arg(static_cast<qulonglong>(freeBytes / (1024.0 * 1024.0)))
            .arg(storageProbePath),
        QStringLiteral("  selected data path: %1")
            .arg(dataPathExists ? QStringLiteral("already exists; review before install")
                                : QStringLiteral("not present")),
        QStringLiteral("  TCP port %1 available: %2")
            .arg(static_cast<unsigned int>(authorityPort))
            .arg(portFree ? QStringLiteral("pass") : QStringLiteral("fail")),
        QStringLiteral("  existing /opt/helicoid installation: %1")
            .arg(existing ? QStringLiteral("review before repair") : QStringLiteral("none found")),
        QStringLiteral("  UDP listeners reported: %1")
            .arg(static_cast<qulonglong>(udpPorts.size())),
    };
    if (!bindAddress.isEmpty()) {
        lines.insert(3, QStringLiteral("  configured private host address %1: %2")
            .arg(bindAddress,
                 !bindCheckAvailable ? QStringLiteral("could not verify")
                 : bindAddressAvailable ? QStringLiteral("assigned to this host")
                                        : QStringLiteral("not assigned to this host")));
    }
    if (composeDeployment) {
        lines.insert(4, QStringLiteral("  Docker Engine reachable: %1 (%2)")
                         .arg(dockerServerVersion.isEmpty() ? QStringLiteral("fail") : QStringLiteral("pass"),
                              dockerServerVersion.isEmpty() ? QStringLiteral("unavailable") : dockerServerVersion));
        lines.insert(5, QStringLiteral("  Docker Compose 2.17.0 or newer: %1 (%2)")
                         .arg(supportsComposeVersion(composeVersion) ? QStringLiteral("pass") : QStringLiteral("fail"),
                              composeVersion.isEmpty() ? QStringLiteral("unavailable") : composeVersion));
        if (!composeRuntimeAvailable) {
            lines.append(QStringLiteral(
                "  Compose is not ready; install or repair Docker Engine and its Compose plugin before setup."));
        }
    }
    const QJsonValue lifecycleValue = object.value(QStringLiteral("lifecycle"));
    if (!lifecycleValue.isObject()) {
        lines.append(QStringLiteral("  Lifecycle status: not reported by this host check."));
    } else {
        const QJsonObject lifecycle = lifecycleValue.toObject();
        const QJsonValue backendAvailableValue =
            lifecycle.value(QStringLiteral("backend_available"));
        const bool backendAvailable = backendAvailableValue.toBool(false);
        if (!backendAvailableValue.isBool() || !backendAvailable) {
            lines.append(QStringLiteral(
                "  Lifecycle status: unavailable (the root-owned server backend or noninteractive sudo is unavailable)."));
        } else {
            const auto state = safeLifecycleToken(lifecycle.value(QStringLiteral("state")), 48);
            const QSet<QString> knownStates{
                QStringLiteral("not_installed"), QStringLiteral("in_progress"),
                QStringLiteral("attention_required"), QStringLiteral("completed"),
                QStringLiteral("unavailable")};
            if (!state || !knownStates.contains(*state)) {
                lines.append(QStringLiteral("  Lifecycle status: backend response was invalid."));
            } else {
                lines.append(QStringLiteral("  Lifecycle status: %1").arg(*state));
                const QJsonValue actionValue = lifecycle.value(QStringLiteral("action"));
                if (!actionValue.isNull()) {
                    const auto action = safeLifecycleToken(actionValue, 32);
                    if (action) {
                        lines.append(QStringLiteral("  Last lifecycle action: %1").arg(*action));
                    }
                }
                const auto phase = safeLifecycleToken(
                    lifecycle.value(QStringLiteral("current_phase")), 48);
                if (phase) {
                    lines.append(QStringLiteral("  Current lifecycle phase: %1").arg(*phase));
                }
                for (const auto& [key, label] : {
                         std::pair{QStringLiteral("service_installed"), QStringLiteral("Service installed")},
                         std::pair{QStringLiteral("bootstrap_ready"), QStringLiteral("Administrator bootstrap ready")},
                         std::pair{QStringLiteral("server_ready_for_enrollment"), QStringLiteral("Ready for enrollment")}}) {
                    const QJsonValue flag = lifecycle.value(key);
                    if (flag.isBool()) {
                        lines.append(QStringLiteral("  %1: %2")
                            .arg(label, flag.toBool() ? QStringLiteral("yes") : QStringLiteral("no")));
                    }
                }
                const QJsonObject capabilities =
                    lifecycle.value(QStringLiteral("capabilities")).toObject();
                if (capabilities.value(QStringLiteral("contract_version")).toInt(-1) == 2) {
                    const QJsonValue actionsValue =
                        capabilities.value(QStringLiteral("supported_actions"));
                    QStringList actions;
                    if (actionsValue.isArray() && actionsValue.toArray().size() <= 16) {
                        for (const QJsonValue& value : actionsValue.toArray()) {
                            const auto action = safeLifecycleToken(value, 32);
                            if (action) {
                                actions.append(*action);
                            }
                        }
                    }
                    if (!actions.isEmpty()) {
                        lines.append(QStringLiteral("  Supported lifecycle actions: %1")
                                         .arg(actions.join(QStringLiteral(", "))));
                    }
                    const QJsonObject reasons =
                        capabilities.value(QStringLiteral("reasons_unavailable")).toObject();
                    const QString legacyLimitation = safeLifecycleReason(
                        reasons.value(QStringLiteral("online_update_backup_restore")));
                    if (!capabilities.value(QStringLiteral("online_update")).toBool(false)) {
                        QString limitation = safeLifecycleReason(
                            reasons.value(QStringLiteral("online_update")));
                        if (limitation.isEmpty()) {
                            limitation = legacyLimitation;
                        }
                        lines.append(QStringLiteral("  Signed online update and rollback: unavailable%1")
                            .arg(limitation.isEmpty() ? QString()
                                 : QStringLiteral(" (%1)").arg(limitation)));
                    }
                    if (!capabilities.value(QStringLiteral("backup_and_restore")).toBool(false)) {
                        QString limitation = safeLifecycleReason(
                            reasons.value(QStringLiteral("backup_and_restore")));
                        if (limitation.isEmpty()) {
                            limitation = legacyLimitation;
                        }
                        lines.append(QStringLiteral(
                            "  Deployment backup and restore orchestration: unavailable%1")
                            .arg(limitation.isEmpty() ? QString()
                                 : QStringLiteral(" (%1)").arg(limitation)));
                    }
                } else {
                    lines.append(QStringLiteral("  Lifecycle capabilities: backend response was invalid."));
                }
            }
        }
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace

std::optional<SSHSetupAddress> parseSSHSetupAddress(const QString& input, QString* error)
{
    const QString address = input.trimmed();
    if (error != nullptr) {
        error->clear();
    }
    const auto fail = [error](const QString& reason) -> std::optional<SSHSetupAddress> {
        if (error != nullptr) {
            *error = reason;
        }
        return std::nullopt;
    };
    if (address.isEmpty() || address.toUtf8().size() > 255 ||
        address.contains(QRegularExpression(QStringLiteral("\\s"))) ||
        address.contains(QLatin1Char('%')) || address.contains(QLatin1Char('/')) ||
        address.contains(QLatin1Char('\\')) || address.contains(QLatin1Char('?')) ||
        address.contains(QLatin1Char('#'))) {
        return fail(QStringLiteral("Use [user@]host, optionally followed by :port."));
    }
    for (const QChar character : address) {
        if (character.unicode() > 0x7f) {
            return fail(QStringLiteral("SSH addresses must use ASCII host and user names."));
        }
    }

    QString destination = address;
    const qsizetype at = address.indexOf(QLatin1Char('@'));
    if (at >= 0) {
        if (at == 0 || at != address.lastIndexOf(QLatin1Char('@'))) {
            return fail(QStringLiteral("The SSH user name is invalid."));
        }
        const QString user = address.left(at);
        if (user.startsWith(QLatin1Char('-')) || user.size() > 64 ||
            !std::all_of(user.cbegin(), user.cend(), [](QChar character) {
                const ushort value = character.unicode();
                return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                       (value >= '0' && value <= '9') || value == '_' || value == '-' || value == '.';
            })) {
            return fail(QStringLiteral("The SSH user name is invalid."));
        }
        destination = address.mid(at + 1);
    }

    QString host;
    std::uint16_t port = 22;
    QString sshHost;
    if (destination.startsWith(QLatin1Char('['))) {
        const qsizetype close = destination.indexOf(QLatin1Char(']'));
        if (close < 0) {
            return fail(QStringLiteral("The bracketed IPv6 address is invalid."));
        }
        host = destination.mid(1, close - 1);
        const QString tail = destination.mid(close + 1);
        if (!tail.isEmpty() && (!tail.startsWith(QLatin1Char(':')) ||
                                !parsePort(tail.mid(1), port))) {
            return fail(QStringLiteral("The SSH port must be between 1 and 65535."));
        }
        sshHost = host;
        QHostAddress parsed;
        if (!parsed.setAddress(host) || parsed.protocol() != QAbstractSocket::IPv6Protocol) {
            return fail(QStringLiteral("Use a valid bracketed IPv6 address."));
        }
    } else {
        const qsizetype colonCount = destination.count(QLatin1Char(':'));
        if (colonCount == 1) {
            const qsizetype colon = destination.lastIndexOf(QLatin1Char(':'));
            std::uint16_t parsedPort = 22;
            if (parsePort(destination.mid(colon + 1), parsedPort)) {
                host = destination.left(colon);
                port = parsedPort;
            } else {
                return fail(QStringLiteral("The SSH port must be between 1 and 65535."));
            }
        } else {
            host = destination;
        }
        QHostAddress parsed;
        if (colonCount > 1) {
            if (!parsed.setAddress(host) || parsed.protocol() != QAbstractSocket::IPv6Protocol) {
                return fail(QStringLiteral("Use a valid IPv6 address in brackets when specifying a port."));
            }
            sshHost = host;
        } else if (parsed.setAddress(host)) {
            sshHost = host;
        } else if (asciiHostName(host)) {
            sshHost = host;
        } else {
            return fail(QStringLiteral("Enter a valid host name or IP address."));
        }
    }

    if (at >= 0) {
        destination = address.left(at + 1) + sshHost;
    } else {
        destination = sshHost;
    }
    return SSHSetupAddress{destination, port};
}

bool validSSHSetupDataDirectory(const QString& input)
{
    const QString path = input.trimmed();
    const QString root = QStringLiteral("/var/lib/helicoid/data");
    if (path.isEmpty() || path.size() > 512 || !path.startsWith(root) ||
        (path.size() > root.size() && path.at(root.size()) != QLatin1Char('/')) ||
        path.endsWith(QLatin1Char('/')) || path.contains(QStringLiteral("//")) ||
        QDir::cleanPath(path) != path) {
        return false;
    }
    for (const QChar character : path) {
        const ushort value = character.unicode();
        if (!(value == '/' || (value >= 'a' && value <= 'z') ||
              (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
              value == '-' || value == '_' || value == '.')) {
            return false;
        }
    }
    const QStringList segments = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    for (qsizetype index = 1; index < segments.size(); ++index) {
        if (segments[index].isEmpty() || segments[index] == QStringLiteral(".") ||
            segments[index] == QStringLiteral("..")) {
            return false;
        }
    }
    return true;
}

void runSSHSetupPreflight(QObject* owner,
                          const SSHSetupAddress& address,
                          std::uint16_t authorityPort,
                          SSHSetupDeploymentKind deploymentKind,
                          const QString& bindAddress,
                          const QString& dataDirectory,
                          std::function<void(SSHSetupCheckResult)> completion)
{
    if (!completion) {
        return;
    }
    QHostAddress parsedBindAddress;
    const bool validBindAddress = bindAddress.isEmpty() ||
        (!bindAddress.contains(QLatin1Char('%')) && bindAddress.toUtf8().size() <= 64 &&
         parsedBindAddress.setAddress(bindAddress));
    if (owner == nullptr || authorityPort == 0 || !validBindAddress ||
        !validSSHSetupDataDirectory(dataDirectory)) {
        completion({false, QStringLiteral("The SSH host-check request is invalid. No host changes were made.")});
        return;
    }
    auto* process = new QProcess(owner);
    auto* timeout = new QTimer(process);
    timeout->setSingleShot(true);
    timeout->setInterval(kTimeoutMilliseconds);
    auto finished = std::make_shared<bool>(false);
    auto stdoutBytes = std::make_shared<QByteArray>();
    auto stderrBytes = std::make_shared<QByteArray>();

    auto complete = [process, timeout, finished, completion = std::move(completion)](
                              SSHSetupCheckResult result) mutable {
        if (*finished) {
            return;
        }
        *finished = true;
        timeout->stop();
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
        process->deleteLater();
        completion(std::move(result));
    };
    auto drainOutput = [process, stdoutBytes, stderrBytes, complete]() mutable -> bool {
        stdoutBytes->append(process->readAllStandardOutput());
        stderrBytes->append(process->readAllStandardError());
        if (stdoutBytes->size() + stderrBytes->size() > kMaximumOutputBytes) {
            complete({false, QStringLiteral("SSH preflight output exceeded its 64 KiB limit.")});
            return false;
        }
        return true;
    };

    QObject::connect(process, &QProcess::readyReadStandardOutput, owner, [drainOutput]() mutable {
        drainOutput();
    });
    QObject::connect(process, &QProcess::readyReadStandardError, owner, [drainOutput]() mutable {
        drainOutput();
    });
    QObject::connect(process,
                     qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     owner,
                     [drainOutput, complete, stdoutBytes, stderrBytes, authorityPort,
                      deploymentKind, bindAddress](
                         int exitCode, QProcess::ExitStatus status) mutable {
                         if (!drainOutput()) {
                             return;
                         }
                         if (status != QProcess::NormalExit || exitCode != 0) {
                             const QByteArray errors = *stderrBytes;
                             const bool identityRefused = errors.contains("Host key verification failed") ||
                                 errors.contains("REMOTE HOST IDENTIFICATION HAS CHANGED");
                             complete({false,
                                       identityRefused
                                           ? QStringLiteral("SSH refused the host identity in the existing known_hosts file. No host changes were made.")
                                           : QStringLiteral("SSH preflight failed. Check trusted host identity, SSH key authentication and remote Python 3 availability. No host changes were made.")});
                             return;
                         }
                         QString error;
                         const auto report = makeReport(*stdoutBytes, *stderrBytes, authorityPort,
                                                        deploymentKind, bindAddress, error);
                         if (!report) {
                             complete({false, error + QStringLiteral(" No host changes were made.")});
                             return;
                         }
                         complete({true, *report + QStringLiteral("\n\nNo files or services were changed.")});
                     });
    QObject::connect(process, &QProcess::errorOccurred, owner,
                     [complete](QProcess::ProcessError error) mutable {
                         if (error == QProcess::FailedToStart) {
                             complete({false, QStringLiteral("Could not start OpenSSH. No host changes were made.")});
                         }
                     });
    QObject::connect(timeout, &QTimer::timeout, owner, [complete]() mutable {
        complete({false, QStringLiteral("SSH host check timed out after 45 seconds. No host changes were made.")});
    });

#if defined(Q_OS_WIN)
    process->setProgram(QStringLiteral("ssh.exe"));
#else
    process->setProgram(QStringLiteral("/usr/bin/ssh"));
#endif
    process->setArguments({
        QStringLiteral("-v"),
        QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
        QStringLiteral("-o"), QStringLiteral("StrictHostKeyChecking=yes"),
        QStringLiteral("-o"), QStringLiteral("UpdateHostKeys=no"),
        QStringLiteral("-o"), QStringLiteral("ConnectTimeout=10"),
        QStringLiteral("-o"), QStringLiteral("ServerAliveInterval=5"),
        QStringLiteral("-o"), QStringLiteral("ServerAliveCountMax=2"),
        QStringLiteral("-o"), QStringLiteral("ClearAllForwardings=yes"),
        QStringLiteral("-o"), QStringLiteral("ControlMaster=no"),
        QStringLiteral("-o"), QStringLiteral("ControlPath=none"),
        QStringLiteral("-p"), QString::number(static_cast<unsigned int>(address.port)),
        QStringLiteral("--"), address.destination,
        QStringLiteral("/usr/bin/python3 -"),
    });
    QByteArray script = kPreflightScript;
    const QByteArray dataPathHex = dataDirectory.toUtf8().toHex();
    script.replace(QByteArrayLiteral("{{DATA_PATH_HEX}}"), dataPathHex);
    script.replace(QByteArrayLiteral("{{BIND_ADDRESS_HEX}}"), bindAddress.toUtf8().toHex());
    script.replace(QByteArrayLiteral("{{CHECK_COMPOSE}}"),
                   deploymentKind == SSHSetupDeploymentKind::Compose
                       ? QByteArrayLiteral("True") : QByteArrayLiteral("False"));
    QObject::connect(process, &QProcess::started, owner, [process, script, complete]() mutable {
        if (process->write(script) != script.size()) {
            complete({false, QStringLiteral("Could not send the bounded SSH preflight request. No host changes were made.")});
            return;
        }
        process->closeWriteChannel();
    });
    process->start(QIODevice::ReadWrite);
    timeout->start();
}

} // namespace helicoid::manager
