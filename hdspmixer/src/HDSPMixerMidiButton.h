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

#ifndef HDSPMIXER_MIDI_BUTTON_H
#define HDSPMIXER_MIDI_BUTTON_H

#include <FL/Fl_Button.H>

// Forward declaration
class HDSPMixerWindow;
class HDSPMixerFader;

class HDSPMixerMidiButton : public Fl_Button {
private:
    HDSPMixerWindow *mixer_window;
    HDSPMixerFader *target_fader;
    int strip_index;
    int dest_index;
    bool is_input;
    bool learning;
    bool blink_state;
    
    static void blink_cb(void *data);
    
    // Get the mixer window (lazy initialization)
    HDSPMixerWindow* get_mixer_window();
    
public:
    HDSPMixerMidiButton(int x, int y, int w, int h, const char *label = "M");
    
    // Explicitly set the mixer window (optional, can be auto-detected)
    void set_mixer_window(HDSPMixerWindow *win) { mixer_window = win; }
    
    // Set the target fader for MIDI control
    void set_target(HDSPMixerFader *fader, int strip_idx, int dest_idx, bool input);
    
    void start_learning();
    void stop_learning();
    bool is_learning() const { return learning; }
    
    // Called when MIDI learn completes
    void on_learn_complete();
    
    void draw() override;
    int handle(int event) override;
};

#endif // HDSPMIXER_MIDI_BUTTON_H
