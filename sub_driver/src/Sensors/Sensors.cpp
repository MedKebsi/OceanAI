/**
 * @file Sensors.cpp
 * @author Daniel Kim
 * @brief Reads sensor from our 9dof sensor array
 * @version 0.1
 * @date 2022-06-28
 * 
 * @copyright Copyright (c) 2022 OceanAI (https://github.com/daniel360kim/OceanAI) 
 * 
 */
#include "Sensors.h"

#include <ADC.h>
#include <ADC_util.h>

#include "../config.h"
#include "../debug.h"
#include "LowPass.h"
#include "../Time/Time.h"

BMP388_DEV baro;
Bmi088Accel accel(Wire, 0x18);
Bmi088Gyro gyro(Wire, 0x68);

LIS3MDL mag;

Timer<1, micros> TDS_interrupt;
Timer<1, micros> voltage_interrupt;

namespace Filter
{
    LowPass<2> gyr_x(10, 1e3, true); 
    LowPass<2> gyr_y(10, 1e3, true); 
    LowPass<2> gyr_z(10, 1e3, true);

    LowPass<2> acc_x(10, 1e3, true); 
    LowPass<2> acc_y(10, 1e3, true); 
    LowPass<2> acc_z(10, 1e3, true);     
};


ADC adc;

volatile bool UnifiedSensors::bar_flag = false;
volatile bool UnifiedSensors::accel_flag = false;
volatile bool UnifiedSensors::gyro_flag = false;
volatile bool UnifiedSensors::mag_flag = true;

volatile bool UnifiedSensors::TDS_flag = false;
volatile bool UnifiedSensors::voltage_flag = false;

UnifiedSensors::UnifiedSensors() {}

void UnifiedSensors::initNavSensors()
{
    int status[10]; 

    uint8_t baro_stat = baro.begin(BMP388_I2C_ALT_ADDR); //returns 0 for error 1 for success
    if(baro_stat == 0) 
    {
        status[0] = -1;
    }
    baro.setPresOversampling(OVERSAMPLING_X4);
    baro.setTempOversampling(OVERSAMPLING_X16);
    baro.enableInterrupt();
    baro.setTimeStandby(TIME_STANDBY_80MS);
    baro.startNormalConversion();

    status[1] = accel.begin();
    accel.setOdr(Bmi088Accel::ODR_800HZ_BW_140HZ);
    accel.pinModeInt1(Bmi088Accel::PUSH_PULL, Bmi088Accel::ACTIVE_HIGH);
    accel.mapDrdyInt1(true);
    accel.setRange(Bmi088Accel::RANGE_3G);

    status[2] = gyro.begin();
    gyro.setOdr(Bmi088Gyro::ODR_1000HZ_BW_116HZ);
    gyro.pinModeInt3(Bmi088Gyro::PUSH_PULL,Bmi088Gyro::ACTIVE_HIGH);
    gyro.mapDrdyInt3(true);
    gyro.setRange(Bmi088Gyro::RANGE_250DPS);

    if(!mag.init())
    {
        status[3] = -1;
    }

    mag.enableDefault();

    
    for(int i = 0; i < 10; i++)
    {
        if(status[i] < 0)
        {
            switch(i)
            {
                case 0:
                    #if DEBUG_ON == true
                        char* message = (char*)"Sensors: Barometer initialization error";
                        Debug::error.addToBuffer(micros(), Debug::Critical_Error, message);

                        #if LIVE_DEBUG == true
                            Serial.println(F(message));
                        #endif
                        
                    #endif 
                    
                    break;
                case 1:
                    #if DEBUG_ON == true
                        message = (char*)"Sensors: Accelerometer initialization error";
                        Debug::error.addToBuffer(micros(), Debug::Critical_Error, message);

                        #if LIVE_DEBUG == true
                            Serial.println(F(message));
                        #endif
                        
                    #endif 
                    break;
                case 2:
                    #if DEBUG_ON == true
                        message = (char*)"Sensors: Gyroscope initialization error";
                        Debug::error.addToBuffer(micros(), Debug::Critical_Error, message);

                        #if LIVE_DEBUG == true
                            Serial.println(F(message));
                        #endif
                        
                    #endif 
                    break;

                case 3:
                    #if DEBUG_ON == true
                        message = (char*)"Sensors: Magentometer initialization error";
                        Debug::error.addToBuffer(micros(), Debug::Critical_Error, message);

                        #if LIVE_DEBUG == true
                            Serial.println(F(message));
                        #endif
                        
                    #endif 

                    break;
                
                default:
                    #if DEBUG_ON == true
                        message = (char*)"Sensors: Unknown sensor error initialization error";
                        Debug::error.addToBuffer(micros(), Debug::Critical_Error, message);

                        #if LIVE_DEBUG == true
                            Serial.println(F(message));
                        #endif
                        
                    #endif 
                    Serial.println(F(""));
            }
            //indicate error
        } 

    }

    #if DEBUG_ON == true
        char* message = (char*)"Sensors: All sensors successfully initialized";
        Debug::success.addToBuffer(micros(), Debug::Success, message);

        #if LIVE_DEBUG == true
            Serial.println(F(message));
        #endif
        
    #endif 
}

void UnifiedSensors::initADC()
{
    adc.adc0->setAveraging(1); // set number of averages
    adc.adc0->setResolution(12); // set bits of resolution
    adc.adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // change the conversion speed
    adc.adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED); // change the sampling speed

    ////// ADC1 /////
    #ifdef ADC_DUAL_ADCS
    adc.adc1->setAveraging(1); // set number of averages
    adc.adc1->setResolution(12); // set bits of resolution
    adc.adc1->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // change the conversion speed
    adc.adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED); // change the sampling speed
    #endif
}

void UnifiedSensors::initTDS(uint8_t TDS_pin)
{
    pinMode(TDS_pin, INPUT);
    this->TDS_pin = TDS_pin;
    TDS_interrupt.every(10000, TDS_drdy);
}

void UnifiedSensors::initVoltmeter(uint8_t input_pin)
{
    pinMode(input_pin, INPUT);
    voltage_pin = input_pin;
    voltage_interrupt.every(1000000, voltage_drdy);
}

void UnifiedSensors::setInterrupts(uint8_t bar_int, uint8_t accel_int, uint8_t gyro_int, uint8_t mag_int)
{
    pinMode(bar_int, INPUT);
    pinMode(accel_int, INPUT);
    pinMode(gyro_int, INPUT);
    pinMode(mag_int, INPUT);

    attachInterrupt(digitalPinToInterrupt(bar_int), bar_drdy, RISING);
    attachInterrupt(digitalPinToInterrupt(accel_int), accel_drdy, RISING);
    attachInterrupt(digitalPinToInterrupt(gyro_int), gyro_drdy, RISING);
    attachInterrupt(digitalPinToInterrupt(mag_int), mag_drdy, RISING);
}

void UnifiedSensors::setGyroBias()
{
    double gxIntegration = 0.0; 
    double gyIntegration = 0.0;
    double gzIntegration = 0.0;

    for(uint8_t i = 0; i < 100; i++)
    {
        double gx, gy, gz;
        returnRawGyro(&gx, &gy, &gz);
        gxIntegration += gx;
        gyIntegration += gy;
        gzIntegration += gz;
        delayMicroseconds(100);
    }

    gx_bias = gxIntegration / 100;
    gy_bias = gyIntegration / 100;
    gz_bias = gzIntegration / 100;
}
void UnifiedSensors::returnRawBaro(double *pres, double *temp)
{
    baro.getTempPres(*temp, *pres);
}
    
void UnifiedSensors::returnRawAccel(double *x, double *y, double *z, double *tempC)
{
    accel.readSensor();
    *x = accel.getAccelX_mss();
    *y = accel.getAccelY_mss();
    *z = accel.getAccelZ_mss();
    *tempC = accel.getTemperature_C();

}
void UnifiedSensors::returnRawGyro(double *x, double *y, double *z)
{   
    gyro.readSensor();
    *x = gyro.getGyroX_rads() - gx_bias;
    *y = gyro.getGyroY_rads() - gy_bias;
    *z = gyro.getGyroZ_rads() - gz_bias;
}

void UnifiedSensors::returnRawMag(double *x, double *y, double *z)
{

    mag.read();

    //Convert to mTesla. Rangle is +/-4 so we divide by 6842 to get gauss, then mult. by 100 to get mtesla
    *x = mag.m.x / 68.42 - HARD_IRON_BIAS[0]; 
    *y = mag.m.y / 68.42 - HARD_IRON_BIAS[1];
    *z = mag.m.z / 68.42- HARD_IRON_BIAS[2];  
}

double UnifiedSensors::readTDS()
{

    int tds = adc.adc0->analogRead(TDS_pin);
    
    double averageVoltage = tds * (double)VREF / 1024.0;
    double compensationCoefficient = 1.0 + 0.02 * (temp - 25.0);    //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
    double compensationVolatge = averageVoltage / compensationCoefficient;  //temperature compensation
    double tdsValue = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 * compensationVolatge * compensationVolatge + 857.39 * compensationVolatge) * 0.5; //convert voltage value to tds value

    return tdsValue;
}

double UnifiedSensors::readVoltage()
{
    
    double reading = adc.adc1->analogRead(voltage_pin) * (double)VREF / 1024.0;
    reading = reading * (9.95 + 1.992) / 1.992;

    if(reading <= 0.1)
        reading = 0.0;
    
    return reading;

}

void UnifiedSensors::logToStruct(Data &data)
{
    
    if(UnifiedSensors::mag_flag)
    {
        returnRawMag(&data.mag.x, &data.mag.y, &data.mag.z);

        UnifiedSensors::mag_flag = false;
    }

    if(UnifiedSensors::accel_flag && UnifiedSensors::gyro_flag)
    {

        returnRawAccel(&data.racc.x, &data.racc.y, &data.racc.z, &data.bmi_temp);

        data.facc.x = Filter::acc_x.filt(data.racc.x);
        data.facc.y = Filter::acc_y.filt(data.racc.y);
        data.facc.z = Filter::acc_z.filt(data.racc.z);
        UnifiedSensors::accel_flag = false;


        returnRawGyro(&data.rgyr.x, &data.rgyr.y, &data.rgyr.z);

        data.fgyr.x = Filter::gyr_x.filt(data.rgyr.x);
        data.fgyr.y = Filter::gyr_y.filt(data.rgyr.y);
        data.fgyr.z = Filter::gyr_z.filt(data.rgyr.z);

        UnifiedSensors::gyro_flag = false;

    }

    if(UnifiedSensors::bar_flag)
    {
        returnRawBaro(&data.bmp_rpres, &data.bmp_rtemp);
        UnifiedSensors::bar_flag = false;
    }

    if(UnifiedSensors::TDS_flag)
    {
        data.TDS = readTDS();
        UnifiedSensors::TDS_flag = false;
    }

    if(UnifiedSensors::voltage_flag)
    {
        data.voltage = readVoltage();
        UnifiedSensors::voltage_flag = false;
    }

    TDS_interrupt.tick();
    voltage_interrupt.tick();

}


