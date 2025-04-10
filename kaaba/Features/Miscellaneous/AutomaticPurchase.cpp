#include "AutomaticPurchase.hpp"

#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"

AutomaticPurchase g_AutomaticPurchase;

// this kinda sucks but if we update queued commands in GameEvent
// they get updated way too late and will cause you to get kicked
void AutomaticPurchase::UpdateQueuedCommands( ) {
	if( g_Vars.misc.autobuy_first_weapon > 0 ) {
		m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_second_weapon > 0 ) {
		if( g_Vars.misc.autobuy_second_weapon == 3 )
			m_iQueuedUpCommands += 3;
		else
			m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_armor ) {
		m_iQueuedUpCommands += 2;
	}

	if( g_Vars.misc.autobuy_hegrenade ) {
		m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_molotovgrenade ) {
		m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_smokegreanade ) {
		m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_flashbang ) {
		m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_zeus ) {
		m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_defusekit ) {
		m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_decoy ) {
		m_iQueuedUpCommands += 1;
	}
}

// chase <3
// chase why so cute............
// we love chase!
void AutomaticPurchase::GameEvent( ) {
	if( !g_Vars.misc.autobuy_enabled )
		return;

	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	int iTeam = pLocal->m_iTeamNum( );

	auto weapons = pLocal->m_hMyWeapons( );
	bool bFoundAuto = false;
	for( size_t i = 0; i < 48; ++i ) {
		auto weaponHandle = weapons[ i ];
		if( !weaponHandle.IsValid( ) )
			break;

		auto pWeapon = ( C_BaseCombatWeapon * )weaponHandle.Get( );
		if( !pWeapon )
			continue;

		if( pWeapon->m_iItemDefinitionIndex( ) == WEAPON_SCAR20 ||
			pWeapon->m_iItemDefinitionIndex( ) == WEAPON_G3SG1 ) {
			bFoundAuto = true;
		}
	}

	if( g_Vars.misc.autobuy_first_weapon > 0 ) {
		switch( g_Vars.misc.autobuy_first_weapon ) {
			case 1: // scar20 / g3sg1
			{
				if( bFoundAuto )
					break;

				if( iTeam == TEAM_TT ) {
					g_pEngine->ClientCmd( XorStr( "buy g3sg1" ) );
				}
				else if( iTeam == TEAM_CT ) {
					g_pEngine->ClientCmd( XorStr( "buy scar20" ) );
				}

				// m_iQueuedUpCommands += 1;
				break;
			}
			case 2:
				g_pEngine->ClientCmd( XorStr( "buy ssg08" ) );
				break;
			case 3:
				g_pEngine->ClientCmd( XorStr( "buy awp" ) );
				break;
			default:
				break;
		}
	}

	if( g_Vars.misc.autobuy_second_weapon > 0 ) {
		switch( g_Vars.misc.autobuy_second_weapon ) {
			case 1: // elite
				g_pEngine->ClientCmd( XorStr( "buy elite" ) );
				//m_iQueuedUpCommands += 1;
				break;
			case 2: // deagle / revolver
				g_pEngine->ClientCmd( XorStr( "buy deagle" ) );
				//m_iQueuedUpCommands += 1;
				break;
			case 3: // tec-9
				g_pEngine->ClientCmd( XorStr( "buy cz75a; buy fiveseven;buy tec9" ) );
				//m_iQueuedUpCommands += 1;
				break;
			default:
				break;
		}
	}

	if( g_Vars.misc.autobuy_armor ) {
		g_pEngine->ClientCmd( XorStr( "buy vest" ) );
		g_pEngine->ClientCmd( XorStr( "buy vesthelm" ) );

		//m_iQueuedUpCommands += 2;
	}

	if( g_Vars.misc.autobuy_hegrenade ) {
		g_pEngine->ClientCmd( XorStr( "buy hegrenade" ) );

		//m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_molotovgrenade ) {
		if( iTeam == TEAM_TT )
			g_pEngine->ClientCmd( XorStr( "buy molotov" ) );
		else if( iTeam == TEAM_CT )
			g_pEngine->ClientCmd( XorStr( "buy incgrenade" ) );

		//m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_smokegreanade ) {
		g_pEngine->ClientCmd( XorStr( "buy smokegrenade" ) );

		//m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_flashbang ) {
		g_pEngine->ClientCmd( XorStr( "buy flashbang" ) );

		//m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_zeus ) {
		g_pEngine->ClientCmd( XorStr( "buy taser" ) );

		//m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_defusekit ) {
		g_pEngine->ClientCmd( XorStr( "buy defuser" ) );

		//m_iQueuedUpCommands += 1;
	}

	if( g_Vars.misc.autobuy_decoy ) {
		g_pEngine->ClientCmd( XorStr( "buy decoy" ) );

		//m_iQueuedUpCommands += 1;
	}
}

// super fast method of purchasing restriced weapons (awp, scout)
void AutomaticPurchase::HandleRestrictedWeapon( ) {
	const auto pLocal = C_CSPlayer::GetLocalPlayer( );
	if( !pLocal )
		return;

	if( !g_pEngine->IsInGame( ) || !g_pEngine->IsConnected( ) )
		return;

	if( !g_Vars.misc.autobuy_enabled )
		return;

	// we arent buying a (potentially) restriced primary weapon
	if( g_Vars.misc.autobuy_first_weapon < 2 )
		return;

	UpdateQueuedCommands( );

	return;

	static int nSentCommands = 0;

	if( !g_pGameRules.IsValid( ) )
		return;

	const float m_flRestartRoundTime = g_pGameRules->m_flRestartRoundTime( );
	if( m_flRestartRoundTime != 0.f && g_pGlobalVars->curtime >= m_flRestartRoundTime ) {
		// 10 TOLERANCE WHAT THEE FUCUUUUUK ( vouched by destiny )
		const int nMaximumCommands = ( g_pCVar->FindVar( XorStr( "sv_quota_stringcmdspersecond" ) )->GetInt( ) - m_iQueuedUpCommands ) - 10;

		if( nSentCommands < nMaximumCommands ) {
			if( g_Vars.misc.autobuy_first_weapon == 3 )
				g_pEngine->ClientCmd( XorStr( "buy awp" ) );
			else
				g_pEngine->ClientCmd( XorStr( "buy ssg08" ) );

			nSentCommands = nSentCommands + 1;
			//printf( XorStr( "attempting to buy weapon at frame: %i\n" ), m_pGlobalVars->framecount );
		}
	}
	else {
		nSentCommands = 0;
	}

	// reset after everything has been called :p
	m_iQueuedUpCommands = 0;
}