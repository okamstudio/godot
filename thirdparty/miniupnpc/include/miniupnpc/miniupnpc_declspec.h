#ifndef MINIUPNPC_DECLSPEC_H_INCLUDED
#define MINIUPNPC_DECLSPEC_H_INCLUDED

#if defined(_WIN32) && !defined(MINIUPNP_STATICLIB)
	/* for windows dll */
	#ifdef MINIUPNP_EXPORTS
		#define MINIUPNP_LIBSPEC __declspec(dllexport)
	#else
		#define MINIUPNP_LIBSPEC __declspec(dllimport)
	#endif
#else
	#if defined(__GNUC__) && __GNUC__ >= 4
		/* fix dynlib for OS X 10.9.2 and Apple LLVM version 5.0 */
		#define MINIUPNP_LIBSPEC __attribute__ ((visibility ("default")))
	#else
		#define MINIUPNP_LIBSPEC
	#endif
#endif

#endif /* MINIUPNPC_DECLSPEC_H_INCLUDED */

