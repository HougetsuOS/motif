/*
 * XmA11yI.h - internal interface of the accessibility bridge skeleton
 * (plan section 7.3).
 */
#ifndef XMA11YI_H
#define XMA11YI_H

#include <X11/Intrinsic.h>

/* Register the Xt display hook when $MOTIF_A11Y_LOG is set; called from
   vendor-shell init (one per process). */
extern void _XmA11yInit (Display *dpy) ;

#endif /* XMA11YI_H */
