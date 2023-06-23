# Rayplayer

Playing videos in raylib with FFmpeg.

**This is an experimental library, use at your own risk!**

## Dependencies

- raylib
- ffmpeg
- gcc
- make

On Arch Linux, you can install them by:

```bash
sudo pacman -S --needed raylib ffmpeg
```

## Build

To build the player, type:

```bash
make ray
```

## Usage

```bash
./ray xxx.mp4
```

## TO-DO

- [x] Audio
- [ ] Adjust PTS
- [ ] Audio and video synchronization
