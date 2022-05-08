/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "snes9x.h"

void S9xUsage (void);
char * S9xParseArgs (char **, int);
void S9xParseArgsForCheats (char **, int);
void S9xLoadConfigFiles (char **, int);
void S9xSetInfoString (const char *);

// Routines the port has to implement even if it doesn't use them

void S9xPutImage (int, int);
void S9xInitDisplay (int, char **);
void S9xDeinitDisplay (void);
void S9xTextMode (void);
void S9xGraphicsMode (void);
void S9xToggleSoundChannel (int);
bool8 S9xOpenSnapshotFile (const char *, bool8, STREAM *);
void S9xCloseSnapshotFile (STREAM);
const char * S9xStringInput (const char *);
std::string S9xGetDirectory (enum s9x_getdirtype);
std::string S9xGetFilenameInc (std::string, enum s9x_getdirtype);
std::string S9xBasename (std::string);
std::string S9xBasenameNoExt (std::string);

// Routines the port has to implement if it uses command-line

void S9xExtraUsage (void);
void S9xParseArg (char **, int &, int);

// Routines the port may implement as needed

void S9xExtraDisplayUsage (void);
void S9xParseDisplayArg (char **, int &, int);
void S9xSetTitle (const char *);
void S9xInitInputDevices (void);
void S9xProcessEvents (bool8);
const char * S9xSelectFilename (const char *, const char *, const char *, const char *);

#endif
