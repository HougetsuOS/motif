/*
 * ThemeI.h - internal interface of the theme layer (plan section 7.1).
 */
#ifndef XMTHEMEI_H
#define XMTHEMEI_H

#include <X11/Intrinsic.h>

/* Auto-apply $MOTIF_THEME at the first vendor-shell realize. */
extern void _XmThemeEnvInit (Widget shell) ;

#endif /* XMTHEMEI_H */
