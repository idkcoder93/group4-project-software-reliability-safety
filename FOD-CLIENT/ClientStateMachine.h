#pragma once

//Enforces that the client cannot submit a FOD report unless it is in the CONNECTED state, preventing premature transmission 

enum class ClientState
{
    DISCONNECTED,       //Initial / after clean shutdown
    CONNECTING,         //TCP connect() in progress
    AUTHENTICATING,     //Auth packet sent, awaiting server accept
    CONNECTED,          //Authenticated and idle — ready to report
    REPORTING,          //Gathering user input for a FOD report
    WAITING_RESPONSE    //FOD packet sent, awaiting server reply
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

    //Returns true only when in CONNECTED state
    bool canReport() const;

    //Attempt a state transition
    bool transition(ClientState newState);
};