#include "PlayerList.hpp"

#include "../../SDK/variables.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"

PlayerList g_PlayerList;

void PlayerList::UpdatePlayerList( ) {
	// whoops
	if( !g_pPlayerResource.IsValid( ) || !( *g_pPlayerResource.Xor( ) ) ) {
		ClearPlayerData( );
		return;
	}

	if( !g_pEngine->IsConnected( ) || !g_pEngine->IsInGame( ) ) {
		ClearPlayerData( );
		return;
	}

	// erase outdated playerlist entries
	for( auto a = m_vecPlayerData.begin( ); a != m_vecPlayerData.end( );) {
		auto pEntity = ( C_CSPlayer * )g_pEntityList->GetClientEntity( a->second.m_nPlayerIndex );

		if( !pEntity ) {
			a = m_vecPlayerData.erase( a );
			continue;
		}

		// player isnt connected, can't do shit on him
		if( !( *g_pPlayerResource.Xor( ) )->GetConnected( a->second.m_nPlayerIndex ) ) {
			// sometimes the check above fails, let's make sure
			// that source engine released this player and he really is gone
			if( !pEntity ) {
				a = m_vecPlayerData.erase( a );
				continue;
			}
		}

		// go to next entry
		a = next( a );
	}

	// update player list
	for( int i = 1; i <= g_pGlobalVars->maxClients; ++i ) {
		auto pEntity = ( C_CSPlayer * )g_pEntityList->GetClientEntity( i );
		if( !pEntity )
			continue;

		// don't wanna plist ourselves kek
		if( i == g_pEngine->GetLocalPlayer( ) )
			continue;

		// we need their name bruh
		player_info_t info;
		if( !g_pEngine->GetPlayerInfo( i, &info ) )
			continue;

		// i mean, come on
		if( info.ishltv )
			continue;

		// setup this entry
		PlayerListInfo_t uEntry;
		uEntry.m_nPlayerIndex = i;
		uEntry.m_szPlayerName = info.szName;

		// epic (handle bots separately)
		const __int64 ulSteamId = info.fakeplayer ? info.iSteamID : info.steamID64;
		if( m_vecPlayerData.find( ulSteamId ) == m_vecPlayerData.end( ) ) {
			m_vecPlayerData.insert( { ulSteamId, uEntry } );
		}
	}
}

std::unordered_map< __int64, PlayerList::PlayerListInfo_t > PlayerList::GetPlayerData( ) {
	if( m_vecPlayerData.empty( ) )
		return { };

	return m_vecPlayerData;
}

void PlayerList::ClearPlayerData( ) {
	if( m_vecPlayerData.empty( ) )
		return;

	m_vecPlayerData.clear( );
}

std::vector<std::string> PlayerList::GetConnectedPlayers( ) {
	if( m_vecPlayerData.empty( ) )
		return { };

	std::vector<std::string> vecPlayers;

	for( auto a : m_vecPlayerData ) {
		if( hash_32_fnv1a( a.second.m_szPlayerName.data( ) ) == hash_32_fnv1a_const( XorStr( "Deez1337Nuts" ) ) )
			continue;

		vecPlayers.push_back( a.second.m_szPlayerName );
	}

	return vecPlayers;
}