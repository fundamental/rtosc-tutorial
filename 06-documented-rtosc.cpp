#include <jack/jack.h>
#include <rtosc/rtosc.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <rtosc/rtosc.h>
#include <rtosc/ports.h>
#include <rtosc/port-sugar.h>
#include <rtosc/thread-link.h>
#include <fstream>
#include "jack_osc.h"
//Global
static float Fs=0.0;
rtosc::ThreadLink bToU(1024,20), uToB(1024,20);

//Type Definition
struct osc_t
{
    //Internal
    float state;
};

struct sequencer_t
{
    //External
    float freq[8];
    float noise_decay;
    float noise_level;
    float bpm;

    //Internal
    float norm_freq[8];
    float noise_decay_internal;
    float noise_state;
    float t;
};

struct lfo_t
{
    //External
    float freq;
    float amount;

    //Internal
    float state;
};

struct lpf_t
{
    //External
    float f;
    float Q;

    //Internal
    float z1, z2;
};

//Noise Generator
float randf(void)
{
    return rand()*2.0/RAND_MAX - 1.0;
}
void gen_noise(float *out, float *in, unsigned nframes)
{
    for(unsigned i=0; i<nframes; ++i)
        out[i] = in[i]*randf();
}


//Square Oscillator
void init_osc(struct osc_t *osc)
{
    osc->state = 0.0;
}

void gen_square(float *out, float *in, osc_t *osc, unsigned nframes)
{
    for(unsigned i=0; i<nframes; ++i) {
        out[i] = osc->state > 0.5 ? 0.5 : -0.5;
        osc->state += in[i];
        if(osc->state > 1.0)
            osc->state -= 1.0;
    }
}


//Sequencer
void update_seq(sequencer_t *seq)
{
    for(int i=0; i<8; ++i)
        seq->norm_freq[i] = seq->freq[i]/Fs;
    seq->noise_decay_internal = powf(seq->noise_decay, seq->bpm/(Fs*60.0));
}

void init_seq(sequencer_t *seq)
{
    for(int i=0; i<8; ++i)
        seq->freq[i] = 80*i;
    seq->noise_decay = 0.0;
    seq->noise_level = 0.0;
    seq->noise_state = 0.0;
    seq->bpm = 120.0;
    seq->t = 0.0;
    update_seq(seq);
}

void gen_seq(float *out_f, float *out_amp, sequencer_t *seq, unsigned nframes)
{
    update_seq(seq);
    const float dt = seq->bpm/(Fs*60);
    int ti = seq->t;

    for(unsigned i=0; i<nframes; ++i) {
        int nti = seq->t;
        seq->t += dt;
        if(seq->t >= 8)
            seq->t -= 8;
        if(nti != ti)
            seq->noise_state = 0.2;
        out_f[i]   = seq->norm_freq[nti];
        out_amp[i] = seq->noise_level*seq->noise_state;
        seq->noise_state *= seq->noise_decay_internal;
    }
}

//LFO

void init_lfo(lfo_t *lfo)
{
    lfo->freq   = 1.0;
    lfo->amount = 0.2;
}

void gen_lfo(float *out, lfo_t *lfo, unsigned nframes)
{
    const float dt = lfo->freq/Fs;

    for(unsigned i=0; i<nframes; ++i) {
        lfo->state += dt;
        if(lfo->state > 1.0)
            lfo->state -= 1.0;
        out[i] = sinf(2*M_PI*lfo->state);
    }
}

//LPF


void init_lpf(lpf_t *lpf)
{
    lpf->f = 8e3;
    lpf->Q = 2.0;
    lpf->z1 = 0.0;
    lpf->z2 = 0.0;
}

void do_filter(float *out, float *in, float *in_f, lpf_t *lpf, unsigned nframes)
{
    float base_freq = lpf->f;
    for(unsigned i=0; i<nframes; ++i) {
        float freq = pow(2,log(base_freq)/log(2)+in_f[i]);
        float f = tan(freq*3.14/Fs);
        float r = f + 1 /lpf->Q;
        float g = 1 / (f * r + 1);

        float hp = (in[i] - r*lpf->z1 - lpf->z2) * g;
        float bp = lpf->z1 + f*hp;
        float lp = lpf->z2 + f*bp;
        lpf->z1 += 2*f*hp;
        lpf->z2 += 2*f*bp;
        out[i] = lp;
    }
};

//Util
void do_sum(float *out, float *in1, float *in2, unsigned nframes)
{
    for(unsigned i=0; i<nframes; ++i)
        out[i] = in1[i] + in2[i];
    for(unsigned i=0; i<nframes; ++i)
        if(out[i] > 1.0)
            out[i] = 1.0;
    for(unsigned i=0; i<nframes; ++i)
        if(out[i] < -1.0)
            out[i] = -1.0;
}

//Dispatch Container
// tag::rtdata-impl[]
class RtDataImpl: public rtosc::RtData
{
    public:
        RtDataImpl(void)
        {
            memset(internal_buffer, 0, sizeof(internal_buffer));
            loc      = internal_buffer;
            loc_size = sizeof(internal_buffer);
            obj      = NULL;
        }

        virtual void reply(const char *msg) override
        {
            bToU.raw_write(msg);
        }
    private:
        char internal_buffer[256];
};
// end::rtdata-impl[]

struct osc_t osc;
struct sequencer_t seq;
struct lfo_t lfo;
struct lpf_t filter;


// tag::ports[]
#define Units(x) rMap(units, x)
#define UnitLess rProp(unitless)
#define rObject sequencer_t
rtosc::Ports seq_ports = {
    rArrayF(freq, 8, rLog(0.1, 10e3), Units(Hz), "Frequency"),
    rParamF(noise_level, rLinear(0, 3), UnitLess, "White Noise Peak Gain"),
    rParamF(noise_decay, rLinear(1e-4,1), UnitLess, "Noise Decay Over Step"),
    rParamF(bpm, rLinear(0,1000), Units(bpm), "Beats Per Minute"),
};
#undef rObject

#define rObject lfo_t
rtosc::Ports lfo_ports = {
    rParamF(freq, rLog(0.1, 1e3), Units(Hz), "Frequency"),
    rParamF(amount, rLinear(0,8), UnitLess, "LFO Strength"),
};
#undef rObject

#define rObject lpf_t
rtosc::Ports filter_ports = {
    rParamF(f, rLog(0.1, 10e3), "Cutoff Frequency"),
    rParamF(Q, rLinear(0,5), "Q"),
};
#undef rObject

#define BasePort(name,doc) {#name "/", rDoc(doc), &name##_ports, [](const char *msg, \
                       rtosc::RtData &d)\
                       {while(*msg != '/') msg++; msg++; d.obj = &name; \
                       name##_ports.dispatch(msg,d);}}

rtosc::Ports ports = {
    BasePort(seq, "Sequencer"),
    BasePort(lfo, "Low Frequency Oscillator"),
    BasePort(filter, "Low Pass Filter"),
    {"echo", rProp(internal) rDoc("Echo Message Back") , 0,
            [](const char *m, rtosc::RtData &d) {d.reply(m-1);}},
};
// end::ports[]

jack_client_t *client;
jack_port_t   *port;
jack_port_t   *josc;

int process(unsigned nframes, void *v)
{
    (void) v;
    float seq_sqr[nframes];
    float seq_noise[nframes];
    float sqr[nframes];
    float noise[nframes];
    float lfo_f_out[nframes];
    float filter_in[nframes];
    float *output = (float*) jack_port_get_buffer(port, nframes);
	
    void *josc_buf = jack_port_get_buffer(josc, nframes);
    jack_midi_event_t in_event;
	jack_nframes_t event_count = jack_midi_get_event_count(josc_buf);
    // tag::buf-read[]
	if(event_count)
	{
		for(unsigned i=0; i<event_count; i++)
		{
			jack_midi_event_get(&in_event, josc_buf, i);
            assert(*in_event.buffer == '/');
            RtDataImpl d;
            ports.dispatch((char*)in_event.buffer+1, d);
		}
	}

    while(uToB.hasNext())
    {
        RtDataImpl d;
        ports.dispatch(uToB.read(), d);
    }
    // end::buf-read


    gen_seq(seq_sqr, seq_noise, &seq, nframes);
    gen_noise(noise, seq_noise, nframes);
    gen_square(sqr, seq_sqr, &osc, nframes);
    gen_lfo(lfo_f_out, &lfo, nframes);
    do_sum(filter_in, noise, sqr, nframes);
    do_filter(output, filter_in, lfo_f_out, &filter, nframes);
    return 0;
}

extern void middleware_init(void);
extern void middleware_tick(void);

// tag::oscdump[]
int main(int argc, char **argv)
{
    if(argc == 3 && !strcmp(argv[1], "--dump-oscdoc")) {
        std::ofstream file(argv[2], std::ofstream::out);
        rtosc::OscDocFormatter formatter{&ports, "rtosc-tutorial",
            "http://example.com/", "http://example.com/",
                "John", "Smith"};
        file << formatter;
        file.close();
    }
    // end::oscdump[]


    middleware_init();
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
    init_osc(&osc);
    init_seq(&seq);
    init_lfo(&lfo);
    init_lpf(&filter);

	jack_activate(client);

    while(1)
        middleware_tick();
}
