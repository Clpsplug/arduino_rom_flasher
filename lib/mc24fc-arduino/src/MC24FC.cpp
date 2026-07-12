/**
 * @file MC24FC.cpp
 * @brief API for Microchip Technologies Inc.'s MC24XX32, 64, 128, 256, 512 EEPROMs.
 *        (XX may be AA, LC, or FC)
 *        Refer to the official datasheet for valid specific values.
 * @author ClpsPLUG
 * @license MIT
 */

#include "MC24FC.h"
#include <Wire.h>

// Try to poll maximum I2C buffer size by using macro for the board, if available.
// Seeed Studio boards may or may not define those.
#if defined(I2C_BUFFER_LENGTH)
// On Arduino Uno, usually 32, but on R4 this could be 255. On ESP32, this is likely 128.
constexpr std::uint16_t MAX_I2C_BUFFER_SIZE = I2C_BUFFER_LENGTH;
#elif defined(BUFFER_SIZE)
// ESP8266. Likely 128.
constexpr std::uint16_t MAX_I2C_BUFFER_SIZE = BUFFER_SIZE;
#else
// The ultimate fallback is 32 byte
constexpr std::uint16_t MAX_I2C_BUFFER_SIZE = 32;
#endif

MC24FC::MC24FC(std::uint8_t i2c_addr, std::uint32_t max_capacity_kilobits, std::uint32_t page_size) :
    _i2cAddr(i2c_addr & 0x7F) // Address is 7-bit
    , _maxCapacityInBits(max_capacity_kilobits * 1024)
    , _pageSize(page_size)
    , _currentAddress(0xFFFF) // Keep the address out of bounds (well, except for 512 EEPROM)
    , _error(MC24FCError::OK) {
}

void MC24FC::init() {
    Wire.begin();
    // Quick ACK check
    Wire.beginTransmission(_i2cAddr); // control byte, write op
    const auto ret = Wire.endTransmission(); // immediately end to check ACK
    setError(ret);
}

bool MC24FC::readByte(std::uint16_t address, std::uint8_t *data) {
    _currentAddress = address;
    if (_currentAddress >= _maxCapacityInBits / 8) {
        _error = MC24FCError::ADDRESS_OUT_OF_RANGE;
        return false;
    }
    // We must convert the address to big endian
    char address_bytes[2];
    address_bytes[0] = static_cast<char>(address >> 8);
    address_bytes[1] = static_cast<char>(address & 0xFF);

    // Address the EEPROM as "Write" to write the random access address
    Wire.beginTransmission(_i2cAddr);
    Wire.write(address_bytes, 2); // Make sure we use the 'non-string' overload. Don't omit the second arg.
    const auto ret = Wire.endTransmission(false); // Keep the line hot
    setError(ret);
    if (ret != 0) {
        return false;
    }
    // Address the EEPROM as "Read" to read from the written address above
    // NOTE: "Read" bit is handled internally.
    if (
        const auto size = Wire.requestFrom(_i2cAddr, 1); size != 1
        || !Wire.available()) {
        _error = MC24FCError::EEPROM_DID_NOT_REPLY;
        return false;
    }
    if (data != nullptr) {
        *data = Wire.read();
    }
    if (
        _currentAddress + 1 >= _maxCapacityInBits / 8
        || _currentAddress == UINT16_MAX
    ) {
        _currentAddress = 0;
    } else {
        _currentAddress++;
    }
    return true;
}

std::uint8_t MC24FC::readNext() {
    // Address the EEPROM as "Read" to read from the written address above
    Wire.requestFrom(_i2cAddr, 1);
    const std::uint8_t val = Wire.read();
    if (
        _currentAddress >= _maxCapacityInBits / 8
        || _currentAddress == UINT16_MAX
    ) {
        _currentAddress = 0;
    } else {
        _currentAddress++;
    }
    return val;
}

std::uint16_t MC24FC::readNextBytes(char *outbuf, std::uint16_t length) {
    if (outbuf == nullptr) {
        _error = MC24FCError::INVALID_PARAMETER;
        return 0;
    }

    std::uint16_t read_bytes = 0;
    Wire.requestFrom(_i2cAddr, length);
    while (Wire.available()) {
        outbuf[read_bytes] = static_cast<char>(Wire.read());
        read_bytes++;
        if (
            _currentAddress + 1 >= _maxCapacityInBits / 8
            || _currentAddress == UINT16_MAX
        ) {
            _currentAddress = 0;
            _error = MC24FCError::ADDRESS_OUT_OF_RANGE;
            break;
        }
        _currentAddress++;
    }

    return read_bytes;
}


void MC24FC::writeAt(std::uint16_t offset, const char *buf, std::uint16_t length) {
    auto current_written_bytes = 0;
    auto remaining_length = length;
    auto buf_offset = 0;

    if (buf == nullptr) {
        _error = MC24FCError::INVALID_PARAMETER;
        return;
    }

    if (_pageSize < length) {
        _error = MC24FCError::WRITE_DATA_TOO_LARGE;
        return;
    }

    while (buf_offset < length) {
        // We must convert the address to big endian
        char address_bytes[2];
        address_bytes[0] = static_cast<char>(offset >> 8);
        address_bytes[1] = static_cast<char>(offset & 0xFF);

        // Address the EEPROM as "Write" to write the random access address
        Wire.beginTransmission(_i2cAddr);
        Wire.write(address_bytes, 2); // Make sure we use the 'non-string' overload. Don't omit the second arg.
        current_written_bytes = sizeof(_i2cAddr) + sizeof(address_bytes);

        const auto allowed_max_size = MAX_I2C_BUFFER_SIZE - current_written_bytes;
        const auto written_size = this->writeIntoPage(
            offset + buf_offset,
            buf + buf_offset,
            remaining_length,
            allowed_max_size
            );

        buf_offset += written_size;
        remaining_length -= written_size;
        {
            auto ret = Wire.endTransmission(); // Actually end the transmission so that we don't exceed 32 bytes
            setError(ret);
            // Non-ACK result at this point is an error.
            if (ret != 0) {
                break;
            }
        }

        // The EEPROM will stop ACK-ing for write command at this point
        // because it's busy writing the data. We need to poll the ACK.
        // To do so, we send a control byte of WRITE until we get an ACK.
        {
            int ret;
            while (true) {
                Wire.beginTransmission(_i2cAddr); // control byte, write op
                ret = Wire.endTransmission(); // immediately end to check ACK
                if (ret == 0) {
                    break;
                }
                if (ret == 4 || ret == 5) {
                    // Error 4 and 5 must NOT continue and report the error.
                    break;
                }
                delayMicroseconds(100); // don't spam.
            }
            setError(ret);
        }
    }
    _currentAddress += offset - remaining_length;
}

std::uint16_t MC24FC::writeIntoPage(std::uint16_t offset, const char *buf, std::uint16_t length,
    std::uint16_t i2c_limit) const {
    const std::uint16_t page_remaining = _pageSize - offset % _pageSize;
    const auto writable_size = std::min(length, std::min(i2c_limit, page_remaining));
    Wire.write(buf, writable_size);
    return writable_size;
}

void MC24FC::setError(const int wire_return_code) {
    switch (wire_return_code) {
        case 0:
            _error = MC24FCError::OK;
            break;
        case 1:
            _error = MC24FCError::I2C_BUFFER_OVERFLOW;
            break;
        case 2:
            _error = MC24FCError::WRONG_I2C_ADDRESS;
            break;
        case 3:
            _error = MC24FCError::I2C_AGENT_DOESNT_ACK;
            break;
        case 4:
            _error = MC24FCError::UNKNOWN_I2C_ERROR;
            break;
        case 5:
            _error = MC24FCError::I2C_FAIL;
            break;
        default:
            _error = MC24FCError::I2C_UNIMPLEMENTED_ERROR;
            break;
    }
}

MC24FCError MC24FC::getError() const {
    return _error;
}
