#include "../Hooked.hpp"
#include "../../SDK/Classes/Player.hpp"

void __fastcall Hooked::CalcView( void* ecx, void* edx, int a3, int a4, int a5, int a6, int a7 ) {
	auto pEntity = ( C_CSPlayer* )ecx;

	if( pEntity->EntIndex( ) != g_pEngine->GetLocalPlayer( ) )
		return oCalcView( ecx, a3, a4, a5, a6, a7 );

	// prevent client's ModifyEyePos from being called
	// https://i.imgur.com/LMYq6Jf.png

	const bool m_bUseNewAnimstate = *( int* )( ( uintptr_t )ecx + 0x39E1 );
	*( bool* )( ( uintptr_t )ecx + 0x39E1 ) = false;
	oCalcView( ecx, a3, a4, a5, a6, a7 );
	*( bool* )( ( uintptr_t )ecx + 0x39E1 ) = m_bUseNewAnimstate;
}