#include <FL/Fl_Window.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl.H>

const char *fields[] = {
    "/seq/freq1",
    "/lfo/freq",
    "/filter/freq"
};
unsigned nsliders = sizeof(fields)/sizeof(*fields);
Fl_Slider **sliders;


void gui_setup(void)
{
    Fl_Window *win = new Fl_Window(300,300);
    Fl_Pack *pack = new Fl_Pack(0,0,300,300);
    pack->type(Fl_Pack::HORIZONTAL);
    sliders = new Fl_Slider*[nsliders];
    for(int i=0; i<nsliders; ++i) {
        sliders[i] = new Fl_Slider(0,0,0,0);
        sliders[i]->type(FL_VERTICAL);
        //Assert that it is a float based parameter

        //Fetch min/max
        
        //Fetch description
    }

    win->show();
}

void gui_tick(void)
{
    Fl::wait(0.1f);
}
