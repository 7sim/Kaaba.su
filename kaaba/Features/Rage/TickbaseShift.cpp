#include "TickbaseShift.hpp"
#include "../../source.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../Rage/EnginePrediction.hpp"
#include "../../Libraries/minhook-master/include/MinHook.h"
#include "../../Hooking/Hooked.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../Features/Visuals/EventLogger.hpp"
#include "ServerAnimations.hpp"
#include "../Miscellaneous/Movement.hpp"
#include <Themida/C/ThemidaSDK.h>
#include "../Rage/FakeLag.hpp"

TickbaseSystem g_TickbaseController;

void TickbaseSystem::Charge( ) {
	m_bPrepareCharge = m_bCharge = true;
}

void TickbaseSystem::UnCharge( bool forceProcess ) {
	if( !IsCharged( ) )
		return;

	m_bPrepareCharge = m_bCharge = m_bPreparedRecharge = false;
	if( forceProcess ) {
		while( m_iProcessTicks ) {
			Hooked::oCL_Move( m_iProcessTicks == 1, 0.f );

			m_iProcessTicks--;
		}
	}
	else {
		m_iProcessTicks = 0;
	}
}

bool TickbaseSystem::CanCharge( ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return false;

	if( pLocal->IsDead( ) )
		return false;

	if( g_Vars.rage.fake_flick_key.enabled && !g_FakeLag.m_bPlayingOnMrx && !g_Vars.globals.m_bFakeWalking ) {
		// we want to automatically recharge
		if( fabsf( g_pGlobalVars->realtime - m_flLastExploitTime ) > 0.4f ) {
			Charge( );
		}
	}
	else {
		// not enabled?
		// we prolly want to uncharge
		UnCharge( true );
	}

	if( !m_bCharge || !m_bPreparedRecharge )
		return false;

	return true;
}

bool TickbaseSystem::IncrementProcessTicks( ) {
	if( !CanCharge( ) )
		return false;

	if( IsCharged( ) ) {
		m_bPrepareCharge = m_bCharge = m_bPreparedRecharge = false;

		return false;
	}

	// tell the controller to fix tickbase
	// after we done charging
	m_bFixedCharge = false;

	++m_iProcessTicks;

	return m_iProcessTicks <= m_iMaxProcessTicks;
}

bool TickbaseSystem::IsCharged( ) {
	return m_iProcessTicks >= m_iMaxProcessTicks;
}

void TickbaseSystem::CL_Move( bool bFinalTick, float accumulated_extra_samples ) {
	if( !g_Vars.rage.fake_flick || g_FakeLag.m_bPlayingOnMrx ) {
		UnCharge( );
		return Hooked::oCL_Move( bFinalTick, accumulated_extra_samples );
	}

	if( IncrementProcessTicks( ) ) {
		return;
	}

	if( IsCharged( ) && !m_bFixedCharge ) {
		m_bFixCharge = true;
	}

	Hooked::oCL_Move( bFinalTick, accumulated_extra_samples );
}

void TickbaseSystem::RunExploits( bool* bSendPacket, CUserCmd* pCmd ) {
	VM_TIGER_RED_START;

	C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer( );

	if( !pLocal )
		return;

	const auto pWeapon = reinterpret_cast< C_WeaponCSBaseGun* >( pLocal->m_hActiveWeapon( ).Get( ) );
	if( !pWeapon )
		return;

	m_bRunningExploit = false;
	auto pWeaponData = pWeapon->GetCSWeaponData( );
	if( !pWeaponData.IsValid( ) )
		return;

	if( !g_Vars.rage.fake_flick_key.enabled ) {
		m_iFixAmount = m_iShiftAmount = 0;
		return;
	}

	if( !IsCharged( ) || !m_bFixedCharge || m_bFixCharge ) {
		m_bTapShot = false;
		return;
	}

	static bool bLastBreakSide = false;

	if( g_Vars.rage.fake_flick_key.enabled && !g_FakeLag.m_bPlayingOnMrx ) {
		m_bRunningExploit = true;

		if( !g_Movement.PressingMovementKeys( pCmd ) ) {
			const float flXor13_37 = XorFlt( 13.37f );
			pCmd->forwardmove = bLastBreakSide ? flXor13_37 : -flXor13_37;
		}
		else {
			return DoRegularShifting( g_Vars.rage.fake_flick_factor );
		}
	}

	bLastBreakSide = !bLastBreakSide;

	static int nLastShiftTick = pCmd->tick_count;
	if( abs( nLastShiftTick - pCmd->tick_count ) > ( g_Vars.rage.fake_flick_factor + 1 ) ) {
		if( g_Vars.rage.fake_flick_key.enabled ) {
			*bSendPacket = true;
			pCmd->viewangles.y += g_Vars.rage.fake_flick_angle;

			DoRegularShifting( g_Vars.rage.fake_flick_factor );
			nLastShiftTick = pCmd->tick_count;
		}
	}
	else {
		g_TickbaseController.m_angNonExploitAngle = pCmd->viewangles;
		m_bTapShot = false;
	}

	VM_TIGER_RED_END;
}

void TickbaseSystem::DoRegularShifting( int iAmount ) {
	if( iAmount >= 10 ) {
		m_bTapShot = true;
	}

	m_iFixAmount = m_iShiftAmount = iAmount;
	m_bPreFix = m_bPostFix = true;
	m_bCMFix = true;
}
