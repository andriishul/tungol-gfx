#ifndef TUNGOL_EXPORT_H
#define TUNGOL_EXPORT_H

#ifdef TUNGOL_STATIC_DEFINE
#  define TUNGOL_EXPORT
#  define TUNGOL_NO_EXPORT
#else
#  ifndef TUNGOL_EXPORT
#    ifdef TUNGOL_EXPORTS
        /* We are building this library */
#      define TUNGOL_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define TUNGOL_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef TUNGOL_NO_EXPORT
#    define TUNGOL_NO_EXPORT 
#  endif
#endif

#ifndef TUNGOL_DEPRECATED
#  define TUNGOL_DEPRECATED __declspec(deprecated)
#endif

#ifndef TUNGOL_DEPRECATED_EXPORT
#  define TUNGOL_DEPRECATED_EXPORT TUNGOL_EXPORT TUNGOL_DEPRECATED
#endif

#ifndef TUNGOL_DEPRECATED_NO_EXPORT
#  define TUNGOL_DEPRECATED_NO_EXPORT TUNGOL_NO_EXPORT TUNGOL_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef TUNGOL_NO_DEPRECATED
#    define TUNGOL_NO_DEPRECATED
#  endif
#endif

#endif /* TUNGOL_EXPORT_H */

