/* Copyright (c) 2014 Neale Pickett, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include "dat.h"
#include "fns.h"
#include "patchlevel.h"

void
mainloop(shape_event)
int shape_event;
{
	XEvent ev;

	for (;;) {
		getevent(&ev);

#ifdef	DEBUG_EV
		if (debug) {
			ShowEvent(&ev);
			printf("\n");
		}
#endif
		switch (ev.type) {
		default:
#ifdef	SHAPE
			if (shape && ev.type == shape_event)
				shapenotify((XShapeEvent *)&ev);
			else
#endif
				fprintf(stderr, "9wm: unknown ev.type %d\n", ev.type);
			break;
		case ButtonPress:
			button(&ev.xbutton);
			break;
		case ButtonRelease:
			break;
		case MapRequest:
			mapreq(&ev.xmaprequest);
			break;
		case ConfigureRequest:
			configurereq(&ev.xconfigurerequest);
			break;
		case CirculateRequest:
			circulatereq(&ev.xcirculaterequest);
			break;
		case UnmapNotify:
			unmap(&ev.xunmap);
			break;
		case CreateNotify:
			newwindow(&ev.xcreatewindow);
			break;
		case DestroyNotify:
			destroy(ev.xdestroywindow.window);
			break;
		case ClientMessage:
			clientmesg(&ev.xclient);
			break;
		case ColormapNotify:
			cmap(&ev.xcolormap);
			break;
		case PropertyNotify:
			property(&ev.xproperty);
			break;
		case SelectionClear:
			fprintf(stderr, "9wm: SelectionClear (this should not happen)\n");
			break;
		case SelectionNotify:
			fprintf(stderr, "9wm: SelectionNotify (this should not happen)\n");
			break;
		case SelectionRequest:
			fprintf(stderr, "9wm: SelectionRequest (this should not happen)\n");
			break;
		case EnterNotify:
			enter(&ev.xcrossing);
			break;
		case ReparentNotify:
			reparent(&ev.xreparent);
			break;
		case FocusIn:
			focusin(&ev.xfocus);
			break;
		case MotionNotify:
		case Expose:
		case FocusOut:
		case ConfigureNotify:
		case MapNotify:
		case MappingNotify:
			/* not interested */
			trace("ignore", 0, &ev);
			break;
		}
	}
}


void
configurereq(e)
XConfigureRequestEvent *e;
{
	XWindowChanges wc;
	Client *c;

	/* we don't set curtime as nothing here uses it */
	c = getclient(e->window, 0);
	trace("configurereq", c, e);

	e->value_mask &= ~CWSibling;

	if (c) {
		gravitate(c, 1);
		if (e->value_mask & CWX)
			c->x = e->x;
		if (e->value_mask & CWY)
			c->y = e->y;
		if (e->value_mask & CWWidth)
			c->dx = e->width;
		if (e->value_mask & CWHeight)
			c->dy = e->height;
		if (e->value_mask & CWBorderWidth)
			c->border = e->border_width;
		gravitate(c, 0);
		if (e->value_mask & CWStackMode) {
			if (wc.stack_mode == Above)
				top(c);
			else
				e->value_mask &= ~CWStackMode;
		}
		if (c->parent != c->screen->root && c->window == e->window) {
			wc.x = c->x-BORDER;
			wc.y = c->y-BORDER;
			wc.width = c->dx+2*(BORDER-1);
			wc.height = c->dy+2*(BORDER-1);
			wc.border_width = 1;
			wc.sibling = None;
			wc.stack_mode = e->detail;
			XConfigureWindow(dpy, c->parent, e->value_mask, &wc);
			sendconfig(c);
		}
	}

	if (c && c->init) {
		wc.x = BORDER-1;
		wc.y = BORDER-1;
	}
	else {
		wc.x = e->x;
		wc.y = e->y;
	}
	wc.width = e->width;
	wc.height = e->height;
	wc.border_width = 0;
	wc.sibling = None;
	wc.stack_mode = Above;
	e->value_mask &= ~CWStackMode;
	e->value_mask |= CWBorderWidth;

	XConfigureWindow(dpy, e->window, e->value_mask, &wc);
}

void
mapreq(e)
XMapRequestEvent *e;
{
	Client *c;
	int i;

	curtime = CurrentTime;
	c = getclient(e->window, 0);
	trace("mapreq", c, e);

	if (c == 0 || c->window != e->window) {
		/* workaround for stupid NCDware */
		fprintf(stderr, "9wm: bad mapreq c %p w %x, rescanning\n",
			c, (int)e->window);
		for (i = 0; i < num_screens; i++)
			scanwins(&screens[i]);
		c = getclient(e->window, 0);
		if (c == 0 || c->window != e->window) {
			fprintf(stderr, "9wm: window not found after rescan\n");
			return;
		}
	}

	switch (c->state) {
	case WithdrawnState:
		if (c->parent == c->screen->root) {
			if (!manage(c, 0))
				return;
			break;
		}
		XReparentWindow(dpy, c->window, c->parent, BORDER-1, BORDER-1);
		XAddToSaveSet(dpy, c->window);
		/* fall through... */
	case NormalState:
		XMapWindow(dpy, c->window);
		XMapRaised(dpy, c->parent);
		top(c);
		setwstate(c, NormalState);
		if (c->trans != None && current && c->trans == current->window)
				active(c);
		break;
	case IconicState:
		unhidec(c, 1);
		break;
	}
}

void
unmap(e)
XUnmapEvent *e;
{
	Client *c;

	curtime = CurrentTime;
	c = getclient(e->window, 0);
	if (c) {
		switch (c->state) {
		case IconicState:
			if (e->send_event) {
				unhidec(c, 0);
				withdraw(c);
			}
			break;
		case NormalState:
			if (c == current)
				nofocus();
			if (!c->reparenting)
				withdraw(c);
			break;
		}
		c->reparenting = 0;
	}
}

void
circulatereq(e)
XCirculateRequestEvent *e;
{
	fprintf(stderr, "It must be the warlock Krill!\n");  /* :-) */
}

void
newwindow(e)
XCreateWindowEvent *e;
{
	Client *c;
	ScreenInfo *s;

	/* we don't set curtime as nothing here uses it */
	if (e->override_redirect)
		return;
	c = getclient(e->window, 1);
	if (c && c->window == e->window && (s = getscreen(e->parent))) {
		c->x = e->x;
		c->y = e->y;
		c->dx = e->width;
		c->dy = e->height;
		c->border = e->border_width;
		c->screen = s;
		if (c->parent == None)
			c->parent = c->screen->root;
	}
}

void
destroy(w)
Window w;
{
	Client *c;

	curtime = CurrentTime;
	c = getclient(w, 0);
	if (c == 0)
		return;

	rmclient(c);

	/* flush any errors generated by the window's sudden demise */
	ignore_badwindow = 1;
	XSync(dpy, False);
	ignore_badwindow = 0;
}

void
clientmesg(e)
XClientMessageEvent *e;
{
	Client *c;

	curtime = CurrentTime;
	if (e->message_type == exit_9wm) {
		cleanup();
		exit(0);
	}
	if (e->message_type == restart_9wm) {
		fprintf(stderr, "*** 9wm restarting ***\n");
		cleanup();
		execvp(myargv[0], myargv);
		perror("9wm: exec failed");
		exit(1);
	}
	if (e->message_type == wm_change_state) {
		c = getclient(e->window, 0);
		if (e->format == 32 && e->data.l[0] == IconicState && c != 0) {
			if (normal(c))
				hide(c);
		}
		else
			fprintf(stderr, "9wm: WM_CHANGE_STATE: format %d data %ld w 0x%x\n",
				e->format, e->data.l[0], (int)e->window);
		return;
	}
	fprintf(stderr, "9wm: strange ClientMessage, type 0x%x window 0x%x\n",
		(int)e->message_type, (int)e->window);
}

void
cmap(e)
XColormapEvent *e;
{
	Client *c;
	int i;

	/* we don't set curtime as nothing here uses it */
	if (e->new) {
		c = getclient(e->window, 0);
		if (c) {
			c->cmap = e->colormap;
			if (c == current)
				cmapfocus(c);
		}
		else
			for (c = clients; c; c = c->next) {
				for (i = 0; i < c->ncmapwins; i++)
					if (c->cmapwins[i] == e->window) {
						c->wmcmaps[i] = e->colormap;
						if (c == current)
							cmapfocus(c);
						return;
					}
			}
	}
}

void
property(e)
XPropertyEvent *e;
{
	Atom a;
	int delete;
	Client *c;

	/* we don't set curtime as nothing here uses it */
	a = e->atom;
	delete = (e->state == PropertyDelete);
	c = getclient(e->window, 0);
	if (c == 0)
		return;

	switch (a) {
	case XA_WM_ICON_NAME:
		if (c->iconname != 0)
			XFree((char*) c->iconname);
		c->iconname = delete ? 0 : getprop(c->window, a);
		setlabel(c);
		renamec(c, c->label);
		return;
	case XA_WM_NAME:
		if (c->name != 0)
			XFree((char*) c->name);
		c->name = delete ? 0 : getprop(c->window, a);
		setlabel(c);
		renamec(c, c->label);
		return;
	case XA_WM_TRANSIENT_FOR:
		gettrans(c);
		return;
	}
	if (a == _9wm_hold_mode) {
		c->hold = getiprop(c->window, _9wm_hold_mode);
		if (c == current)
			draw_border(c, 1);
	}
	else if (a == wm_colormaps) {
		getcmaps(c);
		if (c == current)
			cmapfocus(c);
	}
}

void
reparent(e)
XReparentEvent *e;
{
	Client *c;
	XWindowAttributes attr;
	ScreenInfo *s;

	/* we don't set curtime as nothing here uses it */
	if (!getscreen(e->event) || e->override_redirect)
		return;
	if ((s = getscreen(e->parent)) != 0) {
		c = getclient(e->window, 1);
		if (c != 0 && (c->dx == 0 || c->dy == 0)) {
			XGetWindowAttributes(dpy, c->window, &attr);
			c->x = attr.x;
			c->y = attr.y;
			c->dx = attr.width;
			c->dy = attr.height;
			c->border = attr.border_width;
			c->screen = s;
			if (c->parent == None)
				c->parent = c->screen->root;
		}
	}
	else {
		c = getclient(e->window, 0);
		if (c != 0 && (c->parent == c->screen->root || withdrawn(c)))
			rmclient(c);
	}
}

#ifdef	SHAPE
void
shapenotify(e)
XShapeEvent *e;
{
	Client *c;

	/* we don't set curtime as nothing here uses it */
	c = getclient(e->window, 0);
	if (c == 0)
		return;

	setshape(c);
}
#endif

void
enter(e)
XCrossingEvent *e;
{
	Client *c;

	curtime = e->time;
	if (e->mode != NotifyGrab || e->detail != NotifyNonlinearVirtual)
		return;
	c = getclient(e->window, 0);
	if (c != 0 && c != current) {
		/* someone grabbed the pointer; make them current */
		XMapRaised(dpy, c->parent);
		top(c);
		active(c);
	}
}

void
focusin(e)
XFocusChangeEvent *e;
{
	Client *c;

	curtime = CurrentTime;
	if (e->detail != NotifyNonlinearVirtual)
		return;
	c = getclient(e->window, 0);
	if (c != 0 && c->window == e->window && c != current) {
		/* someone grabbed keyboard or seized focus; make them current */
		XMapRaised(dpy, c->parent);
		top(c);
		active(c);
	}
}
