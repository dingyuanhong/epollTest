
#if defined(WINCE) || defined(_WIN32_WCE)  
#   define SYS_OS_WINCE  
#elif defined(WIN64) || defined(_WIN64) || defined(__WIN64__)  
#   define SYS_OS_WIN64  
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)  
#   define SYS_OS_WIN32 
#elif defined(__linux__) || defined(__linux)  
#   define SYS_OS_LINUX  
#elif defined(__unix__) // all unices not caught above
#   define SYS_OS_UNIX
#elif defined(__APPLE__)
    #include "TargetConditionals.h"
    #if TARGET_IPHONE_SIMULATOR
    #   define SYS_IOS_SIMULATOR
    #elif TARGET_OS_IPHONE
	#define __IOS_IPHONE__ 1
    #   define SYS_IOS_IPHONE
    #elif TARGET_OS_MAC
    #   define SYS_OS_MAC
	#   define __MAC__ 1
    #else
    #   error "Unknown Apple platform"
    #endif
#elif defined(_POSIX_VERSION)
#   error "_POSIX_VERSION OS is unsupported" 
#else  
#   error "This OS is unsupported"  
#endif  
 
#if defined(__ANDROID__)
#   define SYS_OS_ANDROID
#endif

#if defined(SYS_OS_WIN32) || defined(SYS_OS_WIN64) || defined(SYS_OS_WINCE)  
#   define SYS_OS_WIN  
#else  
#   define SYS_OS_UNIX  
#endif  

#if defined(_MSC_VER)
#   if (_MSC_VER <= 1200)  
#       define SYS_CC_MSVC6  
#   endif  
#   define SYS_CC_MSVC
#elif defined(__GNUC__)  
#   define SYS_CC_GNUC  
#else  
#   error "This CC is unsupported"  
#endif