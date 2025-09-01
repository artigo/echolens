#ifndef RECORDER_H
#define RECORDER_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>

#define OUR_APP_NAME "PipeWire-Auto-Recorder-Internal"

struct wav_header
{
    char riff_header[4];
    uint32_t wav_size;
    char wave_header[4];
    char fmt_header[4];
    uint32_t fmt_chunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_header[4];
    uint32_t data_size;
};

#define MAX_NODES 100
struct node_info
{
    uint32_t id;
    char name[256];
    bool is_mic;
    char app_name[128];
};

#define MAX_LINKS 100
struct link_info
{
    uint32_t id;
    uint32_t output_node_id;
    uint32_t input_node_id;
};

#define MAX_RECORDINGS 10
struct recording_session
{
    uint32_t mic_id;
    char mic_name[256];
    int ref_count;
    char trigger_app_name[256];
    struct pw_stream *stream;
    struct spa_hook stream_listener;
    struct spa_audio_info_raw audio_info;
    void *buffer;
    size_t buffer_size;
};

struct data
{
    struct pw_main_loop *loop;
    struct pw_context *context;
    struct pw_core *core;
    struct pw_registry *registry;
    struct spa_hook registry_listener;
    int num_tracked_nodes;
    struct node_info tracked_nodes[MAX_NODES];
    int num_tracked_links;
    struct link_info tracked_links[MAX_LINKS];
    int num_active_recordings;
    struct recording_session active_recordings[MAX_RECORDINGS];
    struct pw_stream_events stream_events;
};

//Intializes data structure, which holds all of the application's data.
struct data init_data();

//Nodes are the basic building blocks, representing devices or applications.
struct node_info* find_node(struct data *self, uint32_t id);
void add_node(struct data *self, uint32_t id, const struct spa_dict *props, bool is_mic);
void remove_node(struct data *self, uint32_t id);

//Links connect the output ports of one node to the input ports of another.
struct link_info* find_link(struct data *self, uint32_t id);
void add_link(struct data *self, uint32_t id, uint32_t output_node_id, uint32_t input_node_id);
void remove_link(struct data *self, uint32_t id);

void sanitize_filename_part(char *sanitized_name, const char *original_name, size_t max_len);
void write_wav_file(const char *filename, struct recording_session *session);

//This is the main audio processing callback. It is called by PipeWire whenever new audio data is available. 
void on_stream_process(void *userdata);
//This callback is triggered when an audio parameter changes on the stream.
void on_stream_param_changed(void *userdata, uint32_t id, const struct spa_pod *param);

//These functions handle the creation and destruction of audio recording sessions.
struct recording_session* find_recording_session(struct data *self, uint32_t mic_id);
void create_recording_session(struct data *app_data, uint32_t mic_id, const char* mic_name, const char *app_name);
void destroy_recording_session(struct data *self, struct recording_session *session);

//Event handlers for the registry.
void registry_global(void *data, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const struct spa_dict *props);
void registry_global_remove(void *data, uint32_t id);


#endif
