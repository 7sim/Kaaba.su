#include "../Hooked.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../Features/Rage/LagCompensation.hpp"
#include "../../Features/Miscellaneous/Miscellaneous.hpp"

int __fastcall Hooked::SendDatagram( INetChannel* ecx, void* edx, void* a3 ) {
	auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal ) {
		return oSendDatagram( ecx, a3 );
	}
	
	const auto nBackupInSequenceNr = ecx->m_nInSequenceNr;
	
	g_Misc.OnSendDatagram( ecx );

	auto oRet = oSendDatagram( ecx, a3 );

	ecx->m_nInSequenceNr = nBackupInSequenceNr;

	return oRet;
}

void __fastcall Hooked::ProcessPacket( INetChannel* ecx, void* edx, void* a3, bool a4 ) {
	oProcessPacket( ecx, a3, a4 );
	///nvm
}