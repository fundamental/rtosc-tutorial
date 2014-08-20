#include <FL/Fl_Window.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl.H>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <rtosc/rtosc.h>
#include <lo/lo.h>
#include <string>
#include <stdio.h>

/**********************************************************************
 *                           FLTK GUI Code                            *
 *                                                                    *
 *    Define a simple set of linear sliders based upon the contents   *
 *    of the 'fields' array and using oscdoc for reflection           *
 **********************************************************************/
const char *fields[] = {
    "/seq/freq1",
    "/lfo/freq",
    "/filter/freq"
};
unsigned nfields = sizeof(fields)/sizeof(*fields);
Fl_Slider **sliders;


Fl_Window *fltk_window;
void gui_init(void)
{
    fltk_window  = new Fl_Window(300,300);
    Fl_Pack *pack = new Fl_Pack(0,0,300,300);
    pack->type(Fl_Pack::HORIZONTAL);
    sliders = new Fl_Slider*[nfields];
    for(unsigned i=0; i<nfields; ++i) {
        sliders[i] = new Fl_Slider(0,0,100,20);
        sliders[i]->type(FL_VERTICAL);
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
static void
print_element_names(xmlNode * a_node)
{
    xmlNode *cur_node = NULL;
    //cur_node->properties;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            printf("node type: Element, name: %s\n", cur_node->name);
        }

        print_element_names(cur_node->children);
    }
}

// - message_in pattern=xyz
//   - desc
//   - param_f min=abc max=def
xmlNode *findSubtree(xmlNode *root, const std::string pattern)
{
    (void) root;
    (void) pattern;
    return NULL;
}

xmlDoc *doc = NULL;
void getPortParams(const std::string name,
        float &min, float &max, std::string &doc)
{
    auto *node = findSubtree(xmlDocGetRootElement(::doc), name);
}

void updateSliderConfig(void *data, size_t data_size)
{
    LIBXML_TEST_VERSION

    /*parse the file and get the DOM */
    doc = xmlReadMemory((char*)data, data_size, NULL, NULL, 0);

    if (doc == NULL) {
        printf("error: Invalid oscdoc received\n");
    }
    
    xmlFreeDoc(doc);
    xmlCleanupParser();
}


/**********************************************************************
 *                         liblo Interface                            *
 *                                                                    *
 **********************************************************************/
const char *osc_addr = 0;

void send(const char *path, const char *args="", ...)
{
    char buffer[1024];
    va_list va;
    va_start(va, args);
    size_t result = rtosc_vmessage(buffer, 1024, path, args, va);
    va_end(va);
    lo_address addr = lo_address_new_from_url(osc_addr);
    lo_message msg  = lo_message_deserialise((void*)buffer, result, NULL);
    lo_send_message(addr, path, msg);
}

static int handler_function(const char *path_, const char *types_, lo_arg **argv,
        int argc, lo_message msg, void *user_data)
{
    (void) argc;
    (void) msg;
    (void) user_data;
    std::string path  = path_;
    std::string types = types_;
    if(path == "/oscdoc" && types == "b")
        updateSliderConfig(lo_blob_dataptr(argv[0]),
                           lo_blob_datasize(argv[0]));
    if(types == "f")
    {
        for(unsigned i=0; i<nfields; ++i)
            if(path == fields[i])
                sliders[i]->value(argv[0]->f);
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

    //Show the GUI and operate normally
    fltk_window->show();

    while(fltk_window->shown())
    {
        lo_server_recv_noblock(server, 10);
        gui_tick();
    }

    return 0;
}
