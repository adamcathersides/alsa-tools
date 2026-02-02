/*
 *   HDSPMixer - MIDI Setup Button
 *   
 *   A small button that opens the MIDI setup dialog.
 *   Add this button to your UI for easy access to MIDI configuration.
 *
 *   Copyright (C) 2026 (MIDI Implementation)
 *    
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#pragma implementation
#include "HDSPMixerMidiSetupButton.h"
#include "HDSPMixerWindow.h"
#include "HDSPMixerMidiSetup.h"
#include <FL/fl_draw.H>

static void setup_btn_cb(Fl_Widget *w, void *data)
{
    HDSPMixerMidiSetupButton *btn = static_cast<HDSPMixerMidiSetupButton*>(w);
    btn->open_setup();
}

HDSPMixerMidiSetupButton::HDSPMixerMidiSetupButton(int x, int y, int w, int h, const char *label)
    : Fl_Button(x, y, w, h, label ? label : "MIDI"),
      mixer_window(NULL)
{
    box(FL_UP_BOX);
    labelsize(10);
    labelfont(FL_HELVETICA_BOLD);
    tooltip("Open MIDI Setup (Ctrl+M)");
    callback(setup_btn_cb, this);
}

void HDSPMixerMidiSetupButton::set_mixer_window(HDSPMixerWindow *win)
{
    mixer_window = win;
}

HDSPMixerWindow* HDSPMixerMidiSetupButton::get_mixer_window()
{
    if (mixer_window) {
        return mixer_window;
    }
    
    // Try to find it from the widget hierarchy
    Fl_Window *win = window();
    if (win) {
        // Attempt to get the mixer window
        mixer_window = dynamic_cast<HDSPMixerWindow*>(win);
        if (!mixer_window) {
            // Walk up the hierarchy
            Fl_Widget *w = this;
            while (w) {
                Fl_Window *fw = w->as_window();
                if (fw && fw->label() && strncmp(fw->label(), "HDSPMixer", 9) == 0) {
                    mixer_window = (HDSPMixerWindow*)fw;
                    break;
                }
                w = w->parent();
            }
        }
    }
    return mixer_window;
}

void HDSPMixerMidiSetupButton::open_setup()
{
    HDSPMixerWindow *win = get_mixer_window();
    if (win) {
        win->show_midi_setup();
    }
}
