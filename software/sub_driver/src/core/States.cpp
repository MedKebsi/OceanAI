/**
 * @file States.cpp
 * @author Daniel Kim
 * @brief finite state automation driver - states classes
 * @version 0.1
 * @date 2022-09-30
 *
 * @copyright Copyright (c) 2022 OceanAI (https://github.com/daniel360kim/OceanAI)
 *
 */

#include "States.h"
#include "Timer.h"
#include "cpu.h"
#include "../Data/TransportManager.h"
#include "../Sensors/Sensors.h"
#include "../Sensors/thermistor.h"
#include "../Sensors/transducer.h"
#include "../Sensors/tds.h"
#include "../Sensors/voltage.h"

#include "../Data/SD/SD.h"
#include "../Data/hitl.h"

#include "pins.h"


#include "indication/OutputFuncs.h"
#include "indication/LED.h"

#include <Arduino.h>
#include <teensy_clock/teensy_clock.h>
#include <cstdint>

#include "../Navigation/SensorFusion/Fusion.h"
#include "../navigation/Orientation.h"
#include "../Navigation/Postioning.h"
#include "../Navigation/hitl_navigation.h"

#include "debug.h"
#include "Time.h"
#include "core/Timer.h"
#include "module/stepper.h"
#include "module/limit.h"


static Fusion SFori;

static Sensors::Thermistor external_temp(RX_RF, 10000, 4100, 25, 30, HZ_TO_NS(5));
static Sensors::Transducer external_pres(TX_RF, 30, HZ_TO_NS(5));
static Sensors::TotalDissolvedSolids total_dissolved_solids(TDS, 30, HZ_TO_NS(5));


static Sensors::Voltage voltmeter(v_div, 30, HZ_TO_NS(5));

static Orientation ori;
 
static LED signal(SIGNAL);

static int64_t previous_time;
static teensy_clock::time_point start_time;

static Velocity nav_v;
static Position nav_p;

static LoggedData data;

static SD_Logger logger(MissionDuration::mission_time, HZ_TO_NS(10));
 
static bool warning = false;

static StepperPins pins_b{
    STP_b,
    DIR_b,
    MS1_b,
    MS2_b,
    ERR_b,
    STOP_b};

constexpr double STEPS_PER_HALF = 224.852;
constexpr int STEPPER_HALF_STEPS = 27000;
static Buoyancy buoyancy(pins_b, Stepper::Resolution::HALF, StepperProperties(STEPPER_HALF_STEPS / STEPS_PER_HALF, STEPPER_HALF_STEPS));

static CurrentState currentState;

static StaticJsonDocument<STATIC_JSON_DOC_SIZE> data_json;

#if UI_ON
    static CurrentState callbackState; //State to go back to after going into idle
#endif

#if HITL_ON
    HITL::DataProviderManager data_provider((int64_t)618LL*1000000000LL);

    HITL::Data location;
    HITL::Data depth;
    HITL::Data pressure;
    HITL::Data salinity;
    HITL::Data temperature;

    HITLNavigation hitl_nav;
    
#endif

/**
 * @brief Functions that run in multiple states
 */
void continuousFunctions(StateAutomation *state)
{
    data.time_ns = scoped_timer.elapsed(); // scoped timer is a global object to measure time since program epoch
    data.delta_time = (scoped_timer.elapsed() - previous_time) / 1000000000.0;
    previous_time = scoped_timer.elapsed();

    data.system_state = static_cast<uint8_t>(currentState);

#if PRINT_STATE
    StateAutomation::printState(Serial, currentState);
#endif

    CPU::log_cpu_info(data);

    Sensors::logData(data);

    external_temp.logToStruct(data);
    external_pres.logToStruct(data);
    total_dissolved_solids.logToStruct(data);
    #if HITL_ON
    data_provider.update(data.time_ns);

    data.HITL.index = (uint16_t)data_provider.getIndex();
    data.HITL.timestamp = data_provider.getTimestamp();
    data.HITL.location.latitude = location[0];
    data.HITL.location.longitude = location[1];
    data.HITL.depth = depth[0];
    data.HITL.pressure = pressure[0];
    data.HITL.salinity = salinity[0];
    data.HITL.temperature = temperature[0];

    data.HITL.distance = hitl_nav.getTotalDistanceTraveled(data.HITL.location.latitude, data.HITL.location.longitude);
    data.HITL.averageSpeed = hitl_nav.getAverageSpeed(data.HITL.location.latitude, data.HITL.location.longitude, data.time_ns);
    data.HITL.currentSpeed = hitl_nav.getSpeedX(data.HITL.location.latitude, data.HITL.location.longitude, data_provider.getTimestamp());

    #endif
    voltmeter.logToStruct(data);

    nav_v.updateVelocity(data);
    nav_p.updatePosition(data);
    SFori.update(data);

    Quaternion relative = Orientation::toQuaternion(data.rel_ori.x, data.rel_ori.y, data.rel_ori.z);
    data.wfacc = ori.convertAccelFrame(relative, data.racc.x, data.racc.y, data.racc.z);

    data.relative = static_cast<Angles_4D>(relative);

#if OPTICS_ON == true
    camera.capture();
    //INFO_LOG("Captured image");
    logger.log_image(camera);
#endif

    signal.blink(80);

    if (!warning)
    {
        LEDa.displaySpectrum();
        LEDb.displaySpectrum();
    }
    else
    {
        LEDb.blink(255, 0, 0, data.sd_log_rate_hz);
    }

    buoyancy.logToStruct(data);
    buoyancy.setMinPulseWidth(MIN_PULSE_WIDTH);

    logger.update_sd_capacity(data);
    data.sd_log_rate_hz = logger.getLoggingIntervalHz();

    #if SD_ON
    if (!logger.logData(data))
    {
        warning = true;
        return;
    }
    #endif

#if UI_ON
    //Send/receive data to/from the UI
    //If GUI wants to change the state, it will be handled here
    if(TransportManager::handleTransport(data))
    {
        callbackState = currentState;
        state->setState(IdleMode::getInstance());
        return;
    }
    TransportManager::Commands commands = TransportManager::getCommands();
    #if HITL_ON
        data_provider.update_frequency_scale(commands.hitl_scale);

        //NS to HZ
        data.hitl_rate = ((double)data_provider.get_frequency() / 1000000.0);
        data.hitl_progress = data_provider.get_progress() * 100.0; //percentage
    #endif

    #if SD_ON
    if(commands.sd_log_enable > 0)
    {
        logger.setLoggingInterval(std::lround((1.0 / commands.sd_log_enable) * 1000000000.0)); //hz to ns
        if(!logger.logData(data))
        {
            warning = true;   
        }
    }
    #endif


#endif

}
///****************///

/**
 * Initialization state functions
 */

/**
 * @brief All the initialization functions
 *
 * @param state
 */
void Initialization::enter(StateAutomation *state)
{
    start_time = teensy_clock::now(); // begin the scoped timer
    // Flashy lights!
    output.startupSequence();
    // If there was a crash from the last run, report it to SD
    #if SD_ON
    if (CrashReport)
    {
        if (!logger.log_crash_report())
        {
            warning = true;
        }
    }
    #endif

    #if HITL_ON
        location.addColumn(0);
        location.addColumn(1);
        depth.addColumn(2);
        pressure.addColumn(3);
        salinity.addColumn(4);
        temperature.addColumn(5);

        pressure.addTransform([](double x) { return x / 10.132; }); //Convert the raw pressure to atm

        data_provider.add_provider(&location);
        data_provider.add_provider(&depth);
        data_provider.add_provider(&pressure);
        data_provider.add_provider(&salinity);
        data_provider.add_provider(&temperature);

        hitl_nav.setInitialCoordinate(HITL_DATA_ALPHA[0][0], HITL_DATA_ALPHA[0][1], scoped_timer.elapsed());
    #endif

    if (voltmeter.readRaw() <= 11.1 && voltmeter.readRaw() >= 6)
    {
        //ERROR_LOG(Debug::Critical_Error, "Low battery voltage");
        state->setState(ErrorIndication::getInstance());
    }

    CPU::init();


#if LIVE_DEBUG == true
    while (!Serial)
        ; // Wait for serial montior to open
#endif
    Serial.begin(2000000);

    #if UI_ON
        TransportManager::init();
        TransportManager::Commands stepper_commands = TransportManager::getCommands();
        buoyancy.setSpeed(stepper_commands.stepper_speed);
        buoyancy.setMaxSpeed(stepper_commands.stepper_speed);
        buoyancy.setAcceleration(stepper_commands.stepper_speed);

        #if HITL_ON
            data_provider.update_frequency_scale(stepper_commands.hitl_scale);
        #endif

    #else
        buoyancy.setSpeed(1500);
        buoyancy.setMaxSpeed(1500);
        buoyancy.setAcceleration(500);
    #endif

    LEDa.setColor(255, 0, 255);
    LEDb.setColor(255, 0, 255);


    // I2C Scanner
    //Sensors::scanI2C();
    //SUCCESS_LOG("I2C Scanner Complete");

    LEDa.setColor(0, 255, 255);
    LEDb.setColor(0, 255, 255);

    // Initialize the navigation sensors (IMU, Barometer, Magnetometer)
    if (!Sensors::initAll())
    {
        state->setState(ErrorIndication::getInstance());
        return;
    }

    Sensors::setInterrupts();
    SUCCESS_LOG("Nav Sensor Initialization Complete\n");

    #if DEBUG_ON
        Angles_3D<double> bias = Sensors::setGyroBias();
        INFO_LOGf("Gyro bias set to x:%d y:%d z:%d", bias.x, bias.y, bias.z)
    #else
        Sensors::setGyroBias();
    #endif

// Initialize the optical camera
#if OPTICS_ON == true
    camera.saveSettings();
    if (!camera.initialize())
    {
        //ERROR_LOG(Debug::Critical_Error, "Camera Initialization Failed");
        state->setState(ErrorIndication::getInstance());
    }
    else
    {
        SUCCESS_LOG("Camera Initialization Complete");
    }
#endif

    #if SD_ON
    // Initialize the SD card
    if (!logger.init())
    {
        state->setState(ErrorIndication::getInstance());
        return;
    }
    #endif
    SUCCESS_LOG("SD Card Initialization Complete");

    LEDb.blink(255, 0, 0, 1000);
    LEDa.blink(255, 0, 0, 1000);

    currentState = CurrentState::INITIALIZATION;
}

void Initialization::run(StateAutomation *state)
{
    // initialization happens once and we move on...
    state->setState(Calibrate::getInstance());
}

void Initialization::exit(StateAutomation *state)
{
    output.indicateCompleteStartup();
    // set previous time before main loop
    previous_time = scoped_timer.elapsed();
}

void ErrorIndication::enter(StateAutomation *state)
{
    currentState = CurrentState::ERROR_INDICATION;
}

void ErrorIndication::run(StateAutomation *state)
{
    output.indicateError();
    continuousFunctions(state);
}

void ErrorIndication::exit(StateAutomation *state)
{

}

void IdleMode::enter(StateAutomation *state)
{
    currentState = CurrentState::IDLE_MODE;
    buoyancy.setSpeed(0);
}

void IdleMode::run(StateAutomation *state)
{
    continuousFunctions(state);
    buoyancy.run();
    #if UI_ON
    if(TransportManager::getCommands().system_state == 0)
    {
        switch(callbackState)
        {
            case CurrentState::CALIBRATE:
                state->setState(Calibrate::getInstance());
                return;
            case CurrentState::DIVING_MODE:
                state->setState(Diving::getInstance());
                return;
            case CurrentState::ERROR_INDICATION:
                state->setState(ErrorIndication::getInstance());
                return;
            case CurrentState::INITIALIZATION:
                state->setState(Initialization::getInstance());
                return;
            case CurrentState::RESURFACING:
                state->setState(Resurfacing::getInstance());
                return;
            default:
                state->setState(Calibrate::getInstance());
                return;
        }
    }
    #endif
}


void IdleMode::exit(StateAutomation *state)
{
    #if UI_ON
        TransportManager::setIdle(false);
    #endif
}

void Diving::enter(StateAutomation *state)
{
    // Set current state for reading
    currentState = CurrentState::DIVING_MODE;

    // Initialize stepper settings
    #if UI_ON
        TransportManager::Commands stepper_commands = TransportManager::getCommands();
        buoyancy.setSpeed(stepper_commands.stepper_speed);
        buoyancy.setMaxSpeed(stepper_commands.stepper_speed);
        buoyancy.setAcceleration(stepper_commands.stepper_acceleration);
    #else
        buoyancy.setSpeed(1500);
        buoyancy.setMaxSpeed(1500);
        buoyancy.setAcceleration(500);
    #endif
    buoyancy.setResolution(Stepper::Resolution::HALF);
    buoyancy.setMinPulseWidth(MIN_PULSE_WIDTH); // how long to wait between high and low pulses
    buoyancy.sink();              // set the direction of the stepper motors
}

void Diving::run(StateAutomation *state)
{
    // If we have filled the ballast start resurfacing
    // Probably want to add some type of pressure parameter so the sub goes to a certain depth
    if (buoyancy.sinking && buoyancy.currentPosition() == buoyancy.targetPosition())
    {
        #if UI_ON
        state->setState(Resurfacing::getInstance());
        return;
        #else
        state->setState(Resurfacing::getInstance());
        return;
        #endif
    }

    buoyancy.update(); // update the stepper motors

    // call the continuous loop functions
    continuousFunctions(state);
}

void Diving::exit(StateAutomation *state)
{
}

void Resurfacing::enter(StateAutomation *state)
{
    currentState = CurrentState::RESURFACING;
    #if UI_ON
        TransportManager::Commands stepper_commands = TransportManager::getCommands();
        buoyancy.setSpeed(stepper_commands.stepper_speed);
        buoyancy.setMaxSpeed(stepper_commands.stepper_speed);
        buoyancy.setAcceleration(stepper_commands.stepper_acceleration);
    #else  
        buoyancy.setSpeed(1500);
        buoyancy.setMaxSpeed(1500);
        buoyancy.setAcceleration(500);
    #endif
    buoyancy.setResolution(Stepper::Resolution::HALF);
    buoyancy.rise();
    buoyancy.setMinPulseWidth(MIN_PULSE_WIDTH); // how long to wait between high and low pulses
}

void Resurfacing::run(StateAutomation *state)
{
    // If we emptied the ballast, we move to the surface
    if (buoyancy.currentPosition() == buoyancy.targetPosition())
    {
        if (m_iterations >= 3) // every three times we dive and resurface, recalibrate to correct any integrated small errors
        {
            state->setState(Calibrate::getInstance());
            m_iterations = 0;
            return;
        }
        else
        {
            state->setState(Diving::getInstance());
            return;
        }
    }
    
    buoyancy.update(); // update the stepper motors
    // Just filling and refilling syringe at this point. need to check accelerometers and pressure
    // sensors to check minimums and maximums of our dive

    continuousFunctions(state);
}

void Resurfacing::exit(StateAutomation *state)
{
    m_iterations++;
}

void Calibrate::enter(StateAutomation *state)
{
    currentState = CurrentState::CALIBRATE;
    #if UI_ON
        TransportManager::Commands stepper_commands = TransportManager::getCommands();
        buoyancy.setSpeed(stepper_commands.stepper_speed);
        buoyancy.setMaxSpeed(stepper_commands.stepper_speed);
        buoyancy.setAcceleration(stepper_commands.stepper_acceleration);
    #else
        buoyancy.setSpeed(1500);
        buoyancy.setMaxSpeed(1500);
        buoyancy.setAcceleration(1500);
    #endif

    buoyancy.setResolution(Stepper::Resolution::HALF);
    buoyancy.setMinPulseWidth(MIN_PULSE_WIDTH); // how long to wait between high and low pulses

    buoyancy.move(500000);
}

void Calibrate::run(StateAutomation *state)
{
    continuousFunctions(state);
    buoyancy.update();

    if (buoyancy.limit.state() == true)
    {
        state->setState(Resurfacing::getInstance());
    }
}

void Calibrate::exit(StateAutomation *state)
{
    buoyancy.setCurrentPosition(0);
}

/**
 * Singleton return instance functions
 */

// Instantiates the singletons
Initialization Initialization::instance;
ErrorIndication ErrorIndication::instance;
IdleMode IdleMode::instance;
Diving Diving::instance;
Resurfacing Resurfacing::instance;
Calibrate Calibrate::instance;

Initialization &Initialization::getInstance()
{
    return instance;
}

ErrorIndication &ErrorIndication::getInstance()
{
    return instance;
}

IdleMode &IdleMode::getInstance()
{
    return instance;
}

Diving &Diving::getInstance()
{
    return instance;
}

Resurfacing &Resurfacing::getInstance()
{
    return instance;
}

Calibrate &Calibrate::getInstance()
{
    return instance;
}
