/*
 *   HDSPMixer - MIDI Setup Button
 *
 *   Copyright (C) 2026 (MIDI Implementation)
 *    
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#ifndef HDSPMIXER_MIDI_SETUP_BUTTON_H
#define HDSPMIXER_MIDI_SETUP_BUTTON_H

#include <FL/Fl_Button.H>

class HDSPMixerWindow;

class HDSPMixerMidiSetupButton : public Fl_Button {
private:
    HDSPMixerWindow *mixer_window;
    
    HDSPMixerWindow* get_mixer_window();
    
public:
    HDSPMixerMidiSetupButton(int x, int y, int w, int h, const char *label = NULL);
    
    void set_mixer_window(HDSPMixerWindow *win);
    void open_setup();
};

#endif // HDSPMIXER_MIDI_SETUP_BUTTON_H
