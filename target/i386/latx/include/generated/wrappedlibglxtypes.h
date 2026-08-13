#ifndef __wrappedlibglxTYPES_H_
#define __wrappedlibglxTYPES_H_

#ifndef LIBNAME
#error You should only #include this file inside a wrapped*.c file
#endif
#ifndef ADDED_FUNCTIONS
#define ADDED_FUNCTIONS()
#endif

typedef void* (*pFp_t)(void*);
typedef void (*vFpp_t)(void*, void*);
typedef uintptr_t (*LFppp_t)(void*, void*, void*);
typedef void (*vFpL_t)(void*, uintptr_t);
typedef int32_t (*iFpLLp_t)(void*, uintptr_t, uintptr_t, void*);
typedef int32_t (*iFpLp_t)(void*, uintptr_t, void*);
typedef void* (*pFppipi_t)(void*, void*, int32_t, void*, int32_t);

#define SUPER() ADDED_FUNCTIONS() \
	GO(glXGetProcAddress, pFp_t) \
	GO(glXGetProcAddressARB, pFp_t) \
	GO(glXDestroyContext,vFpp_t) \
	GO(glXDestroyPbuffer,vFpL_t) \
        GO(glXCreatePbuffer,LFppp_t) \
        GO(glXMakeContextCurrent,iFpLLp_t) \
        GO(glXMakeCurrent,iFpLp_t) \
        GO(glXCreateNewContext,pFppipi_t)

#endif // __wrappedlibglxTYPES_H_
