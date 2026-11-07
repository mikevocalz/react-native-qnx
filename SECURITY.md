# Security policy

React Native QNX is experimental. The active RN 0.88 development line receives
security fixes as maintainers can verify them; no supported production release
or response-time guarantee is established.

## Report a vulnerability

Use [GitHub private vulnerability reporting](https://github.com/mikevocalz/react-native-qnx/security/advisories/new)
when it is available. If the repository does not offer that flow, open a minimal
issue asking the owner for a private contact, without exploit details, target
addresses, credentials or licensed files. Do not put sensitive reports in
public issues, CI logs or screenshots.

Include the affected revision, dependency versions, host/target identity,
reproduction steps, observed impact and a small sanitized reproducer. Label
host, QNX VM and physical-board results separately.

Dependency changes must pass the frozen graph and source-contract checks.
The [braces mitigation](docs/security/braces.md) records a local backport;
registry findings remain visible. Qt/QNX license and entitlement material
belongs outside the repository and public build artifacts.
