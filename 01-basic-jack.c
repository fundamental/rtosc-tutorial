#include <jack/jack.h>
#include "jack_osc.h"
#include <unistd.h>

jack_client_t *client;
jack_port_t   *port;
jack_port_t   *josc;
float Fs;

int process(unsigned nframes, void *v)
{
    (void) v;
    float *output = (float*) jack_port_get_buffer(port, nframes);

    void *josc_buf = jack_port_get_buffer(josc, nframes);
    jack_midi_event_t in_event;
	jack_nframes_t event_count = jack_midi_get_event_count(josc_buf);
	if(event_count)
	{
		for(unsigned i=0; i<event_count; i++)
		{
            //Do nothing
            (void) in_event;
		}
	}


    for(unsigned i=0; i<nframes; ++i)
        output[i] = 0.0;
    return 0;
}

int main()
{
	const char *client_name = "rtosc-tutorial";
	jack_options_t options = JackNullOption;
	jack_status_t status;

	client = jack_client_open(client_name, options, &status, NULL);
	if(!client)
        return 1;

	jack_set_process_callback(client, process, 0);
	//jack_on_shutdown(client, jack_shutdown, 0);

    Fs = jack_get_sample_rate(client);

	port = jack_port_register (client, "out",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);
    josc = jack_port_register(client, "osc",
                      JACK_DEFAULT_OSC_TYPE,
                      JackPortIsInput, 0);

    //Setup
	jack_activate(client);

    while(1)
        sleep(1);
}
