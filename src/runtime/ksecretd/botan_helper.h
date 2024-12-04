/*
 *  This file is part of the KDE libraries
 *  SPDX-FileCopyrightText: 2026 Nicolas Fella <nicolas.fella@gmx.de>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include <QByteArray>
#include <botan/symkey.h>

template<typename T>
QByteArray botanToQba(T value)
{
    return QByteArray((const char *)value.data(), value.size());
}

template<>
inline QByteArray botanToQba(Botan::SymmetricKey value)
{
    return QByteArray((const char *)value.begin(), value.size());
}

inline std::span<const uint8_t> qbaToBotan(QByteArrayView data)
{
    return std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(data.constData()), data.size());
}
