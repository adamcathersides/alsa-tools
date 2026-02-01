/*
 *   HDSPMixer - MIDI Learn Button Widget
 *
 *   Copyright (C) 2026 (MIDI Implementation)
 *    
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#pragma implementation
#include "HDSPMixerMidiButton.h"
#include "HDSPMixerWindow.h"
#include "HDSPMixerFader.h"
#include "HDSPMixerMidi.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

HDSPMixerMidiButton::HDSPMixerMidiButton(int x, int y, int w, int h, const char *label)
    : Fl_Button(x, y, w, h, label),
      mixer_window(NULL),
      target_fader(NULL),
      strip_index(-1),
      dest_index(-1),
      is_input(false),
      learning(false)
{
    callback(button_cb, this);
    box(FL_UP_BOX);
    labelsize(9);
}

void HDSPMixerMidiButton::set_target(HDSPMixerFader *fader, int strip_idx, 
                                     int dest_idx, bool input)
{
    target_fader = fader;
    strip_index = strip_idx;
    dest_index = dest_idx;
    is_input = input;
}

void HDSPMixerMidiButton::start_learning()
{
    if (!mixer_window || !target_fader) {
        return;
    }
    
    learning = true;
    
    // Set this fader as the learn target in the MIDI controller
    if (mixer_window->midi_controller) {
        mixer_window->midi_controller->set_learn_mode(true);
        mixer_window->midi_controller->set_learn_target(target_fader, 
            strip_index, dest_index, is_input);
    }
    
    // Start blinking animation
    Fl::add_timeout(0.3, blink_cb, this);
    
    redraw();
}

void HDSPMixerMidiButton::stop_learning()
{
    learning = false;
    
    if (mixer_window && mixer_window->midi_controller) {
        mixer_window->midi_controller->set_learn_mode(false);
        mixer_window->midi_controller->clear_learn_target();
    }
    
    // Stop blinking animation
    Fl::remove_timeout(blink_cb, this);
    
    redraw();
}

void HDSPMixerMidiButton::button_cb(Fl_Widget *widget, void *data)
{
    HDSPMixerMidiButton *btn = static_cast<HDSPMixerMidiButton *>(data);
    
    if (btn->learning) {
        // Cancel learning
        btn->stop_learning();
    } else {
        // Start learning
        btn->start_learning();
    }
}

void HDSPMixerMidiButton::blink_cb(void *data)
{
    HDSPMixerMidiButton *btn = static_cast<HDSPMixerMidiButton *>(data);
    
    if (!btn->learning) {
        return;
    }
    
    // Toggle visual state for blinking effect
    btn->redraw();
    
    // Continue blinking
    Fl::repeat_timeout(0.3, blink_cb, data);
}

void HDSPMixerMidiButton::draw()
{
    if (learning) {
        // Draw in red when learning
        static bool blink_state = false;
        blink_state = !blink_state;
        
        if (blink_state) {
            color(FL_RED);
            labelcolor(FL_WHITE);
        } else {
            color(FL_DARK_RED);
            labelcolor(FL_WHITE);
        }
        box(FL_DOWN_BOX);
    } else {
        // Check if this fader has a MIDI mapping
        bool has_mapping = false;
        if (mixer_window && mixer_window->midi_controller) {
            // We'd need to iterate through mappings to check this
            // For now, use default colors
        }
        
        color(FL_GRAY);
        labelcolor(FL_BLACK);
        box(FL_UP_BOX);
    }
    
    Fl_Button::draw();
}

int HDSPMixerMidiButton::handle(int event)
{
    switch (event) {
        case FL_PUSH:
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                // Right-click: clear MIDI mapping for this fader
                if (mixer_window && mixer_window->midi_controller) {
                    // We'd need to identify which CC is mapped to this fader
                    // and remove it - this requires extending the API
                    // For now, just cancel learning if active
                    if (learning) {
                        stop_learning();
                    }
                }
                return 1;
            }
            break;
    }
    
    return Fl_Button::handle(event);
}
