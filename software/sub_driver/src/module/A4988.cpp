/**
 * @file A4988.cpp
 * @author Daniel Kim       
 * @brief A4988 stepper motor driver
 * @version 0.1
 * @date 2022-07-25
 * 
 * @copyright Copyright (c) 2022 Daniel Kim https://github.com/daniel360kim/OceanAI
 * 
 */

#include <Arduino.h>

#include "A4988.h"
#include "module_settings.h"

A4988::A4988(const uint8_t step_pin, const uint8_t dir_pin, const uint8_t ms1_pin, const uint8_t ms2_pin, const uint8_t ms3_pin, const uint8_t slp, const uint8_t rst, const uint8_t limit_pin)
{
    this->step_pin = step_pin;
    this->dir_pin = dir_pin;
    this->ms1_pin = ms1_pin;
    this->ms2_pin = ms2_pin;
    this->ms3_pin = ms3_pin;
    this->slp = slp;
    this->rst = rst;

    pinMode(step_pin, OUTPUT);
    pinMode(dir_pin, OUTPUT);
    pinMode(ms1_pin, OUTPUT);
    pinMode(ms2_pin, OUTPUT);
    pinMode(ms3_pin, OUTPUT);
    pinMode(slp, OUTPUT);
    pinMode(rst, OUTPUT);

    setState(true);
    setDirection(true);
    setResolution(Resolution::Full);

    limit.begin(limit_pin);

}

/**
 * @brief begin with default configurations
 * 
 */
void A4988::begin()
{
    digitalWrite(slp, HIGH); //keep awake
}

void A4988::setResolution(Resolution resolution)
{
    this->resolution = resolution;

    switch (resolution)
    {
    case Full:
        digitalWrite(ms1_pin, LOW);
        digitalWrite(ms2_pin, LOW);
        digitalWrite(ms3_pin, LOW);
        steps_per_revolution = 200;
        break;
    case Half:
        digitalWrite(ms1_pin, HIGH);
        digitalWrite(ms2_pin, LOW);
        digitalWrite(ms3_pin, LOW);
        steps_per_revolution = 400;
        break;
    case Quarter:
        digitalWrite(ms1_pin, LOW);
        digitalWrite(ms2_pin, HIGH);
        digitalWrite(ms3_pin, LOW);
        steps_per_revolution = 800;
        break;
    case Eighth:
        digitalWrite(ms1_pin, HIGH);
        digitalWrite(ms2_pin, HIGH);
        digitalWrite(ms3_pin, LOW);
        steps_per_revolution = 1600;
        break;
    case Sixteenth:
        digitalWrite(ms1_pin, HIGH);
        digitalWrite(ms2_pin, HIGH);
        digitalWrite(ms3_pin, HIGH);
        steps_per_revolution = 3200;
        break;
    default:
        digitalWrite(ms1_pin, LOW);
        digitalWrite(ms2_pin, LOW);
        digitalWrite(ms3_pin, LOW);
        steps_per_revolution = 200;
        break;
    }
}

/**
 * @brief configures the power state of the stepper driver
 * 
 * @param en_state true to enable, false to disable
 * @param slp_state true to keep awake, false to sleep
 */
void A4988::setState( bool slp_state)
{
    digitalWrite(slp, slp_state);
}

/**
 * @brief sets the direction the motor is spinning
 * 
 * @param dir_state true to spin clockwise, false to spin counterclockwise
 */
void A4988::setDirection(bool dir_state)
{
    digitalWrite(dir_pin, dir_state);
    direction = dir_state;
}

bool A4988::calibrate()
{
    if(limit.state() == true)
    {
        return true;
    }
    setDirection(true);
    while(limit.state() == false);

    current_position = 0;
    return true;
}

void A4988::toPosition(uint8_t position)
{
    if(position == current_position)
    {
        return;
    }

    if(position < current_position)
    {
        setDirection(false);
    }
    else
    {
        setDirection(true);
    }
    int steps_to_turn = (abs(current_position - position) / Module::DISPLACEMENT_PER_CIRCLE) * steps_per_revolution;
    for(int i = 0; i < steps_to_turn; i++)
    {
        digitalWrite(step_pin, HIGH);
        digitalWrite(step_pin, LOW);
    }

    current_position = position; //update the current position
}

void A4988::logToStruct(Data &data)
{
    
}

