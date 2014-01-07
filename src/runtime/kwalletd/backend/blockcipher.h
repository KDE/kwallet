/* This file is part of the KDE project
   Copyright (C) 2001 George Staikos <staikos@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef BLOCKCIPHER_H
#define BLOCKCIPHER_H

/* @internal
 */
class BlockCipher {
	public:
		BlockCipher();
		virtual ~BlockCipher();

		/*
		 *  Return the current blocksize in bytes.
		 */
		int blockSize() const;

		/*
		 *  Set the encryption key to key.  Return true on success.
		 */
		virtual bool setKey(void *key, int bitlength) = 0;

		/*
		 *  Get the required (or if it's variable, then the maximum) key
		 *  length for this cipher in bits.
		 */
		virtual int keyLen() const = 0;

		/*
		 *  True if the key is of a variable length.  In this case,
		 *  getKeyLen() will return the maximum length.
		 */
		virtual bool variableKeyLen() const = 0;

		/*
		 *  True if all settings are good and we are ready to encrypt.
		 */
		virtual bool readyToGo() const = 0;

		/*
		 *  Encrypt the block.  Returns the number of bytes successfully
		 *  encrypted.  Can return -1 on error.
		 */
		virtual int encrypt(void *block, int len) = 0;

		/*
		 *  Decrypt the block.  Returns as does encrypt();
		 */
		virtual int decrypt(void *block, int len) = 0;

	protected:
		int _blksz;
		int _keylen;   // in bits
};

#endif // BLOCKCIPHER_H
