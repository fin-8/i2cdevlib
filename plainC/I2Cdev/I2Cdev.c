// I2Cdev library collection - Main I2C device class
// Abstracts bit and byte I2C R/W functions into a convenient class
// 6/9/2012 by Jeff Rowberg <jeff@rowberg.net>
//
// Changelog:
//      2013-05-06 - add Francesco Ferrara's Fastwire v0.24 implementation with small modifications
//      2013-05-05 - fix issue with writing bit values to words (Sasquatch/Farzanegan)
//      2012-06-09 - fix major issue with reading > 32 bytes at a time with Arduino Wire
//                 - add compiler warnings when using outdated or IDE or limited I2Cdev implementation
//      2011-11-01 - fix write*Bits mask calculation (thanks sasquatch @ Arduino forums)
//      2011-10-03 - added automatic Arduino version detection for ease of use
//      2011-10-02 - added Gene Knight's NBWire TwoWire class implementation with small modifications
//      2011-08-31 - added support for Arduino 1.0 Wire library (methods are different from 0.x)
//      2011-08-03 - added optional timeout parameter to read* methods to easily change from default
//      2011-08-02 - added support for 16-bit registers
//                 - fixed incorrect Doxygen comments on some methods
//                 - added timeout value for read operations (thanks mem @ Arduino forums)
//      2011-07-30 - changed read/write function structures to return success or byte counts
//                 - made all methods static for multi-device memory savings
//      2011-07-28 - initial release

/* ============================================
I2Cdev device library code is placed under the MIT license
Copyright (c) 2013 Jeff Rowberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
===============================================
*/

#include "I2Cdev.h"

// private functions
uint8_t Fastwire_waitInt();

/** Read a single bit from an 8-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register regAddr to read from
 * @param bitNum Bit position to read (0-7)
 * @param data Container for single bit value
 * @param timeout Optional read timeout in milliseconds (0 to disable, leave off to use default class value in I2Cdev::readTimeout)
 * @return Status of read operation (true = success)
 */
uint8_t I2Cdev_readBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t *data, uint16_t timeout) {
	uint8_t b;
	uint8_t count = I2Cdev_readByte(devAddr, regAddr, &b, timeout);
	*data = b & (1 << bitNum);
	return count;
}

/** Read a single bit from a 16-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register regAddr to read from
 * @param bitNum Bit position to read (0-15)
 * @param data Container for single bit value
 * @param timeout Optional read timeout in milliseconds (0 to disable, leave off to use default class value in I2Cdev::readTimeout)
 * @return Status of read operation (true = success)
 */
uint8_t I2Cdev_readBitW(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint16_t *data, uint16_t timeout) {
	uint16_t b;
	uint8_t count = I2Cdev_readWord(devAddr, regAddr, &b, timeout);
	*data = b & (1 << bitNum);
	return count;
}

/** Read multiple bits from an 8-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register regAddr to read from
 * @param bitStart First bit position to read (0-7)
 * @param length Number of bits to read (not more than 8)
 * @param data Container for right-aligned value (i.e. '101' read from any bitStart position will equal 0x05)
 * @param timeout Optional read timeout in milliseconds (0 to disable, leave off to use default class value in I2Cdev::readTimeout)
 * @return Status of read operation (true = success)
 */
uint8_t I2Cdev_readBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t *data, uint16_t timeout) {
	// 01101001 read byte
	// 76543210 bit numbers
	//    xxx   args: bitStart=4, length=3
	//    010   masked
	//   -> 010 shifted
	uint8_t count, b;
	if ((count = I2Cdev_readByte(devAddr, regAddr, &b, timeout)) != 0) {
		uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
		b &= mask;
		b >>= (bitStart - length + 1);
		*data = b;
	}
	return count;
}

/** Read multiple bits from a 16-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register regAddr to read from
 * @param bitStart First bit position to read (0-15)
 * @param length Number of bits to read (not more than 16)
 * @param data Container for right-aligned value (i.e. '101' read from any bitStart position will equal 0x05)
 * @param timeout Optional read timeout in milliseconds (0 to disable, leave off to use default class value in I2Cdev::readTimeout)
 * @return Status of read operation (1 = success, 0 = failure, -1 = timeout)
 */
uint8_t I2Cdev_readBitsW(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint16_t *data, uint16_t timeout) {
	// 1101011001101001 read byte
	// fedcba9876543210 bit numbers
	//    xxx           args: bitStart=12, length=3
	//    010           masked
	//           -> 010 shifted
	uint8_t count;
	uint16_t w;
	if ((count = I2Cdev_readWord(devAddr, regAddr, &w, timeout)) != 0) {
		uint16_t mask = ((1 << length) - 1) << (bitStart - length + 1);
		w &= mask;
		w >>= (bitStart - length + 1);
		*data = w;
	}
	return count;
}

/** Read single byte from an 8-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register regAddr to read from
 * @param data Container for byte value read from device
 * @param timeout Optional read timeout in milliseconds (0 to disable, leave off to use default class value in I2Cdev::readTimeout)
 * @return Status of read operation (true = success)
 */
uint8_t I2Cdev_readByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint16_t timeout) {
	return I2Cdev_readBytes(devAddr, regAddr, 1, data, timeout);
}

/** Read single word from a 16-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register regAddr to read from
 * @param data Container for word value read from device
 * @param timeout Optional read timeout in milliseconds (0 to disable, leave off to use default class value in I2Cdev::readTimeout)
 * @return Status of read operation (true = success)
 */
uint8_t I2Cdev_readWord(uint8_t devAddr, uint8_t regAddr, uint16_t *data, uint16_t timeout) {
	return I2Cdev_readWords(devAddr, regAddr, 1, data, timeout);
}

/** Read multiple bytes from an 8-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr First register regAddr to read from
 * @param length Number of bytes to read
 * @param data Buffer to store read data in
 * @param timeout Optional read timeout in milliseconds (0 to disable, leave off to use default class value in I2Cdev::readTimeout)
 * @return Number of bytes read (-1 indicates failure)
 */
uint8_t I2Cdev_readBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data, uint16_t timeout) {
	#ifdef I2CDEV_SERIAL_DEBUG
		Serial.print("I2C (0x");
		Serial.print(devAddr, HEX);
		Serial.print(") reading ");
		Serial.print(length, DEC);
		Serial.print(" bytes from 0x");
		Serial.print(regAddr, HEX);
		Serial.print("...");
	#endif

	int8_t count = 0;
	uint32_t t1 = millis();

	// Fastwire library
	// no loop required for fastwire
	uint8_t status = Fastwire_readBuf(devAddr << 1, regAddr, data, length);
	if (status == 0) {
		count = length; // success
	} else {
		count = -1; // error
	}

	// check for timeout
	if (timeout > 0 && millis() - t1 >= timeout && count < length) count = -1; // timeout

	#ifdef I2CDEV_SERIAL_DEBUG
		Serial.print(". Done (");
		Serial.print(count, DEC);
		Serial.println(" read).");
	#endif

	return count;
}

/** Read multiple words from a 16-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr First register regAddr to read from
 * @param length Number of words to read
 * @param data Buffer to store read data in
 * @param timeout Optional read timeout in milliseconds (0 to disable, leave off to use default class value in I2Cdev::readTimeout)
 * @return Number of words read (-1 indicates failure)
 */
uint8_t I2Cdev_readWords(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint16_t *data, uint16_t timeout) {
	#ifdef I2CDEV_SERIAL_DEBUG
		Serial.print("I2C (0x");
		Serial.print(devAddr, HEX);
		Serial.print(") reading ");
		Serial.print(length, DEC);
		Serial.print(" words from 0x");
		Serial.print(regAddr, HEX);
		Serial.print("...");
	#endif

	int8_t count = 0;
	uint32_t t1 = millis();

	// Fastwire library
	// no loop required for fastwire
	uint16_t intermediate[(uint8_t)length];
	uint8_t status = Fastwire_readBuf(devAddr << 1, regAddr, (uint8_t *)intermediate, (uint8_t)(length * 2));
	if (status == 0) {
		count = length; // success
		for (uint8_t i = 0; i < length; i++) {
			data[i] = (intermediate[2*i] << 8) | intermediate[2*i + 1];
		}
	} else {
		count = -1; // error
	}

	if (timeout > 0 && millis() - t1 >= timeout && count < length) count = -1; // timeout

	#ifdef I2CDEV_SERIAL_DEBUG
		Serial.print(". Done (");
		Serial.print(count, DEC);
		Serial.println(" read).");
	#endif

	return count;
}

/** write a single bit in an 8-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register regAddr to write to
 * @param bitNum Bit position to write (0-7)
 * @param value New bit value to write
 * @return Status of operation (true = success)
 */
uint8_t I2Cdev_writeBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data) {
	uint8_t b;
	I2Cdev_readByte(devAddr, regAddr, &b);
	b = (data != 0) ? (b | (1 << bitNum)) : (b & ~(1 << bitNum));
	return I2Cdev_writeByte(devAddr, regAddr, b);
}

/** write a single bit in a 16-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register regAddr to write to
 * @param bitNum Bit position to write (0-15)
 * @param value New bit value to write
 * @return Status of operation (true = success)
 */
uint8_t I2Cdev_writeBitW(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint16_t data) {
	uint16_t w;
	I2Cdev_readWord(devAddr, regAddr, &w);
	w = (data != 0) ? (w | (1 << bitNum)) : (w & ~(1 << bitNum));
	return I2Cdev_writeWord(devAddr, regAddr, w);
}

/** Write multiple bits in an 8-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register regAddr to write to
 * @param bitStart First bit position to write (0-7)
 * @param length Number of bits to write (not more than 8)
 * @param data Right-aligned value to write
 * @return Status of operation (true = success)
 */
uint8_t I2Cdev_writeBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data) {
	//      010 value to write
	// 76543210 bit numbers
	//    xxx   args: bitStart=4, length=3
	// 00011100 mask byte
	// 10101111 original value (sample)
	// 10100011 original & ~mask
	// 10101011 masked | value
	uint8_t b;
	if (readByte(devAddr, regAddr, &b) != 0) {
		uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
		data <<= (bitStart - length + 1); // shift data into correct position
		data &= mask; // zero all non-important bits in data
		b &= ~(mask); // zero all important bits in existing byte
		b |= data; // combine data with existing byte
		return I2Cdev_writeByte(devAddr, regAddr, b);
	} else {
		return false;
	}
}

/** Write multiple bits in a 16-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register regAddr to write to
 * @param bitStart First bit position to write (0-15)
 * @param length Number of bits to write (not more than 16)
 * @param data Right-aligned value to write
 * @return Status of operation (true = success)
 */
uint8_t I2Cdev_writeBitsW(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint16_t data) {
	//              010 value to write
	// fedcba9876543210 bit numbers
	//    xxx           args: bitStart=12, length=3
	// 0001110000000000 mask word
	// 1010111110010110 original value (sample)
	// 1010001110010110 original & ~mask
	// 1010101110010110 masked | value
	uint16_t w;
	if (readWord(devAddr, regAddr, &w) != 0) {
		uint16_t mask = ((1 << length) - 1) << (bitStart - length + 1);
		data <<= (bitStart - length + 1); // shift data into correct position
		data &= mask; // zero all non-important bits in data
		w &= ~(mask); // zero all important bits in existing word
		w |= data; // combine data with existing word
		return I2Cdev_writeWord(devAddr, regAddr, w);
	} else {
		return false;
	}
}

/** Write single byte to an 8-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register address to write to
 * @param data New byte value to write
 * @return Status of operation (true = success)
 */
uint8_t I2Cdev_writeByte(uint8_t devAddr, uint8_t regAddr, uint8_t data) {
	return I2Cdev_writeBytes(devAddr, regAddr, 1, &data);
}

/** Write single word to a 16-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr Register address to write to
 * @param data New word value to write
 * @return Status of operation (true = success)
 */
uint8_t I2Cdev_writeWord(uint8_t devAddr, uint8_t regAddr, uint16_t data) {
	return I2Cdev_writeWords(devAddr, regAddr, 1, &data);
}

/** Write multiple bytes to an 8-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr First register address to write to
 * @param length Number of bytes to write
 * @param data Buffer to copy new data from
 * @return Status of operation (true = success)
 */
uint8_t I2Cdev_writeBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t* data) {
	#ifdef I2CDEV_SERIAL_DEBUG
		Serial.print("I2C (0x");
		Serial.print(devAddr, HEX);
		Serial.print(") writing ");
		Serial.print(length, DEC);
		Serial.print(" bytes to 0x");
		Serial.print(regAddr, HEX);
		Serial.print("...");
	#endif

	uint8_t status = 0;
	Fastwire_beginTransmission(devAddr);
	Fastwire_write(regAddr);

	for (uint8_t i = 0; i < length; i++) {
		#ifdef I2CDEV_SERIAL_DEBUG
			Serial.print(data[i], HEX);
			if (i + 1 < length) Serial.print(" ");
		#endif
		Fastwire_write((uint8_t) data[i]);
	}

	Fastwire_stop();
	//status = Fastwire_endTransmission();

	#ifdef I2CDEV_SERIAL_DEBUG
		Serial.println(". Done.");
	#endif

	return status == 0;
}

/** Write multiple words to a 16-bit device register.
 * @param devAddr I2C slave device address
 * @param regAddr First register address to write to
 * @param length Number of words to write
 * @param data Buffer to copy new data from
 * @return Status of operation (true = success)
 */
uint8_t I2Cdev_writeWords(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint16_t* data) {
	#ifdef I2CDEV_SERIAL_DEBUG
		Serial.print("I2C (0x");
		Serial.print(devAddr, HEX);
		Serial.print(") writing ");
		Serial.print(length, DEC);
		Serial.print(" words to 0x");
		Serial.print(regAddr, HEX);
		Serial.print("...");
	#endif

	uint8_t status = 0;
	Fastwire_beginTransmission(devAddr);
	Fastwire_write(regAddr);

	for (uint8_t i = 0; i < length * 2; i++) {
		#ifdef I2CDEV_SERIAL_DEBUG
			Serial.print(data[i], HEX);
			if (i + 1 < length) Serial.print(" ");
		#endif
		Fastwire_write((uint8_t)(data[i] >> 8));       // send MSB
		status = Fastwire_write((uint8_t)data[i++]);   // send LSB
		if (status != 0) break;
	}

	Fastwire_stop();
	//status = Fastwire_endTransmission();

	#ifdef I2CDEV_SERIAL_DEBUG
		Serial.println(". Done.");
	#endif

	return status == 0;
}

/** Default timeout value for read operations.
 * Set this to 0 to disable timeout detection.
 */
uint16_t I2Cdev_readTimeout = I2CDEV_DEFAULT_READ_TIMEOUT;


// I2C library
//////////////////////
// Copyright(C) 2012
// Francesco Ferrara
// ferrara[at]libero[point]it
//////////////////////

/*
FastWire
- 0.24 added stop
- 0.23 added reset

 This is a library to help faster programs to read I2C devices.
 Copyright(C) 2012 Francesco Ferrara
 occhiobello at gmail dot com
 [used by Jeff Rowberg for I2Cdevlib with permission]
 */

uint8_t Fastwire_waitInt() {
	int l = 250;
	while (!(TWCR & (1 << TWINT)) && l-- > 0);
	return l > 0;
}

void Fastwire_setup(int16_t khz, uint8_t pullup) {
	TWCR = 0;

	// activate internal pull-ups for twi (PORTC bits 4 & 5)
	// as per note from atmega8 manual pg167
	if (pullup) PORTC |=  ((1 << 4) | (1 << 5));
	else        PORTC &= ~((1 << 4) | (1 << 5));

	TWSR = 0; // no prescaler => prescaler = 1
	TWBR = ((16000L / khz) - 16) / 2; // change the I2C clock rate
	TWCR = 1 << TWEN; // enable twi module, no interrupt
}

// added by Jeff Rowberg 2013-05-07:
// Arduino Wire-style "beginTransmission" function
// (takes 7-bit device address like the Wire method, NOT 8-bit: 0x68, not 0xD0/0xD1)
uint8_t Fastwire_beginTransmission(uint8_t device) {
	uint8_t twst, retry;
	retry = 2;
	do {
		TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO) | (1 << TWSTA);
		if (!Fastwire_waitInt()) return 1;
		twst = TWSR & 0xF8;
		if (twst != TW_START && twst != TW_REP_START) return 2;

		//Serial.print(device, HEX);
		//Serial.print(" ");
		TWDR = device << 1; // send device address without read bit (1)
		TWCR = (1 << TWINT) | (1 << TWEN);
		if (!Fastwire_waitInt()) return 3;
		twst = TWSR & 0xF8;
	} while (twst == TW_MT_SLA_NACK && retry-- > 0);
	if (twst != TW_MT_SLA_ACK) return 4;
	return 0;
}

uint8_t Fastwire_writeBuf(uint8_t device, uint8_t address, uint8_t *data, uint8_t num) {
	uint8_t twst, retry;

	retry = 2;
	do {
		TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO) | (1 << TWSTA);
		if (!Fastwire_waitInt()) return 1;
		twst = TWSR & 0xF8;
		if (twst != TW_START && twst != TW_REP_START) return 2;

		//Serial.print(device, HEX);
		//Serial.print(" ");
		TWDR = device & 0xFE; // send device address without read bit (1)
		TWCR = (1 << TWINT) | (1 << TWEN);
		if (!Fastwire_waitInt()) return 3;
		twst = TWSR & 0xF8;
	} while (twst == TW_MT_SLA_NACK && retry-- > 0);
	if (twst != TW_MT_SLA_ACK) return 4;

	//Serial.print(address, HEX);
	//Serial.print(" ");
	TWDR = address; // send data to the previously addressed device
	TWCR = (1 << TWINT) | (1 << TWEN);
	if (!Fastwire_waitInt()) return 5;
	twst = TWSR & 0xF8;
	if (twst != TW_MT_DATA_ACK) return 6;

	for (uint8_t i = 0; i < num; i++) {
		//Serial.print(data[i], HEX);
		//Serial.print(" ");
		TWDR = data[i]; // send data to the previously addressed device
		TWCR = (1 << TWINT) | (1 << TWEN);
		if (!Fastwire_waitInt()) return 7;
		twst = TWSR & 0xF8;
		if (twst != TW_MT_DATA_ACK) return 8;
	}
	//Serial.print("\n");

	return 0;
}

uint8_t Fastwire_write(uint8_t value) {
	uint8_t twst;
	//Serial.println(value, HEX);
	TWDR = value; // send data
	TWCR = (1 << TWINT) | (1 << TWEN);
	if (!Fastwire_waitInt()) return 1;
	twst = TWSR & 0xF8;
	if (twst != TW_MT_DATA_ACK) return 2;
	return 0;
}

uint8_t Fastwire_readBuf(uint8_t device, uint8_t address, uint8_t *data, uint8_t num) {
	uint8_t twst, retry;

	retry = 2;
	do {
		TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO) | (1 << TWSTA);
		if (!Fastwire_waitInt()) return 16;
		twst = TWSR & 0xF8;
		if (twst != TW_START && twst != TW_REP_START) return 17;

		//Serial.print(device, HEX);
		//Serial.print(" ");
		TWDR = device & 0xfe; // send device address to write
		TWCR = (1 << TWINT) | (1 << TWEN);
		if (!Fastwire_waitInt()) return 18;
		twst = TWSR & 0xF8;
	} while (twst == TW_MT_SLA_NACK && retry-- > 0);
	if (twst != TW_MT_SLA_ACK) return 19;

	//Serial.print(address, HEX);
	//Serial.print(" ");
	TWDR = address; // send data to the previously addressed device
	TWCR = (1 << TWINT) | (1 << TWEN);
	if (!Fastwire_waitInt()) return 20;
	twst = TWSR & 0xF8;
	if (twst != TW_MT_DATA_ACK) return 21;

	/***/

	retry = 2;
	do {
		TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO) | (1 << TWSTA);
		if (!Fastwire_waitInt()) return 22;
		twst = TWSR & 0xF8;
		if (twst != TW_START && twst != TW_REP_START) return 23;

		//Serial.print(device, HEX);
		//Serial.print(" ");
		TWDR = device | 0x01; // send device address with the read bit (1)
		TWCR = (1 << TWINT) | (1 << TWEN);
		if (!Fastwire_waitInt()) return 24;
		twst = TWSR & 0xF8;
	} while (twst == TW_MR_SLA_NACK && retry-- > 0);
	if (twst != TW_MR_SLA_ACK) return 25;

	for (uint8_t i = 0; i < num; i++) {
		if (i == num - 1)
			TWCR = (1 << TWINT) | (1 << TWEN);
		else
			TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
		if (!Fastwire_waitInt()) return 26;
		twst = TWSR & 0xF8;
		if (twst != TW_MR_DATA_ACK && twst != TW_MR_DATA_NACK) return twst;
		data[i] = TWDR;
		//Serial.print(data[i], HEX);
		//Serial.print(" ");
	}
	//Serial.print("\n");
	Fastwire_stop();

	return 0;
}

void Fastwire_reset() {
	TWCR = 0;
}

uint8_t Fastwire_stop() {
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
	if (!Fastwire_waitInt()) return 1;
	return 0;
}

