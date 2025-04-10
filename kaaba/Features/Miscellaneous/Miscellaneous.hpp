#pragma once
#include "../../source.hpp"

class Miscellaneous {
public:
	float m_flThirdpersonTransparency;

	void ThirdPerson( );
	void Modulation( );
	void SkyBoxChanger( );
	void PreserveKillfeed( );
	void RemoveSmoke( );
	void ForceCrosshair( );
	void OnSendDatagram( INetChannel* chan );
	void OverrideFOV( CViewSetup *vsView );
private:
	Color_f m_WallsColor = Color_f( 1.0f, 1.0f, 1.0f, 1.0f );
	Color_f m_PropsColor = Color_f( 1.0f, 1.0f, 1.0f, 1.0f );
public:
	bool m_bHoldChatHostage = false;
};

extern Miscellaneous g_Misc;