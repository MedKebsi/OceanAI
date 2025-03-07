/**
 * @file TransportManager.cpp
 * @author Daniel Kim
 * @brief 
 * @version 0.1
 * @date 2022-12-29
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "TransportManager.h"
#include "logged_data.h"
#include "../core/Timer.h"
#include "../core/StateAutomation.h"

#include <Arduino.h>
#include <electricui.h>
#include <eui_macro.h>


namespace TransportManager
{
#if UI_ON
    static Packet telemetry_data; //Data to send/receive over serial to/from GUI

    static bool idle_called = false; //Flag to check if idle has been called

    static int64_t previous_telem_send_time = 0; //Time of last telemetry send

    static bool packet_one = true; //Flag to switch between packets

    //Pairing telemetry data with names to transport to GUI
    //Identifiers must be unique and short to optimize data transfer
    static eui_message_t tracked_variables[] =
    {
        //Data being sent to GUI
        EUI_UINT16("lt", telemetry_data.loop_time),
        EUI_FLOAT("v", telemetry_data.voltage),
        EUI_FLOAT("reg", telemetry_data.regulator),
        EUI_UINT8("sst", telemetry_data.system_state),
        EUI_FLOAT("it", telemetry_data.internal_temp),

        EUI_UINT16("ind", telemetry_data.hitl_data.index),
        EUI_DOUBLE("ts", telemetry_data.hitl_data.timestamp),
        EUI_DOUBLE("lat", telemetry_data.hitl_data.location.latitude),
        EUI_DOUBLE("lon", telemetry_data.hitl_data.location.longitude),
        EUI_DOUBLE("td", telemetry_data.hitl_data.distance),
        EUI_DOUBLE("as", telemetry_data.hitl_data.averageSpeed),
        EUI_DOUBLE("sx", telemetry_data.hitl_data.currentSpeed),
        EUI_FLOAT_ARRAY_RO("hd", telemetry_data.hitl_sensor_data),

        EUI_FLOAT("hr", telemetry_data.hitl_rate),
        EUI_FLOAT("hp", telemetry_data.hitl_progress),

        EUI_UINT16("sdhz", telemetry_data.sd_log_interval_hz),

        EUI_FLOAT("xd", telemetry_data.rel_ori.x),
        EUI_FLOAT("yd", telemetry_data.rel_ori.y),
        EUI_FLOAT("zd", telemetry_data.rel_ori.z),

        EUI_FLOAT_ARRAY_RO("gd", telemetry_data.gyr),
        EUI_FLOAT_ARRAY_RO("ad", telemetry_data.acc),
        EUI_FLOAT_ARRAY_RO("md", telemetry_data.mag),
        EUI_FLOAT("x", telemetry_data.x),
        EUI_FLOAT("y", telemetry_data.y),
        EUI_FLOAT("z", telemetry_data.z),

        EUI_INT16("bsp", telemetry_data.buoyancy.current_position),
        EUI_INT16("bst", telemetry_data.buoyancy.target_position),
        EUI_INT16("bss", telemetry_data.buoyancy.speed),
        EUI_INT16("bsa", telemetry_data.buoyancy.acceleration),

        EUI_INT16("psp", telemetry_data.pitch.current_position),
        EUI_INT16("pst", telemetry_data.pitch.target_position),
        EUI_INT16("pss", telemetry_data.pitch.speed),
        EUI_INT16("psa", telemetry_data.pitch.acceleration),

        //Data received from the GUI
        EUI_UINT8("ssc", telemetry_data.commands.system_state),
        EUI_INT16("bsc", telemetry_data.commands.buoyancy.speed),
        EUI_INT16("bac", telemetry_data.commands.buoyancy.acceleration),
        EUI_INT16("psc", telemetry_data.commands.pitch.speed),
        EUI_INT16("pac", telemetry_data.commands.pitch.acceleration),
        EUI_UINT8("pd", telemetry_data.commands.pitch.direction),
        EUI_UINT8("pr", telemetry_data.commands.recalibrate_pitch),
        EUI_UINT8("ap", telemetry_data.commands.auto_pitch),

        EUI_FLOAT("hds", telemetry_data.commands.hitl_scale),
        EUI_UINT8("sde", telemetry_data.commands.sd_log_enable),
        EUI_UINT16("sdr", telemetry_data.commands.sd_log_interval_hz),
    };


    //Serial write function
    static eui_interface_t serial_comms = EUI_INTERFACE(&serial_write);

    /**
     * @brief Receive data from GUI
     * 
     */
    void serial_rx_handler()
    {
        // While we have data, we will pass those bytes to the ElectricUI parser
        while (Serial.available() > 0)
        {
            eui_parse(Serial.read(), &serial_comms); // Ingest a byte
        }
    }

    /**
     * @brief Send data to GUI
     * 
     * @param data Data to send
     * @param len size of data
     */
    void serial_write(uint8_t *data, uint16_t len)
    {
        Serial.write(data, len); // output on the main serial port
    }

    void init()
    {
        while(!Serial)
        {
            //Wait for serial to connect
        }
        eui_setup_interface(&serial_comms); // Setup the interface
        EUI_TRACK(tracked_variables); // Track the variables, send to the library

        //Provide an identifier
        eui_setup_identifier((char*)"OceanAI", 8);
    }

    /**
     * @brief Sends and receives data to the GUI
     * 
     * @param logged_data shared packet between gui and computer
     * @return true change state to idle
     * @return false keep current state
     */
    bool handleTransport(LoggedData &logged_data)
    {
        serial_rx_handler();

        /**
         * @brief Sends data to GUI within the interval
         * Sends data in two packets to stay within UART buffer size
         */
        int64_t current_time = scoped_timer.elapsed();
        if(current_time - previous_telem_send_time <= SEND_INTERVAL)
        {
            return false;
        }
        else
        {
            previous_telem_send_time = current_time;

            telemetry_data.convert(logged_data); //Convert logged data to telemetry data

            //Send data to GUI
            if(packet_one)
            {
                eui_send_tracked("lt");
                eui_send_tracked("v");
                eui_send_tracked("r");
                eui_send_tracked("sst");
                eui_send_tracked("it");

                eui_send_tracked("ind");
                eui_send_tracked("ts");
                eui_send_tracked("lat");
                eui_send_tracked("lon");
                eui_send_tracked("td");
                eui_send_tracked("as");
                eui_send_tracked("sx");

                eui_send_tracked("hd");

                eui_send_tracked("hr");
                eui_send_tracked("hp");

                eui_send_tracked("sdhz");

                eui_send_tracked("xd");
                eui_send_tracked("yd");
                eui_send_tracked("zd");

                packet_one = false;
            }
            else
            {
                eui_send_tracked("gd");
                eui_send_tracked("ad");
                eui_send_tracked("md");

                eui_send_tracked("bsp");
                eui_send_tracked("bst");
                eui_send_tracked("bss");
                eui_send_tracked("bsa");

                eui_send_tracked("psp");
                eui_send_tracked("pst");
                eui_send_tracked("pss");
                eui_send_tracked("psa");

                eui_send_tracked("ap");

                eui_send_tracked("x");
                eui_send_tracked("y");
                eui_send_tracked("z");

                packet_one = true;
            }

            if(telemetry_data.commands.system_state != 0 && !idle_called)
            {
                idle_called = true;
                return true; //Change state to idle
            }
            else
            {
                return false; //Keep current state
            }
        }
    }

    Commands getCommands()
    {
        return telemetry_data.commands;
    }

    void setIdle(bool command)
    {
        idle_called = command;
    }

#endif
}