#ifndef __EVENT_H__
#define __EVENT_H__

#undef EVENT_MAX // Frickin' Windows. :\

typedef enum
{
	EVENT_KEYDOWN,
	EVENT_KEYUP,
	EVENT_MAX
} EventID;

bool Event_Add(EventID ID, void (*Callback)(void *Arg));
bool Event_Delete(EventID ID);
bool Event_Trigger(EventID ID, void *Arg);

#endif
