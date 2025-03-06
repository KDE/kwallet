# KWallet Framework

Safe desktop-wide storage for passwords

## Introduction

This framework contains 3 main components:
* Interface to KWallet, the safe desktop-wide storage for passwords on KDE work 
spaces.
* kwalletd exposes the KWallet API and proxies it to Secret Service, it can use ksecretd or any other Secret Service provider
* ksecretd exposes the Secret Service DBus API and is used to safely store the passwords on KDE work spaces.

The library can be built alone, without ksecretd, by setting the
`BUILD_KSECRETD` option to `OFF` and `BUILD_KWALLETD` option to `OFF`.


## Migrating to another Secret Service provider
ksecretd is the codebase that used to be kwalletd, but now exposes only the Secret Service API and it can be replaced with any other Secret Service provider.

kwalletd will use ksecretd by default but can be configured to not depend on it but instead use any other system-provided Secret Service provider, with the following config in kwalletrc

```
[KSecretD]
Enabled=false
```

To export existing wallet entries to another Secret Service provider, the following additional configuration is needed:

```
[Migration]
MigrateTo3rdParty=true
```

The first time kwalletd will encounter this configuration, it will perform a migration of all existing wallets and write the content in the new Secret Service provider.

## Known Issues
To use a GPG wallet while ksecretd is enabled, add the `no-allow-external-cache` option to your `~/.gnupg/gpg-agent.conf` ([BUG: 458085](https://bugs.kde.org/458085)).
