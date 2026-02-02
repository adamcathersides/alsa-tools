/*
 *   HDSPMixer - MIDI Setup Dialog
 *
 *   Copyright (C) 2026 (MIDI Implementation)
 *    
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#ifndef HDSPMIXER_MIDI_SETUP_H
#define HDSPMIXER_MIDI_SETUP_H

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <vector>
#include <string>
#include <alsa/asoundlib.h>

class HDSPMixerWindow;

// Structure to hold MIDI port information
struct MidiPortInfo {
    int client;
    int port;
    std::string client_name;
    std::string port_name;
};

class HDSPMixerMidiSetup : public Fl_Double_Window {
private:
    HDSPMixerWindow *mixer_window;
    
    // UI elements
    Fl_Box *available_label;
    Fl_Hold_Browser *port_browser;
    Fl_Box *connected_label;
    Fl_Hold_Browser *connected_browser;
    Fl_Button *connect_btn;
    Fl_Button *disconnect_btn;
    Fl_Button *refresh_btn;
    Fl_Button *close_btn;
    Fl_Button *clear_mappings_btn;
    Fl_Box *status_label;
    
    // Port data
    std::vector<MidiPortInfo> available_ports;
    std::vector<MidiPortInfo> connected_ports;
    
    // Helper functions
    std::vector<MidiPortInfo> get_available_midi_ports();
    std::vector<MidiPortInfo> get_connected_ports();
    bool connect_midi_port(int client, int port);
    bool disconnect_midi_port(int client, int port);
    void update_status();
    
public:
    HDSPMixerMidiSetup(HDSPMixerWindow *win);
    ~HDSPMixerMidiSetup();
    
    void show();
    void refresh_ports();
    void refresh_connections();
    void connect_selected();
    void disconnect_selected();
    void clear_all_mappings();
};

#endif // HDSPMIXER_MIDI_SETUP_H
