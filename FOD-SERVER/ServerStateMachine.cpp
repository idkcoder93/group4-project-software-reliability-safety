#include "ServerStateMachine.h"
#include "Logger.h"
#include <string>

namespace FODServer
{
    // private

    const char* ServerStateMachine::stateToString(ServerState s)
    {
        const char* name = "UNKNOWN";
        switch (s)
        {
        case ServerState::DISCONNECTED: name = "DISCONNECTED"; break;
        case ServerState::CONNECTED:    name = "CONNECTED";    break;
        case ServerState::INSPECTION:   name = "INSPECTION";   break;
        case ServerState::HAZARD:       name = "HAZARD";       break;
        case ServerState::CLEARED:      name = "CLEARED";      break;
        default:                        /* no action required */ break;
        }
        return name;
    }

    // Explicit transition table
    bool ServerStateMachine::isValidTransition(ServerState from, ServerState to)
    {
        bool valid = false;
        switch (from)
        {
        case ServerState::DISCONNECTED:
            valid = (to == ServerState::CONNECTED);
            break;

        case ServerState::CONNECTED:
            valid = (to == ServerState::INSPECTION) ||
                (to == ServerState::DISCONNECTED);
            break;

        case ServerState::INSPECTION:
            valid = (to == ServerState::HAZARD) ||
                (to == ServerState::CLEARED) ||
                (to == ServerState::DISCONNECTED);
            break;

        case ServerState::HAZARD:
            valid = (to == ServerState::CLEARED) ||
                (to == ServerState::INSPECTION) ||
                (to == ServerState::DISCONNECTED);
            break;

        case ServerState::CLEARED:
            valid = (to == ServerState::INSPECTION) ||
                (to == ServerState::DISCONNECTED);
            break;

        default:
            /* no action required */
            break;
        }
        return valid;
    }

    // public

    ServerStateMachine::ServerStateMachine()
        : currentState(ServerState::DISCONNECTED)
    {
    }

    ServerState ServerStateMachine::getState() const
    {
        return currentState;
    }

    const char* ServerStateMachine::currentStateName() const
    {
        return stateToString(currentState);
    }

    bool ServerStateMachine::transition(ServerState newState)
    {
        bool success = false;
        if (isValidTransition(currentState, newState))
        {
            const std::string msg = std::string("[STATE] ") +
                stateToString(currentState) + " -> " + stateToString(newState);
            Logger::log(msg, Logger::INFO);
            currentState = newState;
            success = true;
        }
        else
        {
            const std::string msg = std::string("[STATE] REJECTED: ") +
                stateToString(currentState) + " -> " + stateToString(newState) +
                " (invalid transition)";
            Logger::log(msg, Logger::WARNING);
        }
        return success;
    }

    bool ServerStateMachine::canAcceptReport() const
    {
        return (currentState == ServerState::CONNECTED) ||
            (currentState == ServerState::CLEARED) ||
            (currentState == ServerState::HAZARD);
    }
}