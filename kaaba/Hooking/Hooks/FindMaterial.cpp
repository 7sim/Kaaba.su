#include "../hooked.hpp"
#include "../../Utils/extern/FnvHash.hpp"
#include "../../Features/Rage/EnginePrediction.hpp"

IMaterial* __fastcall Hooked::FindMaterial( void* ecx, void* edx, char const* pMaterialName, const char *pTextureGroupName, bool complain, const char *pComplainPrefix ) {
	// note - maxwell; this code is far from pretty, but it works so who cares i guess. i'm not really concerned about the heavy usage of strstr here since this function
	// is only called during map loads. if you want to improve upon this, be my guest.

	IMaterial* pOriginalMaterial = oFindMaterial( ecx, pMaterialName, pTextureGroupName, complain, pComplainPrefix );

	if( g_Vars.esp.brightness_adjustment == 2 ) {
		bool bIsBombsiteDecal = strstr( pMaterialName, XorStr( "bombsite" ) );

		// we need to keep these intact or the map looks retarded.
		// note - maxwell; water still seems to be fucked, look in hammer later.
		if( bIsBombsiteDecal || strstr( pMaterialName, XorStr( "engine" ) ) || strstr( pMaterialName, XorStr( "glass" ) ) || strstr( pMaterialName, XorStr( "water" ) ) 
			|| strstr( pMaterialName, XorStr( "rescue" ) ) )
			return pOriginalMaterial;

		bool bIsDecal = strstr( pMaterialName, XorStr( "decals" ) );

		// let's hide all the ugly ass decals.
		if( bIsDecal && !bIsBombsiteDecal ) {
			static IMaterial* pDecalMaterial = oFindMaterial( ecx, pMaterialName, nullptr, true, NULL );
			pDecalMaterial->SetMaterialVarFlag( MATERIAL_VAR_NO_DRAW, true );

			return pDecalMaterial;
		}

		static IMaterial* pMeasureMaterial = oFindMaterial( ecx, XorStr( "dev/dev_measuregeneric01b" ), nullptr, true, NULL );
		pMeasureMaterial->ColorModulate( 0.65f, 0.65f, 0.65f );

		if( pOriginalMaterial->IsErrorMaterial( ) || !pOriginalMaterial || pTextureGroupName == nullptr || pMaterialName == nullptr ) {
			return pOriginalMaterial;
		} else {
			if( !strcmp( pTextureGroupName, XorStr( "World textures" ) ) ) {
				if( pMeasureMaterial ) {
					return pMeasureMaterial;
				} else {
					return pOriginalMaterial;
				}
			} else {
				return pOriginalMaterial;
			}
		}
	}

	return pOriginalMaterial;
}