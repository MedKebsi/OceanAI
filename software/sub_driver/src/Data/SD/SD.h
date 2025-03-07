/**
 * @file SD.h
 * @author Daniel Kim
 * @brief Logs SD data from struct and reads it back
 * @version 0.1
 * @date 2022-07-01
 * 
 * @copyright Copyright (c) 2022 OceanAI (https://github.com/daniel360kim/OceanAI)
 * 
 */
#ifndef SD_h
#define SD_h

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <cstdint>
#include <vector>
#include <CrashReport.h>
#include <queue>
#include <memory>
#include <ArduinoJson.h>
#include <ArduCAM.h>

#include "DataFile.h"
#include "../logged_data.h"
#include "../../core/Timer.h"
#include "../../core/timed_function.h"
#include "../../Sensors/Camera/camera.h"

extern SdFs sd;
extern FsFile file;

class OV2640_Mini; //forward declaration

class SD_Logger
{
public:
    SD_Logger() {}
    SD_Logger(const int64_t duration_ns, int64_t log_interval_ns);

    bool log_crash_report();
    bool init(bool format = false);
    bool logData(LoggedData &data);
    void setLoggingInterval(int64_t interval_ns) { m_log_interval = interval_ns; }
    uint16_t getLoggingIntervalHz();

    static bool closeFile() { return file.close(); }
    static bool reopenFile(const char* filename);

    void log_image(OV2640_Mini &camera);

    const char* get_data_filename() const { return m_data_filename; }

    void update_sd_capacity(LoggedData &data);

    bool removeAllDataFiles();

private:
    const char* m_data_filename; //name of the binary data
    const char* m_csv_filename; //name of the ascii data
    char* m_current_image_filename; //name of the current image that will be saved
    int64_t m_previous_log_time;
    unsigned long long m_write_iterations;

    unsigned long m_image_file_increment = 0;
    
    static void flush(void*);
    static void getCapacity(uint32_t &capacity);

    std::queue<LoggedData> write_buf;

    Time::Async<void, void*> flusher;
    Time::Async<void, uint32_t&> capacity_updater;

    uint64_t m_log_file_size;
    int m_log_interval;

    bool m_inital_cap_updated = false;
};





#endif

