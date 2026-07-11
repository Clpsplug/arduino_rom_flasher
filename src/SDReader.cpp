#include "SDReader.h"
#include <string.h>

using namespace ecp;

namespace {
bool endsWith(const char *input, const char *suffix) {
    auto length = strlen(input);
    auto sufLength = strlen(suffix);
    return strcmp(input + length - sufLength, suffix) == 0;
}
}

SDReader::SDReader(int chipSelectPin) :
    _chipSelectPin(chipSelectPin)
    , _error(SDReaderError::NotInitialized) {
}

SDReader::~SDReader()
= default;

bool SDReader::init() {
    if (!SD.begin(_chipSelectPin)) {
        _error = SDReaderError::NoCard;
        return false;
    }
    _error = SDReaderError::FileNotOpen;
    return true;
}

const char *SDReader::getFileNameEndingWith(const char *extension) {
    auto root = SD.open("/");
    auto entry = root.openNextFile();
    char candidate[64] = {0,};
    while (entry) {
        // File starting with _ is macOS artifact...
        if (entry.name()[0] == '_') {
            entry = root.openNextFile();
            continue;
        }
        if (endsWith(entry.name(), extension)) {
            if (strlen(candidate) != 0) {
                _error = SDReaderError::FileAmbiguous;
                Serial.println("Ambiguous!");
                return "";
            }
            strcpy(candidate, entry.name());
        }
        entry = root.openNextFile();
    }

    if (strlen(candidate) == 0) {
        _error = SDReaderError::FileMissing;
        Serial.println("No file!");
    } else {
        _error = SDReaderError::OK;
    }

    return candidate;
}

void SDReader::open(const char *file_name) {
    _file = SD.open(file_name);
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
