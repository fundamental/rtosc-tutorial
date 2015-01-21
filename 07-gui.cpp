#include <FL/Fl_Window.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl.H>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <rtosc/rtosc.h>
#include <lo/lo.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <err.h>

/**********************************************************************
 *                           FLTK GUI Code                            *
 *                                                                    *
 *    Define a simple set of linear sliders based upon the contents   *
 *    of the 'fields' array and using oscdoc for reflection           *
 **********************************************************************/
const char *fields[] = {
    "/seq/freq1",
    "/lfo/freq",
    "/filter/f"
};
unsigned nfields = sizeof(fields)/sizeof(*fields);
Fl_Slider **sliders;

void send(const char *path, const char *args="", ...);
void slider_cb(Fl_Slider *s, void *data)
{
    int offset = (size_t)data;
    send(fields[offset], "f", s->value());
}

Fl_Window *fltk_window;
void gui_init(void)
{
    fltk_window  = new Fl_Window(300,300, "rtosc Tutorial GUI");
    Fl_Pack *pack = new Fl_Pack(0,0,300,300);
    pack->type(Fl_Pack::HORIZONTAL);
    sliders = new Fl_Slider*[nfields];
    for(size_t i=0; i<nfields; ++i) {
        sliders[i] = new Fl_Slider(0,0,100,20);
        sliders[i]->type(FL_VERTICAL);
        sliders[i]->callback((Fl_Callback_p)slider_cb, (void*)i);
    }
}

void gui_tick(void)
{
    Fl::wait(0.1f);
}


/**********************************************************************
 *                         Xml Handling Code                          *
 *                                                                    *
 *    Parse the oscdoc to extract information about what sorts of     *
 *    messages will be accepted and to validate the user generated    *
 *    list of port names to work with.                                *
 **********************************************************************/
bool oscdoc_str_eq(const char *path, const char *pattern)
{
    if(!strcmp(pattern, path))
        return true;

    //Compare ignoring {}
    while(*pattern && *path)
    {
        if(*pattern != '[') {
            if(*pattern != *path)
                break;
            pattern++;
            path++;
        } else {
            while(*pattern && *pattern != ']')
                pattern++;
            if(*pattern == ']')
                pattern++;
            while(isdigit(*path))
                path++;
        }
    }

    return !*path && !*pattern;
}

bool pattern_is(xmlNode *node, const char *pattern)
{
    for(auto *props = node->properties; props; props = props->next) {
        if(!strcmp("pattern", (char*)props->name) &&
                oscdoc_str_eq(pattern, (char*)props->children->content)) {
            return true;
        }
    }
    return false;
}

// - message_in pattern=xyz
//   - desc
//   - param_f min=abc max=def
xmlNode *findSubtree(xmlNode *root, const std::string pattern)
{
    if(!root->children)
        return NULL;

    root = root->children;

    for(xmlNode *node = root; node; node = node->next) {
        if(node->type == XML_ELEMENT_NODE &&
                !strcmp("message_in", (const char*)node->name) &&
                pattern_is(node, pattern.c_str())) {
            return node->children;
        }
    }
    return NULL;
}

xmlDoc *doc = NULL;
void getPortParams(const std::string name,
        float &min, float &max, std::string &doc)
{
    auto *node_ = findSubtree(xmlDocGetRootElement(::doc), name);
    if(!node_)
        errx(1, "Mismatch between oscdoc and specified fields");

    for(xmlNode *node = node_; node; node = node->next) {
        if(node->type == XML_ELEMENT_NODE &&
                !strcmp("desc", (const char*)node->name))
            doc = (const char*)node->children->content;

        if(node->type == XML_ELEMENT_NODE &&
                !strcmp("param_f", (const char*)node->name)) {
            for(auto *props = node->children->next->properties; props; props = props->next) {
                if(!strcmp("min", (char*)props->name))
                    min = atof((char*)props->children->content);
                if(!strcmp("max", (char*)props->name))
                    max = atof((char*)props->children->content);
            }
        }
    }
}

void updateSliderConfig(const char *data, size_t data_size)
{
    LIBXML_TEST_VERSION

    doc = xmlReadMemory(data, data_size, NULL, NULL, 0);

    if (doc == NULL) {
        printf("error: Invalid oscdoc received\n");
    }

    for(unsigned i=0; i<nfields; ++i) {
        float min = 0.0f;
        float max = 1.0f;
        std::string doc = "undocumented";
        getPortParams(fields[i], min, max, doc);
        sliders[i]->tooltip(strdup(doc.c_str()));
        sliders[i]->minimum(min);
        sliders[i]->maximum(max);
        printf("port is [%f,%f], '%s'\n", min, max, doc.c_str());
    }


    xmlFreeDoc(doc);
    xmlCleanupParser();
}


/**********************************************************************
 *                         liblo Interface                            *
 *                                                                    *
 **********************************************************************/
const char *osc_addr = 0;

void send(const char *path, const char *args, ...)
{
    char buffer[1024];
    va_list va;
    va_start(va, args);
    size_t result = rtosc_vmessage(buffer, 1024, path, args, va);
    va_end(va);
    lo_address addr = lo_address_new_from_url(osc_addr);
    if(!addr) {
        fprintf(stderr, "Invalid liblo address '%s'\n", osc_addr);
        fprintf(stderr, "Example URL: \"osc.udp://localhost:1234\"\n");
        exit(1);
    }
    lo_message msg  = lo_message_deserialise((void*)buffer, result, NULL);
    lo_send_message(addr, path, msg);
}

bool got_response = false;
static int handler_function(const char *path_, const char *types_, lo_arg **argv,
        int argc, lo_message msg, void *user_data)
{
    (void) argc;
    (void) msg;
    (void) user_data;
    std::string path  = path_;
    std::string types = types_;
    if(path == "/oscdoc" && types == "s") {
        got_response = true;
        updateSliderConfig(&argv[0]->s, strlen(&argv[0]->s));
    } if(types == "f") {
        for(unsigned i=0; i<nfields; ++i) {
            if(path == fields[i]) {
                sliders[i]->value(argv[0]->f);
            }
        }
    }
    return 0;
}



int main(int argc, char **argv)
{
    //Get Port
    if(argc != 2) {
        fprintf(stderr, "usage: %s OSC_ADDRESS\n", argv[0]);
        return 1;
    }
    osc_addr = argv[1];

    //Setup Liblo connection
    lo_server server = lo_server_new_with_proto(NULL, LO_UDP, NULL);
    lo_server_add_method(server, NULL, NULL, handler_function, NULL);

    //Setup GUI
    gui_init();

    //Request information
    //- oscdoc
    //- current values of ports
    send("/oscdoc");
    for(unsigned i=0; i<nfields; ++i)
        send(fields[i]);


    //wait up to 0.2 sec for all pending requests to get handled
    for(int i=0; i<10; ++i)
        lo_server_recv_noblock(server, 20);

    if(!got_response)
        errx(1, "No response from client. Bad OSC address?");

    //Show the GUI and operate normally
    fltk_window->show();

    while(fltk_window->shown())
    {
        while(lo_server_recv_noblock(server, 10));
        gui_tick();
    }

    return 0;
}
