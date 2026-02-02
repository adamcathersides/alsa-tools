/*
 *   HDSPMixer - MIDI Controller Support
 *
 *   Copyright (C) 2026 (MIDI Implementation)
 *    
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#ifndef HDSPMIXER_MIDI_H
#define HDSPMIXER_MIDI_H

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <poll.h>
#include <map>
#include <string>
#include <vector>
#include <FL/Fl.H>
#include "defines.h"

// Forward declarations
class HDSPMixerWindow;
class HDSPMixerFader;

// MIDI CC mapping structure
struct MidiCCMapping {
    int cc_number;           // MIDI CC number (0-127)
    int channel;             // MIDI channel (0-15)
    HDSPMixerFader *fader;   // Pointer to the fader (may be NULL if loaded from config)
    int strip_index;         // Strip index (0-based)
    int dest_index;          // Destination index
    bool is_input;           // true for input, false for playback
};

class HDSPMixerMidi {
private:
    HDSPMixerWindow *window;
    snd_seq_t *seq_handle;
    int seq_port;
    pthread_t midi_thread;
    volatile bool running;
    volatile bool learn_mode;
    
    // CC number to mapping (key = channel * 128 + cc)
    std::map<int, MidiCCMapping> cc_mappings;
    
    // Learning state
    HDSPMixerFader *learn_target_fader;
    int learn_target_strip;
    int learn_target_dest;
    bool learn_target_is_input;
    
    // MIDI thread
    static void *midi_thread_func(void *arg);
    void process_midi_events();
    void handle_midi_cc(int channel, int cc, int value);
    
    // Configuration file handling
    std::string config_file_path;
    void save_mappings();
    void load_mappings();
    
public:
    // Callback for learn completion (public so static callback can access)
    Fl_Awake_Handler learn_callback;
    void *learn_callback_data;
    
    HDSPMixerMidi(HDSPMixerWindow *win);
    ~HDSPMixerMidi();
    
    // Initialization
    bool initialize();
    void shutdown();
    
    // Get the window (for callbacks)
    HDSPMixerWindow* get_window() const { return window; }
    
    // Learn mode control
    void set_learn_mode(bool enabled);
    bool get_learn_mode() const { return learn_mode; }
    
    // Assign a fader to learn mode
    void set_learn_target(HDSPMixerFader *fader, int strip_idx, int dest_idx, bool is_input);
    void clear_learn_target();
    
    // Set callback for when learning completes
    void set_learn_callback(Fl_Awake_Handler cb, void *data);
    
    // Manual mapping
    void add_mapping(int cc_number, int channel, HDSPMixerFader *fader, 
                    int strip_idx, int dest_idx, bool is_input);
    void remove_mapping(int cc_number, int channel);
    void clear_all_mappings();
    
    // Query mappings
    bool has_mapping(int cc_number, int channel) const;
    MidiCCMapping get_mapping(int cc_number, int channel) const;
    
    // Get list of available MIDI ports
    static std::vector<std::string> get_midi_ports();
    
    // Configuration
    std::string get_config_path() const { return config_file_path; }
    
    // Get sequencer info for connection instructions
    int get_client_id() const { return seq_handle ? snd_seq_client_id(seq_handle) : -1; }
    int get_port_id() const { return seq_port; }
};

// Helper function to convert MIDI value (0-127) to fader position
int midi_value_to_fader_pos(int midi_value);

// Helper function to convert fader position to MIDI value (0-127)
int fader_pos_to_midi_value(int fader_pos);

#endif // HDSPMIXER_MIDI_H
