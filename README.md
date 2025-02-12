# KWallet Framework

Safe desktop-wide storage for passwords

## Introduction

This framework contains 3 main components:
* Interface to KWallet, the safe desktop-wide storage for passwords on KDE work 
spaces.
* kwalletd exposes the KWallet api and proxies it to SecretService, it can use ksecretd or any other SecretService provider
* ksecretd exposes the Secret Service DBus API and is used to safely store the passwords on KDE work spaces.

The library can be built alone, without ksecretd, by setting the
`BUILD_KSECRETD` option to `OFF` and `BUILD_KWALLETD` option to `OFF`.


## Migrating to Secret Service
ksecretd is the codebase that used to be kwalletd, but now exposes only the SecretService api and it can be replaced with any other SecretService provider.

kwalletd will use ksecretd by default but can be configured to not depend from it but instead use any other system-provided secret service provider, with the following config in kwalletrc

```
[Migration]
UseKWalletBackend=false
```

The first time kwalletd will encounter this configuration, it will perform a migration of all existing wallets and write the content in the new SecretService provider.
