# HDSPMixer MIDI CC Control - Complete Implementation Package

This package adds full MIDI CC learning and fader control to the HDSPMixer application for RME HDSP sound cards.

## Features

- **MIDI CC Learning**: Click the "M" button on any channel strip, move a MIDI fader/knob, and it's mapped
- **MIDI Device Selection**: GUI dialog to connect MIDI devices (no need for `aconnect`)
- **Persistent Mappings**: MIDI assignments saved to `~/.hdspmixer_midi.conf`
- **Non-blocking MIDI**: Uses poll() for responsive MIDI without blocking the UI
- **Thread-safe**: Proper FLTK threading for reliable fader updates

## Package Contents

### New Files (6 files - copy to `hdspmixer/src/`)
- `HDSPMixerMidi.h` - MIDI controller class header
- `HDSPMixerMidi.cxx` - MIDI controller implementation
- `HDSPMixerMidiButton.h` - MIDI learn button header  
- `HDSPMixerMidiButton.cxx` - MIDI learn button widget
- `HDSPMixerMidiSetup.h` - MIDI device selection dialog header
- `HDSPMixerMidiSetup.cxx` - MIDI device selection dialog

### Replacement Files (6 files - replace existing in `hdspmixer/src/`)
- `HDSPMixerFader.h/.cxx` - Modified with mixer_window pointer
- `HDSPMixerIOMixer.h/.cxx` - Modified with MIDI button on each strip
- `HDSPMixerWindow.h/.cxx` - Modified with MIDI controller and setup dialog

### Manual Edit Required (1 file)
- `hdspmixer.cxx` - Add ONE line (see below)

## Installation

### Step 1: Backup your original files
```bash
cd alsa-tools/hdspmixer/src
mkdir -p backup
cp HDSPMixer*.cxx HDSPMixer*.h hdspmixer.cxx backup/
```

### Step 2: Copy ALL the new files
```bash
# Copy new MIDI files
cp /path/to/package/HDSPMixerMidi.* .
cp /path/to/package/HDSPMixerMidiButton.* .
cp /path/to/package/HDSPMixerMidiSetup.* .

# Replace modified files
cp /path/to/package/HDSPMixerFader.* .
cp /path/to/package/HDSPMixerIOMixer.* .
cp /path/to/package/HDSPMixerWindow.* .
```

### Step 3: Edit hdspmixer.cxx (ONE LINE CHANGE)

Open your existing `hdspmixer.cxx` and add this line at the very start of main():

```cpp
int main(int argc, char **argv)
{
    Fl::lock();  // <-- ADD THIS LINE for MIDI threading
    
    // ... rest of your existing code stays EXACTLY the same ...
```

**Do NOT replace hdspmixer.cxx** - your version has different HDSPMixerCard constructor calls.

### Step 4: Update Makefile.am

Add these lines to `hdspmixer_SOURCES`:
```makefile
	HDSPMixerMidi.h \
	HDSPMixerMidi.cxx \
	HDSPMixerMidiButton.h \
	HDSPMixerMidiButton.cxx \
	HDSPMixerMidiSetup.h \
	HDSPMixerMidiSetup.cxx
```

### Step 5: Rebuild
```bash
cd alsa-tools/hdspmixer
autoreconf -fi
./configure
make clean
make
sudo make install
```

## Usage

### Connecting a MIDI Controller

**Option 1 - Via GUI (recommended):**
1. Menu → Options → MIDI Setup (or press Ctrl+M)
2. Select your MIDI device from "Available MIDI Devices"
3. Click "Connect"
   
**Option 2 - Via command line:**
```bash
# Find your controller
aconnect -l
# Connect (replace with your client:port numbers)
aconnect 28:0 128:0
```

### Learning MIDI CC Mappings

1. Click the "M" button on any channel strip (turns red = learning mode)
2. Move a fader or knob on your MIDI controller
3. The mapping is saved automatically
4. Repeat for other faders

### Clearing Mappings

- Menu → Options → MIDI Clear Mappings
- Or via MIDI Setup dialog → "Clear Mappings" button

## Technical Details

- **ALSA Sequencer**: Uses non-blocking ALSA seq API with poll()
- **FLTK Threading**: Uses `Fl::lock()` and `Fl::awake()` for thread-safe UI updates
- **Fader Range**: MIDI 0-127 maps to fader position 0 to 137*CF (where CF=8)
- **Config File**: `~/.hdspmixer_midi.conf` stores mappings

## Troubleshooting

### No MIDI events received
- Check `aconnect -l` to verify your controller is detected
- Verify the connection with `aseqdump -p 128:0` (use HDSPMixer's port)
- Look for "MIDI CC Event" messages in terminal output

### Faders not moving
- Make sure you added `Fl::lock();` to hdspmixer.cxx
- Check terminal for "update_fader_cb" messages
- Verify your controller sends CC messages (not NRPN or sysex)

### Build errors
- Ensure FLTK development headers are installed
- Ensure ALSA development libraries are installed
- Don't replace hdspmixer.cxx - only add the Fl::lock() line

## License

GPL v2 or later (same as HDSPMixer)
