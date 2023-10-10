#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "event.h"

static struct
{
	void (*Callback)(void *Arg);
} EventList[NUM_EVENTS];

bool Event_Add(EventID ID, void (*Callback)(void *Arg))
{
	if(ID<0||ID>=NUM_EVENTS||Callback==NULL)
		return false;

	EventList[ID].Callback=Callback;

	return true;
}

bool Event_Delete(EventID ID)
{
	if(ID<0||ID>=NUM_EVENTS)
		return false;

	EventList[ID].Callback=NULL;

	return true;

}

bool Event_Trigger(EventID ID, void *Arg)
{
	if(ID<0||ID>=NUM_EVENTS||EventList[ID].Callback==NULL)
		return false;

	EventList[ID].Callback(Arg);

	return true;
}
