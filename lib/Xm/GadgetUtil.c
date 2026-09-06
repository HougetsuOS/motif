/* 
 * Motif
 *
 * Copyright (c) 1987-2012, The Open Group. All rights reserved.
 *
 * These libraries and programs are free software; you can
 * redistribute them and/or modify them under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * These libraries and programs are distributed in the hope that
 * they will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with these librararies and programs; if not, write
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth
 * Floor, Boston, MA 02110-1301 USA
*/ 
#ifdef REV_INFO
#ifndef lint
static char rcsid[] = "$XConsortium: GadgetUtil.c /main/16 1996/10/23 15:00:52 cde-osf $"
#endif
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <Xm/XmP.h>
#include <Xm/GadgetP.h>
#include <X11/Shell.h>
#include <X11/ShellP.h>
#include "XmPlat/XmPlatP.h"
#include <Xm/DropSMgr.h>
#include "GadgetUtiI.h"
#include "XmI.h"


/********    Static Function Declarations    ********/


/********    End Static Function Declarations    ********/



/************************************************************************
 *
 *  _XmInputForGadget
 *	This routine is a front-end for XmObjectAtPoint which returns a
 *      gadget or NULL if XmbjectAtPoint is not sensitive.
 *
 ************************************************************************/
XmGadget 
_XmInputForGadget(
        Widget wid,
        int x,
        int y )
{
    Widget widget;

    widget = XmObjectAtPoint (wid, x, y);

    if (!widget  ||  !XtIsSensitive (widget))
	return ((XmGadget) NULL);

   return ((XmGadget) widget);
}




/************************************************************************
 *
 *  XmConfigureObject
 *	Wrapper around Xt equivalent + DropSite update.
 *
 ************************************************************************/
void 
XmeConfigureObject(
        Widget wid,
#if NeedWidePrototypes
        int x,
        int y,
        int width,
        int height,
        int border_width )
#else
        Position x,
        Position y,
        Dimension width,
        Dimension height,
        Dimension border_width )
#endif /* NeedWidePrototypes */
{
    _XmWidgetToAppContext(wid);
    XmDropSiteStartUpdate(wid);

    _XmAppLock(app);

    if (!width && !height) {
	XtWidgetGeometry   desired, preferred ;
	desired.request_mode = 0;
        XtQueryGeometry(wid, &desired, &preferred);
	width = preferred.width;
        height = preferred.height;
    }
    if (!width)  width++;                
    if (!height) height++;
    XtConfigureWidget(wid, x, y, width, height, border_width);

    XmDropSiteEndUpdate(wid);
    _XmAppUnlock(app);
}




/************************************************************************
 *
 *  XmeRedisplayGadgets
 *	Redisplay any gadgets contained within the manager mw which
 *	are intersected by the region.
 *
 ************************************************************************/
void 
XmeRedisplayGadgets(
        Widget w,
        register XEvent *event,
        Region region )
{
   CompositeWidget mw = (CompositeWidget) w ;
   register int i;
   register Widget child;
   XtExposeProc expose;
  
   _XmWidgetToAppContext(w);

   _XmAppLock(app);
   for (i = 0; i < mw->composite.num_children; i++)
   {
      child = mw->composite.children[i];
      if (XmIsGadget(child) && XtIsManaged(child))
      {
         if (region == NULL)
         {
	    XmPlatEvent pev = _XmPlatEventOf (event) ;
            if (child->core.x < _XmPlatEventX (pev) + (int) _XmPlatEventWidth (pev) &&
                child->core.x + child->core.width > _XmPlatEventX (pev) &&
                child->core.y < _XmPlatEventY (pev) + (int) _XmPlatEventHeight (pev) &&
                child->core.y + child->core.height > _XmPlatEventY (pev))
            {
		
	       _XmProcessLock();
	       expose = child->core.widget_class->core_class.expose;
	       _XmProcessUnlock();

               if (expose)
                  (*(expose))
                     (child, event, region);
            }
         }
         else
         {
            if (XRectInRegion (region, child->core.x, child->core.y,
                               child->core.width, child->core.height))
            {
 	      _XmProcessLock();
	      expose = child->core.widget_class->core_class.expose;
	      _XmProcessUnlock();

              if (expose)
                  (*(expose))
                     (child, event, region);
            }
         }
      }
   }
   _XmAppUnlock(app);
}




/************************************************************************
 *
 *  _XmDispatchGadgetInput
 *	Call the gadgets class function and send the desired data to it.
 *
 ************************************************************************/
void 
_XmDispatchGadgetInput(
        Widget wid,
        XEvent *event,
        Mask mask )
{
        XmGadget g = (XmGadget) wid ;
   if ((g->gadget.event_mask & mask) && 
       XtIsSensitive ((Widget)g) && XtIsManaged ((Widget)g))
   {
      if (event != NULL) 
      {
	 XmPlatEvent pev = _XmPlatEventOf (event) ;
	 XmPlatEvent synth ;

	 switch(mask) {
	   case XmENTER_EVENT:
	     synth = _XmPlatEventSynth (pev, XmPlatEventCrossing) ;
	     break ;
	   case XmLEAVE_EVENT:
	     /* Leave rewrites to LeaveNotify; the Synth helper's crossing
		default is Enter, so flip the record directly. */
	     synth = _XmPlatEventSynth (pev, XmPlatEventCrossing) ;
	     ((XEvent *) _XmPlatEventRaw (synth))->type = LeaveNotify ;
	     break ;
	   case XmFOCUS_IN_EVENT:
	     synth = _XmPlatEventSynth (pev, XmPlatEventFocus) ;
	     break ;
	   case XmFOCUS_OUT_EVENT:
	     synth = _XmPlatEventSynth (pev, XmPlatEventFocus) ;
	     ((XEvent *) _XmPlatEventRaw (synth))->type = FocusOut ;
	     break ;
	   case XmMOTION_EVENT:
	     synth = _XmPlatEventSynth (pev, XmPlatEventPointer) ;
	     break ;
	   case XmARM_EVENT:
	     /* press-like: keep key/press, else rewrite to ButtonPress */
	     synth = _XmPlatEventCopy (pev) ;
	     if (! _XmPlatEventIsType (pev, XmPlatEventKey) &&
		 ! _XmPlatEventIsButtonPress (pev))
	       ((XEvent *) _XmPlatEventRaw (synth))->type = ButtonPress ;
	     break ;
	   case XmACTIVATE_EVENT:
	     synth = _XmPlatEventCopy (pev) ;
	     if (! _XmPlatEventIsType (pev, XmPlatEventKey) &&
		 ! _XmPlatEventIsButtonRelease (pev))
	       ((XEvent *) _XmPlatEventRaw (synth))->type = ButtonRelease ;
	     break ;
	   case XmKEY_EVENT:
	     synth = _XmPlatEventCopy (pev) ;
	     if (! _XmPlatEventIsType (pev, XmPlatEventKey) &&
		 ! _XmPlatEventIsButtonPress (pev))
	       ((XEvent *) _XmPlatEventRaw (synth))->type = KeyPress ;
	     break ;
	   case XmHELP_EVENT:
	     synth = _XmPlatEventCopy (pev) ;
	     if (! _XmPlatEventIsType (pev, XmPlatEventKey))
	       ((XEvent *) _XmPlatEventRaw (synth))->type = KeyPress ;
	     break ;
           default:
	     synth = _XmPlatEventCopy (pev) ;
	     break ;
         }
   
         (*(((XmGadgetClass) (g->object.widget_class))->
             gadget_class.input_dispatch)) ((Widget) g, 
                                               (XEvent *) _XmPlatEventRaw (synth),
					       mask) ;
	 _XmPlatEventFreeCopy (synth) ;
      } 
      else
      {
         (*(((XmGadgetClass) (g->object.widget_class))->
             gadget_class.input_dispatch)) ((Widget) g,
                                                  (XEvent *) event, mask) ;
      }
   }
}

