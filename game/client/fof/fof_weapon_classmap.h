#ifndef FOF_WEAPON_CLASSMAP_H
#define FOF_WEAPON_CLASSMAP_H
#ifdef _WIN32
#pragma once
#endif

// The client prediction class map is keyed by script name while each network
// leaf has its own C++ class.  Keying the registrar by the C++ class permits
// left- and right-hand leaves to share one scripts/weapon_*.txt entry.
#define FOF_LINK_WEAPON_CLASS( scriptName, clientClassName ) \
	static C_BaseEntity *FoF_##clientClassName##_Factory() \
	{ \
		return static_cast< C_BaseEntity * >( new clientClassName ); \
	} \
	class FoF_##clientClassName##_ClassMapRegistration \
	{ \
	public: \
		FoF_##clientClassName##_ClassMapRegistration() \
		{ \
			GetClassMap().Add( #scriptName, #clientClassName, sizeof( clientClassName ), &FoF_##clientClassName##_Factory ); \
		} \
	}; \
	static FoF_##clientClassName##_ClassMapRegistration g_FoF_##clientClassName##_ClassMapRegistration;

#endif // FOF_WEAPON_CLASSMAP_H
