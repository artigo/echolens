#include "recorder.h"

int main(int argc, char *argv[])
{
    struct data data = init_data();
    const struct pw_registry_events registry_events = {
        PW_VERSION_REGISTRY_EVENTS, .global = registry_global, .global_remove = registry_global_remove,
    };
    pw_init(&argc, &argv);
    printf("Starting automatic microphone recorder.\n");
    printf("Recording will start when a mic is used and save when it's released.\n");

    data.loop = pw_main_loop_new(NULL);
    data.context = pw_context_new(pw_main_loop_get_loop(data.loop), NULL, 0);
    data.core = pw_context_connect(data.context, NULL, 0);
    data.registry = pw_core_get_registry(data.core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(data.registry, &data.registry_listener, &registry_events, &data);

    pw_main_loop_run(data.loop);

    pw_core_disconnect(data.core);
    pw_context_destroy(data.context);
    pw_main_loop_destroy(data.loop);
    return 0;
}
