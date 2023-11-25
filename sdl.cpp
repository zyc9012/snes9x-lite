/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <SDL2/SDL.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "snes9x.h"
#include "memmap.h"
#include "apu/apu.h"
#include "apu/resampler.h"
#include "gfx.h"
#include "snapshot.h"
#include "controls.h"
#include "display.h"
#include "conffile.h"

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;
SDL_AudioDeviceID audioDev;

Resampler audio_buffer;

#define SOUND_BUFFER_SIZE (256)

static const char *rom_filename = NULL;

static int frame_advance = 0;

void S9xMessage(int type, int number, const char *message)
{
  printf(message);
  printf("\n");

  if (type == S9X_FATAL_ERROR)
  {
    exit(1);
  }
}

const char *S9xStringInput(const char *message)
{
  return NULL;
}

void S9xExtraUsage(void)
{
}

void S9xParseArg(char **argv, int &i, int argc)
{
}

void S9xParsePortConfig(ConfigFile &conf, int pass)
{
}

void blitPixSimple1x1(uint8 *srcPtr, int srcRowBytes, uint8 *dstPtr, int dstRowBytes, int width, int height)
{
  width <<= 1;

  for (; height; height--)
  {
    memcpy(dstPtr, srcPtr, width);
    srcPtr += srcRowBytes;
    dstPtr += dstRowBytes;
  }
}

void S9xPutImage(int width, int height)
{
  void *pixels;
  int pitch;
  SDL_LockTexture(texture, NULL, &pixels, &pitch);

  blitPixSimple1x1((uint8 *)GFX.Screen, GFX.Pitch, (uint8 *)pixels, pitch, width, height);

  SDL_UnlockTexture(texture);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

std::string S9xGetDirectory(enum s9x_getdirtype dirtype)
{
  std::string retval = Memory.ROMFilename;
  size_t pos;

  switch (dirtype)
  {
  case HOME_DIR:
    retval = std::string(getenv("HOME"));
    break;

  case ROMFILENAME_DIR:
    retval = Memory.ROMFilename;
    pos = retval.rfind("/");
    if (pos != std::string::npos)
      retval = retval.substr(pos);
    break;

  default:
    retval = std::string(".");
    break;
  }
  return retval;
}

std::string S9xGetFilenameInc(std::string ex, enum s9x_getdirtype dirtype)
{
  struct stat buf;

  SplitPath path = splitpath(Memory.ROMFilename);
  std::string directory = S9xGetDirectory(dirtype);

  if (ex[0] != '.')
  {
    ex = "." + ex;
  }

  std::string new_filename;
  unsigned int i = 0;
  do
  {
    std::string new_extension = std::to_string(i);
    while (new_extension.length() < 3)
      new_extension = "0" + new_extension;
    new_extension += ex;

    new_filename = path.stem + new_extension;
    i++;
  } while (stat(new_filename.c_str(), &buf) == 0 && i < 1000);

  return new_filename;
}

const char *S9xBasename(const char *f)
{
  const char *p;

  if ((p = strrchr(f, '/')) != NULL || (p = strrchr(f, '\\')) != NULL)
    return (p + 1);

  return (f);
}

bool8 S9xOpenSnapshotFile(const char *filename, bool8 read_only, STREAM *file)
{
  if (read_only)
  {
    if ((*file = OPEN_STREAM(filename, "rb")))
      return (true);
    else
      fprintf(stderr, "Failed to open file stream for reading.\n");
  }
  else
  {
    if ((*file = OPEN_STREAM(filename, "wb")))
    {
      return (true);
    }
    else
    {
      fprintf(stderr, "Couldn't open stream with zlib.\n");
    }
  }

  fprintf(stderr, "Couldn't open snapshot file:\n%s\n", filename);

  return false;
}

void S9xCloseSnapshotFile(STREAM file)
{
  CLOSE_STREAM(file);
}

bool8 S9xInitUpdate(void)
{
  return (TRUE);
}

bool8 S9xDeinitUpdate(int width, int height)
{
  S9xPutImage(width, height);
  return (TRUE);
}

bool8 S9xContinueUpdate(int width, int height)
{
  return (TRUE);
}

void S9xToggleSoundChannel(int c)
{
  static uint8 sound_switch = 255;

  if (c == 8)
    sound_switch = 255;
  else
    sound_switch ^= 1 << c;

  S9xSetSoundControl(sound_switch);
}

void S9xAutoSaveSRAM(void)
{
  Memory.SaveSRAM(S9xGetFilename(".srm", SRAM_DIR).c_str());
}

void S9xSyncSpeed(void)
{
#ifndef NOSOUND
  if (Settings.SoundSync)
  {
    return;
  }
#endif

  if (Settings.DumpStreams)
    return;

  if (Settings.HighSpeedSeek > 0)
    Settings.HighSpeedSeek--;

  if (Settings.TurboMode)
  {
    if ((++IPPU.FrameSkip >= Settings.TurboSkipFrames) && !Settings.HighSpeedSeek)
    {
      IPPU.FrameSkip = 0;
      IPPU.SkippedFrames = 0;
      IPPU.RenderThisFrame = TRUE;
    }
    else
    {
      IPPU.SkippedFrames++;
      IPPU.RenderThisFrame = FALSE;
    }

    return;
  }

  static struct timeval next1 = {0, 0};
  struct timeval now;

  while (gettimeofday(&now, NULL) == -1)
    ;

  // If there is no known "next" frame, initialize it now.
  if (next1.tv_sec == 0)
  {
    next1 = now;
    next1.tv_usec++;
  }

  // If we're on AUTO_FRAMERATE, we'll display frames always only if there's excess time.
  // Otherwise we'll display the defined amount of frames.
  unsigned limit = (Settings.SkipFrames == AUTO_FRAMERATE) ? (timercmp(&next1, &now, <) ? 10 : 1) : Settings.SkipFrames;

  IPPU.RenderThisFrame = (++IPPU.SkippedFrames >= limit) ? TRUE : FALSE;

  if (IPPU.RenderThisFrame)
    IPPU.SkippedFrames = 0;
  else
  {
    // If we were behind the schedule, check how much it is.
    if (timercmp(&next1, &now, <))
    {
      unsigned lag = (now.tv_sec - next1.tv_sec) * 1000000 + now.tv_usec - next1.tv_usec;
      if (lag >= 500000)
      {
        // More than a half-second behind means probably pause.
        // The next line prevents the magic fast-forward effect.
        next1 = now;
      }
    }
  }

  // Delay until we're completed this frame.
  // Can't use setitimer because the sound code already could be using it. We don't actually need it either.
  while (timercmp(&next1, &now, >))
  {
    // If we're ahead of time, sleep a while.
    unsigned timeleft = (next1.tv_sec - now.tv_sec) * 1000000 + next1.tv_usec - now.tv_usec;
    usleep(timeleft);

    while (gettimeofday(&now, NULL) == -1)
      ;
    // Continue with a while-loop because usleep() could be interrupted by a signal.
  }

  // Calculate the timestamp of the next frame.
  next1.tv_usec += Settings.FrameTime;
  if (next1.tv_usec >= 1000000)
  {
    next1.tv_sec += next1.tv_usec / 1000000;
    next1.tv_usec %= 1000000;
  }
}

bool S9xPollButton(uint32 id, bool *pressed)
{
  return false;
}

bool S9xPollAxis(uint32 id, int16 *value)
{
  return false;
}

bool S9xPollPointer(uint32 id, int16 *x, int16 *y)
{
  return false;
}

void S9xHandlePortCommand(s9xcommand_t cmd, int16 data1, int16 data2)
{
}

#define ASSIGN_BUTTONf(n, s) S9xMapButton(n, S9xGetCommandT(s), false)

void S9xSetupDefaultKeymap(void)
{
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_d, "Joypad1 X");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_v, "Joypad1 A");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_c, "Joypad1 B");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_x, "Joypad1 Y");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_a, "Joypad1 L");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_s, "Joypad1 R");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_KP_ENTER, "Joypad1 Select");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_KP_SPACE, "Joypad1 Start");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_UP, "Joypad1 Up");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_DOWN, "Joypad1 Down");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_LEFT, "Joypad1 Left");
  ASSIGN_BUTTONf(SDL_KeyCode::SDLK_RIGHT, "Joypad1 Right");
}

bool audio_write_samples(int16_t *data, int samples)
{
  bool retval = true;
  auto empty = audio_buffer.space_empty();
  if (samples > empty)
  {
    retval = false;
    audio_buffer.dump(audio_buffer.buffer_size / 2 - empty);
  }
  audio_buffer.push(data, samples);

  return retval;
}

void audio_mix(void *userdata, unsigned char *output, int bytes)
{
  if (audio_buffer.avail() >= bytes >> 1)
    audio_buffer.read((int16_t *)output, bytes >> 1);
  else
  {
    audio_buffer.read((int16_t *)output, audio_buffer.avail());
    audio_buffer.add_silence(audio_buffer.buffer_size / 2);
  }
}

static std::vector<int16_t> temp_buffer;
void S9xSamplesAvailable(void *data)
{
#ifndef NOSOUND

  bool clear_leftover_samples = false;
  int samples = S9xGetSampleCount();
  int space_free = audio_buffer.space_empty();

  if (space_free < samples)
  {
    if (!Settings.SoundSync)
      clear_leftover_samples = true;

    if (Settings.SoundSync && !Settings.TurboMode && !Settings.Mute)
    {
      for (int i = 0; i < 200; i++) // Wait for a max of 5ms
      {
        space_free = audio_buffer.space_empty();
        if (space_free < samples)
          usleep(50);
        else
          break;
      }
    }
  }

  if (space_free < samples)
    samples = space_free & ~1;

  if (samples == 0)
  {
    S9xClearSamples();
    return;
  }

  if ((int)temp_buffer.size() < samples)
    temp_buffer.resize(samples);
  S9xMixSamples((uint8_t *)temp_buffer.data(), samples);
  audio_write_samples(temp_buffer.data(), samples);

  if (clear_leftover_samples)
    S9xClearSamples();

#endif // NOSOUND
}

bool8 S9xOpenSoundDevice(void)
{
#ifndef NOSOUND

  SDL_AudioSpec wanted, obtained;
  unsigned int bufferSize;

  /* set the audio format */
  wanted.freq = Settings.SoundPlaybackRate;
  wanted.format = AUDIO_S16;
  wanted.channels = 2; /* 1 = mono, 2 = stereo */
  wanted.samples = SOUND_BUFFER_SIZE;
  wanted.callback = audio_mix;
  wanted.userdata = NULL;

  audioDev = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);

  if (audioDev == 0)
  {
    fprintf(stderr, "Failed to SDL_OpenAudioDevice: %s\n", SDL_GetError());
    exit(1);
  }

  audio_buffer.resize(SOUND_BUFFER_SIZE * 4 * obtained.freq / 1000);

  SDL_PauseAudioDevice(audioDev, 1);

#endif // NOSOUND
  S9xSetSamplesAvailableCallback(S9xSamplesAvailable, NULL);

  return (TRUE);
}

void S9xExit(void)
{
  S9xSetSoundMute(TRUE);
  Settings.StopEmulation = TRUE;

  SDL_CloseAudioDevice(audioDev);

  Memory.SaveSRAM(S9xGetFilename(".srm", SRAM_DIR).c_str());
  S9xResetSaveTimer(FALSE);
  S9xUnmapAllControls();
  Memory.Deinit();
  S9xDeinitAPU();

  SDL_Quit();
}

void S9xInitDisplay(int argc, char **argv)
{
  sprintf(String, "\"%s\" %s: %s", Memory.ROMName, TITLE, VERSION);

  int w = SNES_WIDTH, h = SNES_HEIGHT_EXTENDED;
  window = SDL_CreateWindow(String, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w * 2, h * 2, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (window == NULL)
  {
    fprintf(stderr, "Failed to SDL_CreateWindow\n");
    exit(1);
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer == NULL)
  {
    fprintf(stderr, "Failed to SDL_CreateRenderer\n");
    exit(1);
  }

  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
  if (texture == NULL)
  {
    fprintf(stderr, "Failed to SDL_CreateTexture\n");
    exit(1);
  }

  SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);

  S9xGraphicsInit();
}

#undef main
int main(int argc, char **argv)
{
  if (argc < 2)
    S9xUsage();

  printf("\n\nSnes9x " VERSION "\n");

  memset(&Settings, 0, sizeof(Settings));
  Settings.MouseMaster = TRUE;
  Settings.SuperScopeMaster = TRUE;
  Settings.JustifierMaster = TRUE;
  Settings.MultiPlayer5Master = TRUE;
  Settings.FrameTimePAL = 20000;
  Settings.FrameTimeNTSC = 16667;
  Settings.SixteenBitSound = TRUE;
  Settings.Stereo = TRUE;
  Settings.SoundPlaybackRate = 48000;
  Settings.SoundInputRate = 31950;
  Settings.Transparency = TRUE;
  Settings.AutoDisplayMessages = TRUE;
  Settings.InitialInfoStringTimeout = 120;
  Settings.HDMATimingHack = 100;
  Settings.BlockInvalidVRAMAccessMaster = TRUE;
  Settings.StopEmulation = TRUE;
  Settings.WrongMovieStateProtection = TRUE;
  Settings.DumpStreamsMaxFrames = -1;
  Settings.StretchScreenshots = 1;
  Settings.SnapshotScreenshots = TRUE;
  Settings.SkipFrames = AUTO_FRAMERATE;
  Settings.TurboSkipFrames = 15;
  Settings.CartAName[0] = 0;
  Settings.CartBName[0] = 0;

  CPU.Flags = 0;

  S9xLoadConfigFiles(argv, argc);
  rom_filename = S9xParseArgs(argv, argc);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0)
  {
    fprintf(stderr, "Failed to initialize SDL with joystick support. Retrying without.\n");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
      fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
      exit(1);
    }
  }

  if (!Memory.Init() || !S9xInitAPU())
  {
    fprintf(stderr, "Snes9x: Memory allocation failure - not enough RAM/virtual memory available.\nExiting...\n");
    Memory.Deinit();
    S9xDeinitAPU();
    exit(1);
  }

  S9xInitSound(0);
  S9xSetSoundMute(TRUE);

  S9xReportControllers();

  uint32 saved_flags = CPU.Flags;
  bool8 loaded = FALSE;

  if (rom_filename)
  {
    loaded = Memory.LoadROM(rom_filename);

    if (!loaded && rom_filename[0])
    {
      SplitPath path = splitpath(rom_filename);
      std::string s = makepath("", S9xGetDirectory(ROM_DIR), path.stem, path.ext);
      loaded = Memory.LoadROM(s.c_str());
    }
  }

  if (!loaded)
  {
    fprintf(stderr, "Error opening the ROM file.\n");
    exit(1);
  }

  Memory.LoadSRAM(S9xGetFilename(".srm", SRAM_DIR).c_str());

  CPU.Flags = saved_flags;
  Settings.StopEmulation = FALSE;

  S9xInitDisplay(argc, argv);
  S9xSetupDefaultKeymap();

#ifdef JOYSTICK_SUPPORT
  uint32 JoypadSkip = 0;
#endif

  S9xSetSoundMute(FALSE);
  SDL_PauseAudioDevice(audioDev, 0);

  while (1)
  {
    SDL_Event event;
    SDL_PollEvent(&event);

    if (event.type == SDL_QUIT)
    {
      break;
    }

    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
    {
      S9xReportButton(event.key.keysym.sym, event.key.state == SDL_PRESSED);
    }

    if (!Settings.Paused)
    {
      S9xMainLoop();
    }

    if (Settings.Paused && frame_advance)
    {
      S9xMainLoop();
      frame_advance = 0;
    }

    if (Settings.Paused)
      S9xSetSoundMute(TRUE);

    if (Settings.Paused)
    {
      usleep(100000);
    }

    if (!Settings.Paused)
      S9xSetSoundMute(FALSE);
  }

  return (0);
}
