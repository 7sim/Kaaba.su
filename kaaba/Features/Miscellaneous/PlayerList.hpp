#pragma once
#include "../../SDK/sdk.hpp"
#include <unordered_map>

class PlayerList {
public:
	struct PlayerListInfo_t {
		// pre-defined, don't modify
		int m_nPlayerIndex;
		std::string m_szPlayerName;

		// regular vars, free to modify
		bool m_bAddToWhitelist = false;
		bool m_bDisableVisuals = false;
		bool m_bHighPriority = false;

		bool m_bForcePitch = false;
		int m_iForceYaw = 0;

		float m_flForcedPitch = 0.f;
		float m_flForcedYaw = 0.f;

		bool m_bDisableResolver = false;
		bool m_bDisableBodyPred = false;
		
		bool m_bEdgeCorrection = false;
		int m_iOverridePreferBaim = 0;
		int m_iOverrideAccuracyBoost = 0;

		int m_iSelectedRequest = 0;
	};

	std::unordered_map< __int64, PlayerListInfo_t > m_vecPlayerData;
	std::unordered_map< __int64, PlayerListInfo_t > GetPlayerData( );

	void UpdatePlayerList( );
	void ClearPlayerData( );

	std::vector<std::string> GetConnectedPlayers( );

	// only use for comparison purposes
	inline PlayerListInfo_t GetSettings( __int64 ulSteamID ) {
		if( m_vecPlayerData.empty( ) )
			return { };

		if( m_vecPlayerData.find( ulSteamID ) == m_vecPlayerData.end( ) )
			return { };

		return m_vecPlayerData.at( ulSteamID );
	}
};

extern PlayerList g_PlayerList;