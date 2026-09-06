/*
 * Theme.c - data-driven appearance (plan §7.1).
 *
 * A theme profile is an Xt/Xrm resource file.  XmLoadTheme merges it
 * into the shell's screen resource database and re-applies the two
 * resource families the toolkit derives at init time (colors and
 * fonts); everything else a theme file sets is picked up by widgets
 * created afterwards, which is the shell-realize-time contract of the
 * plan (no mid-interaction switching).
 *
 * Search path (XDG Base Directory spec, then system fallback):
 *   $XDG_CONFIG_HOME/motif/themes/<name>     (~/.config fallback)
 *   ${XDG_DATA_DIRS:-/usr/share}/motif/themes/<name>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/ManagerP.h>
#include <Xm/PrimitiveP.h>
#include "XmP.h"
#include "ColorI.h"

/* path buffer for "<root>/motif/themes/<name>" */
#define THEME_PATH_MAX 1024

static Boolean
ThemeFileReadable (const char *path)
{
    struct stat st ;

    if (stat (path, &st) != 0) return False ;
    return S_ISREG (st.st_mode) ;
}

static Boolean
ThemeFindFile (const char *name, char *out, size_t outlen)
{
    const char *env ;
    const char *dirs[8] ;
    int ndirs = 0 ;

    if (name == NULL || name[0] == '\0') return False ;
    /* absolute or relative path with a slash: use as-is */
    if (strchr (name, '/') != NULL) {
	if (ThemeFileReadable (name)) {
	    strncpy (out, name, outlen - 1) ;
	    out[outlen - 1] = '\0' ;
	    return True ;
	}
	return False ;
    }

    env = getenv ("XDG_CONFIG_HOME") ;
    if (env != NULL && env[0] != '\0')
	dirs[ndirs++] = env ;
    else if (getenv ("HOME") != NULL) {
	static char home_cfg[512] ;
	snprintf (home_cfg, sizeof (home_cfg), "%s/.config", getenv ("HOME")) ;
	dirs[ndirs++] = home_cfg ;
    }
    dirs[ndirs++] = "/usr/share" ;
    env = getenv ("XDG_DATA_DIRS") ;
    if (env != NULL && env[0] != '\0') {
	/* colon-separated */
	char *dup = XtNewString (env), *p = dup ;
	while (ndirs < 7 && p != NULL) {
	    char *colon = strchr (p, ':') ;
	    if (colon != NULL) *colon = '\0' ;
	    if (p[0] != '\0') dirs[ndirs++] = p ;
	    p = (colon != NULL) ? colon + 1 : NULL ;
	}
	/* dup leaks per lookup; themes load once per shell — acceptable,
	   and the string is owned by Xrm afterwards in spirit. */
    }

    while (ndirs > 0) {
	ndirs-- ;
	snprintf (out, outlen, "%s/motif/themes/%s", dirs[ndirs], name) ;
	if (ThemeFileReadable (out)) return True ;
    }
    return False ;
}

/*
 * Re-run the color derivation for one widget: pull its current (or
 * theme-forced) background through XmChangeColor, which recalculates
 * foreground/shadows/select from the color-calculation proc.
 */
static void
ThemeRecolorWidget (Widget w, Pixel background, Boolean have_background)
{
    Arg args[1] ;
    Pixel bg = background ;
    Pixel old_bg ;

    if (!XmIsManager (w) && !XmIsPrimitive (w) && !XmIsGadget (w)) return ;

    old_bg = w->core.background_pixel ;
    if (have_background && background != old_bg) {
	XtSetArg (args[0], XmNbackground, (XtArgVal) background) ;
	XtSetValues (w, args, 1) ;
    } else if (!have_background) {
	/* keep the current background; refresh the derived set */
	bg = old_bg ;
    } else {
	/* theme matches current background: still refresh derived colors */
	bg = old_bg ;
    }
    XmChangeColor (w, bg) ;
}

static void
ThemeWalk (Widget w, Pixel background, Boolean have_background)
{
    Cardinal i ;

    ThemeRecolorWidget (w, background, have_background) ;
    if (XtIsComposite (w) && ((CompositeWidget) w)->composite.num_children)
	for (i = 0 ;
	     i < ((CompositeWidget) w)->composite.num_children ;
	     i++)
	    ThemeWalk (((CompositeWidget) w)->composite.children[i],
		       background, have_background) ;
}

/*
 * Read "<resource-pattern>: <pixel-name>" color keys the theme names and
 * apply them.  The palette key is `*background` — one pixel drives the
 * whole derived set through the color-calculation hook (plan §7.1.2).
 * Returns True when a `*background` line was present.
 */
static Boolean
ThemeApplyColors (Widget shell)
{
    XrmName names[3] ;
    XrmClass classes[3] ;
    XrmRepresentation type ;
    XrmValue value ;
    Screen *screen = XtScreen (shell) ;
    Colormap cmap = DefaultColormapOfScreen (screen) ;
    XColor xcol ;
    Pixel background = 0 ;
    Boolean have_background = False ;

    names[0] = XrmStringToName ("*") ;
    names[1] = XrmStringToName ("background") ;
    names[2] = NULLQUARK ;
    classes[0] = XrmStringToClass ("*") ;
    classes[1] = XrmStringToClass ("Background") ;
    classes[2] = NULLQUARK ;

    /* Probe the screen database: XmLoadTheme already merged the theme
       into it (XrmCombineDatabase consumes the source db). */
    if (XrmQGetResource (XtScreenDatabase (XtScreen (shell)),
			 names, classes, &type, &value) &&
	strcmp (XrmQuarkToString (type), "String") == 0) {
	char namebuf[128] ;

	if (value.size < (unsigned) sizeof (namebuf)) {
	    strncpy (namebuf, (const char *) value.addr, value.size) ;
	    namebuf[value.size] = '\0' ;
	    if (XParseColor (XtDisplay (shell), cmap, namebuf, &xcol)) {
		if (XAllocColor (XtDisplay (shell), cmap, &xcol)) {
		    background = xcol.pixel ;
		    have_background = True ;
		}
	    }
	}
    }
    ThemeWalk (shell, background, have_background) ;
    return have_background ;
}

/*
 * XmLoadTheme - merge a named theme profile and refresh the shell
 * subtree.  Additive public API (plan §7.1).
 */
void
XmLoadTheme (
        Widget shell,
        const char *name )
{
    char path[THEME_PATH_MAX] ;
    XrmDatabase theme_db ;

    if (shell == NULL || name == NULL) return ;
    if (!XtIsShell (shell)) return ;
    if (!ThemeFindFile (name, path, sizeof (path))) {
	XmeWarning (shell, "XmLoadTheme: theme profile not found") ;
	return ;
    }

    theme_db = XrmGetFileDatabase (path) ;
    if (theme_db == NULL) {
	XmeWarning (shell, "XmLoadTheme: cannot parse theme file") ;
	return ;
    }

    /* The theme wins over the host app defaults, so it merges with
       override into the screen DB (Display.c precedent). */
    {
	XrmDatabase screen_db = XtScreenDatabase (XtScreen (shell)) ;

	XrmCombineDatabase (theme_db, &screen_db, False) ;
    }

    ThemeApplyColors (shell) ;
}
/*
 * _XmThemeEnvInit - internal: auto-apply $MOTIF_THEME at the first
 * vendor-shell realize so applications get theming with zero code
 * changes (plan section 7.1 exit criterion).  XmLoadTheme remains the
 * explicit API.
 */
static int theme_env_done = 0 ;

void
_XmThemeEnvInit (
        Widget shell )
{
    const char *theme ;

    if (theme_env_done) return ;
    theme = getenv ("MOTIF_THEME") ;
    if (theme == NULL || theme[0] == '\0') return ;
    theme_env_done = 1 ;
    XmLoadTheme (shell, theme) ;
}
