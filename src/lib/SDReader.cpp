#include "SDReader.h"
#include <SPI.h>
#include <SdFat.h>
#include <cstring>

using namespace ecp;

namespace {
SdFat sd;
File empty_file = File();
bool endsWith(const char *input, const char *suffix) {
    auto length = strlen(input);
    auto sufLength = strlen(suffix);
    return strcmp(input + length - sufLength, suffix) == 0;
}
} // namespace
SDReader::SDReader() : SDReader(10) {
}

SDReader::SDReader(int chipSelectPin) :
    _chipSelectPin(chipSelectPin), _error(SDReaderError::NotInitialized), _file(empty_file), _lastReadFileName("") {
}

SDReader::~SDReader() = default;

bool SDReader::init() {
    delay(100); // Try to wait before the CS pin become stable.
    if (!sd.begin(SdSpiConfig(_chipSelectPin,
            DEDICATED_SPI, // TODO:: Make this configurable
            SD_SCK_MHZ(1)))) {
        _error = SDReaderError::NoCard;
        return false;
    }
    _error = SDReaderError::FileNotOpen;
    return true;
}

void SDReader::deinit() {
    sd.end();
    _error = SDReaderError::NotInitialized;
}

const char *SDReader::getFileNameEndingWith(const char *extension) {
    auto root = sd.open("/");
    auto entry = root.openNextFile();
    char candidate[64] = {
        0,
    };
    while (entry) {
        // File starting with _ is macOS artifact...
        char filename[64];
        entry.getName(filename, sizeof(filename));
        Serial.println(filename);
        if (filename[0] == '_' || filename[0] == '.') {
            entry = root.openNextFile();
            continue;
        }
        if (endsWith(filename, extension)) {
            if (strlen(candidate) != 0) {
                _error = SDReaderError::FileAmbiguous;
                this->_lastReadFileName[0] = '\0';
                return "";
            }
            strcpy(candidate, filename);
        }
        entry = root.openNextFile();
    }

    if (strlen(candidate) == 0) {
        _error = SDReaderError::FileMissing;
    } else {
        _error = SDReaderError::OK;
    }

    strcpy(this->_lastReadFileName, candidate);
    return this->_lastReadFileName;
}

void SDReader::open(const char *file_name) {
    _file = sd.open(file_name);
    if (!_file) {
        _error = SDReaderError::FileMissing;
    }
    _error = SDReaderError::OK;
}

int SDReader::read(char *buf, size_t buf_size) {
    return _file.read(buf, buf_size);
}

bool SDReader::isEOF() {
    return !_file.available();
}

void SDReader::close() {
    _file.close();
    _error = SDReaderError::FileNotOpen;
}

SDReaderError SDReader::getError() const {
    return _error;
}
