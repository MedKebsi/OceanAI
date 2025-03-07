/**
 * @file debug.cpp
 * @author Daniel Kim
 * @brief debugging macros
 * @version 0.1
 * @date 2022-12-25 (christmas yay!)
 * 
 * @copyright Copyright (c) 2022 OceanAI (https://github.com/daniel360kim/OceanAI)
 * 
 */

#include <cstdarg>
#include <string>
#include <sstream>

#include "debug.h"

#if DEBUG_ON
/*
* Three types of debug message
* Error: errors within the system
* Success: successful events within the system
* Info: information about the system
*/
Debug error(100);
Debug success(100);
Debug info(100);

DebugMessage debugMessage;

/**
 * @brief Construct a new Debug Message:: Debug Message object
 * Creates a debug message with a variable number of arguments
 * Follows the same format as printf
 * @param severity severity of message
 * @param message the message to be printed
 * @param ... arguments to be printed
 */
void DebugMessage::createMessagef(Severity severity, std::string message, ...)
{
    m_Timestamp = scoped_timer.elapsed();
    m_Severity = severity;

    va_list args;

    va_start(args, message);

    int i = 0;
    
    while (message[i] != '\0')
    {
        if (message[i] == '%')
        {
            switch(message[i+1])
            {
                case 'd':
                {
                    int32_t arg = va_arg(args, int32_t);
                    std::stringstream ss;
                    ss << arg;
                    m_Message += ss.str();
                    break;
                }
                case 'f':
                {
                    float arg = va_arg(args, double);
                    std::stringstream ss;
                    ss << arg;
                    m_Message += ss.str();
                    break;
                }
                case 's':
                {
                    char* arg = va_arg(args, char*);
                    m_Message += arg;
                    break;
                }
                default:
                {
                    m_Message += message[i];
                    break;
                }
            }
            i++;
        }
        else
        {
            m_Message += message[i];
        }
        i++;
    }

    va_end(args);

}

void DebugMessage::createMessage(Severity severity, std::string message)
{
    m_Timestamp = scoped_timer.elapsed(); //log the timestamp
    m_Severity = severity; 
    m_Message = message; 
}

void DebugMessage::SetTimestamp(int64_t timestamp)
{
    m_Timestamp = timestamp;
}

void DebugMessage::SetSeverity(Severity severity)
{
    m_Severity = severity;
}

void DebugMessage::SetMessage(std::string message)
{
    m_Message = message;
}

int64_t DebugMessage::GetTimestamp() const
{
    return m_Timestamp;
}

int64_t DebugMessage::GetTimestampMillis() const
{
    return m_Timestamp / 1000000;
}

int32_t DebugMessage::GetTimestampSeconds() const
{
    return m_Timestamp / 1000000000;
}

Severity DebugMessage::GetSeverity() const
{
    return m_Severity;
}

std::string DebugMessage::GetMessage() const
{
    return m_Message;
}

void DebugMessage::appendMessage(std::string message)
{
    m_Message += message;
}
//Debug Class

/**
 * @brief Adds a message to the debug message list
 * 
 * @param addMessage  the message to be added
 */
void Debug::addMessage(DebugMessage addMessage)
{ 
    m_Messages.push_back(addMessage);

    #if LIVE_DEBUG //print the message to the serial monitor if live debug is enabled
        Serial.print(addMessage.GetTimestampMillis()); Serial.print("ms\t");
        Serial.print(addMessage.GetMessage().c_str()); Serial.print("\n");
    #endif

    /**
     * If the number of messages exceeds the maximum number of messages, delete the oldest message
     */
    if(m_Messages.size() > m_maxMessages)
    {
        m_Messages.erase(m_Messages.begin());
    }
}

/**
 * @brief Prints all messages in the debug message list to the serial monitor
 * 
 * @param deleteMessages delete the messages after printing
 */
void Debug::printMessages(bool deleteMessages)
{
    for(auto message : m_Messages)
    {
        Serial.print(message.GetTimestampMillis()); Serial.print("ms\t");
        Serial.print(message.GetMessage().c_str()); Serial.print("\n");
    }

    if(deleteMessages)
    {
        m_Messages.clear();
    }
}

/**
 * @brief Prints a specified number of messages in the debug message list to the serial monitor
 * 
 * @param numMessages how many messages to print
 * @param deleteMessages delete the messages after printing
 */
void Debug::printMessages(int numMessages, bool deleteMessages)
{
    for(int i = 0; i < numMessages; i++)
    {
        Serial.print(m_Messages[i].GetTimestampMillis()); Serial.print("ms\t");
        Serial.print(m_Messages[i].GetMessage().c_str()); Serial.print("\n");

        if(deleteMessages)
        {
            m_Messages.erase(m_Messages.begin() + i);
        }
    }
}

/**
 * @brief Clears all messages in the debug message list
 * 
 */
void Debug::clearMessages()
{
    m_Messages.clear();
}
#endif






