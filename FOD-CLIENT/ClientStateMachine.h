#pragma once

// Client state machine — enforces that the client cannot submit a FOD report
// unless it is in the CONNECTED state, preventing premature transmission.

namespace FODClient
{
    enum class ClientState
    {
        DISCONNECTED,       // Initial / after clean shutdown
        CONNECTING,         // TCP connect() in progress
        AUTHENTICATING,     // Auth packet sent, awaiting server accept
        CONNECTED,          // Authenticated and idle - ready to report
        REPORTING,          // Gathering user input for a FOD report
        WAITING_RESPONSE,   // FOD packet sent, awaiting server reply
        RECEIVING_BITMAP    // Receiving runway zone bitmap from server
    };

    class ClientStateMachine
    {
    private:
        ClientState currentState;

        static const char* stateToString(ClientState s);
        static bool isValidTransition(ClientState from, ClientState to);

    public:
        ClientStateMachine();

        ClientState getState() const;
        const char* currentStateName() const;

        //returns true only when in CONNECTED state
        bool canReport() const;

        //attempt a state transition
        bool transition(ClientState newState);
    };
}