#include <stdint.h>
#include <stdbool.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include "../../system/system.h"
#include "../audio.h"

static uint8_t buffer[MAX_AUDIO_SAMPLES*sizeof(int16_t)*2];
static struct pw_stream *stream=NULL;
static struct pw_thread_loop *loop=NULL;

static void on_process(void *data)
{
	struct pw_buffer *buffer=pw_stream_dequeue_buffer(stream);

	if(buffer==NULL)
		return;

	if(buffer->buffer->datas[0].data==NULL)
		return;

	int stride=sizeof(int16_t)*2;
	int n_frames=buffer->buffer->datas[0].maxsize/stride;

	if(buffer->requested)
		n_frames=SPA_MIN((int)buffer->requested, n_frames);

	Audio_FillBuffer(buffer->buffer->datas[0].data, n_frames);

	buffer->buffer->datas[0].chunk->offset=0;
	buffer->buffer->datas[0].chunk->stride=stride;
	buffer->buffer->datas[0].chunk->size=n_frames*stride;

	pw_stream_queue_buffer(stream, buffer);
}

static volatile bool streamReady=false;

static void on_state_changed(void *data, enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
    DBGPRINTF(DEBUG_INFO, "Stream state changed: %s to %s\n", pw_stream_state_as_string(old), pw_stream_state_as_string(state));

    if(state==PW_STREAM_STATE_PAUSED||state==PW_STREAM_STATE_STREAMING)
	{
		streamReady=true;
		pw_thread_loop_signal(loop, false);
	}
}

static const struct pw_stream_events stream_events=
{
        PW_VERSION_STREAM_EVENTS,
        .process=on_process,
		.state_changed=on_state_changed,
};

bool AudioPipeWire_Init(void)
{
	pw_init(NULL, NULL);

	loop=pw_thread_loop_new(NULL, NULL);

	if(loop==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - PipeWire: Failed to create loop.\n");
		return false;
	}

	pw_thread_loop_lock(loop);

	if(pw_thread_loop_start(loop)<0)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - PipeWire: Failed to start loop.\n");
		pw_thread_loop_unlock(loop);
		return false;
	}

	stream=pw_stream_new_simple(pw_thread_loop_get_loop(loop), "vkEnginePipeWire",
								pw_properties_new(
									PW_KEY_MEDIA_TYPE, "Audio",
									PW_KEY_MEDIA_CATEGORY, "Playback",
									PW_KEY_MEDIA_ROLE, "Music",
									NULL),
								&stream_events,
								NULL);

	if(stream==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - PipeWire: Failed to create stream.\n");
		pw_thread_loop_unlock(loop);
		return false;
	}

	struct spa_pod_builder builder=SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	struct spa_pod_frame f;

	spa_pod_builder_push_object(&builder, &f, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
	spa_pod_builder_add(&builder, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_audio), SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
	spa_pod_builder_add(&builder, SPA_FORMAT_AUDIO_format, SPA_POD_Id(SPA_AUDIO_FORMAT_S16), 0);
	spa_pod_builder_add(&builder, SPA_FORMAT_AUDIO_rate, SPA_POD_Int(AUDIO_SAMPLE_RATE), 0);
	spa_pod_builder_add(&builder, SPA_FORMAT_AUDIO_channels, SPA_POD_Int(2), 0);
	const struct spa_pod *params=spa_pod_builder_pop(&builder, &f);

	if(pw_stream_connect(stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
	                  PW_STREAM_FLAG_AUTOCONNECT|PW_STREAM_FLAG_MAP_BUFFERS|PW_STREAM_FLAG_RT_PROCESS,
	                  &params, 1)<0)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - PipeWire: Failed to connect stream.\n");
		pw_thread_loop_unlock(loop);
		return false;
	}

	// Wait for the stream to come up and continue on
	while(!streamReady)
		pw_thread_loop_wait(loop);

	pw_thread_loop_unlock(loop);

	return true;
}

void AudioPipeWire_Destroy(void)
{
	pw_thread_loop_stop(loop);

	pw_thread_loop_lock(loop);
	pw_stream_destroy(stream);
	pw_thread_loop_unlock(loop);
	pw_thread_loop_destroy(loop);

	pw_deinit();
}
