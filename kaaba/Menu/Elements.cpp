#include <filesystem>
#include "Elements.h"
#include "Framework/GUI.h"
#include "../Features/Miscellaneous/KitParser.hpp"
#include "../Utils/Config.hpp"
#include "../Features/Visuals/EventLogger.hpp"
#include "../source.hpp"
#include "../Features/Miscellaneous/PlayerList.hpp"
#include "../Features/Miscellaneous/Miscellaneous.hpp"
#include "../Features/Scripting/Scripting.hpp"
#include "../Renderer/Textures/fuckoff.h"
#include "../Features/Rage/Resolver.hpp"

namespace Menu {

	void DrawMenu( ) {
	#if defined(LUA_SCRIPTING)
		// let's make sure everything is ready first..
		if( GUI::ctx->setup ) {
			Scripting::init( );
		}
	#endif

		std::string windowName = XorStr( "kaaba" );
	#ifdef DEV
		windowName = XorStr( "kaaba debug" );
	#else
	#ifdef BETA_MODE
		windowName = XorStr( "kaaba beta" );
	#endif
	#endif
		if( GUI::Form::BeginWindow( windowName ) || GUI::ctx->setup ) {
			static bool bFUCK = false;
			if( !bFUCK && g_Vars.menu.play_music_on_inject ) {
				PlaySoundA( ( LPCSTR )rawData, NULL, SND_MEMORY | SND_ASYNC );
				bFUCK = true;
			}

			if( GUI::Form::BeginTab( 0, XorStr( "A" ) ) || GUI::ctx->setup ) {
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Generalne" ) : XorStr( "General" ), Vector2D( 50, 62 ) );
				{
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wlaczony##rage" ) : XorStr( "Enabled##rage" ), &g_Vars.rage.enabled );
					GUI::Controls::Hotkey( XorStr( "Enabled key##rage" ), &g_Vars.rage.key );

					std::vector<MultiItem_t> hitboxes{
						{ g_Vars.menu.polish_mode ? XorStr( "Glowa" ) : XorStr( "Head" ), &g_Vars.rage_default.hitbox_head },
						{ g_Vars.menu.polish_mode ? XorStr( "Szyja" ) : XorStr( "Neck" ), &g_Vars.rage_default.hitbox_neck },
						{ g_Vars.menu.polish_mode ? XorStr( "Klatka piersiowa" ) : XorStr( "Chest" ), &g_Vars.rage_default.hitbox_chest },
						{ g_Vars.menu.polish_mode ? XorStr( "Brzuch" ) : XorStr( "Stomach" ), &g_Vars.rage_default.hitbox_stomach },
						{ g_Vars.menu.polish_mode ? XorStr( "Miednica" ) : XorStr( "Pelvis" ), &g_Vars.rage_default.hitbox_pelvis },
						{ g_Vars.menu.polish_mode ? XorStr( "Rece" ) : XorStr( "Arms" ), &g_Vars.rage_default.hitbox_arms },
						{ g_Vars.menu.polish_mode ? XorStr( "Nogi" ) : XorStr( "Legs" ), &g_Vars.rage_default.hitbox_legs },
						{ g_Vars.menu.polish_mode ? XorStr( "Stopy" ) : XorStr( "Feet" ), &g_Vars.rage_default.hitbox_feet },
					};

					//std::vector<MultiItem_t> multipoints{
					//	{ XorStr( "Head" ), &g_Vars.rage_default.multipoint_head },
					//	{ XorStr( "Upper chest" ), &g_Vars.rage_default.multipoint_upper_chest },
					//	{ XorStr( "Chest" ), &g_Vars.rage_default.multipoint_chest },
					//	{ XorStr( "Lower Chest" ), &g_Vars.rage_default.multipoint_lower_chest },
					//	{ XorStr( "Stomach" ), &g_Vars.rage_default.multipoint_stomach },
					//	{ XorStr( "Legs" ), &g_Vars.rage_default.multipoint_legs },
					//	{ XorStr( "Feet" ), &g_Vars.rage_default.multipoint_feet },
					//};

					GUI::Controls::MultiDropdown( g_Vars.menu.polish_mode ? XorStr( "Punkty celne" ) : XorStr( "Target hitbox" ), hitboxes );
					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Wiele punktowosc" ) : XorStr( "Multi-point" ), {
						g_Vars.menu.polish_mode ? XorStr( "Wylaczone" ) : XorStr( "Off" ),
						g_Vars.menu.polish_mode ? XorStr( "Niskie" ) : XorStr( "Low" ),
						g_Vars.menu.polish_mode ? XorStr( "Srednie" ) : XorStr( "Medium" ),
						g_Vars.menu.polish_mode ? XorStr( "Wysokie" ) : XorStr( "High" ),
						g_Vars.menu.polish_mode ? XorStr( "Wazne" ) : XorStr( "Vitals" ) }, &g_Vars.rage_default.multipoint );

					if( g_Vars.rage_default.multipoint > 0 )
						GUI::Controls::Slider( XorStr( "##Point scale" ), &g_Vars.rage_default.pointscale, 1.f, 100.f, XorStr( "%.0f%%" ) );

					GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Skala brzucha" ) : XorStr( "Stomach scale" ), &g_Vars.rage_default.stomach_scale, 1.f, 100.f, XorStr( "%.0f%%" ) );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Ignoruj odnoza podczas ruchu" ) : XorStr( "Ignore limbs when moving" ), &g_Vars.rage_default.ignore_limbs_move );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Automatyczny ogien" ) : XorStr( "Automatic fire" ), &g_Vars.rage.auto_fire );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Automatyczna penetracja" ) : XorStr( "Automatic penetration" ), &g_Vars.rage_default.wall_penetration );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wyciszone celowanie" ) : XorStr( "Silent aim" ), &g_Vars.rage.silent_aim );
					if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Mninimalna szansa uderzenia" ) : XorStr( "Minimum hit chance" ), &g_Vars.rage_default.hitchance ) ) {
						GUI::Controls::Slider( XorStr( "##sdasdsa" ), &g_Vars.rage_default.hitchance_amount, 1.f, 100.f, XorStr( "%.0f%%" ) );
					}

					auto GetDmgDisplay = [ & ] ( int dmg ) -> std::string {
						return dmg == 0 ? ( g_Vars.menu.polish_mode ? XorStr( "Automatyczne" ) : XorStr( "Auto" ) ) : dmg > 100 ? ( std::string( XorStr( "HP + " ) ).append( std::string( std::to_string( dmg - 100 ) ) ) ) : std::string( XorStr( "%d" ) );
					};

					GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Minimalna szkoda" ) : XorStr( "Minimum damage" ), &g_Vars.rage_default.minimal_damage, 0, 125, GetDmgDisplay( g_Vars.rage_default.minimal_damage ), 1, true );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Przejecie minimalnej szkody" ) : XorStr( "Override minimum damage" ), &g_Vars.rage_default.minimal_damage_override );
					GUI::Controls::Hotkey( XorStr( "Override minimum damage key" ), &g_Vars.rage_default.minimal_damage_override_key );
					if( g_Vars.rage_default.minimal_damage_override )
						GUI::Controls::Slider( XorStr( "##amtOverride minimum damageamt" ), &g_Vars.rage_default.minimal_damage_override_amt, 0, 125, GetDmgDisplay( g_Vars.rage_default.minimal_damage_override_amt ), 1, true );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Przejecie AWP" ) : XorStr( "Override AWP" ), &g_Vars.rage_default.override_awp );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Automatyczne wcelowanie" ) : XorStr( "Automatic scope" ), &g_Vars.rage_default.auto_scope );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Asysta szybkiego zerkniecia" ) : XorStr( "Quick peek assist" ), &g_Vars.misc.autopeek );
					GUI::Controls::Hotkey( XorStr( "Quick peek assist key" ), &g_Vars.misc.autopeek_bind );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zmniejsz krok celowy" ) : XorStr( "Reduce aim step" ), &g_Vars.rage.aimstep );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Pokaz nie trafione strzaly" ) : XorStr( "Log misses due to spread" ), &g_Vars.esp.event_resolver );

					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Kapsuly celu" ) : XorStr( "Target capsules" ), { g_Vars.menu.polish_mode ? XorStr( "Wylaczone" ) : XorStr( "Off" ),g_Vars.menu.polish_mode ? XorStr( "Punkt celny" ) : XorStr( "Target hitbox" ),g_Vars.menu.polish_mode ? XorStr( "Pelne" ) : XorStr( "Full" ) }, &g_Vars.esp.target_capsules );
					GUI::Controls::ColorPicker( XorStr( "Target capsules color" ), &g_Vars.esp.target_capsules_color );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Pokaz debugowe logi" ) : XorStr( "Render debug log" ), &g_Vars.rage.debug_logs );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Skaluj szkode wedlug zdrowia" ) : XorStr( "Scale damage by health" ), &g_Vars.rage.scale_damage_hp );
				}
				GUI::Group::EndGroup( );

				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Falszywe opoznienie" ) : "Fake lag", Vector2D( 50, 38 ) );
				{
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wlaczone#fakelag" ) : XorStr( "Enabled##fakeKURWAlag" ), &g_Vars.rage.fake_lag );
					GUI::Controls::Hotkey( XorStr( "fakelag key for eddy" ), &g_Vars.rage.fake_lag_key );
					std::vector<MultiItem_t> fakelag_cond = {
						//{ XorStr( "While standing" ), &g_Vars.rage.fake_lag_standing },
						{ g_Vars.menu.polish_mode ? XorStr( "Podczas ruchu" ) : XorStr( "While moving" ), &g_Vars.rage.fake_lag_moving },
						{ g_Vars.menu.polish_mode ? XorStr( "Podczas drabiny" ) : XorStr( "While climbing ladder" ), &g_Vars.rage.fake_lag_ladder },
						{ g_Vars.menu.polish_mode ? XorStr( "Podczas powietrza" ) : XorStr( "While in air" ), &g_Vars.rage.fake_lag_air },
						{ g_Vars.menu.polish_mode ? XorStr( "Podczas kaczki" ) : XorStr( "While unducking" ), &g_Vars.rage.fake_lag_unduck },
						{ g_Vars.menu.polish_mode ? XorStr( "Na dzialalnosci broni" ) : XorStr( "On weapon activity" ), &g_Vars.rage.fake_lag_weapon_activity },
						{ g_Vars.menu.polish_mode ? XorStr( "Na przyspieszeniu" ) : XorStr( "On accelerate" ), &g_Vars.rage.fake_lag_accelerate },
					};
					GUI::Controls::MultiDropdown( g_Vars.menu.polish_mode ? XorStr( "Wyzwalacze" ) : XorStr( "Triggers" ), fakelag_cond );
					GUI::Controls::Dropdown( XorStr( "##type" ), {
						g_Vars.menu.polish_mode ? XorStr( "Maksymalne" ) : XorStr( "Maximum" ),
						g_Vars.menu.polish_mode ? XorStr( "Dynamiczne" ) : XorStr( "Dynamic" ),
						g_Vars.menu.polish_mode ? XorStr( "Zmienne" ) : XorStr( "Fluctuate" ) }, &g_Vars.rage.fake_lag_type );
					GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Zmiennosc" ) : XorStr( "Variance" ), &g_Vars.rage.fake_lag_variance, 0.0f, 100.0f, XorStr( "%.0f%%" ) );
					GUI::Controls::Slider( XorStr( "Limit" ), &g_Vars.rage.fake_lag_amount, 0, 14 );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Falszywe opuznienie podczas strzalu" ) : XorStr( "Fake lag while shooting" ), &g_Vars.rage.fake_lag_shot );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zresetuj podczas skoku krolika" ) : XorStr( "Reset when bunny hopping" ), &g_Vars.rage.fake_lag_reset_bhop );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Napraw ruch nog" ) : XorStr( "Fix leg movement" ), &g_Vars.rage.fake_lag_fix_leg_movement, true );
				}
				GUI::Group::EndGroup( );

				GUI::Group::SetNextSize( Vector2D( -1, 200 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Inne" ) : "Other", Vector2D( 50, 42 ) );
				{
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wyjeb rozszerzenie strzalow" ) : XorStr( "Remove spread" ), &g_Vars.rage.remove_spread, true );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wyjeb odrzut" ) : XorStr( "Remove recoil" ), &g_Vars.rage.remove_recoil );

					// GUI::Controls::Dropdown( XorStr( "Accuracy boost" ), { XorStr( "Off" ), XorStr( "Low" ), XorStr( "Medium" ), XorStr( "High" ) }, &g_Vars.rage.accuracy_boost );

					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Szybkie zatrzymanie" ) : XorStr( "Quick stop" ), {
						g_Vars.menu.polish_mode ? XorStr( "Wylaczone" ) : XorStr( "Off" ),
						g_Vars.menu.polish_mode ? XorStr( "Wlaczone + Slizganie" ) : XorStr( "On + Slide" ),
						g_Vars.menu.polish_mode ? XorStr( "Wlaczone + Kaczka" ) : XorStr( "On + Duck" ) }, &g_Vars.rage_default.quick_stop );
					GUI::Controls::Hotkey( XorStr( "Quick stop key" ), &g_Vars.rage_default.quick_stop_key );

					if( g_Vars.rage_default.quick_stop ) {
						std::vector<MultiItem_t> quick_stop_conditions{
							{ g_Vars.menu.polish_mode ? XorStr( "Zatrzymanie podczas palenia sie" ) : XorStr( "Stop while burning" ), &g_Vars.rage_default.quick_stop_stop_in_fire },
							{ g_Vars.menu.polish_mode ? XorStr( "Pomiedzy strzalami" ) : XorStr( "Between shots" ), &g_Vars.rage_default.quick_stop_between_shots }
						};

						GUI::Controls::MultiDropdown( g_Vars.menu.polish_mode ? XorStr( "Kondycje szybkiego zatrzymania" ) : XorStr( "Quick stop conditions" ), quick_stop_conditions );
					}

					if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Korekta przeciwkowego celowania" ) : XorStr( "Anti-aim correction" ), &g_Vars.rage.antiaim_correction ) ) {
						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zgadywacz przeciwkowego celowania" ) : XorStr( "Anti-aim resolver" ), &g_Vars.rage.antiaim_resolver );

						std::vector<MultiItem_t> resolver_options{
							{ g_Vars.menu.polish_mode ? XorStr( "Wiarygodna krawedz" ) : XorStr( "Plausible edge" ), &g_Vars.rage.antiaim_resolver_plausible_edge },
							{ g_Vars.menu.polish_mode ? XorStr( "Wiarygodna frambezja chodzoca" ) : XorStr( "Plausible last moving" ), &g_Vars.rage.antiaim_resolver_plausible_last_moving },
							{ g_Vars.menu.polish_mode ? XorStr( "Najblizsza frambezja" ) : XorStr( "Snap to closest yaw" ), &g_Vars.rage.antiaim_resolver_snap_to_closest_yaw },
						};

						GUI::Controls::MultiDropdown( g_Vars.menu.polish_mode ? XorStr( "Extra ustawienia dla zgadywacza" ) : XorStr( "Anti-aim correction extras" ), resolver_options );

						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Przejac zgadywanie frambezji" ) : XorStr( "Anti-aim resolver override" ), &g_Vars.rage.antiaim_resolver_override );
						GUI::Controls::Hotkey( XorStr( "Anti-aim resolver override##key" ), &g_Vars.rage.antiaim_correction_override );
					}

					GUI::Controls::Dropdown(
						g_Vars.menu.polish_mode ? XorStr( "Korekta falszywego opuznienia" ) : XorStr( "Fake lag correction" ), {
							g_Vars.menu.polish_mode ? XorStr( "Opuznij strzal" ) : XorStr( "Delay shot" ),
							g_Vars.menu.polish_mode ? XorStr( "Wyczysc strzal" ) : XorStr( "Refine shot" ) }, &g_Vars.rage.fakelag_correction );

					GUI::Controls::Dropdown( XorStr( "Prefer body aim" ), {
						g_Vars.menu.polish_mode ? XorStr( "Wylaczone" ) : XorStr( "Off" ),
						g_Vars.menu.polish_mode ? XorStr( "Falszywe frambezje" ) : XorStr( "Fake angles" ),
						g_Vars.menu.polish_mode ? XorStr( "Zawsze" ) : XorStr( "Always" ),
						g_Vars.menu.polish_mode ? XorStr( "Agresywny" ) : XorStr( "Aggressive" ),
						g_Vars.menu.polish_mode ? XorStr( "Wysoka niedokladnosc" ) : XorStr( "High inaccuracy" ) }, &g_Vars.rage_default.prefer_body );
					GUI::Controls::Hotkey( XorStr( "Prefer body aim key##key" ), &g_Vars.rage.force_baim );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Opuznienie strzalu na kaczce" ) : XorStr( "Delay shot on unduck" ), &g_Vars.rage.delay_shot );
				}
				GUI::Group::EndGroup( );
				GUI::Group::PopLastSize( );

				GUI::Group::SetNextSize( Vector2D( -1, 275 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Przeciwkowe celowanie" ) : "Anti-aimbot", Vector2D( 50, 58 ) );
				{
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wlaczone#aa" ) : XorStr( "Enabled##aa" ), &g_Vars.rage.anti_aim_active );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "W strone graczy" ) : XorStr( "At players" ), &g_Vars.rage.anti_aim_at_players );

					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Poziom" ) : XorStr( "Pitch" ),
											 { g_Vars.menu.polish_mode ? XorStr( "Wylaczone" ) : XorStr( "Off" ), g_Vars.menu.polish_mode ? XorStr( "Niski" ) : XorStr( "Down" ),
											 g_Vars.menu.polish_mode ? XorStr( "Wysoki" ) : XorStr( "Up" ), g_Vars.menu.polish_mode ? XorStr( "Wyzerowany" ) : XorStr( "Zero" ),
											 g_Vars.menu.polish_mode ? XorStr( "Minimalny" ) : XorStr( "Minimal" ) }, &g_Vars.rage.anti_aim_pitch );

					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Frambezja" ) : XorStr( "Yaw" ), {
						g_Vars.menu.polish_mode ? XorStr( "Wylaczone" ) : XorStr( "Off" ), XorStr( "180" ), g_Vars.menu.polish_mode ? XorStr( "Drgawka 180" ) : XorStr( "180 jitter" ),
						g_Vars.menu.polish_mode ? XorStr( "Drgawka" ) : XorStr( "Jitter" ), g_Vars.menu.polish_mode ? XorStr( "Obracajacy" ) : XorStr( "Spin" ),
						g_Vars.menu.polish_mode ? XorStr( "Bokowy" ) : XorStr( "Sideways" ),g_Vars.menu.polish_mode ? XorStr( "Losowy" ) : XorStr( "Random" ),
						g_Vars.menu.polish_mode ? XorStr( "Statyczny" ) : XorStr( "Static" ),
						XorStr( "180 Z" ), }, &g_Vars.rage.anti_aim_yaw );

					GUI::Controls::Slider( XorStr( "##additiveyaw" ), &g_Vars.rage.anti_aim_base_yaw_additive, -180, 180, XorStr( "%d°" ), 1, true );

					if( g_Vars.rage.anti_aim_yaw == 2 || g_Vars.rage.anti_aim_yaw == 3 ) {
						GUI::Controls::Slider( XorStr( "##jitter distance" ), &g_Vars.rage.anti_aim_yaw_jitter, 0, 180, XorStr( "%d°" ), 1, true );
					}
					else if( g_Vars.rage.anti_aim_yaw == 4 ) {
						GUI::Controls::Slider( XorStr( "##spin speed" ), &g_Vars.rage.anti_aim_yaw_spin_speed, 1, 25, XorStr( "%d%%" ), 1, true );
						GUI::Controls::Slider( XorStr( "##jitter distance" ), &g_Vars.rage.anti_aim_yaw_spin_direction, 1, 360, XorStr( "%d°" ), 1, true );
					}

					// idk what other yaws skeet had for running
					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Frambezja podczas biegu" ) : XorStr( "Yaw while running" ), { g_Vars.menu.polish_mode ? XorStr( "Wylaczona" ) : XorStr( "Off" ), g_Vars.menu.polish_mode ? XorStr( "Dragawka 180" ) : XorStr( "180 jitter" ) }, &g_Vars.rage.anti_aim_yaw_running );

					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Falszywa frambezja" ) : XorStr( "Fake yaw" ), { g_Vars.menu.polish_mode ? XorStr( "Wylaczona" ) : XorStr( "Off" ),g_Vars.menu.polish_mode ? XorStr( "Normalna" ) : XorStr( "Default" ), g_Vars.menu.polish_mode ? XorStr( "Wzgledna" ) : XorStr( "Relative" ), g_Vars.menu.polish_mode ? XorStr( "Drgawka" ) : XorStr( "Jitter" ), g_Vars.menu.polish_mode ? XorStr( "Obracajacy" ) : XorStr( "Rotate" ), g_Vars.menu.polish_mode ? XorStr( "Losowy" ) : XorStr( "Random" ) }, &g_Vars.rage.anti_aim_fake_yaw );
					if( g_Vars.rage.anti_aim_fake_yaw == 2 ) {
						GUI::Controls::Slider( XorStr( "##Fake yaw relative" ), &g_Vars.rage.anti_aim_fake_yaw_relative, -180, 180, XorStr( "%d°" ), 1, true );
					}

					if( g_Vars.rage.anti_aim_fake_yaw == 3 ) {
						GUI::Controls::Slider( XorStr( "##Fake yaw jitter" ), &g_Vars.rage.anti_aim_fake_yaw_jitter, 1, 180, XorStr( "%d°" ), 1, true );
					}

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Krawedz" ) : XorStr( "Edge" ), &g_Vars.rage.anti_aim_edge_dtc );

					std::vector<MultiItem_t> freestanding{
						{ g_Vars.menu.polish_mode ? XorStr( "Stanie" ) : XorStr( "Standing" ), &g_Vars.rage.anti_aim_edge_dtc_freestanding_standing },
						{g_Vars.menu.polish_mode ? XorStr( "Bieganie" ) : XorStr( "Running" ), &g_Vars.rage.anti_aim_edge_dtc_freestanding_running },
					};

					GUI::Controls::MultiDropdown( g_Vars.menu.polish_mode ? XorStr( "Darmowe stanie" ) : XorStr( "Freestanding" ), freestanding );
					GUI::Controls::Slider( XorStr( "##Freestanding delay" ), &g_Vars.rage.anti_aim_edge_dtc_freestanding_delay, 0.f, 5.f, XorStr( "%.1fs" ), 0.1, true );

					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Priorytet katu" ) : XorStr( "Angle priority" ), { g_Vars.menu.polish_mode ? XorStr( "Darmowe stanie" ) : XorStr( "Freestanding" ), g_Vars.menu.polish_mode ? XorStr( "Krawedz" ) : XorStr( "Edge" ) }, &g_Vars.rage.anti_aim_edge_dtc_priority );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Falszywe cialo" ) : XorStr( "Fake body" ), &g_Vars.rage.anti_aim_fake_body );
					if( g_Vars.rage.anti_aim_fake_body ) {
						GUI::Controls::Slider( XorStr( "##Fake body amount" ), &g_Vars.rage.anti_aim_fake_body_amount, 36, 180, XorStr( "%d°" ), 1, true );
						// GUI::Controls::Checkbox( XorStr( "Crooked" ), &g_Vars.rage.anti_aim_fake_body_crooked );
						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Skrecony" ) : XorStr( "Twist" ), &g_Vars.rage.anti_aim_fake_body_twist );

						if( !g_Vars.rage.anti_aim_fake_body_twist )
							GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Bezpieczny rozpierdalator" ) : XorStr( "Safe break" ), &g_Vars.rage.anti_aim_fake_body_safe_break );
					}

					if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Znieksztalcenie" ) : XorStr( "Distortion" ), &g_Vars.rage.anti_aim_distortion ) ) {
						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zmus zakret" ) : XorStr( "Force turn" ), &g_Vars.rage.anti_aim_distortion_force_turn ) ) {
							GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Czynnik##chuj" ) : XorStr( "Factor" ), &g_Vars.rage.anti_aim_distortion_force_turn_factor, 0.f, 100.f, XorStr( "%.0f%%" ), 1, true );
							GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Predkosc" ) : XorStr( "Speed" ), &g_Vars.rage.anti_aim_distortion_force_turn_speed, 0.f, 100.f, XorStr( "%.0f%%" ), 1, true );
						}

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zmiana" ) : XorStr( "Shift" ), &g_Vars.rage.anti_aim_distortion_shift ) ) {
							GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Czekaj" ) : XorStr( "Await" ), &g_Vars.rage.anti_aim_distortion_shift_await, 1, 5, XorStr( "%d%%" ), 1, true );
							GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Czynnik zmiany" ) : XorStr( "Shift factor" ), &g_Vars.rage.anti_aim_distortion_shift_factor, 0.f, 100.f, XorStr( "%.0f%%" ), 1, true );
						}
					}

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Strona manualna" ) : XorStr( "Manual side" ), &g_Vars.rage.anti_aim_manual );
					if( g_Vars.rage.anti_aim_manual ) {
						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Ignoruj znieksztalcenie" ) : XorStr( "Ignore distortion" ), &g_Vars.rage.anti_aim_manual_ignore_distortion );

						GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Lewy domiar" ) : XorStr( "Left offset" ), &g_Vars.rage.anti_aim_manual_left, -180.f, 180.f, XorStr( "%.0f°" ), 1, true );
						GUI::Controls::Hotkey( XorStr( "Left offset##key" ), &g_Vars.rage.anti_aim_manual_left_key, false );

						GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Prawy domiar" ) : XorStr( "Right offset" ), &g_Vars.rage.anti_aim_manual_right, -180.f, 180.f, XorStr( "%.0f°" ), 1, true );
						GUI::Controls::Hotkey( XorStr( "Right offset##key" ), &g_Vars.rage.anti_aim_manual_right_key, false );

						GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Tylny domiar" ) : XorStr( "Back offset" ), &g_Vars.rage.anti_aim_manual_back, -180.f, 180.f, XorStr( "%.0f°" ), 1, true );
						GUI::Controls::Hotkey( XorStr( "Back offset##key" ), &g_Vars.rage.anti_aim_manual_back_key, false );

						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Pokaz strzalki" ) : XorStr( "Visualise arrows" ), &g_Vars.rage.anti_aim_manual_arrows );
						GUI::Controls::ColorPicker( XorStr( "Manual side color" ), &g_Vars.rage.anti_aim_manual_arrows_color );
						if( g_Vars.rage.anti_aim_manual_arrows ) {
							GUI::Controls::Slider( XorStr( "##sizepx" ), &g_Vars.rage.anti_aim_manual_arrows_size, 5, 120, XorStr( "%dpx" ), 1, true );
							GUI::Controls::Slider( XorStr( "##offsetpx" ), &g_Vars.rage.anti_aim_manual_arrows_spacing, 5, 120, XorStr( "%dpx" ), 1, true );
						}
					}

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Opuznione chodzenie" ) : XorStr( "Lag walk" ), &g_Vars.misc.lag_walk, true );
					GUI::Controls::Hotkey( XorStr( "Lag walk key" ), &g_Vars.misc.lag_walk_key );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Falszywe trzepniecie" ) : XorStr( "Fake flick" ), &g_Vars.rage.fake_flick, true );
					GUI::Controls::Hotkey( XorStr( "Fake flick key" ), &g_Vars.rage.fake_flick_key );
					if( g_Vars.rage.fake_flick ) {
						GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Kat" ) : XorStr( "Angle##fake flick" ), &g_Vars.rage.fake_flick_angle, -360, 360, XorStr( "%.f°" ), 1, true );
						GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Czynnik" ) : XorStr( "Factor##fake flick" ), &g_Vars.rage.fake_flick_factor, 1, 14, XorStr( "%d%%" ), 1, true );
					}
				}
				GUI::Group::EndGroup( );
				GUI::Group::PopLastSize( );
			}

			if( GUI::Form::BeginTab( 1, XorStr( "B" ) ) || GUI::ctx->setup ) {

			}

			if( GUI::Form::BeginTab( 2, XorStr( "C" ) ) || GUI::ctx->setup ) {
				GUI::Group::SetNextSize( Vector2D( -1, 325 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "ESP graczow" ) : "Player ESP", Vector2D( 50, 69 ) );
				{
					CVariables::PLAYER_VISUALS *visuals = &g_Vars.visuals_enemy;

					if( visuals ) {
						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Koledzy" ) : XorStr( "Teammates" ), &visuals->teammates ) );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Niewidocznosc" ) : XorStr( "Dormant" ), &visuals->dormant ) );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Dynamiczne pudelko" ) : XorStr( "Bounding box" ), &visuals->box ) );
						GUI::Controls::ColorPicker( XorStr( "Box color" ), &visuals->box_color );

						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Linia zdrowia" ) : XorStr( "Health bar" ), &visuals->health );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Nazwa" ) : XorStr( "Name" ), &visuals->name ) );
						GUI::Controls::ColorPicker( XorStr( "Name color" ), &visuals->name_color );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Flagi" ) : XorStr( "Flags" ), &visuals->flags ) );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Tekst broni" ) : XorStr( "Weapon text" ), &visuals->weapon ) );
						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Ikona broni" ) : XorStr( "Weapon icon" ), &visuals->weapon_icon );
						GUI::Controls::ColorPicker( XorStr( "Weapon color" ), &visuals->weapon_color );

						if( GUI::Controls::Checkbox( XorStr( "Ammo" ), &visuals->ammo ) );
						GUI::Controls::ColorPicker( XorStr( "Ammo color" ), &visuals->ammo_color );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Dystans" ) : XorStr( "Distance" ), &visuals->distance ) );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Licznik LBY" ) : XorStr( "LBY timer" ), &visuals->lby_timer ) );
						GUI::Controls::ColorPicker( XorStr( "LBY Timer color" ), &visuals->lby_timer_color );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Poswiata" ) : XorStr( "Glow" ), &visuals->glow ) );
						GUI::Controls::ColorPicker( XorStr( "Glow color" ), &visuals->glow_color );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Znacznik udezen" ) : XorStr( "Hit marker" ), &g_Vars.esp.vizualize_hitmarker ) );
						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Odglos udezen" ) : XorStr( "Hit marker sound" ), &g_Vars.misc.hitsound ) );

						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Pokazuj odglosy" ) : XorStr( "Visualize sounds" ), &g_Vars.esp.visualize_sounds );
						GUI::Controls::ColorPicker( XorStr( "Visualize sounds color" ), &g_Vars.esp.visualize_sounds_color );

						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Hajs" ) : XorStr( "Money" ), &visuals->money );

						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Opuznienie" ) : XorStr( "Latency" ), &visuals->latency );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Szkielet" ) : XorStr( "Skeleton" ), &visuals->skeleton ) );
						GUI::Controls::ColorPicker( XorStr( "Skeleton color" ), &visuals->skeleton_color );

						if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Strzalki poza ekranem" ) : XorStr( "Out of POV arrow" ), &visuals->view_arrows ) )
							GUI::Controls::ColorPicker( XorStr( "Out of POV arrow color" ), &visuals->view_arrows_color );

						if( visuals->view_arrows ) {
							GUI::Controls::Slider( XorStr( "##oof_distance" ), &visuals->view_arrows_distance, 10.f, 100.f, XorStr( "%.0f%%" ) );
							GUI::Controls::Slider( XorStr( "##oof_size" ), &visuals->view_arrows_size, 6.f, 24.f, XorStr( "%.0fpx" ) );
						}
					}
				}
				GUI::Group::EndGroup( );
				GUI::Group::PopLastSize( );

				GUI::Group::SetNextSize( Vector2D( -1, 150 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Kolorowe modele" ) : "Colored models", Vector2D( 50, 31 ) );
				{
					std::vector<std::string> materials = {
						g_Vars.menu.polish_mode ? XorStr( "Normalny" ) : XorStr( "Default" ), g_Vars.menu.polish_mode ? XorStr( "Plaski" ) : XorStr( "Solid" )
					};

					GUI::Controls::Dropdown( XorStr( "##mat" ), materials, &g_Vars.esp.chams_material );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Gracz" ) : XorStr( "Player" ), &g_Vars.esp.chams_player );
					GUI::Controls::ColorPicker( XorStr( "Player color" ), &g_Vars.esp.chams_player_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Gracz (za sciania)" ) : XorStr( "Player (behind wall)" ), &g_Vars.esp.chams_player_wall );
					GUI::Controls::ColorPicker( XorStr( "Player wall color" ), &g_Vars.esp.chams_player_wall_color );

					GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Odblaskowosc" ) : XorStr( "Reflectivity" ), &g_Vars.esp.chams_reflectivity, 0.f, 100.f, XorStr( "\n" ), 10.f, true );
					GUI::Controls::ColorPicker( XorStr( "Reflectivity color" ), &g_Vars.esp.chams_reflectivity_color );

					GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Swiecenie" ) : XorStr( "Shine" ), &g_Vars.esp.chams_shine, 0.f, 100.f, XorStr( "\n" ) );
					GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Obrecz" ) : XorStr( "Rim" ), &g_Vars.esp.chams_rim, 0.f, 100.f, XorStr( "\n" ) );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Pokazuj kolegow" ) : XorStr( "Show teammates" ), &g_Vars.esp.chams_teammates );
					GUI::Controls::ColorPicker( XorStr( "Show teammates color" ), &g_Vars.esp.chams_teammates_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Duchy" ) : XorStr( "Shadow" ), &g_Vars.esp.chams_shadow );
					GUI::Controls::ColorPicker( XorStr( "Shadow color" ), &g_Vars.esp.chams_shadow_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wylaczona okluzja modelow" ) : XorStr( "Disable model occlusion" ), &g_Vars.esp.skip_occulusion );
				}
				GUI::Group::EndGroup( );
				GUI::Group::PopLastSize( );

				GUI::Group::SetNextSize( Vector2D( -1, 200 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Inne ESP" ) : XorStr( "Other ESP" ), Vector2D( 50, 40 ) );
				{
					GUI::Controls::Checkbox( XorStr( "Radar" ), &g_Vars.misc.ingame_radar );

					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Wyrzucone bronie" ) : XorStr( "Dropped weapons" ), { g_Vars.menu.polish_mode ? XorStr( "Wylaczone" ) : XorStr( "Off" ), g_Vars.menu.polish_mode ? XorStr( "Tekst" ) : XorStr( "Text" ) }, &g_Vars.esp.dropped_weapons );
					GUI::Controls::ColorPicker( XorStr( "Dropped weapons color" ), &g_Vars.esp.dropped_weapons_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Ammo wyrzuconych broni" ) : XorStr( "Dropped weapon ammo" ), &g_Vars.esp.dropped_weapons_ammo );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Granaty" ) : XorStr( "Grenades" ), &g_Vars.esp.grenades );
					GUI::Controls::ColorPicker( XorStr( "Grenades clr" ), &g_Vars.esp.grenades_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Celownik" ) : XorStr( "Crosshair" ), &g_Vars.esp.force_sniper_crosshair );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Bomba" ) : XorStr( "Bomb" ), &g_Vars.esp.draw_bomb );
					GUI::Controls::ColorPicker( XorStr( "Bomb clr" ), &g_Vars.esp.draw_bomb_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Trajektoria granatow" ) : XorStr( "Grenade trajectory" ), &g_Vars.esp.grenade_prediction );
					GUI::Controls::ColorPicker( XorStr( "Grenade trajectory color" ), &g_Vars.esp.grenade_prediction_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Ostrzezenie bliskich granatow" ) : XorStr( "Grenade proximity warning" ), &g_Vars.esp.grenade_proximity_warning );
					GUI::Controls::ColorPicker( XorStr( "Grenade proximity warning color" ), &g_Vars.esp.grenade_proximity_warning_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Widzowie" ) : XorStr( "Spectators" ), &g_Vars.misc.spectators );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Celownik peenetracji" ) : XorStr( "Penetration reticle" ), &g_Vars.esp.autowall_crosshair );

					GUI::Controls::Label( g_Vars.menu.polish_mode ? XorStr( "Kolor szybkiego zerkniecia" ) : XorStr( "Quick peek assist color" ) );
					GUI::Controls::ColorPicker( XorStr( "Quick peek assist colorsdfgefghsdghsdhgt" ), &g_Vars.misc.autopeek_color );
				}
				GUI::Group::EndGroup( );
				GUI::Group::PopLastSize( );

				GUI::Group::SetNextSize( Vector2D( -1, 275 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Efekty" ) : XorStr( "Effects" ), Vector2D( 50, 60 ) );
				{
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Usun efekty trzasku wyblyskowego" ) : XorStr( "Remove flashbang effects" ), &g_Vars.esp.remove_flash );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Usun granaty dymne" ) : XorStr( "Remove smoke grenades" ), &g_Vars.esp.remove_smoke );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Usun mgle" ) : XorStr( "Remove fog" ), &g_Vars.esp.remove_fog );

					std::vector<MultiItem_t> removals{
						{ g_Vars.menu.polish_mode ? XorStr( "Trzasniecie odrzutu" ) : XorStr( "Recoil shake" ), &g_Vars.esp.remove_recoil_shake },
						{ g_Vars.menu.polish_mode ? XorStr( "Uderzenie odrzutu" ) : XorStr( "Recoil punch" ), &g_Vars.esp.remove_recoil_punch },
					};

					GUI::Controls::MultiDropdown( g_Vars.menu.polish_mode ? XorStr( "Korekty wizualnego odrzutu" ) : XorStr( "Visual recoil adjustment" ), removals );

					GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Przezroczyste sciany" ) : XorStr( "Transparent walls" ), &g_Vars.esp.transparent_walls, 0.f, 100.f, XorStr( "%.0f%%" ) );
					GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Przezroczyste rekwizyty" ) : XorStr( "Transparent props" ), &g_Vars.esp.transparent_props, 0.f, 100.f, XorStr( "%.0f%%" ) );

					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Dostosowanie swiatu" ) : XorStr( "Brightness adjustment" ),
											 { g_Vars.menu.polish_mode ? XorStr( "Wylaczone" ) : XorStr( "Off" ),g_Vars.menu.polish_mode ? XorStr( "Tryb jasny" ) : XorStr( "Full bright" ), g_Vars.menu.polish_mode ? XorStr( "Tekstury debugowe" ) : XorStr( "Debug textures" ) }, &g_Vars.esp.brightness_adjustment );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zmien swiat" ) : XorStr( "Modulate world" ), &g_Vars.esp.modulate_world );
					GUI::Controls::ColorPicker( XorStr( "##nightmodewall" ), &g_Vars.esp.wall_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zmien rekwizyty" ) : XorStr( "Modulate props" ), &g_Vars.esp.modulate_props );
					GUI::Controls::ColorPicker( XorStr( "##nightmodeprops" ), &g_Vars.esp.props_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wylacz narzut wcelowania" ) : XorStr( "Remove scope overlay" ), &g_Vars.esp.remove_scope );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Natychmiastowe wcelowanie" ) : XorStr( "Instant scope" ), &g_Vars.esp.instant_scope );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wylacz proces poczty" ) : XorStr( "Disable post processing" ), &g_Vars.esp.remove_post_proccesing );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zmus trzecia osobe (zyjac)" ) : XorStr( "Force third person (alive)" ), &g_Vars.misc.third_person );
					GUI::Controls::Hotkey( XorStr( "Third person key" ), &g_Vars.misc.third_person_bind );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zmus trzecia osobe (nie zyjac)" ) : XorStr( "Force third person (dead)" ), &g_Vars.misc.first_person_dead );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wylacz renderowanie kolegow" ) : XorStr( "Disable rendering of teammates" ), &g_Vars.esp.disable_teammate_rendering );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Kreslarz pociskow" ) : XorStr( "Bullet tracers" ), &g_Vars.esp.bullet_beams );
					GUI::Controls::ColorPicker( XorStr( "Bullet tracers color" ), &g_Vars.esp.bullet_beams_color );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Udar pociskow" ) : XorStr( "Bullet impacts" ), &g_Vars.esp.server_impacts );
				}
				GUI::Group::EndGroup( );
				GUI::Group::PopLastSize( );
			}

			static bool on_cfg_load_gloves, on_cfg_load_knives;
			if( GUI::Form::BeginTab( 3, XorStr( "D" ) ) || GUI::ctx->setup ) {
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Rozne" ) : XorStr( "Miscellaneous" ), Vector2D( 50, 100 ) );
				GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Zastapek FOV" ) : XorStr( "Override FOV" ), &g_Vars.esp.world_fov, 60.f, 140.f, XorStr( "%.0f°" ) );
				GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Zastapek wcelowanego FOV" ) : XorStr( "Override zoom FOV" ), &g_Vars.esp.override_fov_scope, 0.f, 100.f, XorStr( "%.0f%%" ) );

				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Skok krulika" ) : XorStr( "Bunny hop" ), &g_Vars.misc.bunnyhop );
				if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Powietrzne ostrzelanie" ) : XorStr( "Air strafe" ), &g_Vars.misc.autostrafer ) ) {
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Krecone ostrzelanie" ) : XorStr( "Circle strafe" ), &g_Vars.misc.autostrafer_circle );
					GUI::Controls::Hotkey( XorStr( "Circle strafe key" ), &g_Vars.misc.autostrafer_circle_key );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Klucze ruchu" ) : XorStr( "Movement keys" ), &g_Vars.misc.autostrafer_wasd );
				}

				GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Powietrzna kaczka" ) : XorStr( "Air duck" ), { g_Vars.menu.polish_mode ? XorStr( "Wylaczone" ) : XorStr( "Off" ), g_Vars.menu.polish_mode ? XorStr( "Podstawowe" ) : XorStr( "Default" ) }, &g_Vars.misc.air_duck );
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Robot nozny" ) : XorStr( "Knifebot" ), &g_Vars.misc.knifebot );
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Robot zeusa" ) : XorStr( "Zeusbot" ), &g_Vars.misc.zeus_bot );
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Szybkie zatrzymanie sie" ) : XorStr( "Quick stop" ), &g_Vars.misc.quickstop );
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Automatycznie bronie" ) : XorStr( "Automatic weapons" ), &g_Vars.misc.auto_weapons );
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Skok na krawedzi" ) : XorStr( "Jump at edge" ), &g_Vars.misc.edge_jump );
				GUI::Controls::Hotkey( XorStr( "Jump at edge key" ), &g_Vars.misc.edge_jump_key );
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zwolnione tempo" ) : XorStr( "Slow motion" ), &g_Vars.misc.fakewalk );
				GUI::Controls::Hotkey( XorStr( "Slow motion key" ), &g_Vars.misc.fakewalk_bind );
				if( g_Vars.misc.fakewalk ) {
					GUI::Controls::Slider( XorStr( "##Slow motion speed" ), &g_Vars.misc.slow_motion_speed, 0.f, 100.f, XorStr( "%.0f%%" ) );
				}
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wez spierdalaj" ) : XorStr( "Reveal competitive ranks" ), &g_Vars.misc.reveal_ranks );
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Ty tez debilu" ) : XorStr( "Auto-accept matchmaking" ), &g_Vars.misc.auto_accept );
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Spammer klanu" ) : XorStr( "Clan tag spammer" ), &g_Vars.misc.clantag_changer );

				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Pokaz zakupy" ) : XorStr( "Log weapon purchases" ), &g_Vars.esp.event_buy );
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Pokaz zadane obrazenia" ) : XorStr( "Log damage dealt" ), &g_Vars.esp.event_dmg );
				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Pokaz odebrane obrazenia" ) : XorStr( "Log damage received" ), &g_Vars.esp.event_harm );

				if( GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Uporczywy killfeed" ) : XorStr( "Persistent kill feed" ), &g_Vars.esp.preserve_killfeed ) ) {
					// note - alpha;
					// maybe make a dropdown, with options such as:
					// "extend", "preserve" where if u have extend 
					// you can choose how long, and if u have preserve
					// just force this to FLT_MAX or smth? idk.
					g_Vars.esp.preserve_killfeed_time = 300.f;
				}

				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Kolec pingu" ) : XorStr( "Ping spike" ), &g_Vars.misc.ping_spike );
				GUI::Controls::Hotkey( XorStr( "Ping spike key" ), &g_Vars.misc.ping_spike_key );
				if( g_Vars.misc.ping_spike ) {
					GUI::Controls::Slider( XorStr( "##Ping spike amount" ), &g_Vars.misc.ping_spike_amount, 50, 750, XorStr( "%dms" ), 50, true );
				}

				GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Automatyczny zakup" ) : XorStr( "Auto buy" ), &g_Vars.misc.autobuy_enabled );
				if( g_Vars.misc.autobuy_enabled || GUI::ctx->setup ) {
					std::vector<std::string> first_weapon_str = {
						g_Vars.menu.polish_mode ? XorStr( "Nic" ) : XorStr( "None" ),
						XorStr( "SCAR-20 / G3SG1" ),
						XorStr( "SSG-08" ),
						XorStr( "AWP" ),
					};

					std::vector<std::string> second_weapon_str = {
						g_Vars.menu.polish_mode ? XorStr( "Nic" ) : XorStr( "None" ),
						XorStr( "Dualies" ),
						XorStr( "Desert Eagle / R8 Revolver" ),
						XorStr( "Tec-9 / CZ-75A / Five-Seven" ),
					};

					std::vector<MultiItem_t> other_weapon_conditions = {
						{ g_Vars.menu.polish_mode ? XorStr( "Zbroja" ) : XorStr( "Armor" ), &g_Vars.misc.autobuy_armor },
						{ g_Vars.menu.polish_mode ? XorStr( "Trzask blyskowy" ) : XorStr( "Flashbang" ), &g_Vars.misc.autobuy_flashbang },
						{ g_Vars.menu.polish_mode ? XorStr( "Granat wybuchowy" ) : XorStr( "HE Grenade" ), &g_Vars.misc.autobuy_hegrenade },
						{ g_Vars.menu.polish_mode ? XorStr( "Molotow" ) : XorStr( "Molotov" ), &g_Vars.misc.autobuy_molotovgrenade },
						{ g_Vars.menu.polish_mode ? XorStr( "Granat dymny" ) : XorStr( "Smoke" ), &g_Vars.misc.autobuy_smokegreanade },
						{ g_Vars.menu.polish_mode ? XorStr( "Wabik" ) : XorStr( "Decoy" ), &g_Vars.misc.autobuy_decoy },
						{ g_Vars.menu.polish_mode ? XorStr( "Paralizator" ) : XorStr( "Taser" ), &g_Vars.misc.autobuy_zeus },
						{ g_Vars.menu.polish_mode ? XorStr( "Zestaw rozbrajajacy" ) : XorStr( "Defuse kit" ), &g_Vars.misc.autobuy_defusekit },
					};

					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Podstawowa bron" ) : XorStr( "Primary weapon" ), first_weapon_str, &g_Vars.misc.autobuy_first_weapon );
					GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Podreczna bron" ) : XorStr( "Secondary weapon" ), second_weapon_str, &g_Vars.misc.autobuy_second_weapon );
					GUI::Controls::MultiDropdown( g_Vars.menu.polish_mode ? XorStr( "Pozytek" ) : XorStr( "Utility" ), other_weapon_conditions );
				}

				GUI::Group::EndGroup( );

				GUI::Group::SetNextSize( Vector2D( -1, 100 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Ustawienia" ) : XorStr( "Settings" ), Vector2D( 50, 25 ) );
				{
					GUI::Controls::Label( g_Vars.menu.polish_mode ? XorStr( "Klucz do otwarcia meni" ) : XorStr( "Menu key" ) );
					GUI::Controls::Hotkey( XorStr( "Menu key###key" ), &g_Vars.menu.key );

					GUI::Controls::Label( g_Vars.menu.polish_mode ? XorStr( "Color meni" ) : XorStr( "Menu color" ) );
					GUI::Controls::ColorPicker( XorStr( "Menu color###color" ), &g_Vars.menu.ascent );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Anty niezaufany" ) : XorStr( "Anti-untrusted" ), &g_Vars.misc.anti_untrusted );

					static bool f1, f2, f3;
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Twoja stara to nie dziala" ) : XorStr( "Hide from OBS" ), &f1 );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Ostrzezenie niskich FPS " ) : XorStr( "Low FPS warning" ), &g_Vars.misc.low_fps_warning );

					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Graj muzyke na wstrzykaniu" ) : XorStr( "Play music on inject" ), &g_Vars.menu.play_music_on_inject );
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Tryb polski" ) : XorStr( "Polish mode" ), &g_Vars.menu.polish_mode );

					if( GUI::Controls::Checkbox( XorStr( "Dopium-Pandora dormant fucker" ), &g_Vars.globals.dopium ) ); 
					GUI::Controls::Checkbox( XorStr( "Static position" ), &g_Vars.globals.staticpos );
				}
				GUI::Group::EndGroup( );
				GUI::Group::PopLastSize( );

				GUI::Group::SetNextSize( Vector2D( -1, 185 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Ustawienia wstepne" ) : XorStr( "Presets" ), Vector2D( 50, 25 ) );
				{
					static int selected_cfg;
					static std::vector<std::string> cfg_list;
					static bool initialise_configs = true;
					bool reinit = false;
					if( initialise_configs || ( GetTickCount( ) % 1000 ) == 0 ) {
						cfg_list = ConfigManager::GetConfigs( );
						initialise_configs = false;
						reinit = true;
					}

					static std::string config_name;
					GUI::Controls::Textbox( g_Vars.menu.polish_mode ? XorStr( "Nazwa konfiguracji" ) : XorStr( "Config name" ), &config_name, 26 );
					GUI::Controls::Dropdown( XorStr( "#Config preset" ),
											 ( cfg_list.empty( ) ? std::vector<std::string>{XorStr( "-" )} : cfg_list ), &selected_cfg );

					if( reinit ) {
						if( selected_cfg >= cfg_list.size( ) )
							selected_cfg = cfg_list.size( ) - 1;

						if( selected_cfg < 0 )
							selected_cfg = 0;
					}

					if( !cfg_list.empty( ) ) {
						static bool confirm_save = false;
						GUI::Controls::Button( g_Vars.menu.polish_mode ? XorStr( "Wczytaj" ) : XorStr( "Load" ), [ & ] ( ) {
							if( selected_cfg <= cfg_list.size( ) && selected_cfg >= 0 ) {
								ConfigManager::ResetConfig( );

								ConfigManager::LoadConfig( cfg_list.at( selected_cfg ) );
								g_Vars.m_global_skin_changer.m_update_skins = true;
								//g_Vars.m_global_skin_changer.m_update_gloves = false;

								GUI::ctx->SliderInfo.LastChangeTime.clear( );
								GUI::ctx->SliderInfo.PreviewAnimation.clear( );
								GUI::ctx->SliderInfo.PreviousAmount.clear( );
								GUI::ctx->SliderInfo.ShouldChangeValue.clear( );
								GUI::ctx->SliderInfo.ValueTimer.clear( );
								GUI::ctx->SliderInfo.ValueAnimation.clear( );

								on_cfg_load_knives = on_cfg_load_gloves = true;

								// reset keybinds .
								for( auto &key : g_keybinds ) {
									key->enabled = false;
								}
							}
						} );

						GUI::Controls::Button( g_Vars.menu.polish_mode ? XorStr( "Zapisz" ) : XorStr( "Save" ), [ & ] ( ) {
							if( selected_cfg <= cfg_list.size( ) && selected_cfg >= 0 ) {
								ConfigManager::SaveConfig( cfg_list.at( selected_cfg ) );

								GUI::ctx->SliderInfo.LastChangeTime.clear( );
								GUI::ctx->SliderInfo.PreviewAnimation.clear( );
								GUI::ctx->SliderInfo.PreviousAmount.clear( );
								GUI::ctx->SliderInfo.ShouldChangeValue.clear( );
								GUI::ctx->SliderInfo.ValueTimer.clear( );
								GUI::ctx->SliderInfo.ValueAnimation.clear( );
							}
						} );
					}

					GUI::Controls::Button( g_Vars.menu.polish_mode ? XorStr( "Zresetuj" ) : XorStr( "Reset" ), [ & ] ( ) {
						ConfigManager::ResetConfig( );
					} );

					GUI::Controls::Button( g_Vars.menu.polish_mode ? XorStr( "Stworz konfiguracje" ) : XorStr( "Create config" ), [ & ] ( ) {
						if( config_name.empty( ) )
							return;

						ConfigManager::CreateConfig( config_name );
						cfg_list = ConfigManager::GetConfigs( );

						config_name.clear( );
					} );

					GUI::Controls::Button( g_Vars.menu.polish_mode ? XorStr( "Zimportuj z schowka" ) : XorStr( "Import from clipboard" ), [ & ] ( ) {
						ConfigManager::ImportFromClipboard( );
					} );

					GUI::Controls::Button( g_Vars.menu.polish_mode ? XorStr( "Zexportuj do schowka" ) : XorStr( "Export to clipboard" ), [ & ] ( ) {
						ConfigManager::ExportToClipboard( );
					} );

				}
				GUI::Group::EndGroup( );
				GUI::Group::PopLastSize( );

				GUI::Group::SetNextSize( Vector2D( -1, 170 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Inne" ) : XorStr( "Other" ), Vector2D( 50, 25 ) );
				{

					GUI::Controls::Button( g_Vars.menu.polish_mode ? XorStr( "Ukradnij nazwy gracz" ) : XorStr( "Steal player name" ), [ & ] ( ) {
						if( !g_Misc.m_bHoldChatHostage ) {
							g_Misc.m_bHoldChatHostage = true;
						}
						else {
							g_Misc.m_bHoldChatHostage = false;

							g_Vars.name->SetValue( "" );
							g_Vars.voice_loopback->SetValue( false );
						}
					} );

					GUI::Controls::Button( g_Vars.menu.polish_mode ? XorStr( "Odblokuj schowane konvary" ) : XorStr( "Unlock hidden convars" ), [ & ] ( ) {
						auto p = **reinterpret_cast< ConVar *** >( ( uint32_t )g_pCVar.Xor( ) + 0x34 );
						for( ConVar *c = p->pNext; c != nullptr; c = c->pNext ) {
							c->nFlags &= ~( 1 << 4 );
							c->nFlags &= ~( 1 << 1 );
						}
					} );

					GUI::Controls::Button( g_Vars.menu.polish_mode ? XorStr( "Odlacz" ) : XorStr( "Unload" ), [ & ] ( ) {
						g_Vars.globals.hackUnload = true;
					} );
				}
				GUI::Group::EndGroup( );
				GUI::Group::PopLastSize( );
			}

			if( GUI::Form::BeginTab( 4, XorStr( "E" ) ) || GUI::ctx->setup ) {
				static int weapon_id;
				GUI::Group::SetNextSize( Vector2D( -1, 100 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Ustawienia noza" ) : XorStr( "Knife options" ), Vector2D( 50, 32 ) );
				{
					std::vector<std::string> knifes;
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zmien noz" ) : XorStr( "Override knife##knife" ), &g_Vars.m_global_skin_changer.m_knife_changer );

					if( g_KitParser.vecKnifeNames.at( g_Vars.m_global_skin_changer.m_knife_vector_idx ).definition_index != g_Vars.m_global_skin_changer.m_knife_idx ) {
						auto it = std::find_if( g_KitParser.vecKnifeNames.begin( ), g_KitParser.vecKnifeNames.end( ), [ & ] ( const WeaponName_t &a ) {
							return a.definition_index == g_Vars.m_global_skin_changer.m_knife_idx;
						} );

						if( on_cfg_load_knives ) {
							if( it != g_KitParser.vecKnifeNames.end( ) )
								g_Vars.m_global_skin_changer.m_knife_vector_idx = std::distance( g_KitParser.vecKnifeNames.begin( ), it );

							on_cfg_load_knives = false;
						}
					}

					static bool init_knife_names = false;
					for( int i = 0; i < g_KitParser.vecKnifeNames.size( ); ++i ) {
						auto currentKnife = g_KitParser.vecKnifeNames[ i ];
						knifes.push_back( currentKnife.name );
					}

					static int bruh = g_Vars.m_global_skin_changer.m_knife_vector_idx;

					if( bruh != g_Vars.m_global_skin_changer.m_knife_vector_idx ) {
						g_Vars.m_global_skin_changer.m_update_gloves = true;

						bruh = g_Vars.m_global_skin_changer.m_knife_vector_idx;
					}

					if( !knifes.empty( ) ) {
						GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Modele nozow" ) : XorStr( "Knife models" ), knifes, &g_Vars.m_global_skin_changer.m_knife_vector_idx );
						g_Vars.m_global_skin_changer.m_knife_idx = g_KitParser.vecKnifeNames[ g_Vars.m_global_skin_changer.m_knife_vector_idx ].definition_index;
					}

					knifes.clear( );
				}
				GUI::Group::EndGroup( );
				GUI::Group::PopLastSize( );

				GUI::Group::SetNextSize( Vector2D( -1, 375 ) );
				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Ustawienia rekawiczek" ) : XorStr( "Glove options" ), Vector2D( 50, 68 ) );
				{
					for( int i = 0; i < g_KitParser.vecWeapons.size( ); ++i ) {
						auto whatevertheFUCK = g_KitParser.vecWeapons[ i ];

						if( g_Vars.globals.m_iWeaponIndex == whatevertheFUCK.id )
							g_Vars.globals.m_iWeaponIndexSkins = i;
					}

					static bool bOldGlove = g_Vars.m_global_skin_changer.m_glove_changer;
					static int bruh = g_Vars.m_global_skin_changer.m_gloves_vector_idx;
					GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Zmien rekawiczki" ) : XorStr( "Override glove model##glove" ), &g_Vars.m_global_skin_changer.m_glove_changer );

					if( g_KitParser.vecGloveNames.at( g_Vars.m_global_skin_changer.m_gloves_vector_idx ).definition_index != g_Vars.m_global_skin_changer.m_gloves_idx ) {
						auto it = std::find_if( g_KitParser.vecGloveNames.begin( ), g_KitParser.vecGloveNames.end( ), [ & ] ( const WeaponName_t &a ) {
							return a.definition_index == g_Vars.m_global_skin_changer.m_gloves_idx;
						} );

						if( on_cfg_load_gloves ) {
							if( it != g_KitParser.vecGloveNames.end( ) )
								g_Vars.m_global_skin_changer.m_gloves_vector_idx = std::distance( g_KitParser.vecGloveNames.begin( ), it );
						}
					}

					static std::vector<std::string> gloves;
					for( int i = 0; i < g_KitParser.vecGloveNames.size( ); ++i ) {
						auto whatevertheFUCK = g_KitParser.vecGloveNames[ i ];
						gloves.push_back( whatevertheFUCK.name );
					}

					if( !gloves.empty( ) ) {
						GUI::Controls::Dropdown( g_Vars.menu.polish_mode ? XorStr( "Modele rekawiczek" ) : XorStr( "Glove model" ), gloves, &g_Vars.m_global_skin_changer.m_gloves_vector_idx );
						g_Vars.m_global_skin_changer.m_gloves_idx = g_KitParser.vecGloveNames[ g_Vars.m_global_skin_changer.m_gloves_vector_idx ].definition_index;

						static std::vector<std::string> paint_kits;

						int currentGloveID = 0;
						for( int i = 0; i < g_KitParser.vecWeapons.size( ); ++i ) {
							auto currentGlove = g_KitParser.vecWeapons[ i ];

							if( currentGlove.id == g_Vars.m_global_skin_changer.m_gloves_idx ) {
								currentGloveID = i;
							}
						}

						auto &skin_data = g_Vars.m_skin_changer;
						CVariables::skin_changer_data *skin = nullptr;
						for( size_t i = 0u; i < skin_data.Size( ); ++i ) {
							skin = skin_data[ i ];
							if( skin->m_definition_index == g_Vars.m_global_skin_changer.m_gloves_idx ) {
								break;
							}
						}

						if( currentGloveID != 0 && skin != nullptr ) {
							static int iOldKit = skin->m_paint_kit_index;

							auto &current_weapon = g_KitParser.vecWeapons[ currentGloveID ];

							for( int i = 0; i < current_weapon.m_kits.size( ); ++i ) {
								auto currentKit = current_weapon.m_kits[ i ];
								paint_kits.push_back( currentKit.name.data( ) );
							}

							if( on_cfg_load_gloves ) {
								auto &kit = current_weapon.m_kits[ skin->m_paint_kit_index ];
								if( kit.id != skin->m_paint_kit ) {
									auto it = std::find_if( current_weapon.m_kits.begin( ), current_weapon.m_kits.end( ), [ skin ] ( paint_kit &a ) {
										return a.id == skin->m_paint_kit;
									} );

									if( it != current_weapon.m_kits.end( ) )
										skin->m_paint_kit_index = std::distance( current_weapon.m_kits.begin( ), it );

									skin->m_paint_kit = current_weapon.m_kits[ skin->m_paint_kit_index ].id;
								}

								on_cfg_load_gloves = false;
							}

							GUI::Controls::Listbox( g_Vars.menu.polish_mode ? XorStr( "Skiny rekawiczek" ) : XorStr( "Glove kit" ), paint_kits, &skin->m_paint_kit_index, true, 10 );

							float flOldWear = skin->m_wear;

							if( flOldWear != skin->m_wear /*|| flOldSeed != skin->m_seed*/ ) {
								g_Vars.m_global_skin_changer.m_update_gloves = true;
								flOldWear = skin->m_wear;
								//flOldSeed = skin->m_seed;
							}

							skin->m_paint_kit = current_weapon.m_kits[ skin->m_paint_kit_index ].id;
							skin->m_enabled = true;

							if( iOldKit != skin->m_paint_kit_index ) {
								g_Vars.m_global_skin_changer.m_update_gloves = true;
								iOldKit = skin->m_paint_kit_index;
							}

							paint_kits.clear( );
						}
					}

					gloves.clear( );

					if( bruh != g_Vars.m_global_skin_changer.m_gloves_vector_idx || bOldGlove != g_Vars.m_global_skin_changer.m_glove_changer ) {
						g_Vars.m_global_skin_changer.m_update_gloves = true;

						bruh = g_Vars.m_global_skin_changer.m_gloves_vector_idx;
						bOldGlove = g_Vars.m_global_skin_changer.m_glove_changer;
					}

					GUI::Group::EndGroup( );
				}
				GUI::Group::PopLastSize( );

				GUI::Group::BeginGroup( g_Vars.menu.polish_mode ? XorStr( "Skiny broni" ) : XorStr( "Weapon skin" ), Vector2D( 50, 100 ) );
				{
					g_Vars.m_global_skin_changer.m_active = true;

					auto &current_weapon = g_KitParser.vecWeapons[ /*weapon_id*/g_Vars.globals.m_iWeaponIndexSkins == -1 ? 0 : g_Vars.globals.m_iWeaponIndexSkins ];
					auto idx = g_Vars.globals.m_iWeaponIndex == -1 ? 7 : g_Vars.globals.m_iWeaponIndex;

					auto &skin_data = g_Vars.m_skin_changer;
					CVariables::skin_changer_data *skin = nullptr;
					for( size_t i = 0u; i < skin_data.Size( ); ++i ) {
						skin = skin_data[ i ];
						if( skin->m_definition_index == idx ) {
							break;
						}
					}

					if( skin ) {
						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Wlacz" ) : XorStr( "Enabled##Skins" ), &skin->m_enabled );

						GUI::Controls::Checkbox( XorStr( "StatTrak" ), &skin->m_stat_trak );

						float flOldWear = skin->m_wear;
						float flOldSeed = skin->m_seed;
						GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Jakosc" ) : XorStr( "Quality" ), &skin->m_wear, 0.f, 100.f, XorStr( "%.0f%%" ) );
						GUI::Controls::Slider( g_Vars.menu.polish_mode ? XorStr( "Nasionko" ) : XorStr( "Seed" ), &skin->m_seed, 0, 500, XorStr( "%d" ), 1, true );

						if( flOldWear != skin->m_wear || flOldSeed != skin->m_seed ) {
							g_Vars.m_global_skin_changer.m_update_skins = true;
							flOldWear = skin->m_wear;
							flOldSeed = skin->m_seed;
						}

						GUI::Controls::Checkbox( g_Vars.menu.polish_mode ? XorStr( "Filtruj wedlug broni" ) : XorStr( "Filter by weapon" ), &skin->m_filter_paint_kits );

						if( skin->m_filter_paint_kits ) {
							auto &kit = current_weapon.m_kits[ skin->m_paint_kit_index ];
							if( kit.id != skin->m_paint_kit ) {
								auto it = std::find_if( current_weapon.m_kits.begin( ), current_weapon.m_kits.end( ), [ skin ] ( paint_kit &a ) {
									return a.id == skin->m_paint_kit;
								} );

								if( it != current_weapon.m_kits.end( ) )
									skin->m_paint_kit_index = std::distance( current_weapon.m_kits.begin( ), it );
							}
						}

						static int bruh1 = skin->m_paint_kit_index;
						static int bruh2 = skin->m_paint_kit_no_filter;

						if( skin->m_filter_paint_kits ) {
							static std::vector<std::string> paint_kits;

							for( int i = 0; i < current_weapon.m_kits.size( ); ++i ) {
								auto currentKit = current_weapon.m_kits[ i ];
								paint_kits.push_back( currentKit.name.data( ) );
							}

							if( !paint_kits.empty( ) ) {
								GUI::Controls::Listbox( g_Vars.menu.polish_mode ? XorStr( "Kity farby" ) : XorStr( "Paint kits" ), paint_kits, &skin->m_paint_kit_index, true, 13 );
							}

							paint_kits.clear( );
						}
						else {
							if( !g_Vars.globals.m_vecPaintKits.empty( ) ) {
								GUI::Controls::Listbox( g_Vars.menu.polish_mode ? XorStr( "Kity farby" ) : XorStr( "Paint kits" ), g_Vars.globals.m_vecPaintKits, &skin->m_paint_kit_no_filter, true, 13 );
							}
						}

						if( ( bruh1 != skin->m_paint_kit_index ) || ( bruh2 != skin->m_paint_kit_no_filter ) ) {
							g_Vars.m_global_skin_changer.m_update_skins = true;
							//	g_Vars.m_global_skin_changer.m_update_gloves = true;

							bruh1 = skin->m_paint_kit_index;
							bruh2 = skin->m_paint_kit_no_filter;
						}

						skin->m_paint_kit = skin->m_filter_paint_kits ? current_weapon.m_kits[ skin->m_paint_kit_index ].id : skin->m_paint_kit_no_filter;

						//skin->m_enabled = true;
					}

					GUI::Group::EndGroup( );
				}
			}

			if( GUI::Form::BeginTab( 5, XorStr( "F" ) ) || GUI::ctx->setup ) {
				static int nPlayerListOption = 0;

				auto vecConnectedPlayers = g_PlayerList.GetConnectedPlayers( );
				GUI::ctx->disabled = !g_pEngine->IsConnected( ) || vecConnectedPlayers.empty( );

				GUI::Group::BeginGroup( XorStr( "Players" ), Vector2D( 50, 100 ) );
				{
					// skeet playerlist listbox is 40px wider than ours, compensate for this
					auto vecCursorPos = GUI::PopCursorPos( );
					auto nParentSize = GUI::ctx->ParentSize.x;
					GUI::PushCursorPos( vecCursorPos - Vector2D( 20, 0 ) );
					GUI::ctx->ParentSize.x += 40;

					GUI::Controls::Listbox( XorStr( "##Playerlist" ),
											vecConnectedPlayers.size( ) ?
											vecConnectedPlayers :
											std::vector<std::string>{ }, &nPlayerListOption, false, 20 );

					GUI::ctx->ParentSize.x = nParentSize;
					GUI::PushCursorPos( GUI::PopCursorPos( ) + Vector2D( 20, 0 ) );

					GUI::Controls::Button( XorStr( "Reset all" ), [ & ] ( ) { g_PlayerList.ClearPlayerData( ); } );

					if( vecConnectedPlayers.empty( ) ) {
						g_PlayerList.ClearPlayerData( );
						nPlayerListOption = 0;
					}
				}
				GUI::Group::EndGroup( );

				GUI::Group::BeginGroup( XorStr( "Adjustments" ), Vector2D( 50, 100 ) );
				{
					/*const bool bSomethingWentWrong = nPlayerListOption > g_PlayerList.GetConnectedPlayers( ).size( );
					static bool bWhoops = false;

					if( !bSomethingWentWrong )
						bWhoops = false;

					if( !bWhoops && bSomethingWentWrong ) {
						g_PlayerList.ClearPlayerData( );
						nPlayerListOption = 0;
						bWhoops = true;
					}*/

					if( nPlayerListOption > vecConnectedPlayers.size( ) - 1 )
						nPlayerListOption = 0;

					auto szSelectedPlayer = ( /*bSomethingWentWrong ||*/ vecConnectedPlayers.empty( ) || GUI::ctx->disabled ) ? XorStr( "" ) : vecConnectedPlayers[ nPlayerListOption ];

					__int64 nSteamID = GUI::ctx->disabled ? 69 : 1;
					if( g_PlayerList.GetPlayerData( ).size( ) )
						for( auto data : g_PlayerList.GetPlayerData( ) ) {
							nSteamID = data.first;

							if( data.second.m_szPlayerName == szSelectedPlayer ) {
								break;
							}
						}

					// meme entry (in order to show controls when player-list is disabled)
					if( nSteamID == 69 ) {
						if( g_PlayerList.m_vecPlayerData.find( 69 ) == g_PlayerList.m_vecPlayerData.end( ) ) {
							PlayerList::PlayerListInfo_t uEntry;
							uEntry.m_nPlayerIndex = 69;
							uEntry.m_szPlayerName = XorStr( "Deez1337Nuts" );

							g_PlayerList.m_vecPlayerData.insert( { uEntry.m_nPlayerIndex, uEntry } );
						}
					}

					if( g_PlayerList.m_vecPlayerData.find( nSteamID ) != g_PlayerList.m_vecPlayerData.end( ) ) {
						auto &listOptions = g_PlayerList.m_vecPlayerData.at( nSteamID );

						GUI::Controls::Checkbox( std::string( XorStr( "Add to whitelist##" ) ).append( szSelectedPlayer ), &listOptions.m_bAddToWhitelist );
						GUI::Controls::Checkbox( std::string( XorStr( "Disable visuals##" ) ).append( szSelectedPlayer ), &listOptions.m_bDisableVisuals );
						GUI::Controls::Checkbox( std::string( XorStr( "High priority##" ) ).append( szSelectedPlayer ), &listOptions.m_bHighPriority );

						GUI::Controls::Dropdown( std::string( XorStr( "Force yaw##" ) ).append( szSelectedPlayer ), { XorStr( "-" ), XorStr( "Static" ), XorStr( "Away from me" ), XorStr( "Lower body yaw" ), XorStr( "Nearest enemy" ), XorStr( "Average enemy" ) }, &listOptions.m_iForceYaw );
						GUI::Controls::Slider( std::string( XorStr( "##yaw" ) ).append( szSelectedPlayer ), &listOptions.m_flForcedYaw, -180.f, 180.f, XorStr( "%.0f°" ), 1, true );

						GUI::Controls::Checkbox( std::string( XorStr( "Force pitch##" ) ).append( szSelectedPlayer ), &listOptions.m_bForcePitch );
						GUI::Controls::Slider( std::string( XorStr( "##pitch" ) ).append( szSelectedPlayer ), &listOptions.m_flForcedPitch, -89.f, 89.f, XorStr( "%.0f°" ), 1, true );

						// todo - maxwell; find out what was in this dropdown...
						std::vector<MultiItem_t> correction{
							{ XorStr( "Disable anti-aim resolver" ), &listOptions.m_bDisableResolver },
							{ XorStr( "Disable body prediction" ), &listOptions.m_bDisableBodyPred },
						};
						GUI::Controls::MultiDropdown( std::string( XorStr( "Override anti-aim correction##" ) ).append( szSelectedPlayer ), correction );

						GUI::Controls::Checkbox( std::string( XorStr( "Edge correction##" ) ).append( szSelectedPlayer ), &listOptions.m_bEdgeCorrection );
						GUI::Controls::Dropdown( std::string( XorStr( "Override prefer body aim##" ) ).append( szSelectedPlayer ), { XorStr( "-" ), XorStr( "Fake angles" ), XorStr( "Always" ), XorStr( "Aggressive" ), XorStr( "High inaccuracy" ) }, &listOptions.m_iOverridePreferBaim );
						//GUI::Controls::Dropdown( std::string( XorStr( "Override accuracy boost##" ) ).append( szSelectedPlayer ), { XorStr( "-" ), XorStr( "Low" ), XorStr( "Medium" ), XorStr( "High" ) }, &listOptions.m_iOverrideAccuracyBoost );

						GUI::Controls::Button( XorStr( "Apply to all" ), [ & ] ( ) {
							for( auto &allEntries : g_PlayerList.m_vecPlayerData ) {
								// no need
								if( allEntries.first == nSteamID )
									continue;

								// we don't want to modify these
								const auto szBackupName = allEntries.second.m_szPlayerName;
								const auto nBackupIndex = allEntries.second.m_nPlayerIndex;

								// copy over this player's options to everyone
								std::memcpy( &allEntries.second, &listOptions, sizeof( PlayerList::PlayerListInfo_t ) );

								// restore back backed up variables
								allEntries.second.m_szPlayerName = szBackupName;
								allEntries.second.m_nPlayerIndex = nBackupIndex;
							}
						} );

						static bool bShouldHaveRequestAccess = false;
						bool bIsWhitelistedUser = false;
						for( auto nWhitelistedID : g_Vars.globals.m_vecWhitelistedIds ) {
							if( nWhitelistedID == nSteamID )
								bIsWhitelistedUser = true;

							if( !bShouldHaveRequestAccess ) {
								if( g_Vars.globals.m_nLocalSteamID == nWhitelistedID )
									bShouldHaveRequestAccess = true;
							}
						}

						if( bShouldHaveRequestAccess ) {
							GUI::Controls::Label( XorStr( "[======================]" ) );

							auto &resolverData = listOptions.m_nPlayerIndex > 0 && listOptions.m_nPlayerIndex <= 64 ? g_Resolver.m_arrResolverData.at( listOptions.m_nPlayerIndex ) : ResolverData_t{};

							bool bIskaabaUser = ( resolverData.m_flLastReceivedkaabaDataTime > 0.f && fabs( resolverData.m_flLastReceivedkaabaDataTime - TICKS_TO_TIME( g_pGlobalVars->tickcount ) ) < 2.5f );

							const bool bDisabledBackup = GUI::ctx->disabled;

							std::stringstream str;
							if( !bIskaabaUser )
								str << XorStr( "Not using kaaba" );

							if( bIsWhitelistedUser ) {
								if( !bIskaabaUser ) {
									str << XorStr( " and whitelisted ID" );
								}
								else
									str << XorStr( "Whitelisted ID" );
							}

							if( bIsWhitelistedUser || !bIskaabaUser )
								GUI::Controls::Label( str.str( ).data( ) );

							GUI::ctx->disabled = !bIskaabaUser || bIsWhitelistedUser;

							// note - michal;
							// for the send username request, we could make the client
							// send it via discord webhooks (smth like "[+] requested username for user [steam_username][steamid 64]: [windows_username]
			

							GUI::ctx->disabled = bDisabledBackup;
						}
					}

					// no need to reset it here tbh
					GUI::ctx->disabled = false;
				}
				GUI::Group::EndGroup( );
			}

			GUI::Form::EndWindow( );

			// we not in setup anymore (menu code ran once)
			GUI::ctx->setup = false;
		}
	}
	void Draw( ) {
		DrawMenu( );
	}
}
