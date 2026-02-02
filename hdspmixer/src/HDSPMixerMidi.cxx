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
#include "HDSPMixerIOMixer.h"
#include "HDSPMixerInputs.h"
#include "HDSPMixerPlaybacks.h"
#include "defines.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <FL/Fl.H>
#include <FL/fl_ask.H>

// Structure to pass data to the main thread
struct MidiUpdateData {
    HDSPMixerMidi *midi;
    int strip_index;
    int dest_index;
    bool is_input;
    int fader_pos;
};

// Structure for learn completion
struct LearnCompleteData {
    HDSPMixerMidi *midi;
    int cc;
    int channel;
};

// Callback executed on the main FLTK thread to update fader
static void update_fader_cb(void *data)
{
    MidiUpdateData *ud = static_cast<MidiUpdateData*>(data);
    if (!ud || !ud->midi) {
        delete ud;
        return;
    }
    
    HDSPMixerWindow *window = ud->midi->get_window();
    if (!window) {
        delete ud;
        return;
    }
    
    HDSPMixerFader *fader = NULL;
    
    // Get the correct fader
    if (ud->is_input) {
        if (ud->strip_index >= 0 && ud->strip_index < HDSP_MAX_CHANNELS) {
            fader = window->inputs->strips[ud->strip_index]->fader;
        }
    } else {
        if (ud->strip_index >= 0 && ud->strip_index < HDSP_MAX_CHANNELS) {
            fader = window->playbacks->strips[ud->strip_index]->fader;
        }
    }
    
    if (fader) {
        printf("update_fader_cb: Setting fader pos[%d] = %d\n", ud->dest_index, ud->fader_pos);
        fader->pos[ud->dest_index] = ud->fader_pos;
        fader->redraw();
        fader->sendGain();
        
        // Update the mixer
        window->setMixer(ud->strip_index + 1, ud->is_input ? 0 : 1, ud->dest_index);
        window->checkState();
    }
    
    delete ud;
}

// Callback for learn completion
static void learn_complete_main_cb(void *data)
{
    LearnCompleteData *ld = static_cast<LearnCompleteData*>(data);
    if (ld && ld->midi) {
        printf("Learn complete callback on main thread: CC %d, channel %d\n", ld->cc, ld->channel);
        // Notify any listening buttons
        if (ld->midi->learn_callback) {
            ld->midi->learn_callback(ld->midi->learn_callback_data);
        }
    }
    delete ld;
}

HDSPMixerMidi::HDSPMixerMidi(HDSPMixerWindow *win)
    : window(win),
      seq_handle(NULL),
      seq_port(-1),
      running(false),
      learn_mode(false),
      learn_target_fader(NULL),
      learn_target_strip(-1),
      learn_target_dest(-1),
      learn_target_is_input(false),
      learn_callback(NULL),
      learn_callback_data(NULL)
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
    
    // Open ALSA sequencer in NON-BLOCKING mode
    err = snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);
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
    
    int client_id = snd_seq_client_id(seq_handle);
    printf("===========================================\n");
    printf("MIDI Controller initialized!\n");
    printf("Client ID: %d, Port: %d\n", client_id, seq_port);
    printf("To connect your MIDI controller, run:\n");
    printf("  aconnect <controller_client>:<port> %d:%d\n", client_id, seq_port);
    printf("Or use a GUI like qjackctl or aconnectgui\n");
    printf("===========================================\n");
    
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
    int err;
    
    printf("MIDI thread started, waiting for events...\n");
    
    // Set up poll descriptors for waiting on MIDI events
    int npfds = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
    struct pollfd *pfds = (struct pollfd *)malloc(sizeof(struct pollfd) * npfds);
    if (!pfds) {
        fprintf(stderr, "Failed to allocate poll descriptors\n");
        return;
    }
    snd_seq_poll_descriptors(seq_handle, pfds, npfds, POLLIN);
    
    while (running) {
        // Poll with timeout (100ms) so we can check the running flag
        int poll_result = poll(pfds, npfds, 100);
        
        if (poll_result < 0) {
            if (errno != EINTR) {
                fprintf(stderr, "Poll error: %s\n", strerror(errno));
            }
            continue;
        }
        
        if (poll_result == 0) {
            // Timeout, no events
            continue;
        }
        
        // Read all available events
        while ((err = snd_seq_event_input(seq_handle, &ev)) >= 0) {
            if (ev == NULL) {
                continue;
            }
            
            // Handle different MIDI event types
            switch (ev->type) {
                case SND_SEQ_EVENT_CONTROLLER:
                    printf("MIDI CC Event: channel=%d, cc=%d, value=%d\n",
                           ev->data.control.channel,
                           ev->data.control.param,
                           ev->data.control.value);
                    handle_midi_cc(ev->data.control.channel,
                                 ev->data.control.param,
                                 ev->data.control.value);
                    break;
                    
                case SND_SEQ_EVENT_NOTEON:
                    printf("MIDI Note On: channel=%d, note=%d, velocity=%d\n",
                           ev->data.note.channel,
                           ev->data.note.note,
                           ev->data.note.velocity);
                    break;
                    
                case SND_SEQ_EVENT_PITCHBEND:
                    printf("MIDI Pitchbend: channel=%d, value=%d\n",
                           ev->data.control.channel,
                           ev->data.control.value);
                    break;
                    
                default:
                    printf("MIDI Event type: %d\n", ev->type);
                    break;
            }
        }
        
        // Check for read errors (other than "no events available")
        if (err < 0 && err != -EAGAIN) {
            fprintf(stderr, "MIDI read error: %s\n", snd_strerror(err));
        }
    }
    
    free(pfds);
    printf("MIDI thread stopped\n");
}

void HDSPMixerMidi::handle_midi_cc(int channel, int cc, int value)
{
    printf("handle_midi_cc: channel=%d cc=%d value=%d learn_mode=%d\n", 
           channel, cc, value, learn_mode);
    
    // Create key for lookup (channel * 128 + cc)
    int key = channel * 128 + cc;
    
    if (learn_mode && learn_target_fader) {
        // We're in learn mode - assign this CC to the target fader
        printf("LEARNING: CC %d on channel %d -> strip=%d, dest=%d, is_input=%d\n", 
               cc, channel, learn_target_strip, learn_target_dest, learn_target_is_input);
        
        add_mapping(cc, channel, learn_target_fader,
                   learn_target_strip, learn_target_dest,
                   learn_target_is_input);
        
        // Auto-disable learn mode after successful learn
        learn_mode = false;
        
        // Save the new mapping
        save_mappings();
        
        // Notify UI on main thread
        LearnCompleteData *ld = new LearnCompleteData;
        ld->midi = this;
        ld->cc = cc;
        ld->channel = channel;
        Fl::awake(learn_complete_main_cb, ld);
        
        // Clear target
        learn_target_fader = NULL;
        learn_target_strip = -1;
        learn_target_dest = -1;
        learn_target_is_input = false;
        
        return;
    }
    
    // Check if we have a mapping for this CC
    auto it = cc_mappings.find(key);
    if (it != cc_mappings.end()) {
        MidiCCMapping &mapping = it->second;
        
        printf("Found mapping: strip=%d, dest=%d, is_input=%d, fader=%p\n",
               mapping.strip_index, mapping.dest_index, mapping.is_input, (void*)mapping.fader);
        
        // Convert MIDI value (0-127) to fader position
        int fader_pos = midi_value_to_fader_pos(value);
        
        printf("Converted MIDI value %d to fader pos %d\n", value, fader_pos);
        
        // Schedule fader update on main thread
        MidiUpdateData *ud = new MidiUpdateData;
        ud->midi = this;
        ud->strip_index = mapping.strip_index;
        ud->dest_index = mapping.dest_index;
        ud->is_input = mapping.is_input;
        ud->fader_pos = fader_pos;
        
        Fl::awake(update_fader_cb, ud);
    } else {
        printf("No mapping found for CC %d on channel %d (key=%d)\n", cc, channel, key);
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
    
    printf("MIDI Learn target set: fader=%p, Strip %d, Dest %d, %s\n",
           (void*)fader, strip_idx, dest_idx, is_input ? "Input" : "Playback");
}

void HDSPMixerMidi::clear_learn_target()
{
    learn_target_fader = NULL;
    learn_target_strip = -1;
    learn_target_dest = -1;
    learn_target_is_input = false;
}

void HDSPMixerMidi::set_learn_callback(Fl_Awake_Handler cb, void *data)
{
    learn_callback = cb;
    learn_callback_data = data;
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
    
    printf("MIDI mapping added: CC %d (ch %d) -> Strip %d, Dest %d, key=%d\n",
           cc_number, channel, strip_idx, dest_idx, key);
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
    printf("All MIDI mappings cleared\n");
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
    empty.fader = NULL;
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
        printf("No MIDI config file found at %s (this is normal on first run)\n",
               config_file_path.c_str());
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
        
        // Store the mapping info - fader pointer will be resolved on first use
        MidiCCMapping mapping;
        mapping.cc_number = cc;
        mapping.channel = channel;
        mapping.strip_index = strip;
        mapping.dest_index = dest;
        mapping.is_input = (is_input_int != 0);
        mapping.fader = NULL;  // Will be resolved via strip_index
        
        int key = channel * 128 + cc;
        cc_mappings[key] = mapping;
        
        printf("Loaded mapping: CC %d (ch %d) -> strip %d, dest %d, key=%d\n",
               cc, channel, strip, dest, key);
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
                snprintf(port_name, sizeof(port_name), "%d:%d %s:%s",
                        client,
                        snd_seq_port_info_get_port(pinfo),
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
int midi_value_to_fader_pos(int midi_value)
{
    // MIDI CC values are 0-127
    // Fader position range is 0 to 137*CF (where CF=8)
    // 137*CF = 1096 is the maximum (full volume)
    int max_pos = 137 * CF;
    int pos = (int)(((double)midi_value / 127.0) * (double)max_pos);
    printf("midi_value_to_fader_pos: %d -> %d (max=%d, CF=%d)\n", midi_value, pos, max_pos, CF);
    return pos;
}

int fader_pos_to_midi_value(int fader_pos)
{
    // Convert fader position back to MIDI value
    int max_pos = 137 * CF;
    if (fader_pos > max_pos) fader_pos = max_pos;
    if (fader_pos < 0) fader_pos = 0;
    return (int)((double)fader_pos / (double)max_pos * 127.0);
}
