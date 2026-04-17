#include "pch.h"
#include "CppUnitTest.h"
#include "../FOD-SERVER/ServerStateMachine.h"
#include "../FOD-CLIENT/ClientStateMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    // =========================================================
    // ServerStateMachine Tests
    //
    // Primary coverage: REQ-SYS-060 (operational state machine
    // with well-defined transitions).
    //
    // Secondary coverage: REQ-SYS-080 (connection verification
    // before accepting commands). The canAcceptReport() tests
    // and the "DISCONNECTED cannot skip to a report-accepting
    // state" tests prove that no FOD command path can execute
    // without a prior successful auth, because transition(CONNECTED)
    // is the only route into report-accepting states, and that
    // transition is only called by ServerSession.cpp after
    // User::authenticateUser returns true.
    // =========================================================
    TEST_CLASS(ServerStateMachineTests)
    {
    private:
        // Drive the state machine to a target state using only
        // valid transitions (the public API enforces the FSM,
        // so this is how tests reach non-initial states).
        static void driveTo(FODServer::ServerStateMachine& sm,
            FODServer::ServerState target)
        {
            using FODServer::ServerState;
            switch (target)
            {
            case ServerState::DISCONNECTED:
                break; // initial state
            case ServerState::CONNECTED:
                (void)sm.transition(ServerState::CONNECTED);
                break;
            case ServerState::INSPECTION:
                (void)sm.transition(ServerState::CONNECTED);
                (void)sm.transition(ServerState::INSPECTION);
                break;
            case ServerState::HAZARD:
                (void)sm.transition(ServerState::CONNECTED);
                (void)sm.transition(ServerState::INSPECTION);
                (void)sm.transition(ServerState::HAZARD);
                break;
            case ServerState::CLEARED:
                (void)sm.transition(ServerState::CONNECTED);
                (void)sm.transition(ServerState::INSPECTION);
                (void)sm.transition(ServerState::CLEARED);
                break;
            default:
                break;
            }
        }

    public:
        // --- Initial state ---

        TEST_METHOD(InitialState_IsDisconnected)
        {
            FODServer::ServerStateMachine sm;
            Assert::IsTrue(sm.getState() == FODServer::ServerState::DISCONNECTED);
        }

        TEST_METHOD(InitialState_CannotAcceptReport)
        {
            // REQ-SYS-080: pre-auth state must reject report paths
            FODServer::ServerStateMachine sm;
            Assert::IsFalse(sm.canAcceptReport());
        }

        // --- State name strings ---

        TEST_METHOD(CurrentStateName_Disconnected_CorrectString)
        {
            FODServer::ServerStateMachine sm;
            Assert::AreEqual("DISCONNECTED", sm.currentStateName());
        }

        TEST_METHOD(CurrentStateName_Connected_CorrectString)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CONNECTED);
            Assert::AreEqual("CONNECTED", sm.currentStateName());
        }

        TEST_METHOD(CurrentStateName_Inspection_CorrectString)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::INSPECTION);
            Assert::AreEqual("INSPECTION", sm.currentStateName());
        }

        TEST_METHOD(CurrentStateName_Hazard_CorrectString)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::HAZARD);
            Assert::AreEqual("HAZARD", sm.currentStateName());
        }

        TEST_METHOD(CurrentStateName_Cleared_CorrectString)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CLEARED);
            Assert::AreEqual("CLEARED", sm.currentStateName());
        }

        // --- Legal transitions from DISCONNECTED ---

        TEST_METHOD(Transition_DisconnectedToConnected_Succeeds)
        {
            FODServer::ServerStateMachine sm;
            const bool ok = sm.transition(FODServer::ServerState::CONNECTED);
            Assert::IsTrue(ok);
            Assert::IsTrue(sm.getState() == FODServer::ServerState::CONNECTED);
        }

        // --- Illegal transitions from DISCONNECTED (REQ-SYS-080 proof) ---

        TEST_METHOD(Transition_DisconnectedToInspection_Fails)
        {
            FODServer::ServerStateMachine sm;
            const bool ok = sm.transition(FODServer::ServerState::INSPECTION);
            Assert::IsFalse(ok);
            Assert::IsTrue(sm.getState() == FODServer::ServerState::DISCONNECTED);
        }

        TEST_METHOD(Transition_DisconnectedToHazard_Fails)
        {
            FODServer::ServerStateMachine sm;
            const bool ok = sm.transition(FODServer::ServerState::HAZARD);
            Assert::IsFalse(ok);
            Assert::IsTrue(sm.getState() == FODServer::ServerState::DISCONNECTED);
        }

        TEST_METHOD(Transition_DisconnectedToCleared_Fails)
        {
            FODServer::ServerStateMachine sm;
            const bool ok = sm.transition(FODServer::ServerState::CLEARED);
            Assert::IsFalse(ok);
            Assert::IsTrue(sm.getState() == FODServer::ServerState::DISCONNECTED);
        }

        // --- Legal transitions from CONNECTED ---

        TEST_METHOD(Transition_ConnectedToInspection_Succeeds)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CONNECTED);
            Assert::IsTrue(sm.transition(FODServer::ServerState::INSPECTION));
        }

        TEST_METHOD(Transition_ConnectedToDisconnected_Succeeds)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CONNECTED);
            Assert::IsTrue(sm.transition(FODServer::ServerState::DISCONNECTED));
        }

        // --- Illegal transitions from CONNECTED ---

        TEST_METHOD(Transition_ConnectedToHazard_Fails)
        {
            // Must route through INSPECTION first
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CONNECTED);
            const bool ok = sm.transition(FODServer::ServerState::HAZARD);
            Assert::IsFalse(ok);
            Assert::IsTrue(sm.getState() == FODServer::ServerState::CONNECTED);
        }

        TEST_METHOD(Transition_ConnectedToCleared_Fails)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CONNECTED);
            Assert::IsFalse(sm.transition(FODServer::ServerState::CLEARED));
        }

        TEST_METHOD(Transition_SelfTransition_Connected_Fails)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CONNECTED);
            Assert::IsFalse(sm.transition(FODServer::ServerState::CONNECTED));
        }

        // --- Legal transitions from INSPECTION ---

        TEST_METHOD(Transition_InspectionToHazard_Succeeds)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::INSPECTION);
            Assert::IsTrue(sm.transition(FODServer::ServerState::HAZARD));
        }

        TEST_METHOD(Transition_InspectionToCleared_Succeeds)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::INSPECTION);
            Assert::IsTrue(sm.transition(FODServer::ServerState::CLEARED));
        }

        TEST_METHOD(Transition_InspectionToDisconnected_Succeeds)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::INSPECTION);
            Assert::IsTrue(sm.transition(FODServer::ServerState::DISCONNECTED));
        }

        TEST_METHOD(Transition_InspectionToConnected_Fails)
        {
            // Cannot roll back to CONNECTED from mid-inspection
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::INSPECTION);
            const bool ok = sm.transition(FODServer::ServerState::CONNECTED);
            Assert::IsFalse(ok);
            Assert::IsTrue(sm.getState() == FODServer::ServerState::INSPECTION);
        }

        // --- Legal transitions from HAZARD ---

        TEST_METHOD(Transition_HazardToCleared_Succeeds)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::HAZARD);
            Assert::IsTrue(sm.transition(FODServer::ServerState::CLEARED));
        }

        TEST_METHOD(Transition_HazardToInspection_Succeeds)
        {
            // Re-inspection of an already-hazardous zone
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::HAZARD);
            Assert::IsTrue(sm.transition(FODServer::ServerState::INSPECTION));
        }

        TEST_METHOD(Transition_HazardToDisconnected_Succeeds)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::HAZARD);
            Assert::IsTrue(sm.transition(FODServer::ServerState::DISCONNECTED));
        }

        // --- Legal transitions from CLEARED ---

        TEST_METHOD(Transition_ClearedToInspection_Succeeds)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CLEARED);
            Assert::IsTrue(sm.transition(FODServer::ServerState::INSPECTION));
        }

        TEST_METHOD(Transition_ClearedToHazard_Fails)
        {
            // Must route through INSPECTION to go from CLEARED to HAZARD
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CLEARED);
            Assert::IsFalse(sm.transition(FODServer::ServerState::HAZARD));
        }

        // --- canAcceptReport state-coverage matrix (REQ-SYS-080) ---

        TEST_METHOD(CanAcceptReport_Disconnected_ReturnsFalse)
        {
            FODServer::ServerStateMachine sm;
            Assert::IsFalse(sm.canAcceptReport());
        }

        TEST_METHOD(CanAcceptReport_Connected_ReturnsTrue)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CONNECTED);
            Assert::IsTrue(sm.canAcceptReport());
        }

        TEST_METHOD(CanAcceptReport_Inspection_ReturnsFalse)
        {
            // Mid-inspection - server is processing, not accepting new reports
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::INSPECTION);
            Assert::IsFalse(sm.canAcceptReport());
        }

        TEST_METHOD(CanAcceptReport_Hazard_ReturnsTrue)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::HAZARD);
            Assert::IsTrue(sm.canAcceptReport());
        }

        TEST_METHOD(CanAcceptReport_Cleared_ReturnsTrue)
        {
            FODServer::ServerStateMachine sm;
            driveTo(sm, FODServer::ServerState::CLEARED);
            Assert::IsTrue(sm.canAcceptReport());
        }
    };

    // =========================================================
    // ClientStateMachine Tests
    //
    // Primary coverage: REQ-SYS-060 (client-side state machine).
    //
    // Secondary coverage: REQ-SYS-080. canReport() returns true
    // only in CONNECTED, and CONNECTED is unreachable without
    // passing through AUTHENTICATING - structurally preventing
    // any report path before authentication completes.
    // =========================================================
    TEST_CLASS(ClientStateMachineTests)
    {
    private:
        static void driveTo(FODClient::ClientStateMachine& sm,
            FODClient::ClientState target)
        {
            using FODClient::ClientState;
            switch (target)
            {
            case ClientState::DISCONNECTED:
                break;
            case ClientState::CONNECTING:
                (void)sm.transition(ClientState::CONNECTING);
                break;
            case ClientState::AUTHENTICATING:
                (void)sm.transition(ClientState::CONNECTING);
                (void)sm.transition(ClientState::AUTHENTICATING);
                break;
            case ClientState::CONNECTED:
                (void)sm.transition(ClientState::CONNECTING);
                (void)sm.transition(ClientState::AUTHENTICATING);
                (void)sm.transition(ClientState::CONNECTED);
                break;
            case ClientState::REPORTING:
                (void)sm.transition(ClientState::CONNECTING);
                (void)sm.transition(ClientState::AUTHENTICATING);
                (void)sm.transition(ClientState::CONNECTED);
                (void)sm.transition(ClientState::REPORTING);
                break;
            case ClientState::WAITING_RESPONSE:
                (void)sm.transition(ClientState::CONNECTING);
                (void)sm.transition(ClientState::AUTHENTICATING);
                (void)sm.transition(ClientState::CONNECTED);
                (void)sm.transition(ClientState::REPORTING);
                (void)sm.transition(ClientState::WAITING_RESPONSE);
                break;
            case ClientState::RECEIVING_BITMAP:
                (void)sm.transition(ClientState::CONNECTING);
                (void)sm.transition(ClientState::AUTHENTICATING);
                (void)sm.transition(ClientState::CONNECTED);
                (void)sm.transition(ClientState::REPORTING);
                (void)sm.transition(ClientState::WAITING_RESPONSE);
                (void)sm.transition(ClientState::RECEIVING_BITMAP);
                break;
            default:
                break;
            }
        }

    public:
        // --- Initial state ---

        TEST_METHOD(InitialState_IsDisconnected)
        {
            FODClient::ClientStateMachine sm;
            Assert::IsTrue(sm.getState() == FODClient::ClientState::DISCONNECTED);
        }

        TEST_METHOD(InitialState_CannotReport)
        {
            // REQ-SYS-080: pre-auth client must not be able to report
            FODClient::ClientStateMachine sm;
            Assert::IsFalse(sm.canReport());
        }

        // --- State name strings ---

        TEST_METHOD(CurrentStateName_Disconnected_CorrectString)
        {
            FODClient::ClientStateMachine sm;
            Assert::AreEqual("DISCONNECTED", sm.currentStateName());
        }

        TEST_METHOD(CurrentStateName_Connecting_CorrectString)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::CONNECTING);
            Assert::AreEqual("CONNECTING", sm.currentStateName());
        }

        TEST_METHOD(CurrentStateName_Authenticating_CorrectString)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::AUTHENTICATING);
            Assert::AreEqual("AUTHENTICATING", sm.currentStateName());
        }

        TEST_METHOD(CurrentStateName_Connected_CorrectString)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::CONNECTED);
            Assert::AreEqual("CONNECTED", sm.currentStateName());
        }

        TEST_METHOD(CurrentStateName_Reporting_CorrectString)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::REPORTING);
            Assert::AreEqual("REPORTING", sm.currentStateName());
        }

        TEST_METHOD(CurrentStateName_WaitingResponse_CorrectString)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::WAITING_RESPONSE);
            Assert::AreEqual("WAITING_RESPONSE", sm.currentStateName());
        }

        TEST_METHOD(CurrentStateName_ReceivingBitmap_CorrectString)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::RECEIVING_BITMAP);
            Assert::AreEqual("RECEIVING_BITMAP", sm.currentStateName());
        }

        // --- Happy path connect flow ---

        TEST_METHOD(Transition_DisconnectedToConnecting_Succeeds)
        {
            FODClient::ClientStateMachine sm;
            Assert::IsTrue(sm.transition(FODClient::ClientState::CONNECTING));
        }

        TEST_METHOD(Transition_ConnectingToAuthenticating_Succeeds)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::CONNECTING);
            Assert::IsTrue(sm.transition(FODClient::ClientState::AUTHENTICATING));
        }

        TEST_METHOD(Transition_AuthenticatingToConnected_Succeeds)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::AUTHENTICATING);
            Assert::IsTrue(sm.transition(FODClient::ClientState::CONNECTED));
        }

        // --- REQ-SYS-080: skipping the auth phase must be rejected ---

        TEST_METHOD(Transition_DisconnectedToConnected_Fails)
        {
            // Critical invariant: CONNECTED is unreachable without
            // passing through CONNECTING and AUTHENTICATING.
            FODClient::ClientStateMachine sm;
            Assert::IsFalse(sm.transition(FODClient::ClientState::CONNECTED));
            Assert::IsTrue(sm.getState() == FODClient::ClientState::DISCONNECTED);
        }

        TEST_METHOD(Transition_DisconnectedToAuthenticating_Fails)
        {
            FODClient::ClientStateMachine sm;
            Assert::IsFalse(sm.transition(FODClient::ClientState::AUTHENTICATING));
        }

        TEST_METHOD(Transition_DisconnectedToReporting_Fails)
        {
            FODClient::ClientStateMachine sm;
            Assert::IsFalse(sm.transition(FODClient::ClientState::REPORTING));
        }

        TEST_METHOD(Transition_ConnectingToConnected_Fails)
        {
            // Cannot skip AUTHENTICATING
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::CONNECTING);
            Assert::IsFalse(sm.transition(FODClient::ClientState::CONNECTED));
        }

        TEST_METHOD(Transition_AuthenticatingToReporting_Fails)
        {
            // Cannot skip CONNECTED
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::AUTHENTICATING);
            Assert::IsFalse(sm.transition(FODClient::ClientState::REPORTING));
        }

        // --- Report cycle ---

        TEST_METHOD(Transition_ConnectedToReporting_Succeeds)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::CONNECTED);
            Assert::IsTrue(sm.transition(FODClient::ClientState::REPORTING));
        }

        TEST_METHOD(Transition_ReportingToWaitingResponse_Succeeds)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::REPORTING);
            Assert::IsTrue(sm.transition(FODClient::ClientState::WAITING_RESPONSE));
        }

        TEST_METHOD(Transition_WaitingResponseToReceivingBitmap_Succeeds)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::WAITING_RESPONSE);
            Assert::IsTrue(sm.transition(FODClient::ClientState::RECEIVING_BITMAP));
        }

        TEST_METHOD(Transition_ReceivingBitmapToConnected_Succeeds)
        {
            // Completes the report cycle, ready for the next report
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::RECEIVING_BITMAP);
            Assert::IsTrue(sm.transition(FODClient::ClientState::CONNECTED));
        }

        TEST_METHOD(Transition_WaitingResponseToConnected_Succeeds)
        {
            // Server response without follow-up bitmap
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::WAITING_RESPONSE);
            Assert::IsTrue(sm.transition(FODClient::ClientState::CONNECTED));
        }

        // --- Any active state can disconnect ---

        TEST_METHOD(Transition_ConnectedToDisconnected_Succeeds)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::CONNECTED);
            Assert::IsTrue(sm.transition(FODClient::ClientState::DISCONNECTED));
        }

        TEST_METHOD(Transition_WaitingResponseToDisconnected_Succeeds)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::WAITING_RESPONSE);
            Assert::IsTrue(sm.transition(FODClient::ClientState::DISCONNECTED));
        }

        // --- canReport() coverage matrix (REQ-SYS-080) ---
        // Must return true in CONNECTED only, false in all other states.

        TEST_METHOD(CanReport_Disconnected_ReturnsFalse)
        {
            FODClient::ClientStateMachine sm;
            Assert::IsFalse(sm.canReport());
        }

        TEST_METHOD(CanReport_Connecting_ReturnsFalse)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::CONNECTING);
            Assert::IsFalse(sm.canReport());
        }

        TEST_METHOD(CanReport_Authenticating_ReturnsFalse)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::AUTHENTICATING);
            Assert::IsFalse(sm.canReport());
        }

        TEST_METHOD(CanReport_Connected_ReturnsTrue)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::CONNECTED);
            Assert::IsTrue(sm.canReport());
        }

        TEST_METHOD(CanReport_Reporting_ReturnsFalse)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::REPORTING);
            Assert::IsFalse(sm.canReport());
        }

        TEST_METHOD(CanReport_WaitingResponse_ReturnsFalse)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::WAITING_RESPONSE);
            Assert::IsFalse(sm.canReport());
        }

        TEST_METHOD(CanReport_ReceivingBitmap_ReturnsFalse)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::RECEIVING_BITMAP);
            Assert::IsFalse(sm.canReport());
        }

        // --- Self-transition rejection ---

        TEST_METHOD(Transition_SelfTransition_Connected_Fails)
        {
            FODClient::ClientStateMachine sm;
            driveTo(sm, FODClient::ClientState::CONNECTED);
            Assert::IsFalse(sm.transition(FODClient::ClientState::CONNECTED));
        }
    };
}