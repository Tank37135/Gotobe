# GOtobe 1.0.0

A command-line audio batch conversion tool based on FFmpeg.

GOtobe allows users to convert audio files in bulk using simple text commands.
It supports multiple source formats, multiple tasks in one command, progress
percentage display, and estimated remaining time (ETA).

---

## Features

- Convert audio files in batch using text commands
- Supports multiple source formats to one target format
- Supports multiple tasks in a single command
- Progress percentage displayed in console
- Estimated remaining time (ETA) displayed in console and window title
- Automatic output folder creation
- Detailed conversion log
- No modification to original files

---

## Requirements

- Windows 10 / 11 (64-bit)
- C++17 or later (for building)
- FFmpeg installed and added to system PATH

You should be able to run the following command in `cmd` or PowerShell:

```

ffmpeg -version

```

---

## Folder Structure

```

GOtobe.exe
audio
conversion_log.txt   (auto-generated)

```

- `audio\`  
  Place all source audio files here.

- Output folders  
  Automatically created based on target format, e.g. `MP3\`, `AAC\`, `WAV\`.

---

## Command Syntax

```

[source_format] to [target_format]

```

⚠️ **Important**  
The keyword `to` **must have spaces on both sides**:

✅ `.mp3 to .aac`  
❌ `.mp3to.aac`

---

## Usage Examples

### Convert one format to another

```

.mp3 to .aac

```

Converts all MP3 files in `audio\` to AAC and saves them in `AAC\`.

---

### Convert all audio files to one format

```

all to .wav

```

Converts all supported audio files in `audio\` to WAV.

---

### Convert multiple source formats

```

.mp3,.aac,.flac to .wav

```

Converts MP3, AAC, and FLAC files to WAV.

---

### Multiple tasks in one command

```

.mp3 to .aac | .aac to .wav | all to .flac

```

Tasks are executed **sequentially** in the given order.

---

## Progress & ETA Display

During conversion, GOtobe displays:

- Progress percentage
- Processed files / total files
- Estimated remaining time (ETA)

Example console output:

```

Progress:  45% (9/20) | ETA: 00:02:31

```

The window title is also updated in real time:

```

GOtobe 1.0.0 - 45% | ETA 00:02:31

```

---

## Logging

All operations are logged to:

```

conversion_log.txt

```

The log includes:

- Program start and exit time
- Each task start and end
- FFmpeg output
- Error messages (if any)

---

## Supported Audio Formats

Common formats include (but are not limited to):

```

mp3 wav flac aac ogg m4a wma aiff amr opus
ac3 dts alac ape tta mpc vqf speex

```

(Actual support depends on FFmpeg.)

---

## Exit Program

Type:

```

exit

```

or

```

quit

```

---

## Notes

- Original files are never modified.
- Multiple tasks may process the same file more than once if specified.
- ETA is estimated based on average processing time per file.

---

## License & Disclaimer

This software uses FFmpeg.
Please ensure compliance with all applicable licenses and copyright laws
when converting audio files.

---

Copyright © Tank37135

