//
// Created by Collapsed PLUG on 2026/07/05.
//

#ifndef ARDUINO_MARQUEE_SYSTEM_SD_READER_H
#define ARDUINO_MARQUEE_SYSTEM_SD_READER_H
#include <SD.h>

namespace ecp {
enum class SDReaderError {
    OK,
    NotInitialized,
    NoCard,
    FileAmbiguous,
    FileNotOpen,
    FileMissing,
    MAX
};

class SDReader {
public:
    explicit SDReader(int chipSelectPin = 10);

    ~SDReader();

    [[nodiscard]] bool init();

    const char *getFileNameEndingWith(const char *extension);

    void open(const char *file_name);

    int read(char *buf, size_t buf_size);

    bool isEOF();

    void close();

    [[nodiscard]] SDReaderError getError() const;

private:
    int _chipSelectPin;
    SDReaderError _error;
    File _file;
    char _lastReadFileName[64];
};
} // ECP::ArduinoMarquee

#endif //ARDUINO_MARQUEE_SYSTEM_SD_READER_H
