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
#include <stdio.h>

// Static callback to handle learn completion
static void learn_complete_cb(void *data)
{
    HDSPMixerMidiButton *btn = static_cast<HDSPMixerMidiButton *>(data);
    btn->on_learn_complete();
}

HDSPMixerMidiButton::HDSPMixerMidiButton(int x, int y, int w, int h, const char *label)
    : Fl_Button(x, y, w, h, label),
      mixer_window(NULL),
      target_fader(NULL),
      strip_index(-1),
      dest_index(-1),
      is_input(false),
      learning(false),
      blink_state(false)
{
    box(FL_FLAT_BOX);
    labelsize(9);
    labelfont(FL_HELVETICA_BOLD);
    color(FL_DARK2);
    labelcolor(FL_WHITE);
}

void HDSPMixerMidiButton::set_target(HDSPMixerFader *fader, int strip_idx, 
                                     int dest_idx, bool input)
{
    target_fader = fader;
    strip_index = strip_idx;
    dest_index = dest_idx;
    is_input = input;
}

HDSPMixerWindow* HDSPMixerMidiButton::get_mixer_window()
{
    // If we already have it cached, return it
    if (mixer_window) {
        return mixer_window;
    }
    
    // Otherwise, find the top-level window and check if it's HDSPMixerWindow
    Fl_Window *win = window();
    if (win) {
        // The top window should be HDSPMixerWindow
        // We need to verify this is the right type
        mixer_window = dynamic_cast<HDSPMixerWindow*>(win);
        if (!mixer_window) {
            // If dynamic_cast fails (RTTI disabled), try finding the window
            // by walking up the widget hierarchy
            Fl_Widget *w = this;
            while (w) {
                Fl_Window *fw = w->as_window();
                if (fw && fw->label() && strncmp(fw->label(), "HDSPMixer", 9) == 0) {
                    // This is a bit of a hack but works when RTTI is disabled
                    mixer_window = (HDSPMixerWindow*)fw;
                    break;
                }
                w = w->parent();
            }
        }
    }
    return mixer_window;
}

void HDSPMixerMidiButton::start_learning()
{
    HDSPMixerWindow *win = get_mixer_window();
    
    if (!win) {
        fprintf(stderr, "HDSPMixerMidiButton: Cannot find mixer window\n");
        return;
    }
    
    if (!target_fader) {
        fprintf(stderr, "HDSPMixerMidiButton: No target fader set\n");
        return;
    }
    
    if (!win->midi_controller) {
        fprintf(stderr, "HDSPMixerMidiButton: MIDI controller not initialized\n");
        return;
    }
    
    printf("Starting MIDI learn for strip %d, dest %d, %s\n",
           strip_index, dest_index, is_input ? "input" : "playback");
    
    learning = true;
    blink_state = true;
    
    // Set this fader as the learn target in the MIDI controller
    win->midi_controller->set_learn_mode(true);
    win->midi_controller->set_learn_target(target_fader, 
        strip_index, dest_index, is_input);
    
    // Set callback for when learning completes
    win->midi_controller->set_learn_callback(learn_complete_cb, this);
    
    // Start blinking animation
    Fl::add_timeout(0.3, blink_cb, this);
    
    redraw();
}

void HDSPMixerMidiButton::stop_learning()
{
    learning = false;
    blink_state = false;
    
    HDSPMixerWindow *win = get_mixer_window();
    if (win && win->midi_controller) {
        win->midi_controller->set_learn_mode(false);
        win->midi_controller->clear_learn_target();
        win->midi_controller->set_learn_callback(NULL, NULL);
    }
    
    // Stop blinking animation
    Fl::remove_timeout(blink_cb, this);
    
    redraw();
}

void HDSPMixerMidiButton::on_learn_complete()
{
    printf("MIDI learn completed for button\n");
    learning = false;
    blink_state = false;
    Fl::remove_timeout(blink_cb, this);
    redraw();
}

void HDSPMixerMidiButton::blink_cb(void *data)
{
    HDSPMixerMidiButton *btn = static_cast<HDSPMixerMidiButton *>(data);
    
    if (!btn->learning) {
        return;
    }
    
    // Toggle visual state for blinking effect
    btn->blink_state = !btn->blink_state;
    btn->redraw();
    
    // Continue blinking
    Fl::repeat_timeout(0.3, blink_cb, data);
}

void HDSPMixerMidiButton::draw()
{
    Fl_Color bg_color;
    Fl_Color text_color;
    
    if (learning) {
        // Draw in red/orange when learning, blinking
        if (blink_state) {
            bg_color = FL_RED;
            text_color = FL_WHITE;
        } else {
            bg_color = fl_rgb_color(180, 60, 0); // Dark orange
            text_color = FL_WHITE;
        }
    } else {
        // Normal state - dark gray
        bg_color = FL_DARK2;
        text_color = FL_WHITE;
    }
    
    // Draw background
    fl_color(bg_color);
    fl_rectf(x(), y(), w(), h());
    
    // Draw border
    fl_color(FL_BLACK);
    fl_rect(x(), y(), w(), h());
    
    // Draw label
    fl_color(text_color);
    fl_font(labelfont(), labelsize());
    fl_draw(label(), x(), y(), w(), h(), FL_ALIGN_CENTER);
}

int HDSPMixerMidiButton::handle(int event)
{
    switch (event) {
        case FL_PUSH:
            if (Fl::event_button() == FL_LEFT_MOUSE) {
                if (learning) {
                    // Cancel learning
                    stop_learning();
                } else {
                    // Start learning
                    start_learning();
                }
                return 1;
            } else if (Fl::event_button() == FL_RIGHT_MOUSE) {
                // Right-click: could show menu or clear mapping
                // For now, just cancel learning if active
                if (learning) {
                    stop_learning();
                }
                return 1;
            }
            break;
            
        case FL_ENTER:
        case FL_LEAVE:
            // Could add hover effect here
            return 1;
    }
    
    return Fl_Button::handle(event);
}
