/**
 * @file timed_function.h
 * @author Daniel Kim
 * @brief calls functions at an interval
 * @version 0.1
 * @date 2022-10-01
 * 
 * @copyright Copyright (c) 2022 OceanAI (https://github.com/daniel360kim/OceanAI)
 * 
 */

#ifndef timed_function_h
#define timed_function_h

#include <chrono>
#include <functional>
#include <teensy_clock/teensy_clock.h>
#include <stdint.h>

#include "Timer.h"


namespace Time
{
    /**
     * @brief Calls functions at certain intervals      
     * 
     * @tparam interval how many nanoseconds between calls
     * @tparam scoped based on the time since program epoch or since instantiation: true for program epoch, false for instantiation
     * @tparam T return type of the function to be called
     * @tparam P variadic parameters of the function to be called
     */
    template<typename T, typename...P>
    class Async : public Timer
    {
    public:
        Async() : Timer()
        {
            reset();
        }

        Async(int64_t interval, T(*func)(P...)) : Timer()
        {
            reset();
            
            this->func = func;
            this->interval = interval;
        }

        T tick(P...params)
        {
            if(elapsed() - previous_time >= interval && running)
            {
                *return_val = func(params...);
                previous_time = elapsed();
            }
            return *return_val;
        }

        /**
         * @brief Special tick function for void functions
         * 
         */
        void void_tick(P...params)
        {
            if(elapsed() - previous_time >= interval && running)
            {
                func(params...);
                previous_time = elapsed();
            }
        }

        void setInterval(uint64_t interval)
        {
            this->interval = interval;
        }

        void setFunction(T(*func)(P...))
        {
            this->func = func;
        }

        void stop()
        {
            running = false;
        }
    private:
        T(*func)(P...);

        int64_t previous_time;
        int64_t interval;
        bool running = true;
        T* return_val = nullptr;

    };
    
};


#endif