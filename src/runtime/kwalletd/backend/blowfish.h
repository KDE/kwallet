/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2001 George Staikos <staikos@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _BLOWFISH_H
#define _BLOWFISH_H

#include <config-kwalletbackend.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h> /* For uintXX_t on Tru64 */
#endif

#include "blockcipher.h"
#include "kwalletbackend5_export.h"

/* @internal
 */
class KWALLETBACKEND5_EXPORT BlowFish : public BlockCipher
{
public:
    BlowFish();
    ~BlowFish() override;

    bool setKey(void *key, int bitlength) override;

    int keyLen() const override;

    bool variableKeyLen() const override;

    bool readyToGo() const override;

    int encrypt(void *block, int len) override;

    int decrypt(void *block, int len) override;

private:
    uint32_t _S[4][256];
    uint32_t _P[18];

    void *_key;
    int _keylen;  // in bits

    bool _init;

    bool init();
    uint32_t F(uint32_t x);
    void encipher(uint32_t *xl, uint32_t *xr);
    void decipher(uint32_t *xl, uint32_t *xr);
};

#endif

