#include "ClientStateMachine.h"
#include <iostream>

namespace FODClient
{
    //private

    const char* ClientStateMachine::stateToString(ClientState s)
    {
        const char* name = "UNKNOWN";
        switch (s)
        {
        case ClientState::DISCONNECTED:      name = "DISCONNECTED";      break;
        case ClientState::CONNECTING:        name = "CONNECTING";        break;
        case ClientState::AUTHENTICATING:    name = "AUTHENTICATING";    break;
        case ClientState::CONNECTED:         name = "CONNECTED";         break;
        case ClientState::REPORTING:         name = "REPORTING";         break;
        case ClientState::WAITING_RESPONSE:  name = "WAITING_RESPONSE";  break;
        case ClientState::RECEIVING_BITMAP:  name = "RECEIVING_BITMAP";  break;
        default:                             /* no action required */     break;
        }
        return name;
    }

    //explicit transition tabl every legal edge is listed here.
    bool ClientStateMachine::isValidTransition(ClientState from, ClientState to)
    {
        bool valid = false;
        switch (from)
        {
        case ClientState::DISCONNECTED:
            valid = (to == ClientState::CONNECTING);
            break;

        case ClientState::CONNECTING:
            valid = (to == ClientState::AUTHENTICATING) ||
                (to == ClientState::DISCONNECTED);
            break;

        case ClientState::AUTHENTICATING:
            valid = (to == ClientState::CONNECTED) ||
                (to == ClientState::DISCONNECTED);
            break;

        case ClientState::CONNECTED:
            valid = (to == ClientState::REPORTING) ||
                (to == ClientState::DISCONNECTED);
            break;

        case ClientState::REPORTING:
            valid = (to == ClientState::WAITING_RESPONSE) ||
                (to == ClientState::DISCONNECTED);
            break;

        case ClientState::WAITING_RESPONSE:
            valid = (to == ClientState::RECEIVING_BITMAP) ||
                (to == ClientState::CONNECTED) ||
                (to == ClientState::DISCONNECTED);
            break;

        case ClientState::RECEIVING_BITMAP:
            valid = (to == ClientState::CONNECTED) ||
                (to == ClientState::DISCONNECTED);
            break;

        default:
            //no action required
            break;
        }
        return valid;
    }

    //public

    ClientStateMachine::ClientStateMachine()
        : currentState(ClientState::DISCONNECTED)
    {
    }

    ClientState ClientStateMachine::getState() const
    {
        return currentState;
    }

    const char* ClientStateMachine::currentStateName() const
    {
        return stateToString(currentState);
    }

    bool ClientStateMachine::canReport() const
    {
        return (currentState == ClientState::CONNECTED);
    }

    bool ClientStateMachine::transition(ClientState newState)
    {
        bool success = false;
        if (isValidTransition(currentState, newState))
        {
            currentState = newState;
            std::cout << "[STATE] " << stateToString(currentState) << std::endl;
            success = true;
        }
        else
        {
            std::cout << "[STATE] REJECTED: " << stateToString(currentState)
                << " -> " << stateToString(newState)
                << " (invalid transition)" << std::endl;
        }
        return success;
    }
}