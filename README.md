# KWallet Framework

Safe desktop-wide storage for passwords

## Introduction

This framework contains two main components:
* Interface to KWallet, the safe desktop-wide storage for passwords on KDE work 
spaces.
* kwalletd exposes the KWallet api and proxies it to SecretService, it can use ksecretd or any other SecretService provider
* ksecretd exposes the Secret Service DBus API and is used to safely store the passwords on KDE work spaces.

The library can be built alone, without ksecretd, by setting the
`BUILD_KSECRETD` option to `OFF` and `BUILD_KWALLETD` option to `OFF`.


