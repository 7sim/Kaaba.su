#include "../hooked.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../SDK/Classes/Exploits.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"

void WriteUsercmd( bf_write* buf, CUserCmd* incmd, CUserCmd* outcmd ) {
	__asm
	{
		mov     ecx, buf
		mov     edx, incmd
		push    outcmd
		call    Engine::Displacement.Function.m_WriteUsercmd
		add     esp, 4
	}
}

bool __fastcall Hooked::SendNetMsg( INetChannel* pNetChan, void* edx, INetMessage& msg, bool bForceReliable, bool bVoice ) {
	g_Vars.globals.szLastHookCalled = XorStr( "33" );

	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( pNetChan != g_pEngine->GetNetChannelInfo( ) || !pLocal || !pNetChan )
		return oSendNetMsg( pNetChan, msg, bForceReliable, bVoice );

	if( msg.GetGroup( ) == 9 )
		bVoice = true;

	//if( msg.GetType( ) == 14 )
	//	return false;

	if( msg.GetGroup( ) == 11 ) {
		auto CL_Move = ( CCLCMsg_Move_t* )&msg;
		if( CL_Move ) {
			if( g_TickbaseController.m_iShiftAmount || CL_Move->m_nNewCommands >= 15 || g_pClientState->m_nChokedCommands( ) > 14 ) {
				using assign_lol = std::string& ( __thiscall* )( void*, uint8_t*, size_t );
				auto assign_std_autistic_string = ( assign_lol )Engine::Displacement.Function.m_StdStringAssign;

				// rebuild CL_SendMove
				uint8_t data[ 4000 ];
				bf_write buf;
				buf.m_nDataBytes = 4000;
				buf.m_nDataBits = 32000;
				buf.m_pData = data;
				buf.m_iCurBit = false;
				buf.m_bOverflow = false;
				buf.m_bAssertOnOverflow = false;
				buf.m_pDebugName = false;
				int numCmd = g_pClientState->m_nChokedCommands( ) + 1;
				int nextCmdNr = g_pClientState->m_nLastOutgoingCommand( ) + numCmd;
				if( numCmd > 62 )
					numCmd = 62;

				bool bOk = true;

				auto to = nextCmdNr - numCmd + 1;
				auto from = -1;
				if( to <= nextCmdNr ) {
					int newcmdnr = to >= ( nextCmdNr - numCmd + 1 );
					do {
						bOk = bOk && g_pInput->WriteUsercmdDeltaToBuffer( 0, &buf, from, to, to >= newcmdnr );

						//g_TickbaseController.m_iProcessTicks = std::max( 0, g_TickbaseController.m_iProcessTicks - 1 );

						from = to++;
					} while( to <= nextCmdNr );
				}

				if( bOk ) {
					if( g_TickbaseController.m_iShiftAmount > 0 ) {
						CUserCmd from_cmd, to_cmd;
						from_cmd = g_pInput->m_pCommands[ nextCmdNr % MULTIPLAYER_BACKUP ];
						to_cmd = from_cmd;
						to_cmd.tick_count += 192;
						to_cmd.command_number++;

						do {
							if( numCmd >= 62 ) {
								g_TickbaseController.m_iShiftAmount = 0;
								break;
							}

							to_cmd.command_number++;
							to_cmd.tick_count++;
							WriteUsercmd( &buf, &to_cmd, &from_cmd );

							g_TickbaseController.m_iShiftAmount--;
							numCmd++;
						} while( g_TickbaseController.m_iShiftAmount > 0 );
					}
					else {
						g_TickbaseController.m_iShiftAmount = 0;
					}

					// bypass choke limit
					CL_Move->m_nNewCommands = numCmd;
					CL_Move->m_nBackupCommands = 0;

					int curbit = ( buf.m_iCurBit + 7 ) >> 3;
					assign_std_autistic_string( CL_Move->m_data, buf.m_pData, curbit );
				}
			}
		}
	}
	return oSendNetMsg( pNetChan, msg, bForceReliable, bVoice );
}

void __fastcall Hooked::Shutdown( INetChannel* pNetChan, void* EDX, const char* reason ) {
	g_Vars.globals.szLastHookCalled = XorStr( "35" ); 
	return oShutdown( pNetChan, reason );
}

bool __fastcall Hooked::LooseFileAllowed( void* ecx, void* edx ) {
	return true;
}

void __fastcall Hooked::CheckFileCRCsWithServer( void* ecx, void* edx ) {
	return;
}
