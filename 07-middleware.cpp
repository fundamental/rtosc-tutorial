#include <rtosc/thread-link.h>
#include <rtosc/ports.h>
#include <lo/lo.h>
#include <string>
#include <sstream>

using std::string;

extern rtosc::ThreadLink bToU, uToB;
extern rtosc::Ports ports;
static lo_server server;
static string last_url, curr_url;

void path_search(const char *m)
{
    using rtosc::Ports;
    using rtosc::Port;

    //assumed upper bound of 32 ports (may need to be resized)
    char         types[129];
    rtosc_arg_t  args[128];
    size_t       pos    = 0;
    const Ports *ports  = NULL;
    const char  *str    = rtosc_argument(m,0).s;
    const char  *needle = rtosc_argument(m,1).s;

    //zero out data
    memset(types, 0, sizeof(types));
    memset(args,  0, sizeof(args));

    if(!*str) {
        ports = &::ports;
    } else {
        const Port *port = ::ports.apropos(rtosc_argument(m,0).s);
        if(port)
            ports = port->ports;
    }

    if(ports) {
        //RTness not confirmed here
        for(const Port &p:*ports) {
            if(strstr(p.name, needle)!=p.name)
                continue;
            types[pos]    = 's';
            args[pos++].s = p.name;
            types[pos]    = 'b';
            if(p.metadata && *p.metadata) {
                args[pos].b.data = (unsigned char*) p.metadata;
                auto tmp = rtosc::Port::MetaContainer(p.metadata);
                args[pos++].b.len  = tmp.length();
            } else {
                args[pos].b.data = (unsigned char*) NULL;
                args[pos++].b.len  = 0;
            }
        }
    }

    //Reply to requester [These messages can get quite large]
    char buffer[1024*20];
    size_t length = rtosc_amessage(buffer, sizeof(buffer), "/paths", types, args);
    if(length) {
        lo_message msg  = lo_message_deserialise((void*)buffer, length, NULL);
        lo_address addr = lo_address_new_from_url(last_url.c_str());
        if(addr)
            lo_send_message(addr, buffer, msg);
    }
}

static int handler_function(const char *path, const char *types, lo_arg **argv,
        int argc, lo_message msg, void *user_data)
{
    (void) types;
    (void) argv;
    (void) argc;
    (void) user_data;
    lo_address addr = lo_message_get_source(msg);
    if(addr) {
        const char *tmp = lo_address_get_url(addr);
        if(tmp != last_url) {
            uToB.write("/echo", "ss", "OSC_URL", tmp);
            last_url = tmp;
        }

    }

    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    size_t size = 2048;
    lo_message_serialise(msg, path, buffer, &size);
    if(!strcmp(buffer, "/path-search") && !strcmp("ss", rtosc_argument_string(buffer))) {
        path_search(buffer);
    } else if(!strcmp(buffer, "/oscdoc") && !*rtosc_argument_string(buffer)) {
        rtosc::OscDocFormatter oscformatter{&::ports, "", "", "", "", ""};
        std::stringstream s;
        s << oscformatter;

        lo_address addr = lo_address_new_from_url(last_url.c_str());
        if(addr)
            lo_send(addr, "/oscdoc", "s", s.str().c_str());
    } else
        uToB.raw_write(buffer);

    return 0;
}

void middleware_init(void)
{
    server = lo_server_new_with_proto(NULL, LO_UDP, NULL);
    lo_server_add_method(server, NULL, NULL, handler_function, NULL);
    fprintf(stderr, "lo server running on %d\n", lo_server_get_port(server));
}

void middleware_tick(void)
{
    lo_server_recv_noblock(server, 10);
    while(bToU.hasNext()) {
        const char *rtmsg = bToU.read();
        if(!strcmp(rtmsg, "/echo")
                && !strcmp(rtosc_argument_string(rtmsg),"ss")
                && !strcmp(rtosc_argument(rtmsg,0).s, "OSC_URL")) {
            curr_url = rtosc_argument(rtmsg,1).s;
        } else if(!strcmp(rtmsg, "/broadcast")) {
            //Ignore for this example
        } else if(!curr_url.empty()) {
            //Send to known url
            lo_message msg  = lo_message_deserialise((void*)rtmsg,
                    rtosc_message_length(rtmsg, bToU.buffer_size()), NULL);
            lo_address addr = lo_address_new_from_url(curr_url.c_str());
            lo_send_message(addr, rtmsg, msg);
        }
    }
}
