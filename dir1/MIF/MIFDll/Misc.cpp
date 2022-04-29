/*************************************************************************************
Misc.cpp - Misc stuff

begun 21/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "resource.h"

/*************************************************************************************
IsWAVResource - Tests to see if the given filename is a wave resource "r1234.wav",
where 1234 is the wave number.

inputs
   PWSTR       psz - String to test
returns
   DWORD - Resource number, or 0 if it's not a wave resource
*/
DWORD IsWAVResource (PWSTR psz)
{
   if ((psz[0] != L'r') && (psz[0] != L'R'))
      return 0;
   psz++;

   DWORD dwRet = 0;
   for ( ; iswdigit (psz[0]); psz++)
      dwRet = (dwRet * 10) + (DWORD)(psz[0]-L'0');

   if (!_wcsicmp(psz, L".wav"))
      return dwRet;
   else
      return 0;
}

/*************************************************************************************
CircumrealityImage - String for an image name.
*/
PWSTR CircumrealityImage (void)
{
   return L"Image";
}

/*************************************************************************************
Circumreality3DScene - String for an 3D scene
*/
PWSTR Circumreality3DScene (void)
{
   return L"ThreeDScene";
}

/*************************************************************************************
CircumrealityTitle - String for a title
*/
PWSTR CircumrealityTitle (void)
{
   return L"Title";
}


/*************************************************************************************
CircumrealityText - String for a MML text
*/
PWSTR CircumrealityText (void)
{
   return L"Text";
}


/*************************************************************************************
CircumrealityLotsOfText - String for a MML text
*/
PWSTR CircumrealityLotsOfText (void)
{
   return L"LotsOfText";
}

/*************************************************************************************
Circumreality3DObjects - String for an 3D scene
*/
PWSTR Circumreality3DObjects (void)
{
   return L"ThreeDObjects";
}


/*************************************************************************************
CircumrealityPosition360 - String for an 3D scene
*/
PWSTR CircumrealityPosition360 (void)
{
   return L"Position360";
}


/*************************************************************************************
CircumrealityFOVRange360 - Set the field of view of the 360 degree camera.
*/
PWSTR CircumrealityFOVRange360 (void)
{
   return L"FOVRange360";
}


/*************************************************************************************
CircumrealityThreeDSound - How 3d sound parameters work
*/
PWSTR CircumrealityThreeDSound (void)
{
   return L"ThreeDSound";
}

/*************************************************************************************
CircumrealityWave - String for an wave resource
*/
PWSTR CircumrealityWave (void)
{
   return L"Wave";
}


/*************************************************************************************
CircumrealityCutScene - String for an wave resource
*/
PWSTR CircumrealityCutScene (void)
{
   return L"CutScene";
}


/*************************************************************************************
CircumrealitySpeakScript - String for an script
*/
PWSTR CircumrealitySpeakScript (void)
{
   return L"SpeakScript";
}


/*************************************************************************************
CircumrealityConvScript - String for an script
*/
PWSTR CircumrealityConvScript (void)
{
   return L"ConvScript";
}


/*************************************************************************************
CircumrealityMusic - String for an wave resource
*/
PWSTR CircumrealityMusic (void)
{
   return L"Music";
}


/*************************************************************************************
CircumrealityTitleInfo - String for an wave resource
*/
PWSTR CircumrealityTitleInfo (void)
{
   return L"TitleInfo";
}



/*************************************************************************************
CircumrealityVoice - String for an wave resource
*/
PWSTR CircumrealityVoice (void)
{
   return L"Voice";
}



/*************************************************************************************
CircumrealitySpeak - String for speaking
*/
PWSTR CircumrealitySpeak (void)
{
   return L"Speak";
}


/*************************************************************************************
CircumrealitySilence - String for silence
*/
PWSTR CircumrealitySilence (void)
{
   return L"Silence";
}


/*************************************************************************************
CircumrealityQueue - Sequence of events to run.
*/
PWSTR CircumrealityQueue (void)
{
   return L"Queue";
}


/*************************************************************************************
CircumrealityDelay - Sub-element of CircumrealityQueue
*/
PWSTR CircumrealityDelay (void)
{
   return L"Delay";
}

/*************************************************************************************
CircumrealityTransPros - Transplanted prosody
*/
PWSTR CircumrealityTransPros (void)
{
   return L"TransPros";
}

/*************************************************************************************
CircumrealityIconWindow - Returns the string for the "IconWindow"
*/
PWSTR CircumrealityIconWindow (void)
{
   return L"IconWindow";
}


/*************************************************************************************
CircumrealityVerbWindow - Returns the string for the "VerbWindow"
*/
PWSTR CircumrealityVerbWindow (void)
{
   return L"VerbWindow";
}

/*************************************************************************************
CircumrealityVerbChat - Returns the string for the "VerbChat"
*/
PWSTR CircumrealityVerbChat (void)
{
   return L"VerbChat";
}

/*************************************************************************************
CircumrealityAutoMap - Returns the string for the "AutoMap"
*/
PWSTR CircumrealityAutoMap (void)
{
   return L"AutoMap";
}


/*************************************************************************************
CircumrealityAutoMapShow - Returns the string for the "AutoMapShow"
*/
PWSTR CircumrealityAutoMapShow (void)
{
   return L"AutoMapShow";
}

/*************************************************************************************
CircumrealityMapPointTo - Returns the string for the "AutoMap"
*/
PWSTR CircumrealityMapPointTo (void)
{
   return L"MapPointTo";
}

/*************************************************************************************
CircumrealityGeneralMenu - Returns the string for the "GeneralMenu"
*/
PWSTR CircumrealityGeneralMenu (void)
{
   return L"GeneralMenu";
}

/*************************************************************************************
CircumrealityNLPRuleSet - Returns the string for the "NLPRuleSet"
*/
PWSTR CircumrealityNLPRuleSet (void)
{
   return L"NLPRuleSet";
}

/*************************************************************************************
CircumrealityIconWindowDeleteAll - Returns the string for the "IconWindowDeleteAll"
*/
PWSTR CircumrealityIconWindowDeleteAll (void)
{
   return L"IconWindowDeleteAll";
}


/*************************************************************************************
CircumrealityObjectDisplay - Returns the string for the "ObjectDisplay"
*/
PWSTR CircumrealityObjectDisplay (void)
{
   return L"ObjectDisplay";
}



/*************************************************************************************
CircumrealityDisplayWindow - Returns the string for the "DisplayWindow"
*/
PWSTR CircumrealityDisplayWindow (void)
{
   return L"DisplayWindow";
}


/*************************************************************************************
CircumrealityNotInMain - Returns the string for the "NotInMain"
*/
PWSTR CircumrealityNotInMain (void)
{
   return L"NotInMain";
}

/*************************************************************************************
CircumrealityDisplayWindowDeleteAll - Returns the string for the "DisplayWindowDeleteAll"
*/
PWSTR CircumrealityDisplayWindowDeleteAll (void)
{
   return L"DisplayWindowDeleteAll";
}


/*************************************************************************************
CircumrealityCommandLine - Returns the string for the "CommandLine"
*/
PWSTR CircumrealityCommandLine (void)
{
   return L"CommandLine";
}

/*************************************************************************************
CircumrealityAmbient - Returns the string for the "Ambient"
*/
PWSTR CircumrealityAmbient (void)
{
   return L"Ambient";
}

/*************************************************************************************
CircumrealityAmbientLoopVar - Returns the string for the "AmbientLoopVar"
*/
PWSTR CircumrealityAmbientLoopVar (void)
{
   return L"AmbientLoopVar";
}

/*************************************************************************************
CircumrealityAmbientSounds - Returns the string for the "AmbientSounds"
*/
PWSTR CircumrealityAmbientSounds (void)
{
   return L"AmbientSounds";
}

/*************************************************************************************
CircumrealityTransition - Returns the string for the "Transition"
*/
PWSTR CircumrealityTransition (void)
{
   return L"Transition";
}



/*************************************************************************************
CircumrealityAutoCommand - Returns the string for the "AutoCommand"
*/
PWSTR CircumrealityAutoCommand (void)
{
   return L"AutoCommand";
}

/*************************************************************************************
CircumrealityChangePassword - Returns the string for the "ChangePassword"
*/
PWSTR CircumrealityChangePassword (void)
{
   return L"ChangePassword";
}


/*************************************************************************************
CircumrealityLogOff - Returns the string for the "LogOff"
*/
PWSTR CircumrealityLogOff (void)
{
   return L"LogOff";
}


/*************************************************************************************
CircumrealityPointOutWindow - Returns the string for the "PointOutWindow"
*/
PWSTR CircumrealityPointOutWindow (void)
{
   return L"PointOutWindow";
}


/*************************************************************************************
CircumrealityTutorial - Returns the string for the "Tutorial"
*/
PWSTR CircumrealityTutorial (void)
{
   return L"Tutorial";
}

/*************************************************************************************
CircumrealitySwitchToTab - Returns the string for the "SwitchToTab"
*/
PWSTR CircumrealitySwitchToTab (void)
{
   return L"SwitchToTab";
}



/*************************************************************************************
CircumrealityHelp- Returns the string for the "Help"
*/
PWSTR CircumrealityHelp (void)
{
   return L"Help";
}

/*************************************************************************************
CircumrealityTranscriptMML- Returns the string for the "TranscriptMML"
*/
PWSTR CircumrealityTranscriptMML (void)
{
   return L"TranscriptMML";
}


/*************************************************************************************
CircumrealityTextBackground- Returns the string for the "TextBackground"
*/
PWSTR CircumrealityTextBackground (void)
{
   return L"TextBackground";
}

/*************************************************************************************
ResHelpStringPrefix - Returns the prefix string

inputs
   BOOL        fLightBackground - TRUE if light backgound, FALSE if dark
*/
PWSTR ResHelpStringPrefix (BOOL fLightBackground)
{
   if (fLightBackground)
      return L"<colorblend skipifbackground=true posn=background tcolor=#ffffff bcolor=#c0c0ff/><font color=#101000><small>";
   else
      return L"<colorblend skipifbackground=true posn=background tcolor=#000080 bcolor=#000000/><font color=#ffffff><small>";
   // return L"<colorblend skipifbackground=true posn=background tcolor=#202060 bcolor=#000020/><font color=#ffffff><small>";
}


/*************************************************************************************
ResHelpStringSuffix - Returns the suffix string
*/
PWSTR ResHelpStringSuffix (void)
{
   return L"</small></font>";
}


/*************************************************************************************
CircumrealityUploadImageLimits- Returns the string for the "UploadImageLimits"
*/
PWSTR CircumrealityUploadImageLimits (void)
{
   return L"UploadImageLimits";
}


/*************************************************************************************
CircumrealityBinaryDataRefresh- Returns the string for the "BinaryDataRefresh"
*/
PWSTR CircumrealityBinaryDataRefresh (void)
{
   return L"BinaryDataRefresh";
}


/*************************************************************************************
CircumrealityVoiceChat- Returns the string for the "VoiceChat"
*/
PWSTR CircumrealityVoiceChat (void)
{
   return L"VoiceChat";
}



/*************************************************************************************
CircumrealityVoiceChatInfo - Returns the string for the "VoiceChatInfo"
*/
PWSTR CircumrealityVoiceChatInfo (void)
{
   return L"VoiceChatInfo";
}


/*************************************************************************************
CircumrealityPieChart- Returns the string for the "PieChart"
*/
PWSTR CircumrealityPieChart (void)
{
   return L"PieChart";
}

/*************************************************************************************
CircumrealitySliders- Returns the string for the "Sliders"
*/
PWSTR CircumrealitySliders (void)
{
   return L"Sliders";
}


/*************************************************************************************
CircumrealityHypnoEffect- Returns the string for the "HypnoEffect"
*/
PWSTR CircumrealityHypnoEffect (void)
{
   return L"HypnoEffect";
}


/*************************************************************************************
CircumrealityPreRender - Returns the string for the "PreRender"
*/
PWSTR CircumrealityPreRender (void)
{
   return L"PreRender";
}


/************************************************************************************
DOCUMENT: The following MML tags are supported by the client/server:

Client:

The following should only appear in <ObjectDisplay>
   Image - Causes it to draw a JPG/BMP image. Format described elewhere.
   ThreeDScene - Causes it to draw a 3d scene. Format described elsewhere
   ThreeDObjects - Causes it to draw 3d objects. Format descibed elsewhere
   Title - Causes it to draw a title. Format described elswhere.
   Text - Displays mml text. NOTE: Links in the text will cause a message to be
      sent. If the link contains <> then it will be replaced with the control name,
      such as "<a href="tell mike &lt;myedit&gt/>", will look for "myedit" control.
      <text>
         <origtext v="origtextstring"/>
         <langid v=1033/>        // language ID for the commands
         <mml>
            .. various MML, such as <p>Hello</p>
         </mml>
      </text>


Position360 - Changes the longitude and latitude that looking at for 360 degree views. Format is:
   <position360>
      <lrangle v=90/>         // (Optional) v = angle in DEGREES, 0 = N, 90 = E, 180 = S, etc.
      <udangle v=0/>          // (Optional) v = angle in DEGREES, 0 = straight ahead, 90 = straigh up, etc.
      <fov v=90/>             // (Optional) v = angle in DEGREES of field of view
   </position360>

FOVRange360 - Field of view range for the 360 degree image
   <FOXRange360>
      <min v=15/>             // (Optional) v = angle in DEGREES. Minimum angle for FOC
      <max v=150/>            // (Optional) v = agnle in DEGREES. Maximum angle for FOV
      <curvature v=XXX/>      // (Optional) curvature in the camera, from 0 (flat) to 1 (very wide angle)
   </FOXRange360>

ThreeDSound - Affects how the player hears threeD sound. Used for races with different hearing
   <ThreeDSound>
      <separation v=XXX/>     // (Optional) 1.0 = human hearing, lower numbers (down to 0.0) created focused hearing
      <power v=XXX/>          // (Optional) Power to raise distance to, when affecting distnace sound. 2.0 = normal
      <scale v=XXX/>          // (Optional) All 3D sound volumes are scaled by this much
      <back v=XXX/>           // (Optional) How much sound from behind are scaled. 1.0 = none. 0.75 = human, lower values more focused
   </ThreeDSound>

Wave - Plays a wave file right away.
   <wave>
      <file v=FILENAME/>      // filename
                              // NOTE: This cal also be "r1234.wav", which is wave resource #1234
      <voll v=1.0/>           // (Optional) left volume, 1.0 = no change
      <volr v=1.0/>           // (Optional) right volume, 1.0 = no change
      <fadein v=1/>           // (Optional) fade in time, in seconds
      <fadeout v=1/>          // (optional) fade out time, in seconds
      <ID v=XXX/>             // (Optional) if there, this is the GUID of the object making the sound
      <vol3d q0=xxx q1=yyy q2=zzz q3=aaa/> // (Optional) If this is specified, then voll and volr
                              // are ignored, and this is the 3d space location.
                              // xxx = EW, yyy = NS, zzz = up/down, q4 = volume in decibels (60 = normal speech)
   </wave>

VoiceChat - From the server
   <VoiceChat>
      <name v=NAME/>          // (Optional) name to give to the voice
      <ID v=XXX/>             // GUID for the object speaking
      <Language v=XXX/>       // (Optional) Fictional language being spoken, displayed on user's screen
      <voll v=1.0/>           // (Optional) left volume, 1.0 = no change
      <volr v=1.0/>           // (Optional) right volume, 1.0 = no change
      <vol3d q0=xxx q1=yyy q2=zzz q3=aaa/> // (Optional) If this is specified, then voll and volr
                              // are ignored, and this is the 3d space location.
                              // xxx = EW, yyy = NS, zzz = up/down, q4 = volume in decibels (60 = normal speech)
   </VoiceChat> PLUS special binary appended

VoiceChat - Sent to the server from the client
   <VoiceChat>
      <style v=xxx/>          // (optional) speaking style, use "speak", "yell", or "whisper"
      <Language v=XXX/>       // (Optional) Fictional language being spoken, as was displayed on user's screen
      <ID v=XXX/>           // (Optional) GUID for the object that speaking to
   </VoiceChat> PLUS special binary appended

Music - Plays a MIDI file right away.
   <Music>
      <file v=FILENAME/>      // filename (.mid) (This can be left out if have a <binary> section
      <binary v=XXX/>         // (Optional) binary (hex) version of the MIDI file, so can generate MIDI on server and transmit
      <voll v=1.0/>           // (Optional) left volume, 1.0 = no change
      <volr v=1.0/>           // (Optional) right volume, 1.0 = no change
      <ID v=XXX/>             // (Optional) if there, this is the GUID of the object making the sound
      <vol3d q0=xxx q1=yyy q2=zzz q3=aaa/> // (Optional) If this is specified, then voll and volr
                              // are ignored, and this is the 3d space location.
                              // xxx = EW, yyy = NS, zzz = up/down, q4 = volume in decibels (60 = normal speech)
   </Music>

Voice - Specified the voice to used. Included as part of the <speak> tag.
   <voice>
      <file v=FILENAME.tts/>   // tts file
      <name v=NAME/>          // (Optional) name to give to the voice
      <ID v=XXX/>             // (Optional) GUID for the object speaking
      <Language v=XXX/>       // (Optional) Fictional language being spoken, displayed on user's screen
      <voll v=1.0/>           // (Optional) left volume, 1.0 = no change
      <volr v=1.0/>           // (Optional) right volume, 1.0 = no change
      <vol3d q0=xxx q1=yyy q2=zzz q3=aaa/> // (Optional) If this is specified, then voll and volr
                              // are ignored, and this is the 3d space location.
                              // xxx = EW, yyy = NS, zzz = up/down, q4 = volume in decibels (60 = normal speech)
   </voice>

TransPros - Transplanted prosody definition.

Speak - Has TTS speak.
   <speak (optional - indicates PCM on its way)quality=x (optional)speed=x>
      <voice>...</voice>      // define the voice
      tagged text             // what to speak, could be <transpros>xxx</transpros>
   </speak>

<TextBackground> - Changes the background color
   <Light v=x/> (Optional) - If 1, then set the background to white-ish (with black text), 0 then blackish (with white text)
   <Image v=x/> (Optional) - Image string for background
</TextBackground>

<TranscriptMML> - Used to MML text into the transcript. Used for combat
   <name v=NAME/>          // (Optional) name to give to the voice
   <ID v=XXX/>             // (Optional) GUID for the object speaking
   <MML>...</MML>          // MML text
</TranscriptMML>

Silence - Plays silence for the given number of seconds
   <silence>
      <time v=SECONDS/>       // plays silence for the given number of seconds
   </silence>

Delay - Used only as a sub-element of <queue>. This must contain one (and
only one) element for <time v=SECONDS/> to indicate that the events occur
N seconds after the delay item is reached, or <percent v=PERCENT/> to indicate
the delay occurs that many percent (0..100, or even above) into the next audio
event. All the other actions within delay, such as <image>, <ThreeDScene>, etc.
are run when the delay point is hit.
   <delay>
      <time v=SECONDS/> or <percent v=PERCENT/>
      One or more actions, such as <image>, <ThreeDScene>, etc.
         NOTE: These are ALL run at once
   </delay>

Queue - Sequence of audio events, and others to run. Each audio event
(<silence>, <speak>, <wave>) is run sequentially
waiting for the previous one to finish. Any non-audio events beween
these is acted upon before/after the audio event, as appropriate.
The queue tag can also include delays.
   <queue>
      <ObjectDisplay>xxxx</ObjectDisplay>
      <IconWindow>xxxx</IconWindow>
      <speak>xxxx</speak>
      <delay>xxxx</delay>
      <wave>xxxx</wave>
   </queue>


<IconWindow> - Displays a window with icons in it
   <ID v="XXX"/> - Identifier for icon window. EAch icon window has unqiue ID. Case insensative
   <Name v="XXX"/> - Name to display in window title
   <WindowLoc q0=XXX q1=XXX q2=XXX q3=XXX/> - Optional. This defaults the window
            to being located at the given coords. q0=left, q1=right, q2=top, q3=bottom,
            values range from 0..1, 0 being left/top of main window, 1 being right/bottom of main window
            (Seetings rememebered for each user,
            so once a user changes, the settings will be remembered)
   <Hidden v=1/> - Optional. If TRUE then window will default to being created hidden,
            so it's only accessible from the menu. (Seetings rememebered for each user,
            so once a user changes, the settings will be remembered)
   <AutoShow v=1/> - Optional. If TRUE then the window will automatically be shown,
            even if the user had decided to hide it. Defaults to FALSE - which is no autoshow.
   <Append v=1/> - Optional. If set then existing groups and objects displayed are kept.
            Any groups/objects are appended to the list (or replace matches).
            If not set, then existing groups/objects that are not in the new
            list area cleared away.
   <Delete v=1/> - Optional. If set, then the iconwindow will be removed.
            (So there's no point in having groups)
   <ChatWindow v=1/> - Optional. If set, the iconwindow is used as a chat window,
            so it displays the verb buttons as well as an edit box and language
            selector. If not specified, then it's not a chat window.
   <Language v=XXX/> - Optional, and used only if ChatWindow is set. This
            is the list of languages that are understood by the actor, and displayed
            in the language drop-down.
            XXX is the language string, translated for the user's real-life language (like
            spanish, english, etc.)
   <Group> - Zero or more groups
      <Name v="XXX"/> - Name of group
      <CanChatTo v=1/> - If v=1 then can chat to characters in this group, is not set or undefined
            then can't chat to.
      <ObjectDisplay></ObjectDisplay> - Zero or more objects to display
   </Group>
</IconWindow>

<DisplayWindow> - Displays a window with the on object displayed in it
   <ID v="XXX"/> - Identifier for icon window. EAch icon window has unqiue ID. Case insensative
                  NOTE: The display window "Main" is the default one; when an icon is clicked,
                  the "Main" display will be changed to reflect the new icon. It will
                  be created if necessary.
   <Name v="XXX"/> - Name to display in window title
   <WindowLoc q0=XXX q1=XXX q2=XXX q3=XXX/> - Optional. This defaults the window
            to being located at the given coords. q0=left, q1=right, q2=top, q3=bottom,
            values range from 0..1, 0 being left/top of main window, 1 being right/bottom of main window
            (Seetings rememebered for each user,
            so once a user changes, the settings will be remembered)
   <Hidden v=1/> - Optional. If TRUE then window will default to being created hidden,
            so it's only accessible from the menu. (Seetings rememebered for each user,
            so once a user changes, the settings will be remembered)
   <Delete v=1/> - Optional. If set, then the iconwindow will be removed.
            (So there's no point in having groups)
   <AutoShow v=1/> - Optional. If TRUE then the window will automatically be shown,
            even if the user had decided to hide it. Defaults to FALSE - which is no autoshow.
   <ObjectDisplay>...</ObjectDisplay> - Object to display
</DisplayWindow>

<NotInMain> - Makes sure the given object is NOT displayed in the main view, because it has left the room
   <ID v=XXX/> - Guid for the object ID
</NotInMain>

<VerbWindow> - This causes the verb window to be displayed.
   <IconSize v=0,1,2/> - Optional: 0 = small, 1 = medium, 2=large. (The user can override
            this with their own setting.)
   <Version v=XXX/> - If the version string is different than the user's
            version number, the user will have their verb preferences deleted,
            and replaced by this. Use this when the verbs have changed, to
            ensure that users are updated.
   <WindowLoc q0=XXX q1=XXX q2=XXX q3=XXX/> - Optional. This defaults the window
            to being located at the given coords. q0=left, q1=right, q2=top, q3=bottom,
            values range from 0..1, 0 being left/top of main window, 1 being right/bottom of main window
            (Seetings rememebered for each user,
            so once a user changes, the settings will be remembered)
   <Hidden v=1/> - Optional. If TRUE then window will default to being created hidden,
            so it's only accessible from the menu. (Seetings rememebered for each user,
            so once a user changes, the settings will be remembered)
   <Delete v=1/> - Optional. If set, then the verbwindow will be totally hidden from
            the user.
   <Verb> - One or more of these
      <Icon v=XXX/> - Icon number, 0+
      <LangID v=XXX/> - Language ID to send command in
      <Do v=XXX/> - Command to send. If have "<Click>" in it then requires user
                     to click on an object in and iconwindow
      <Show v=XXX/> - Optional: If different than <do/>, then is string for tooltip
   </Verb>
</VerbWindow>


<VerbChat> - Verbs that will be displayed in the chat window
   <VerbWindow>...</VerbWindow> - As with verb window. Windowloc, Hidden, and Delete are NOT used
</VerbChat>

<ObjectDisplay>
   <ID v=XXX/> - GUID for the ID
   <Name v="XXX"/> - Name of the object
   <Description v="XXX"/> - Description of the object
   <Other v="XXX"/> - Optional description of the object (such as "Worn on head")
   <MainView v=1/> - Optional. If set, then the main view will be set to show this object.
   <Delete v=1/> - Optional. If <ObjectDisplay> by itself, not part of <displaywindow>
               or <iconwindow>, then this will delete any images showing this object, based
               on the ID.
   <Image>, <ThreeDScene>, <ThreeDObjects>, <Title>, or <Text> - Visual display of object
   <ContextMenu> - Optional menu that is associated with the object
      <LangID v=1033/> - Language for the commands
      <Menu> - One or more menu items
         <Show v="XXX"/> - Show this on the menu
         <Do v="XXX/"> - Send this command, in the given language ID
      </Menu>
   </ContextMenu>

   <Sliders> - Optional list of sliders associated with the object
      <Slider v=XXX l=STRING r=STRING color=#RRGGBB/> - v is from -1 to 1. l = left string. r = right string. color = color of slider
   </Sliders>

   <HypnoEffect> - (Optional) Effect shown when this object is in the main view
      See below
   </HypnoEffect>
</ObjectDisplay>

<CommandLine>  - Shows/hides the command line, and sets the title
   <Hidden v=1/> - Optional. If set, hides the command line
   <Name v=XXX/> - Optional. Sets the name of the command line
   <WindowLoc q0=XXX q1=XXX q2=XXX q3=XXX/> - Optional. This defaults the window
            to being located at the given coords. q0=left, q1=right, q2=top, q3=bottom,
            values range from 0..1, 0 being left/top of main window, 1 being right/bottom of main window
            (Seetings rememebered for each user,
            so once a user changes, the settings will be remembered)
</CommandLine>

<GeneralMenu> - Displays a menu that the user can click on
   <LangID v=XXX/> - Language ID for the commands sent back
   <WindowLoc q0=XXX q1=XXX q2=XXX q3=XXX/> - Optional. This defaults the window
            to being located at the given coords. q0=left, q1=right, q2=top, q3=bottom,
            values range from 0..1, 0 being left/top of main window, 1 being right/bottom of main window
            (Seetings rememebered for each user,
            so once a user changes, the settings will be remembered)
   <Menu> - One or more menu items
      <Show v="XXX"/> - Show this on the menu
      <Do v="XXX/"> - Send this command, in the given language ID
      <Default v=1/> - One of the menus can have this (or none). It is the default
                        option and the one chosen if the timer goes off
   </Menu>
   <Exlusive v=1/> - Optional. If set, then the user will only be able to selecte
                     from the menu. The user will not be able to use clicking on
                     images or verb-window.
   <TimeOut v=XXX/> - If > 0, then a timer will be set to go off in this many seconds.
                     If the user hasn't clicked on one of the menu choices by then
                     then the default will automatically be selected
</GeneralMenu>
<GeneralMenu/> - Just sent an empty message to clear out the menu.


<AutoMap> - Sent to update the automatic map
   <ID v=XXX/> - GUID for the room. If the GUID is NULL, or ID not set,
                  and if <YouAreHere v=1/> is set, then informs the client
                  that the player character is nowhere
   <YouAreHere v=XXX/> - Optional (Defaults to 0). If 1 then informs client
                  that the player character is in the room. If 0 then just
                  adds the room to the database
   <CenterOnRoom v=xxx/> - Optional (Defaults to 0).  If 1 then the client will
                  center its display on this room.
   <Name v=XXX/> - Name of the room
   <Loc q0=xxx q1=xxx q2=xxx q3=xxx/> - Location of the room in global/world coords.
                  q0=center x in meters (positive = east),
                  q1 = center y in meters (positive = north),
                  q2 = room width in meters, q3 = room height in meters
   <Shape v=XXX/> - Optional (defaults to 0). 0 = rectangular, 1=elliptical
   <Rotation v=XXX/> - Optional (defaults to 0). Rotation in radians. 0 = none, + = counter-clockwise.
                     Should keep this between -PI/4 and PI/4
   <Zone v=XXX/> - Zone name
   <Region v=XXX/> - Region name
   <Map v=XXX/> - Map name
   <Color v=#rrggbb/> - Optional. Color for the room.
   <Description v=xxx/> - Description for the room, usually the objects in it
   <Exit> - Zero or more
      <Direction v=XXX/> - Direction. 0=N, 1=NE, clockwise, 8=Up, 9=Down, 10=In, 11=Out.
                  Use a direction only once per room
      <To v=XXX/> - GUID for the room that this links to
   </Exit>
</AutoMap>

<AutoMapShow> - Used to show/hide the automap
   <Delete v=1/> - Optional. If set, then the iconwindow will be removed.
            (So there's no point in having groups)
   <AutoShow v=1/> - Optional. If TRUE then the window will automatically be shown,
            even if the user had decided to hide it. Defaults to FALSE - which is no autoshow.
</AutoMapShow>

<MapPointTo> - Have the map point to a location, for directions
   <name v=XXX/> - Text to display such as "This way to Fred's house"
   <loc q0=xxx q1=yyy q2=zzz/> - Location of the point-to location in global/world coords
   <color v=#rrggbb/> - Optional. Color for the arrow
</MapPointTo>

<NLPRuleSet> - Resource used to identify NLP rules that are parsed
   <NLPRuleSet>...</NLPRuleSet> - Can contain 0 or more entries of itself
   <Syn from=XXX to=YYY prob=Z/> - Synonym, from string XXX, to string YYY. Probability = 0.001 to 99
                                   indicating probability of conversion being correct.
                                   XXX and YYY are CFG form, containing (), [], |, %n, and *n
   <Reword from=XXX to=YYY prob=Z/> - Like synonym, but for entire sentence
   <Split from=XXX to1=YYY to2=YYY t3=YYY etc. prob=Z/> - Like Reword, expect produces multiple
                                    sentences.
</NLPRuleSet>

   NOTE: In any message sent back to server, "<object>" will be translated into
   |XXX, where XXX is the object's guid. This makes it easier to parse since
   then object is known. Because it will be in MML, need to use &lt;object&gt;


<UploadImageLimits> - How many images can be uploaded
   <Num v=xxx/> - Maximum number of images
   <MaxWidth v=xxx/> - Maximum width in pixels
   <MaxHeight v=xxx/> - Maximum height in pixels
</UploadImageLimts>

<BinaryDataRefresh> - Tells client that the file is no longer valid
   <file v=xxx/> - File name
</BinaryDataRefresh>

<Ambient> - Specified a collection of ambient sounds to play. This should be part of
            a <AmbientSounds> call.
   <Name v=XXX/> - Name of the ambient sounds. This must be unique.
   <ID v=XXX/> - (Optional) GUID of the object associated with the ambient sounds, if there is one.
   <Random> - One or more random sections
      <Min q0=xxx q1=yyy q2=zzz q3=vvv/> - (Only if want 3D sound) Minimum location and volume of the sounds.
                  xxx = EW, yyy = NS, zzz = UD, vvv = decibels (60 = normal speech)
      <Max q0=xxx q1=yyy q2=zzz q3=vvv/> - (Only if want 3D sound) Like minimum, but the maxum
      <MinDist v=xxx/> - (Only if have 3d sound) Number of meters minimum distance that sound can occur
      <Vol q0=lmin, q1=lmax, q2=rmin, q3=rmax/> - (Only if NOT using 3d sound) Left and
                  right volumes, minimu and maximum values. 1 = default volume for wave
      <Time q0=xxx q1=yyy q2=zzz/> - xxx is the mimum time between sounds (in sec),
                  yyy is the maximum, zzz is the jitter (in sec)
      <Wave v=xxx/> - (Zero or more) One of the wave files that can be played
                              // NOTE: This cal also be "r1234.wav", which is wave resource #1234
      <Music v=xxx/> - (Zero or more) One of the MIDI files that can be played
   </Random>
   
   <Loop> - One of more loop sections
      <voll v=1.0/>           // (Optional) left volume, 1.0 = no change
      <volr v=1.0/>           // (Optional) right volume, 1.0 = no change
      <overlap v=0/>          // (Optional) number of seconds to overlap waves, so smother transition
      <vol3d q0=xxx q1=yyy q2=zzz q3=aaa/> // (Optional) If this is specified, then voll and volr
                              // are ignored, and this is the 3d space location.
                              // xxx = EW, yyy = NS, zzz = up/down, q4 = volume in decibels (60 = normal speech)
      <State> - One or more state sections, numbered automatically from 0
         <Wave v=xxx/> or <Music v=xxx/> - Zero or more wave/music files to play in sequence when enter state
                              // NOTE: This cal also be "r1234.wav", which is wave resource #1234
         <Branch v=xxx [var=yyy oper=zzz val=aaa]/> - Zero or more branches to take
                        xxx is the state number to branch to
                        (Optional) yyy is the variable to check, zzz is the operation to
                        use, -2 for <, -1 for <=, 0 for ==, 1 for >=, 2 for >=,
                        and aaa is the floating-point value
      </State>
   </Loop>
</Ambient>


<AmbientSounds> - Pass this to the client to specify what ambient sounds are currently playing
   <Ambient>...</Ambient> - Zero or more ambient sound schemes to use.
</AmbientSounds>

<AmbientLoopVar v=xxx val=yyy/> - Send this to set the value of the variables used
               but <AmbientLoop> objects. xxx is the varaible name to set, yyy is the
               double value. If a value is not set it's treated as 0.



<Transition> - Describes how to fade as new image shown. Can be part of <Image>, <ThreeDScene>, <ThreeDObjects>, or <Title>
   // fade from color
   <FadeFromDur v=xxx/> - (Optional) # seconds it takes to fade from a color.
   <FadeFromColor  v=xxx/> - (Only if FadeFromDur) Color to fade from

   // fade to color
   <FadeToStart v=xxx/> - (Optional) time at which starts fading to (in sec). Must be > 0
   <FadeToStop  v=xxx/> - (Only if FadeToStart) Time at which finished fading to
   <FadeToColor  v=xxx/> - (Only if FadeToStart) Color to fade to

   // fade in over existing image
   <FadeInDur v=xxx/> - (Optional) # seconds it takes to fade in over the existing image

   // transparent
   <Transparent v=xxx/> - (Optional) RGB color (as a number) used for the transparent part
   <TransparentDist v=xxx/> - (Optional, used if Transparent) Distance that's acceptable. Defaults to 0, but can ge 0..256*3

   // pan/zoom
   <PanStart q0=xxx q1=yyy q2=zzz/> - (Optional) Starting point for the pan
                                          xxx=x from 0..1, yyy=y from 0..1
                                          zzz = width comapred to max width, from 0..1
   <PanStop q0=xxx q1=yyy q2=zzz/> - (If PanStart) Finishing point for the pan. Same params as PanStart
   <PanStartTime v=xxx/> - (If PanStart) Time (in sec) when the pan starts moving. Must be > 0
   <PanStopTime v=xxx/> - (If PanStart) Time (in sec) when the pan stops moving

   // extra resoltion
   <ResExtra v=xxx/> - (Optional) If 1 then the generate image should be sqrt(2) larger in each
                        dimension. If 2, then 2x as large in each dimension.
                        NOTE: Won't affect an existing bitmap image.
</Transition>

<AutoCommand> - Sends a message to the user's machine so an automatic command will be
                  send back once this point in the queue is reached.
   <Command v=xxx/> - Text of the command
   <LangID v=1033/> - Language for the command
   <Silent v=1/> - (Optional) If this is set then the command won't be displayed on the
                     user's transcript. If FALSE then it will be displayed.
   <CanCancel v=1/> - (Optional) If this is set, then any action on the player's part will
                     cancel the command from happening.
   <CancelCommand v=xxx/> (Optional) If this is set and CanCancel=1 is set, then this command
                     will be run if the player issues a command that would cancel the action.
</AutoCommand>

<ChangePassword> - Causes the password stored on the user's machine to be changed.
                  This should only be called in response to a user's change password command
   <Password v=xxx/> - New password
</ChangePassword>

<LogOff/> - If the server receives this from the client it will disconnect the
            client. If the client receives from the server it will disconnect

<Tutorial> - Displays the contents as the tutorial of the transcript window.
   Varaious display tags, like <bold>This is the tutorial.</bold>
</Tutorial>

<PointOutWindow> - Causes the mouse cursor to move and point out the given window
                  on the client. It's used for tutorials. This will display the
                  window if it's hidden.
   <Type v=x/> - Window type. Can be "DisplayWindow", "IconWindow", "Command",
                  "Map", "Transcript", "Verb". "Progress", "Menu" also, work, but
                  cant be made visible if they're not there
   <ID v=x/> - Used only for displaywindow and iconwindow. This is the specific window.
</PointOutWindow>

<SwitchToTab> - Causes the tab on the client to switch to a new one.
   <ID v=x/> - Tab name. "Explore", "Chat", "Combat", "Misc"
</SwitchToTab>

<Help> - Display help in the window.
   <name v=XXX/> - Name of the help topic
   <descshort v=xxx/> - Short description of the help topic
   <help0 v=xxx/> - (Optional) - Help category it's in, separated by forward slash
   <help1 v=xxx/> - (Optional) - Second help category it's in, separated by forward slash
   <function v=xxx/> - (Optional) - Function to call to see if the help topic is visible to
                  the player. The first paramter is the actor object, while the second
                  is the functionparam value.
   <functionparam v=xxx/> - (Optional) - Parameter passed into function
   <book v=xxx/> - (Optional) Book that this is in. Usually leave blank.
   <origtext v="origtextstring"/> - Original text string for the MML
   <langid v=1033/>        // language ID for the commands
   <mml>
      .. various MML, such as <p>Hello</p>
   </mml>
</Help>

<PieChart> - Updates a pie char display
   <ID v=x/> - Pie chart number, from 0+.
   <Name v=x/> - Name of the pie chart. If this ISN'T specified then the pie chart will be deleted.
   <Value v=x/> - Floating point value from 0 to 1.
   <Delta v=x/> - (Optional) How much the value changes per second.
   <Color v=#RRGGBB/> - RGB color to display as
</PieChart>


<CutScene> - Sequence of dialogue, sounds, and visuals
</CutScene>

<SpeakScript> - Has single NPC speak a script
</SpeakScript>

<ConvScript> - Conversation script between 2-4 NPCs.
</ConvScript>

<PreRender> - Everything inside is used for rendering purposes only
   <Direction v=X/> - Direction that prerendering is in. 0 for N, 1 for NE, etc. 0..11
</PreRender>


<HypnoEffect>
   <name v=x/> - Name of the effect
   <duration v=x/> - (Optional) Duration of the effect in seconds. If not specified, set to 0 => no limit
   <priority v=x/> - (Optional) Priority number, used if have finite duration. If not specified, defaults to 1.
</HypnoEffect>

Server:

*/