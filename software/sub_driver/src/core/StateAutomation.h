/**
 * @file StateAutomation.h
 * @author Daniel Kim
 * @brief finite state automation driver
 * @version 0.1
 * @date 2022-09-30
 * 
 * @copyright Copyright (c) 2022 OceanAI (https://github.com/daniel360kim/OceanAI)
 * 
 */

#ifndef STATE_AUTOMATION_H
#define STATE_AUTOMATION_H

#include <Arduino.h>

class StateAutomation;

/**
 * @brief Abstract class that allows for reference to generic state w/o having to specify true type of state
 * 
 */
class State
{
public:
    virtual ~State() {}
    virtual void enter(StateAutomation* state) = 0;
    virtual void run(StateAutomation* state) = 0;
    virtual void exit(StateAutomation* state) = 0;
    
};

/**
 * @brief All the different states
 * 
 */
enum class CurrentState
{
    INITIALIZATION,
    ERROR_INDICATION,
    IDLE_MODE,
    DIVING_MODE,
    RESURFACING,
};

/**
 * @brief StateAutomation class that handles the state transitions and runs current state
 * 
 */
class StateAutomation
{
public:
    StateAutomation() {}

    inline State* getCurrentState() { return m_currentState; }
    void run();
    void setState(State& newState);

    static void printState(Print &printer, CurrentState &state);

private:
    bool m_initialized = false;
    State* m_currentState;
};

#endif