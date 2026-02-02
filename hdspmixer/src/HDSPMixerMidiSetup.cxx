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

#pragma implementation
#include "HDSPMixerMidiSetup.h"
#include "HDSPMixerWindow.h"
#include "HDSPMixerMidi.h"
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Callback for Connect button
static void connect_cb(Fl_Widget *w, void *data)
{
    HDSPMixerMidiSetup *setup = static_cast<HDSPMixerMidiSetup*>(data);
    setup->connect_selected();
}

// Callback for Disconnect button
static void disconnect_cb(Fl_Widget *w, void *data)
{
    HDSPMixerMidiSetup *setup = static_cast<HDSPMixerMidiSetup*>(data);
    setup->disconnect_selected();
}

// Callback for Refresh button
static void refresh_cb(Fl_Widget *w, void *data)
{
    HDSPMixerMidiSetup *setup = static_cast<HDSPMixerMidiSetup*>(data);
    setup->refresh_ports();
}

// Callback for Close button
static void close_cb(Fl_Widget *w, void *data)
{
    HDSPMixerMidiSetup *setup = static_cast<HDSPMixerMidiSetup*>(data);
    setup->hide();
}

// Callback for Clear Mappings button
static void clear_mappings_cb(Fl_Widget *w, void *data)
{
    HDSPMixerMidiSetup *setup = static_cast<HDSPMixerMidiSetup*>(data);
    setup->clear_all_mappings();
}

HDSPMixerMidiSetup::HDSPMixerMidiSetup(HDSPMixerWindow *win)
    : Fl_Double_Window(400, 350, "MIDI Setup"),
      mixer_window(win)
{
    // Available MIDI ports list
    available_label = new Fl_Box(10, 10, 380, 20, "Available MIDI Devices:");
    available_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    available_label->labelfont(FL_HELVETICA_BOLD);
    
    port_browser = new Fl_Hold_Browser(10, 35, 380, 120);
    port_browser->textsize(12);
    
    // Connected ports list  
    connected_label = new Fl_Box(10, 165, 380, 20, "Connected Devices:");
    connected_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    connected_label->labelfont(FL_HELVETICA_BOLD);
    
    connected_browser = new Fl_Hold_Browser(10, 190, 380, 80);
    connected_browser->textsize(12);
    
    // Buttons
    connect_btn = new Fl_Button(10, 280, 90, 25, "Connect");
    connect_btn->callback(connect_cb, this);
    
    disconnect_btn = new Fl_Button(110, 280, 90, 25, "Disconnect");
    disconnect_btn->callback(disconnect_cb, this);
    
    refresh_btn = new Fl_Button(210, 280, 90, 25, "Refresh");
    refresh_btn->callback(refresh_cb, this);
    
    close_btn = new Fl_Button(310, 280, 80, 25, "Close");
    close_btn->callback(close_cb, this);
    
    clear_mappings_btn = new Fl_Button(10, 315, 120, 25, "Clear Mappings");
    clear_mappings_btn->callback(clear_mappings_cb, this);
    clear_mappings_btn->tooltip("Remove all learned MIDI CC mappings");
    
    // Status label
    status_label = new Fl_Box(140, 315, 250, 25);
    status_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    status_label->labelsize(11);
    
    end();
    
    // Don't show in taskbar, make it a tool window
    set_non_modal();
}

HDSPMixerMidiSetup::~HDSPMixerMidiSetup()
{
}

void HDSPMixerMidiSetup::show()
{
    refresh_ports();
    refresh_connections();
    Fl_Double_Window::show();
}

void HDSPMixerMidiSetup::refresh_ports()
{
    port_browser->clear();
    available_ports.clear();
    
    if (!mixer_window || !mixer_window->midi_controller) {
        return;
    }
    
    // Get list of available MIDI output ports (devices that can send to us)
    std::vector<MidiPortInfo> ports = get_available_midi_ports();
    
    int our_client = mixer_window->midi_controller->get_client_id();
    
    for (const auto &port : ports) {
        // Skip our own port
        if (port.client == our_client) {
            continue;
        }
        
        available_ports.push_back(port);
        
        char display_name[256];
        snprintf(display_name, sizeof(display_name), "%d:%d  %s - %s",
                port.client, port.port, port.client_name.c_str(), port.port_name.c_str());
        port_browser->add(display_name);
    }
    
    update_status();
}

void HDSPMixerMidiSetup::refresh_connections()
{
    connected_browser->clear();
    connected_ports.clear();
    
    if (!mixer_window || !mixer_window->midi_controller) {
        return;
    }
    
    // Query current connections to our port
    std::vector<MidiPortInfo> connections = get_connected_ports();
    
    for (const auto &port : connections) {
        connected_ports.push_back(port);
        
        char display_name[256];
        snprintf(display_name, sizeof(display_name), "%d:%d  %s - %s",
                port.client, port.port, port.client_name.c_str(), port.port_name.c_str());
        connected_browser->add(display_name);
    }
}

void HDSPMixerMidiSetup::connect_selected()
{
    int selected = port_browser->value();
    if (selected <= 0 || selected > (int)available_ports.size()) {
        fl_alert("Please select a MIDI device to connect.");
        return;
    }
    
    const MidiPortInfo &port = available_ports[selected - 1];
    
    if (connect_midi_port(port.client, port.port)) {
        printf("Connected to %s:%s (%d:%d)\n", 
               port.client_name.c_str(), port.port_name.c_str(),
               port.client, port.port);
        refresh_connections();
        update_status();
    } else {
        fl_alert("Failed to connect to MIDI device.");
    }
}

void HDSPMixerMidiSetup::disconnect_selected()
{
    int selected = connected_browser->value();
    if (selected <= 0 || selected > (int)connected_ports.size()) {
        fl_alert("Please select a connected device to disconnect.");
        return;
    }
    
    const MidiPortInfo &port = connected_ports[selected - 1];
    
    if (disconnect_midi_port(port.client, port.port)) {
        printf("Disconnected from %s:%s (%d:%d)\n",
               port.client_name.c_str(), port.port_name.c_str(),
               port.client, port.port);
        refresh_connections();
        update_status();
    } else {
        fl_alert("Failed to disconnect MIDI device.");
    }
}

void HDSPMixerMidiSetup::clear_all_mappings()
{
    if (!mixer_window || !mixer_window->midi_controller) {
        return;
    }
    
    int confirm = fl_choice("Clear all MIDI CC mappings?\nThis cannot be undone.",
                           "Cancel", "Clear All", NULL);
    if (confirm == 1) {
        mixer_window->midi_controller->clear_all_mappings();
        update_status();
    }
}

void HDSPMixerMidiSetup::update_status()
{
    if (!mixer_window || !mixer_window->midi_controller) {
        status_label->copy_label("MIDI not initialized");
        return;
    }
    
    int client = mixer_window->midi_controller->get_client_id();
    int port = mixer_window->midi_controller->get_port_id();
    
    char status[128];
    snprintf(status, sizeof(status), "HDSPMixer MIDI: %d:%d", client, port);
    status_label->copy_label(status);
}

std::vector<MidiPortInfo> HDSPMixerMidiSetup::get_available_midi_ports()
{
    std::vector<MidiPortInfo> ports;
    snd_seq_t *seq;
    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;
    
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
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
            unsigned int caps = snd_seq_port_info_get_capability(pinfo);
            
            // We want ports that can READ (output to us) and allow subscriptions
            if ((caps & SND_SEQ_PORT_CAP_READ) &&
                (caps & SND_SEQ_PORT_CAP_SUBS_READ)) {
                
                MidiPortInfo info;
                info.client = client;
                info.port = snd_seq_port_info_get_port(pinfo);
                info.client_name = snd_seq_client_info_get_name(cinfo);
                info.port_name = snd_seq_port_info_get_name(pinfo);
                
                ports.push_back(info);
            }
        }
    }
    
    snd_seq_close(seq);
    return ports;
}

std::vector<MidiPortInfo> HDSPMixerMidiSetup::get_connected_ports()
{
    std::vector<MidiPortInfo> connections;
    
    if (!mixer_window || !mixer_window->midi_controller) {
        return connections;
    }
    
    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        return connections;
    }
    
    int our_client = mixer_window->midi_controller->get_client_id();
    int our_port = mixer_window->midi_controller->get_port_id();
    
    snd_seq_query_subscribe_t *subs;
    snd_seq_query_subscribe_alloca(&subs);
    
    snd_seq_addr_t addr;
    addr.client = our_client;
    addr.port = our_port;
    
    // Query subscribers sending TO our port
    snd_seq_query_subscribe_set_root(subs, &addr);
    snd_seq_query_subscribe_set_type(subs, SND_SEQ_QUERY_SUBS_WRITE);
    snd_seq_query_subscribe_set_index(subs, 0);
    
    while (snd_seq_query_port_subscribers(seq, subs) >= 0) {
        const snd_seq_addr_t *sender = snd_seq_query_subscribe_get_addr(subs);
        
        // Get client and port info
        snd_seq_client_info_t *cinfo;
        snd_seq_port_info_t *pinfo;
        snd_seq_client_info_alloca(&cinfo);
        snd_seq_port_info_alloca(&pinfo);
        
        if (snd_seq_get_any_client_info(seq, sender->client, cinfo) >= 0 &&
            snd_seq_get_any_port_info(seq, sender->client, sender->port, pinfo) >= 0) {
            
            MidiPortInfo info;
            info.client = sender->client;
            info.port = sender->port;
            info.client_name = snd_seq_client_info_get_name(cinfo);
            info.port_name = snd_seq_port_info_get_name(pinfo);
            
            connections.push_back(info);
        }
        
        snd_seq_query_subscribe_set_index(subs, snd_seq_query_subscribe_get_index(subs) + 1);
    }
    
    snd_seq_close(seq);
    return connections;
}

bool HDSPMixerMidiSetup::connect_midi_port(int client, int port)
{
    if (!mixer_window || !mixer_window->midi_controller) {
        return false;
    }
    
    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        return false;
    }
    
    int our_client = mixer_window->midi_controller->get_client_id();
    int our_port = mixer_window->midi_controller->get_port_id();
    
    snd_seq_addr_t sender, dest;
    sender.client = client;
    sender.port = port;
    dest.client = our_client;
    dest.port = our_port;
    
    snd_seq_port_subscribe_t *subs;
    snd_seq_port_subscribe_alloca(&subs);
    snd_seq_port_subscribe_set_sender(subs, &sender);
    snd_seq_port_subscribe_set_dest(subs, &dest);
    
    int err = snd_seq_subscribe_port(seq, subs);
    snd_seq_close(seq);
    
    if (err < 0) {
        fprintf(stderr, "Failed to connect: %s\n", snd_strerror(err));
        return false;
    }
    
    return true;
}

bool HDSPMixerMidiSetup::disconnect_midi_port(int client, int port)
{
    if (!mixer_window || !mixer_window->midi_controller) {
        return false;
    }
    
    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        return false;
    }
    
    int our_client = mixer_window->midi_controller->get_client_id();
    int our_port = mixer_window->midi_controller->get_port_id();
    
    snd_seq_addr_t sender, dest;
    sender.client = client;
    sender.port = port;
    dest.client = our_client;
    dest.port = our_port;
    
    snd_seq_port_subscribe_t *subs;
    snd_seq_port_subscribe_alloca(&subs);
    snd_seq_port_subscribe_set_sender(subs, &sender);
    snd_seq_port_subscribe_set_dest(subs, &dest);
    
    int err = snd_seq_unsubscribe_port(seq, subs);
    snd_seq_close(seq);
    
    if (err < 0) {
        fprintf(stderr, "Failed to disconnect: %s\n", snd_strerror(err));
        return false;
    }
    
    return true;
}
