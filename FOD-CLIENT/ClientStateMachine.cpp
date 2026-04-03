
#include "ClientStateMachine.h"
#include <cstdio>

//private

const char* ClientStateMachine::stateToString(ClientState s)
{
    switch (s)
    {
    case ClientState::DISCONNECTED:     return "DISCONNECTED";
    case ClientState::CONNECTING:       return "CONNECTING";
    case ClientState::AUTHENTICATING:   return "AUTHENTICATING";
    case ClientState::CONNECTED:        return "CONNECTED";
    case ClientState::REPORTING:        return "REPORTING";
    case ClientState::WAITING_RESPONSE: return "WAITING_RESPONSE";
    default:                            return "UNKNOWN";
    }
}

//Explicit transition table every legal edge is listed here.
//Any unlisted edge is rejected
bool ClientStateMachine::isValidTransition(ClientState from, ClientState to)
{
    switch (from)
    {
    case ClientState::DISCONNECTED:
        return (to == ClientState::CONNECTING);

    case ClientState::CONNECTING:
        return (to == ClientState::AUTHENTICATING ||
            to == ClientState::DISCONNECTED);

    case ClientState::AUTHENTICATING:
        return (to == ClientState::CONNECTED ||
            to == ClientState::DISCONNECTED);

    case ClientState::CONNECTED:
        return (to == ClientState::REPORTING ||
            to == ClientState::DISCONNECTED);

    case ClientState::REPORTING:
        return (to == ClientState::WAITING_RESPONSE ||
            to == ClientState::DISCONNECTED);

    case ClientState::WAITING_RESPONSE:
        return (to == ClientState::CONNECTED ||
            to == ClientState::DISCONNECTED);

    default:
        return false;
    }
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
    //client must be in CONNECTED state to submit a report
    return (currentState == ClientState::CONNECTED);
}

bool ClientStateMachine::transition(ClientState newState)
{
    if (!isValidTransition(currentState, newState))
    {
        printf("[STATE] REJECTED: %s -> %s (invalid transition)\n",
            stateToString(currentState), stateToString(newState));
        return false;
    }
    currentState = newState;
    printf("[STATE] %s\n", stateToString(currentState));
    return true;
}