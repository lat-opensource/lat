#if !(defined(GO) && defined(GOM) && defined(GO2) && defined(DATA))
#error meh!
#endif

////simple version for glxgears
//GO(glXChooseVisual, pFpip)
//GO(glXCreateContext,pFpppi)
//GO(glXDestroyContext,vFpp)
////GOM(glXGetProcAddressARB, pFEp)
//GO(glXMakeCurrent,iFppp)
//GO(glXQueryDrawable, iFppip)
//GO(glXSwapBuffers,vFpp)
//GO(glXQueryExtensionsString,pFpi)

#if 1
// __glXGLLoadGLXFunction
GO(glXChooseFBConfig, pFpipp)
GO(glXChooseVisual, pFpip)
GO(glXCopyContext,vFppp)
GO(glXCreateContext,pFpppi)
GO(glXCreateGLXPixmap,pFppp)
GOM(glXCreateNewContext,pFppipi)
GOM(glXCreatePbuffer,pFppp)
GO(glXCreatePixmap,pFppp)
GO(glXCreateWindow,pFpppp)
GOM(glXDestroyContext,vFpp)
GO(glXDestroyGLXPixmap,vFpp)
GOM(glXDestroyPbuffer,vFpp)
GO(glXDestroyPixmap,vFpp)
GO(glXDestroyWindow,vFpp)
GO(glXGetClientString, pFpi)
GO(glXGetConfig, iFppip)
GO(glXGetCurrentContext, pFv)
GO(glXGetCurrentDisplay, pFv)
GO(glXGetCurrentDrawable, pFv)
GO(glXGetCurrentReadDrawable, pFv)
GO(glXGetFBConfigAttrib, iFppip)
GO(glXGetFBConfigs,pFpip)
GOM(glXGetProcAddress, pFEp)
GOM(glXGetProcAddressARB, pFEp)
GO(glXGetSelectedEvent, vFppp)
GO(glXGetVisualFromFBConfig, pFpp)
GO(glXIsDirect,iFpp)
GOM(glXMakeContextCurrent,iFpppp)
GOM(glXMakeCurrent,iFppp)
GO(glXQueryContext,iFppip)
GO(glXQueryDrawable, iFppip)
GO(glXQueryExtension, iFppp)
GO(glXQueryExtensionsString,pFpi)
GO(glXQueryServerString,pFpii)
GO(glXQueryVersion,iFppp)
GO(glXSelectEvent, vFppu)
GO(glXSwapBuffers,vFpp)
GO(glXUseXFont,vFpiii)
GO(glXWaitGL,vFv)
GO(glXWaitX,vFv)
#endif



