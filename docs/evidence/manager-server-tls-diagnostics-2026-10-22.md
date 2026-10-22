# Manager server TLS diagnostics — 2026-10-22

The selected-server health detail now reports the negotiated TLS protocol and
SHA-256 fingerprint of the immediate TLS peer certificate after a successful
HTTPS `/health` response. The loopback HTTP development endpoint is explicitly
shown as unencrypted. The UI says that TLS transport identity and the
deployment signing-key identity are separate checks; the health request does
not establish the deployment identity.

The Server ribbon's Network & Certificates action invokes the same selected
server check and writes the result into the existing server detail panel.
Services, backups, and updates remain disabled until their lifecycle backend
operations are connected.

The Manager continues to use its existing bounded request, 2xx response check,
and redirect rejection. The TLS detail reads `QNetworkReply::sslConfiguration()`
after the reply completes and does not suppress SSL errors or add an
insecure-certificate path. Qt documents the peer certificate chain and session
cipher on [`QSslConfiguration`](https://doc.qt.io/qt-6.10/qsslconfiguration.html),
the reply SSL configuration on
[`QNetworkReply`](https://doc.qt.io/qt-6.10/qnetworkreply.html), and the
certificate digest API on
[`QSslCertificate`](https://doc.qt.io/qt-6.10/qsslcertificate.html).

## Verification

- `cmake -S . -B /private/tmp/hs-manager-qt-config-20261022` stopped at
  `find_package(Qt6 6.10...<6.11 REQUIRED COMPONENTS Widgets Network)` because
  this workspace has no Qt 6.10 package configuration. The changed Manager UI
  was therefore not compiled or driven on screen.
- The response still requires a successful Qt network reply and a 2xx status
  before parsing health JSON or displaying TLS details. No live server probe
  was run for this UI change.
