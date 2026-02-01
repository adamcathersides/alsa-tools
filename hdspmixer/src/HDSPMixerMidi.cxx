/*
 *   HDSPMixer - MIDI Controller Support
 *
 *   Copyright (C) 2026 (MIDI Implementation)
 *    
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#pragma implementation
#include "HDSPMixerMidi.h"
#include "HDSPMixerWindow.h"
#include "HDSPMixerFader.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <FL/fl_ask.H>

HDSPMixerMidi::HDSPMixerMidi(HDSPMixerWindow *win)
    : window(win),
      seq_handle(NULL),
      seq_port(-1),
      running(false),
      learn_mode(false),
      learn_target_fader(NULL),
      learn_target_strip(-1),
      learn_target_dest(-1),
      learn_target_is_input(false)
{
    // Set config file path
    char *home = getenv("HOME");
    if (home) {
        config_file_path = std::string(home) + "/.hdspmixer_midi.conf";
    } else {
        config_file_path = "/tmp/.hdspmixer_midi.conf";
    }
}

HDSPMixerMidi::~HDSPMixerMidi()
{
    shutdown();
}

bool HDSPMixerMidi::initialize()
{
    int err;
    
    // Open ALSA sequencer
    err = snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0);
    if (err < 0) {
        fprintf(stderr, "Error opening ALSA sequencer for MIDI: %s\n", snd_strerror(err));
        return false;
    }
    
    // Set client name
    snd_seq_set_client_name(seq_handle, "HDSPMixer");
    
    // Create input port
    seq_port = snd_seq_create_simple_port(seq_handle, "MIDI In",
                                          SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                          SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    
    if (seq_port < 0) {
        fprintf(stderr, "Error creating MIDI input port: %s\n", snd_strerror(seq_port));
        snd_seq_close(seq_handle);
        seq_handle = NULL;
        return false;
    }
    
    printf("MIDI Controller initialized on port %d\n", seq_port);
    
    // Load saved mappings
    load_mappings();
    
    // Start MIDI processing thread
    running = true;
    if (pthread_create(&midi_thread, NULL, midi_thread_func, this) != 0) {
        fprintf(stderr, "Error creating MIDI thread\n");
        shutdown();
        return false;
    }
    
    return true;
}

void HDSPMixerMidi::shutdown()
{
    if (running) {
        running = false;
        pthread_join(midi_thread, NULL);
    }
    
    if (seq_handle) {
        if (seq_port >= 0) {
            snd_seq_delete_simple_port(seq_handle, seq_port);
            seq_port = -1;
        }
        snd_seq_close(seq_handle);
        seq_handle = NULL;
    }
}

void *HDSPMixerMidi::midi_thread_func(void *arg)
{
    HDSPMixerMidi *midi = static_cast<HDSPMixerMidi *>(arg);
    midi->process_midi_events();
    return NULL;
}

void HDSPMixerMidi::process_midi_events()
{
    snd_seq_event_t *ev;
    
    while (running) {
        // Poll for events with timeout
        snd_seq_event_input(seq_handle, &ev);
        
        if (ev == NULL) {
            usleep(1000); // 1ms sleep to prevent busy-waiting
            continue;
        }
        
        // Handle different MIDI event types
        switch (ev->type) {
            case SND_SEQ_EVENT_CONTROLLER:
                handle_midi_cc(ev->data.control.channel,
                             ev->data.control.param,
                             ev->data.control.value);
                break;
                
            default:
                // Ignore other event types
                break;
        }
        
        snd_seq_free_event(ev);
    }
}

void HDSPMixerMidi::handle_midi_cc(int channel, int cc, int value)
{
    // Create key for lookup (channel * 128 + cc)
    int key = channel * 128 + cc;
    
    if (learn_mode && learn_target_fader) {
        // We're in learn mode - assign this CC to the target fader
        printf("Learning: CC %d on channel %d assigned to fader\n", cc, channel);
        
        add_mapping(cc, channel, learn_target_fader,
                   learn_target_strip, learn_target_dest,
                   learn_target_is_input);
        
        // Auto-disable learn mode after successful learn
        set_learn_mode(false);
        clear_learn_target();
        
        // Visual feedback
        Fl::awake([](void*){
            fl_message("MIDI CC learned successfully!");
        }, nullptr);
        
        // Save the new mapping
        save_mappings();
        
        return;
    }
    
    // Check if we have a mapping for this CC
    auto it = cc_mappings.find(key);
    if (it != cc_mappings.end()) {
        const MidiCCMapping &mapping = it->second;
        
        if (mapping.fader) {
            // Convert MIDI value (0-127) to fader position
            // Faders typically use a larger range
            int fader_pos = midi_value_to_fader_pos(value, 
                mapping.fader->ndb);
            
            // Update the fader position
            mapping.fader->pos[mapping.dest_index] = fader_pos;
            
            // Trigger fader update on main thread
            Fl::lock();
            mapping.fader->damage(FL_DAMAGE_ALL);
            mapping.fader->sendGain();
            Fl::unlock();
            Fl::awake();
        }
    }
}

void HDSPMixerMidi::set_learn_mode(bool enabled)
{
    learn_mode = enabled;
    printf("MIDI Learn mode: %s\n", enabled ? "ON" : "OFF");
}

void HDSPMixerMidi::set_learn_target(HDSPMixerFader *fader, int strip_idx, 
                                     int dest_idx, bool is_input)
{
    learn_target_fader = fader;
    learn_target_strip = strip_idx;
    learn_target_dest = dest_idx;
    learn_target_is_input = is_input;
    
    printf("MIDI Learn target set: Strip %d, Dest %d, %s\n",
           strip_idx, dest_idx, is_input ? "Input" : "Playback");
}

void HDSPMixerMidi::clear_learn_target()
{
    learn_target_fader = NULL;
    learn_target_strip = -1;
    learn_target_dest = -1;
    learn_target_is_input = false;
}

void HDSPMixerMidi::add_mapping(int cc_number, int channel, HDSPMixerFader *fader,
                               int strip_idx, int dest_idx, bool is_input)
{
    int key = channel * 128 + cc_number;
    
    MidiCCMapping mapping;
    mapping.cc_number = cc_number;
    mapping.channel = channel;
    mapping.fader = fader;
    mapping.strip_index = strip_idx;
    mapping.dest_index = dest_idx;
    mapping.is_input = is_input;
    
    cc_mappings[key] = mapping;
    
    printf("MIDI mapping added: CC %d (ch %d) -> Strip %d, Dest %d\n",
           cc_number, channel, strip_idx, dest_idx);
}

void HDSPMixerMidi::remove_mapping(int cc_number, int channel)
{
    int key = channel * 128 + cc_number;
    cc_mappings.erase(key);
    save_mappings();
}

void HDSPMixerMidi::clear_all_mappings()
{
    cc_mappings.clear();
    save_mappings();
}

bool HDSPMixerMidi::has_mapping(int cc_number, int channel) const
{
    int key = channel * 128 + cc_number;
    return cc_mappings.find(key) != cc_mappings.end();
}

MidiCCMapping HDSPMixerMidi::get_mapping(int cc_number, int channel) const
{
    int key = channel * 128 + cc_number;
    auto it = cc_mappings.find(key);
    if (it != cc_mappings.end()) {
        return it->second;
    }
    MidiCCMapping empty;
    empty.cc_number = -1;
    return empty;
}

void HDSPMixerMidi::save_mappings()
{
    std::ofstream file(config_file_path.c_str());
    if (!file.is_open()) {
        fprintf(stderr, "Error: Could not open MIDI config file for writing: %s\n",
                config_file_path.c_str());
        return;
    }
    
    file << "# HDSPMixer MIDI CC Mappings\n";
    file << "# Format: cc_number channel strip_index dest_index is_input\n";
    
    for (const auto &pair : cc_mappings) {
        const MidiCCMapping &m = pair.second;
        file << m.cc_number << " "
             << m.channel << " "
             << m.strip_index << " "
             << m.dest_index << " "
             << (m.is_input ? 1 : 0) << "\n";
    }
    
    file.close();
    printf("MIDI mappings saved to %s\n", config_file_path.c_str());
}

void HDSPMixerMidi::load_mappings()
{
    std::ifstream file(config_file_path.c_str());
    if (!file.is_open()) {
        printf("No MIDI config file found (this is normal on first run)\n");
        return;
    }
    
    cc_mappings.clear();
    
    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        std::istringstream iss(line);
        int cc, channel, strip, dest, is_input_int;
        
        if (!(iss >> cc >> channel >> strip >> dest >> is_input_int)) {
            fprintf(stderr, "Warning: Invalid line %d in MIDI config file\n", line_num);
            continue;
        }
        
        // We can't restore the fader pointer from config, but we'll store
        // the mapping info and resolve it when needed
        // For now, just store the metadata
        MidiCCMapping mapping;
        mapping.cc_number = cc;
        mapping.channel = channel;
        mapping.strip_index = strip;
        mapping.dest_index = dest;
        mapping.is_input = (is_input_int != 0);
        mapping.fader = NULL;  // Will be resolved on first use
        
        int key = channel * 128 + cc;
        cc_mappings[key] = mapping;
    }
    
    file.close();
    printf("Loaded %zu MIDI mappings from %s\n", cc_mappings.size(),
           config_file_path.c_str());
}

std::vector<std::string> HDSPMixerMidi::get_midi_ports()
{
    std::vector<std::string> ports;
    snd_seq_t *seq;
    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;
    
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        return ports;
    }
    
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        int client = snd_seq_client_info_get_client(cinfo);
        
        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            // Check if it's an output port (can send to us)
            unsigned int caps = snd_seq_port_info_get_capability(pinfo);
            if ((caps & SND_SEQ_PORT_CAP_READ) &&
                (caps & SND_SEQ_PORT_CAP_SUBS_READ)) {
                
                char port_name[256];
                snprintf(port_name, sizeof(port_name), "%s:%s",
                        snd_seq_client_info_get_name(cinfo),
                        snd_seq_port_info_get_name(pinfo));
                ports.push_back(std::string(port_name));
            }
        }
    }
    
    snd_seq_close(seq);
    return ports;
}

// Helper functions
int midi_value_to_fader_pos(int midi_value, int max_pos)
{
    // MIDI CC values are 0-127
    // Map to fader position (typically 0 to max_pos)
    // Using floating point for accuracy
    return (int)((double)midi_value / 127.0 * max_pos);
}

int fader_pos_to_midi_value(int fader_pos, int max_pos)
{
    // Convert fader position back to MIDI value
    if (max_pos == 0) return 0;
    return (int)((double)fader_pos / max_pos * 127.0);
}
