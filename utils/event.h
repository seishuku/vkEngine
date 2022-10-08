#ifndef __EVENT_H__
#define __EVENT_H__

#ifdef WIN32
#undef EVENT_MAX // Frickin' Windows. :(
#endif

typedef enum
{
	EVENT_KEYDOWN,
	EVENT_KEYUP,
	EVENT_MOUSE,
	EVENT_MAX
} EventID;

bool Event_Add(EventID ID, void (*Callback)(void *Arg));
bool Event_Delete(EventID ID);
bool Event_Trigger(EventID ID, void *Arg);

#endif
