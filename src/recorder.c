#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "recorder.h"
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>

#define OUR_APP_NAME "PipeWire-Auto-Recorder-Internal"


struct data init_data()
{
    struct data prog_data = {
        .stream_events = {
            .version = PW_VERSION_STREAM_EVENTS,
            .param_changed = on_stream_param_changed,
            .process = on_stream_process
        }
    };
    return prog_data;
}


struct node_info* find_node(struct data *self, uint32_t id)
{
    for (int i = 0; i < self->num_tracked_nodes; i++)
    {
        if (self->tracked_nodes[i].id == id)
        {
            return &self->tracked_nodes[i];
        }
    }
    return NULL;
}

void add_node(struct data *self, uint32_t id, const struct spa_dict *props, bool is_mic)
{
    if (self->num_tracked_nodes < MAX_NODES)
    {
        struct node_info* info = &self->tracked_nodes[self->num_tracked_nodes++];
        info->id = id;
        info->is_mic = is_mic;
        snprintf(info->name, sizeof(info->name), "%s", spa_dict_lookup(props, PW_KEY_NODE_NAME));
        const char *str = spa_dict_lookup(props, PW_KEY_APP_NAME);
        if (str)
        {
            snprintf(info->app_name, sizeof(info->app_name), "%s", str);
        }
        else
        {
            info->app_name[0] = '\0';
        }
    }
}

void remove_node(struct data *self, uint32_t id)
{
    for (int i = 0; i < self->num_tracked_nodes; i++)
    {
        if (self->tracked_nodes[i].id == id)
        {
            memmove(&self->tracked_nodes[i], &self->tracked_nodes[i+1], (self->num_tracked_nodes - 1 -i) * sizeof(struct node_info));
            self->num_tracked_nodes--;
            return;
        }
    }
}

struct link_info* find_link(struct data *self, uint32_t id)
{
    for(int i = 0; i < self->num_tracked_links; i++)
    {
        if (self->tracked_links[i].id == id)
        {
            return &self->tracked_links[i];
        }
    }
    return NULL;
}


void add_link(struct data *self, uint32_t id, uint32_t output_node_id, uint32_t input_node_id)
{
    if (self->num_tracked_links  < MAX_LINKS)
    {
        struct link_info* info = &self->tracked_links[self->num_tracked_links++];
        info->id = id;
        info->output_node_id = output_node_id;
        info->input_node_id = input_node_id;
    }
}

void remove_link(struct data *self, uint32_t id)
{
    for (int i = 0; i < self->num_tracked_links; i++)
    {
        if (self->tracked_links[i].id == id)
        {
            memmove(&self->tracked_links[i], &self->tracked_links[i+1], (self->num_tracked_links-1-i) * sizeof(struct link_info));
            self->num_tracked_links--;
            return;
        }
    }
}

struct recording_session* find_recording_session(struct data *self, uint32_t mic_id)
{
    for (int i = 0; i < self->num_active_recordings; i++)
    {
        if (self->active_recordings[i].mic_id == mic_id)
        {
            return &(self->active_recordings[i]);
        }
    }
    return NULL;
}

void sanitize_filename_part(char *sanitized_name, const char *original_name, size_t max_len)
{
    strncpy(sanitized_name, original_name, max_len - 1);
    sanitized_name[max_len - 1] = '\0';
    for(char *p = sanitized_name; *p; ++p)
    {
        if (*p == ' ' || *p == '/')
        {
            *p = '_';
        }
    }
}

void write_wav_file(const char *filename, struct recording_session *session)
{
    if (!session || session->buffer_size == 0)
    {
        return;
    }
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        perror("fopen");
        return;
    }
    struct wav_header header = {
        .riff_header = {'R', 'I', 'F', 'F'},
        .wave_header = {'W', 'A', 'V', 'E'},
        .fmt_header = {'f', 'm', 't', ' '},
        .data_header = {'d', 'a', 't', 'a'},
        .fmt_chunk_size = 16,
        .audio_format = 1,
        .num_channels = session->audio_info.channels,
        .sample_rate = session->audio_info.rate,
        .bits_per_sample=16,
        .byte_rate = (session->audio_info.channels * 16) / 8,
        .data_size = session->buffer_size,
        .wav_size = 36 + session->buffer_size
    };
    fwrite(&header, sizeof(header), 1, fp);
    fwrite(session->buffer, session->buffer_size, 1, fp);
    fclose(fp);
    printf("-> Recording saved to '%s' (%zu btyes)\n", filename, session->buffer_size);
}



void on_stream_process(void *userdata)
{
    struct recording_session *session = userdata;
    struct pw_buffer *b;
    if ((b = pw_stream_dequeue_buffer(session->stream)))
    {
        struct spa_buffer *buf = b->buffer;
        if(buf->datas[0].data)
        {
            void *data = buf->datas[0].data;
            uint32_t size = buf->datas[0].chunk->size;
            void* nb = realloc(session->buffer, session->buffer_size + size);
            if (nb)
            {
                session->buffer = nb;
                memcpy((uint8_t*)session->buffer + session->buffer_size, data, size);
                session->buffer_size += size;
            }
        }
        pw_stream_queue_buffer(session->stream, b);
    }
}

void on_stream_param_changed(void *userdata, uint32_t id, const struct spa_pod *param)
{
    struct recording_session *session = userdata;
    if (param != NULL && id == SPA_PARAM_Format)
    {
        spa_format_audio_raw_parse(param, &session->audio_info);
        pw_stream_update_params(session->stream, NULL, 0);
    }
}

void create_recording_session(struct data *data, uint32_t mic_id, const char* mic_name, const char *app_name)
{
    if (data->num_active_recordings >= MAX_RECORDINGS)
    {
        return;
    }
    printf("== Starting recording for microphone '%s' (triggered by '%s')\n", mic_name, app_name);
    struct recording_session *session = &data->active_recordings[data->num_active_recordings++];
    memset(session, 0, sizeof(*session));
    session->mic_id = mic_id;
    snprintf(session->mic_name, sizeof(session->mic_name), "%s", mic_name);
    snprintf(session->trigger_app_name, sizeof(session->trigger_app_name), "%s", app_name);
    session->ref_count = 1;
    session->stream = pw_stream_new_simple(
        pw_main_loop_get_loop(data->loop), mic_name,
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture",
                          PW_KEY_APP_NAME, OUR_APP_NAME, NULL),
        &data->stream_events, session);
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1] = {
        spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_S16, .channels = 2, .rate = 48000)) };
    pw_stream_connect(session->stream, PW_DIRECTION_INPUT, mic_id,
                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS, params, 1);
}

void destroy_recording_session(struct data *self, struct recording_session *session)
{
    printf("== Stopping recording for microphone '%s'. Saving file...\n", session->mic_name);
    char filename[512], san_mic_name[128], san_app_name[128];
    time_t now = time(NULL);

    sanitize_filename_part(san_mic_name, session->mic_name, sizeof(san_mic_name));
    sanitize_filename_part(san_app_name, session->trigger_app_name, sizeof(san_app_name));

    strftime(filename, sizeof(filename)-1, "%Y-%m-%d_%H-%M-%S", localtime(&now));
    snprintf(filename + strlen(filename), sizeof(filename) - strlen(filename), "_%s_by_%s.wav", san_mic_name, san_app_name);

    write_wav_file(filename, session);
    pw_stream_destroy(session->stream);
    free(session->buffer);
    for (int i = 0; i < self->num_active_recordings; i++)
    {
        if (self->active_recordings[i].mic_id == session->mic_id)
        {
            memmove(&self->active_recordings[i], &self->active_recordings[i+1], (self->num_active_recordings - 1 - i)  * sizeof(struct recording_session));
            self->num_active_recordings--;
            break;
        }
    }
}

void registry_global(void *data, uint32_t id, uint32_t permissions,
                const char *type, uint32_t version, const struct spa_dict *props) {
    if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
        const char *mc = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        add_node(data, id, props, (mc && strstr(mc, "Audio/Source")));
    } else if (strcmp(type, PW_TYPE_INTERFACE_Link) == 0) {
        const char *out_s = spa_dict_lookup(props, PW_KEY_LINK_OUTPUT_NODE);
        const char *in_s = spa_dict_lookup(props, PW_KEY_LINK_INPUT_NODE);
        if (out_s && in_s) {
            uint32_t out_id = atoi(out_s), in_id = atoi(in_s);
            if (out_id == in_id) return;

            add_link(data, id, out_id, in_id);
            struct node_info* mic = find_node(data, out_id);
            struct node_info* app = find_node(data, in_id);

            if (mic && mic->is_mic && app && strcmp(app->app_name, OUR_APP_NAME) == 0) return;

            if (mic && mic->is_mic) {
                const char *app_name_str = app ? app->name : "UnknownApp";
                printf("-> Mic '%s' in use by '%s'\n", mic->name, app_name_str);
                struct recording_session *s = find_recording_session(data, mic->id);
                if (s) {
                    s->ref_count++; printf("   ...incrementing ref count to %d\n", s->ref_count);
                } else {
                    create_recording_session(data, mic->id, mic->name, app_name_str);
                }
            }
        }
    }
}

void registry_global_remove(void *data, uint32_t id)
{
    struct link_info* link = find_link(data, id);
    if (link)
    {
        if (link->output_node_id == link->input_node_id)
        {
            remove_link(data, id);
            return;
        }
        struct node_info* mic = find_node(data, link->output_node_id);
        struct node_info* app = find_node(data, link->input_node_id);

        if (mic && mic->is_mic && app && strcmp(app->app_name, OUR_APP_NAME) == 0)
        {
            remove_link(data, id);
            return;
        }
        if (mic && mic->is_mic)
        {
            printf("<- Mic '%s' released by '%s'\n", mic->name, app ? app->name : "Unknown App");
            struct recording_session *s = find_recording_session(data, mic->id);
            if (s)
            {
                s->ref_count--;
                printf("...decremting ref count to %d\n", s->ref_count);
                if (s->ref_count <= 0)
                {
                    destroy_recording_session(data, s);
                }
            }
            remove_link(data, id);
        }
        else
        {
            remove_node(data, id);
        }
    }
}
