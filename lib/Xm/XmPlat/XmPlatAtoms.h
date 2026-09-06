/*
 * XmPlatAtoms - narrow atom/property seam for Xt clients outside lib/Xm
 * (clients/mwm).  Declares only the raw-glue family so clients do not
 * pull in the full XmPlatP.h (which carries backend-render internals).
 *
 * The implementations live in the XmPlat X11 backend inside libXm, which
 * mwm already links against.
 */
#ifndef XMPLAT_ATOMS_H
#define XMPLAT_ATOMS_H

#include <X11/Xlib.h>
#include "XmPlat/XmPlatTypes.h"

extern unsigned long _XmPlatInternAtomRaw (Display *dpy, const char *name,
					   XmPlatBoolean only_if_exists) ;
extern void _XmPlatInternAtomsRaw (Display *dpy, char **names,
				   unsigned int nnames,
				   XmPlatBoolean only_if_exists,
				   unsigned long *atoms) ;
extern char *_XmPlatAtomNameRaw (Display *dpy, unsigned long raw) ;

extern void _XmPlatChangeProperty (Display *dpy, unsigned long win,
				   unsigned long prop, unsigned long type,
				   int format, int mode,
				   const unsigned char *data,
				   int nelements) ;
extern int _XmPlatGetWindowProperty (Display *dpy, unsigned long win,
				     unsigned long prop, long offset,
				     long length, XmPlatBoolean delete,
				     unsigned long req_type,
				     unsigned long *ret_type, int *ret_format,
				     unsigned long *ret_nitems,
				     unsigned long *ret_bytes_after,
				     unsigned char **ret_data) ;
extern void _XmPlatDeleteProperty (Display *dpy, unsigned long win,
				   unsigned long prop) ;
extern XmPlatBoolean _XmPlatSendClientMessage (Display *dpy,
					       unsigned long win,
					       XmPlatBoolean propagate,
					       long event_mask,
					       const void *msg) ;

#endif /* XMPLAT_ATOMS_H */
