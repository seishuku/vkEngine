#ifndef __EVENT_H__
#define __EVENT_H__

typedef enum
{
	EVENT_KEYDOWN,
	EVENT_KEYUP,
	EVENT_MOUSEDOWN,
	EVENT_MOUSEUP,
	EVENT_MOUSEMOVE,
	NUM_EVENTS
} EventID;

bool Event_Add(EventID ID, void (*Callback)(void *Arg));
bool Event_Delete(EventID ID);
bool Event_Trigger(EventID ID, void *Arg);

#endif
