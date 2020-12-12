
#include <jack/jack.h>
#include <stdio.h>

#include "jack.h"


static struct jack_output_data
{
    jack_port_t *output_port;
    jack_client_t *client;
    receiver_format_t receiver_format;
}
jo_data;


/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 */
int process (jack_nframes_t nframes, void *arg)
{
	jack_default_audio_sample_t *out;
	
	out = jack_port_get_buffer(jo_data.output_port, nframes);
	memcpy (out, in,
		sizeof (jack_default_audio_sample_t) * nframes);

	return 0;      
}




int jack_output_send(receiver_data_t *data)
{
}



/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown(void *arg)
{
    fprintf(stderr, "client was disconnected from JACK server\n");
	exit(1);
}

/**
 * Called when scream client terminates
 */
void scream_shutdown()
{
    jack_client_close(jo_data.client);
}



int jack_output_init(int latency, char *stream_name)
{
	const char **ports;
	jack_status_t status;

    // init receiver format to track changes
    jo_data.receiver_format.sample_rate = 0;
    jo_data.receiver_format.sample_size = 0;
    jo_data.receiver_format.channels = 2;
    jo_data.receiver_format.channel_map = 0x0003;


	jo_data.client = jack_client_open(stream_name, JackNullOption, &status, NULL);
	if (jo_data.client == NULL)
    {
		fprintf(stderr, "jack_client_open() failed, status = 0x%2.0x\n", status);
		if (status & JackServerFailed) 
        {
			fprintf(stderr, "Unable to connect to JACK server\n");
		}
		return 1;
	}

    // REVIEW_NEEDED
    atexit(scream_shutdown);
    
	if (status & JackServerStarted)
    {
		fprintf(stderr, "JACK server started\n");
	}
	if (status & JackNameNotUnique)
    {
		const char *client_name = jack_get_client_name(jo_data.client);
		fprintf(stderr, "unique name `%s' assigned\n", client_name);
	}

	/* tell the JACK server to call `process()' whenever
	   there is work to be done.
	*/

	jack_set_process_callback(jo_data.client, process, 0);

	jack_on_shutdown(jo_data.client, jack_shutdown, 0);

	printf("engine sample rate: %" PRIu32 "\n", jack_get_sample_rate(jo_data.client));


	jo_data.output_port = jack_port_register(jo_data.client, "output", JACK_DEFAULT_AUDIO_TYPE,
                                            JackPortIsOutput, 0);

	if (jo_data.output_port == NULL)
    {
		fprintf(stderr, "no more JACK ports available\n");
		return 1;
	}

	/* Tell the JACK server that we are ready to roll.  Our
	 * process() callback will start running now. */

	if (jack_activate(jo_data.client))
    {
		fprintf(stderr, "cannot activate client");
		return 1;
	}

	/* Connect the ports.  You can't do this before the client is
	 * activated, because we can't make connections to clients
	 * that aren't running.  Note the confusing (but necessary)
	 * orientation of the driver backend ports: playback ports are
	 * "input" to the backend, and capture ports are "output" from
	 * it.
	 */

	
	ports = jack_get_ports(jo_data.client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
	if (ports == NULL)
    {
		fprintf(stderr, "no physical playback ports\n");
		return 1;
	}

	if (jack_connect(jo_data.client, jack_port_name(jo_data.output_port), ports[0]))
    {
		fprintf(stderr, "cannot connect output ports\n");
	}

	free(ports);


    return 0;
}

