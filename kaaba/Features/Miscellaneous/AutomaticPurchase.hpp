#pragma once
#include "../../SDK/sdk.hpp"

class AutomaticPurchase {
	int m_iQueuedUpCommands;
public:
	void UpdateQueuedCommands( );
	void GameEvent( );
	void HandleRestrictedWeapon( );
};

extern AutomaticPurchase g_AutomaticPurchase;