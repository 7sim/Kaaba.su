#pragma once
#include "../../SDK/sdk.hpp"

class  EventLogger {
public:
	void Main( );
	void PushEvent( std::string prefix, std::string msg, Color prefix_color, Color message_color, float time = 5.f, bool visualise = true );

	EventLogger( ) { };
	~EventLogger( ) { };
};

extern EventLogger g_EventLog;