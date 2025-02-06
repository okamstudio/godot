/*
 * Copyright © 2009 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

/* Definitions used by the library and client */

#ifndef _XINPUT2_H_
#define _XINPUT2_H_

#include <X11/Xlib.h>
#include <X11/extensions/XI2.h>
#include <X11/extensions/Xge.h>
#include <X11/extensions/Xfixes.h> /* PointerBarrier */

/*******************************************************************
 *
 */
typedef struct {
    int                 type;
    char*               name;
    Bool                send_core;
    Bool                enable;
} XIAddMasterInfo;

typedef struct {
    int                 type;
    int                 deviceid;
    int                 return_mode; /* AttachToMaster, Floating */
    int                 return_pointer;
    int                 return_keyboard;
} XIRemoveMasterInfo;

typedef struct {
    int                 type;
    int                 deviceid;
    int                 new_master;
} XIAttachSlaveInfo;

typedef struct {
    int                 type;
    int                 deviceid;
} XIDetachSlaveInfo;

typedef union {
    int                   type; /* must be first element */
    XIAddMasterInfo       add;
    XIRemoveMasterInfo    remove;
    XIAttachSlaveInfo     attach;
    XIDetachSlaveInfo     detach;
} XIAnyHierarchyChangeInfo;

typedef struct
{
    int    base;
    int    latched;
    int    locked;
    int    effective;
} XIModifierState;

typedef XIModifierState XIGroupState;

typedef struct {
    int           mask_len;
    unsigned char *mask;
} XIButtonState;

typedef struct {
    int           mask_len;
    unsigned char *mask;
    double        *values;
} XIValuatorState;


typedef struct
{
    int                 deviceid;
    int                 mask_len;
    unsigned char*      mask;
} XIEventMask;

typedef struct
{
    int         type;
    int         sourceid;
} XIAnyClassInfo;

typedef struct
{
    int         type;
    int         sourceid;
    int         num_buttons;
    Atom        *labels;
    XIButtonState state;
} XIButtonClassInfo;

typedef struct
{
    int         type;
    int         sourceid;
    int         num_keycodes;
    int         *keycodes;
} XIKeyClassInfo;

typedef struct
{
    int         type;
    int         sourceid;
    int         number;
    Atom        label;
    double      min;
    double      max;
    double      value;
    int         resolution;
    int         mode;
} XIValuatorClassInfo;

/* new in XI 2.1 */
typedef struct
{
    int         type;
    int         sourceid;
    int         number;
    int         scroll_type;
    double      increment;
    int         flags;
} XIScrollClassInfo;

typedef struct
{
    int         type;
    int         sourceid;
    int         mode;
    int         num_touches;
} XITouchClassInfo;

/* new in XI 2.4 */
typedef struct
{
    int         type;
    int         sourceid;
    int         num_touches;
} XIGestureClassInfo;

typedef struct
{
    int                 deviceid;
    char                *name;
    int                 use;
    int                 attachment;
    Bool                enabled;
    int                 num_classes;
    XIAnyClassInfo      **classes;
} XIDeviceInfo;

typedef struct
{
    int                 modifiers;
    int                 status;
} XIGrabModifiers;

typedef unsigned int BarrierEventID;

typedef struct
{
    int                 deviceid;
    PointerBarrier      barrier;
    BarrierEventID      eventid;
} XIBarrierReleasePointerInfo;

/**
 * Generic XI2 event. All XI2 events have the same header.
 */
typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;
    Time          time;
} XIEvent;


typedef struct {
    int           deviceid;
    int           attachment;
    int           use;
    Bool          enabled;
    int           flags;
} XIHierarchyInfo;

/*
 * Notifies the client that the device hierarchy has been changed. The client
 * is expected to re-query the server for the device hierarchy.
 */
typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;       /* XI_HierarchyChanged */
    Time          time;
    int           flags;
    int           num_info;
    XIHierarchyInfo *info;
} XIHierarchyEvent;

/*
 * Notifies the client that the classes have been changed. This happens when
 * the slave device that sends through the master changes.
 */
typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;       /* XI_DeviceChanged */
    Time          time;
    int           deviceid;     /* id of the device that changed */
    int           sourceid;     /* Source for the new classes. */
    int           reason;       /* Reason for the change */
    int           num_classes;
    XIAnyClassInfo **classes; /* same as in XIDeviceInfo */
} XIDeviceChangedEvent;

typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;
    Time          time;
    int           deviceid;
    int           sourceid;
    int           detail;
    Window        root;
    Window        event;
    Window        child;
    double        root_x;
    double        root_y;
    double        event_x;
    double        event_y;
    int           flags;
    XIButtonState       buttons;
    XIValuatorState     valuators;
    XIModifierState     mods;
    XIGroupState        group;
} XIDeviceEvent;

typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;       /* XI_RawKeyPress, XI_RawKeyRelease, etc. */
    Time          time;
    int           deviceid;
    int           sourceid;     /* Bug: Always 0. https://bugs.freedesktop.org//show_bug.cgi?id=34240 */
    int           detail;
    int           flags;
    XIValuatorState valuators;
    double        *raw_values;
} XIRawEvent;

typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;
    Time          time;
    int           deviceid;
    int           sourceid;
    int           detail;
    Window        root;
    Window        event;
    Window        child;
    double        root_x;
    double        root_y;
    double        event_x;
    double        event_y;
    int           mode;
    Bool          focus;
    Bool          same_screen;
    XIButtonState       buttons;
    XIModifierState     mods;
    XIGroupState        group;
} XIEnterEvent;

typedef XIEnterEvent XILeaveEvent;
typedef XIEnterEvent XIFocusInEvent;
typedef XIEnterEvent XIFocusOutEvent;

typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;       /* XI_PropertyEvent */
    Time          time;
    int           deviceid;     /* id of the device that changed */
    Atom          property;
    int           what;
} XIPropertyEvent;

typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;
    Time          time;
    int           deviceid;
    int           sourceid;
    unsigned int  touchid;
    Window        root;
    Window        event;
    Window        child;
    int           flags;
} XITouchOwnershipEvent;

typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;
    Time          time;
    int           deviceid;
    int           sourceid;
    Window        event;
    Window        root;
    double        root_x;
    double        root_y;
    double        dx;
    double        dy;
    int           dtime;
    int           flags;
    PointerBarrier barrier;
    BarrierEventID eventid;
} XIBarrierEvent;

typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;
    Time          time;
    int           deviceid;
    int           sourceid;
    int           detail;
    Window        root;
    Window        event;
    Window        child;
    double        root_x;
    double        root_y;
    double        event_x;
    double        event_y;
    double        delta_x;
    double        delta_y;
    double        delta_unaccel_x;
    double        delta_unaccel_y;
    double        scale;
    double        delta_angle;
    int           flags;
    XIModifierState     mods;
    XIGroupState        group;
} XIGesturePinchEvent;

typedef struct {
    int           type;         /* GenericEvent */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    int           extension;    /* XI extension offset */
    int           evtype;
    Time          time;
    int           deviceid;
    int           sourceid;
    int           detail;
    Window        root;
    Window        event;
    Window        child;
    double        root_x;
    double        root_y;
    double        event_x;
    double        event_y;
    double        delta_x;
    double        delta_y;
    double        delta_unaccel_x;
    double        delta_unaccel_y;
    int           flags;
    XIModifierState     mods;
    XIGroupState        group;
} XIGestureSwipeEvent;

_XFUNCPROTOBEGIN

extern Bool     XIQueryPointer(
    Display*            display,
    int                 deviceid,
    Window              win,
    Window*             root,
    Window*             child,
    double*             root_x,
    double*             root_y,
    double*             win_x,
    double*             win_y,
    XIButtonState       *buttons,
    XIModifierState     *mods,
    XIGroupState        *group
);

extern Bool     XIWarpPointer(
    Display*            display,
    int                 deviceid,
    Window              src_win,
    Window              dst_win,
    double              src_x,
    double              src_y,
    unsigned int        src_width,
    unsigned int        src_height,
    double              dst_x,
    double              dst_y
);

extern Status   XIDefineCursor(
    Display*            display,
    int                 deviceid,
    Window              win,
    Cursor              cursor
);

extern Status   XIUndefineCursor(
    Display*            display,
    int                 deviceid,
    Window              win
);

extern Status   XIChangeHierarchy(
    Display*            display,
    XIAnyHierarchyChangeInfo*  changes,
    int                 num_changes
);

extern Status   XISetClientPointer(
    Display*            dpy,
    Window              win,
    int                 deviceid
);

extern Bool     XIGetClientPointer(
    Display*            dpy,
    Window              win,
    int*                deviceid
);

extern int      XISelectEvents(
     Display*            dpy,
     Window              win,
     XIEventMask         *masks,
     int                 num_masks
);

extern XIEventMask *XIGetSelectedEvents(
     Display*            dpy,
     Window              win,
     int                 *num_masks_return
);

extern Status XIQueryVersion(
     Display*           dpy,
     int*               major_version_inout,
     int*               minor_version_inout
);

extern XIDeviceInfo* XIQueryDevice(
     Display*           dpy,
     int                deviceid,
     int*               ndevices_return
);

extern Status XISetFocus(
     Display*           dpy,
     int                deviceid,
     Window             focus,
     Time               time
);

extern Status XIGetFocus(
     Display*           dpy,
     int                deviceid,
     Window             *focus_return);

extern Status XIGrabDevice(
     Display*           dpy,
     int                deviceid,
     Window             grab_window,
     Time               time,
     Cursor             cursor,
     int                grab_mode,
     int                paired_device_mode,
     Bool               owner_events,
     XIEventMask        *mask
);

extern Status XIUngrabDevice(
     Display*           dpy,
     int                deviceid,
     Time               time
);

extern Status XIAllowEvents(
    Display*            display,
    int                 deviceid,
    int                 event_mode,
    Time                time
);

extern Status XIAllowTouchEvents(
    Display*            display,
    int                 deviceid,
    unsigned int        touchid,
    Window              grab_window,
    int                 event_mode
);

extern int XIGrabButton(
    Display*            display,
    int                 deviceid,
    int                 button,
    Window              grab_window,
    Cursor              cursor,
    int                 grab_mode,
    int                 paired_device_mode,
    int                 owner_events,
    XIEventMask         *mask,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers_inout
);

extern int XIGrabKeycode(
    Display*            display,
    int                 deviceid,
    int                 keycode,
    Window              grab_window,
    int                 grab_mode,
    int                 paired_device_mode,
    int                 owner_events,
    XIEventMask         *mask,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers_inout
);

extern int XIGrabEnter(
    Display*            display,
    int                 deviceid,
    Window              grab_window,
    Cursor              cursor,
    int                 grab_mode,
    int                 paired_device_mode,
    int                 owner_events,
    XIEventMask         *mask,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers_inout
);

extern int XIGrabFocusIn(
    Display*            display,
    int                 deviceid,
    Window              grab_window,
    int                 grab_mode,
    int                 paired_device_mode,
    int                 owner_events,
    XIEventMask         *mask,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers_inout
);

extern int XIGrabTouchBegin(
    Display*            display,
    int                 deviceid,
    Window              grab_window,
    int                 owner_events,
    XIEventMask         *mask,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers_inout
);

extern int XIGrabPinchGestureBegin(
    Display*            display,
    int                 deviceid,
    Window              grab_window,
    int                 grab_mode,
    int                 paired_device_mode,
    int                 owner_events,
    XIEventMask         *mask,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers_inout
);

extern int XIGrabSwipeGestureBegin(
    Display*            display,
    int                 deviceid,
    Window              grab_window,
    int                 grab_mode,
    int                 paired_device_mode,
    int                 owner_events,
    XIEventMask         *mask,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers_inout
);

extern Status XIUngrabButton(
    Display*            display,
    int                 deviceid,
    int                 button,
    Window              grab_window,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers
);

extern Status XIUngrabKeycode(
    Display*            display,
    int                 deviceid,
    int                 keycode,
    Window              grab_window,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers
);

extern Status XIUngrabEnter(
    Display*            display,
    int                 deviceid,
    Window              grab_window,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers
);

extern Status XIUngrabFocusIn(
    Display*            display,
    int                 deviceid,
    Window              grab_window,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers
);

extern Status XIUngrabTouchBegin(
    Display*            display,
    int                 deviceid,
    Window              grab_window,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers
);

extern Status XIUngrabPinchGestureBegin(
    Display*            display,
    int                 deviceid,
    Window              grab_window,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers
);

extern Status XIUngrabSwipeGestureBegin(
    Display*            display,
    int                 deviceid,
    Window              grab_window,
    int                 num_modifiers,
    XIGrabModifiers     *modifiers
);

extern Atom *XIListProperties(
    Display*            display,
    int                 deviceid,
    int                 *num_props_return
);

extern void XIChangeProperty(
    Display*            display,
    int                 deviceid,
    Atom                property,
    Atom                type,
    int                 format,
    int                 mode,
    unsigned char       *data,
    int                 num_items
);

extern void
XIDeleteProperty(
    Display*            display,
    int                 deviceid,
    Atom                property
);

extern Status
XIGetProperty(
    Display*            display,
    int                 deviceid,
    Atom                property,
    long                offset,
    long                length,
    Bool                delete_property,
    Atom                type,
    Atom                *type_return,
    int                 *format_return,
    unsigned long       *num_items_return,
    unsigned long       *bytes_after_return,
    unsigned char       **data
);

extern void
XIBarrierReleasePointers(
    Display*                    display,
    XIBarrierReleasePointerInfo *barriers,
    int                         num_barriers
);

extern void
XIBarrierReleasePointer(
    Display*                    display,
    int                         deviceid,
    PointerBarrier              barrier,
    BarrierEventID              eventid
);

extern void XIFreeDeviceInfo(XIDeviceInfo       *info);

_XFUNCPROTOEND

#endif /* XINPUT2_H */
