#pragma once

//Server-side runway state machine
// Manages runway operational status in response to incoming FOD report

namespace FODServer
{
    enum class ServerState
    {
        DISCONNECTED,   // No client connected
        CONNECTED,      // Client authenticated, idle
        INSPECTION,     // FOD report received, awaiting assessment
        HAZARD,         // Debris confirmed on runway
        CLEARED         // Runway confirmed clear
    };

    class ServerStateMachine
    {
    private:
        ServerState currentState;

        static const char* stateToString(ServerState s);
        static bool isValidTransition(ServerState from, ServerState to);

    public:
        ServerStateMachine();

        ServerState getState() const;
        const char* currentStateName() const;

        //attempt a state transition; returns true on success
        bool transition(ServerState newState);

        //can the server accept FOD reports in this state (for convenience)
        bool canAcceptReport() const;
    };
}