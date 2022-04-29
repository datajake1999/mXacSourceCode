This is the source code to my game, three-D application, and text-to-speech.
This code is COPYLEFT. Feel free to redistribute it, modify it, incorporate
it into your own products, sell products based on it, etc.
I don't have time to work on it anymore.
In the unlikely event any of this infringes patents, you take responsibility.

To get this to work:
- Copy "dir1" to "c:\mxac". This is the C++ source code.
- Copy "dir2" to "c:\world". This is the source for the world.
- Run "subst z: c:\mxac"... as administrator.
- Run "subst w: c:\world"... as administrator.
- Use Visual studio.

Some c:\mxac tricks:
- You will need to set a src, bin, and lib directory in Visual studio. 
- You will need to build quite a few libraries that aren't
saved in the zip. Because of circular dependencies, this may be a pain.
- Only the smallest TTS voice files are included in the zip file.
- I changed the TTS file format, so the existing TTS files in the zip
may not work properly. You need the source files to rebuild them, but the
source .wavs aren't included in the zip. You can get them by
googling "arctic blizzard voices", or something like that. It will
be a lot of work to massage the original text though.

Some c:\world tricks:
- One file, Scripts\AshtariCommon.mfl needs to be written by you. It
contains all the passwords for your server, as well as IP addresses.
