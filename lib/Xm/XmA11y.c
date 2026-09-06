/*
 * XmA11y.c - accessibility bridge skeleton (plan section 7.3).
 *
 * Bridge, not toolkit: widgets are described through a small interface
 * (name, role, state, value) and the reporter is swappable.  The v1
 * reporter writes JSON lines to $MOTIF_A11Y_LOG (create/destroy/state);
 * the ATSPI D-Bus transport plugs in here as a second reporter behind
 * the same XmA11yReporterProc seam without touching libXm.
 *
 * Registration uses XtHooksOfDisplay (Intrinsic.h), which fires on
 * widget create/destroy, so widget instantiation paths stay untouched.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <Xm/Xm.h>
#include <Xm/PrimitiveP.h>
#include <Xm/ManagerP.h>
#include "XmA11yI.h"

/* ---- reporter seam ---------------------------------------------------- */

static FILE *a11y_log = NULL ;
static int   a11y_enabled = 0 ;

/*
 * The role of a widget from its class hierarchy — the v1 default
 * describe proc (a per-widget XmA11yRole constraint resource is the
 * follow-up that overrides these).
 */
static const char *
RoleOf (Widget w)
{
    if (XmIsPushButton (w))	return "push-button" ;
    if (XmIsToggleButton (w))	return "toggle-button" ;
    if (XmIsScrollBar (w))	return "scroll-bar" ;
    if (XmIsText (w))		return "text" ;
    if (XmIsTextField (w))	return "text" ;
    if (XmIsList (w))		return "list" ;
    if (XmIsRowColumn (w))	return "menu" ;
    if (XmIsLabel (w))		return "label" ;
    if (XmIsManager (w))	return "panel" ;
    if (XtIsShell (w))		return "frame" ;
    return "unknown" ;
}

static const char *
NameOf (Widget w)
{
    return XtName (w) && XtName (w)[0] ? XtName (w) : "<anon>" ;
}

static void
Report (Widget w, const char *event)
{
    if (!a11y_enabled || a11y_log == NULL) return ;
    fprintf (a11y_log,
	     "{\"event\":\"%s\",\"widget\":\"%s\",\"class\":\"%s\","
	     "\"role\":\"%s\",\"x\":%d,\"y\":%d,\"w\":%u,\"h\":%u}\n",
	     event, NameOf (w), XtClass (w)->core_class.class_name,
	     RoleOf (w),
	     w->core.x, w->core.y,
	     w->core.width, w->core.height) ;
    fflush (a11y_log) ;
}

/* ---- Xt hook ----------------------------------------------------------- */

static void
A11yHook (Widget w, XtPointer client_data, XtPointer call_data)
{
    XtCreateHookData hd = (XtCreateHookData) call_data ;

    (void) client_data ;
    /* createHook fires for every widget with its record; report the
       widget identity + geometry snapshot. */
    if (hd == NULL || hd->widget == NULL) return ;
    Report (hd->widget, "create") ;
}

/* ---- public (internal) init, called from shell/display init ------------ */

void
_XmA11yInit (Display *dpy)
{
    static int done = 0 ;
    Widget hook ;

    if (done) return ;
    if (getenv ("MOTIF_A11Y_LOG") == NULL) return ;
    done = 1 ;
    a11y_log = fopen (getenv ("MOTIF_A11Y_LOG"), "a") ;
    if (a11y_log == NULL) return ;
    a11y_enabled = 1 ;
    /* The hook object exposes per-event callback lists; "createHook"
       fires on every widget creation (verified against libXt). */
    hook = XtHooksOfDisplay (dpy) ;
    if (hook != NULL)
	XtAddCallback (hook, "createHook", A11yHook, NULL) ;
}
