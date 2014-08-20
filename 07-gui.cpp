#include <FL/Fl_Window.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl.H>
#include <libxml/parser.h>
#include <libxml/tree.h>
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
    for(int i=0; i<nfields; ++i) {
        sliders[i] = new Fl_Slider(0,0,0,0);
        sliders[i]->type(FL_VERTICAL);
        //Assert that it is a float based parameter <1>

        //Fetch min/max <2>
        
        //Fetch description <3>
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
    cur_node->properties;

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
    doc = xmlReadMemory(data, data_size, NULL, NULL, 0);

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
static int handler_function(const char *path_, const char *types_, lo_arg **argv,
        int argc, lo_message msg, void *user_data)
{
    (void) argv;
    (void) argc;
    (void) user_data;
    std::string path  = path_;
    std::string types = types_;
    if(path == "/oscdoc" && types == "b")
        updateSliderConfig(lo_blob_dataptr(argv[0]),
                           lo_blob_datasize(argv[0]));
    if(types == "f")
    {
        for(int i=0; i<nfields; ++i)
            if(path == fields[i])
                sliders[i]->value(argv[0]->f);
    }
    return 0;
}



int main(int argc, char **argv)
{
    //Setup Liblo connection
    lo_server server = lo_server_new_with_proto(NULL, LO_UDP, NULL);
    lo_server_add_method(server, NULL, NULL, handler_function, NULL);
    
    //Setup GUI
    gui_init();
    
    //Request information 
    //- oscdoc
    //- current values of ports
    
    //wait up to 1 sec for all pending requests to get handled

    //Show the GUI and operate normally

    return 0;
}
