# MfStub

MfStub is a proxy DLL for `mf.dll`, the main system DLL for [Media Foundation](https://en.wikipedia.org/wiki/Media_Foundation).

MfStub can improve the responsiveness of applications that (poorly) use Media Foundation, including Windows Media Player, by forcing the application to always use normal media sessions, instead of slow, unresponsive, DRM-crippled sessions.

**MfStub is designed and only tested to work with Windows Media Player 11 on Windows 7, but it may work with other applications that (mis)use Media Foundation in a similar way.**

Refer to the **How to Install** section for installation instructions.

### Background

Media Foundation is the primary media processing framework in Windows. It debuted with Windows Vista and is prominently used by most Microsoft applications provided with Windows, including Windows Media Player, Windows Media Center, and Internet Explorer.

One of the hallmark features of Media Foundation is the lockdown and restriction of DRM-shackled content during media playback. This is achieved through the [Protected Media Path](https://learn.microsoft.com/en-us/windows/win32/medfound/protected-media-path), which applications may opt-in to when playing media files.

The Protected Media Path (PMP) can enforce extremely expensive and slow trust-validation procedures, even when playing normal (DRM-free) media. Media playback will not start until the validation completes. This significantly degrades the user's experience, by wasting time and burning their CPU for absolutely zero benefit.

### Purpose

MfStub was specifically designed for use with Windows Media Player 11 on Windows 7, where WMP 11's usage of Media Foundation causes significant issues:

- WMP 11 *always* uses PMP sessions for many media types, including MP3 and WMA, *regardless of whether or not the specific file being played is DRM-shackled*.
- These PMP sessions enact a trust-validation process that is extremely inefficient and slow. On machines with a large [component store](https://en.wikipedia.org/wiki/Side-by-side_assembly#WinSxS), this step may take over 2 minutes to complete, before media playback is allowed to begin.
- WMP 11 initializes a new PMP session *every single time* a new track is played, repeating the extremely slow trust-validation process over again and significantly delaying playback.

As a result, casually listening to music in WMP 11 can result in a miserable user experience of:
1. Click "Play" on a track.
2. Sit through 2 minutes of silence before playback begins.
3. Playback ends and a new track is chosen.
4. Sit through another 2 minutes of silence before playback begins.
5. Rinse and repeat.

**MfStub resolves this issue by forcing WMP 11 to create normal (non-PMP) media sessions instead of PMP sessions.** Now, the file you clicked "Play" on will actually starting playing when you click "Play".

### DRM

MfStub does *not* bypass DRM requirements on DRM-shackled media. In fact, MfStub guarantees you will *never* be able to play DRM-shackled media, because your media player will never be able to create the necessary PMP session for it.

MfStub is only suitable for traditional users that enjoy DRM-free content and possess a normal media library, consisting of ripped CDs, downloads from iTunes, etc.

<br/>

# How to Install
MfStub is a proxy DLL. This means you install it by renaming it to the original DLL and placing it somewhere where the target application will find it first, before finding the original DLL. You also must rename (or hardlink/symlink) the original DLL to the altered name that the proxy expects. Once installed, all of the original DLL's functions are passed through the proxy to the original, except for the functions which need to be modified (in this case, PMP session creation).

Don't worry if the amount of text here makes this seem complicated. It really isn't.

### 1. Download
1. Go to the [Releases](https://github.com/TiberiumFusion/MfStub/releases) and pick a release, such as the [latest](https://github.com/TiberiumFusion/MfStub/releases/latest).
2. Download the `mfstub-all-dlls.zip` archive for that release.
2. Extract the `mfstub-all-dlls.zip` archive.<br/>
  Inside, you will find a bunch of different DLLs. Each one is for a specific Windows version and bitness (32 bit vs 64 bit).

### 2. Pick the correct MfStub DLL
You must pick the DLL that matches **1)** your Windows OS version and **2)** the bitness of the *target application* (not OS).

- `NT60`: for Windows Vista and Server 2008
- `NT61`: for Windows 7 and Server 2008 R2
- `NT63`: for Windows 8.1 and Server 2012 R2
- `NTBC`: for all versions of Bugcrash 10, including Retail, LTSC, Enterprise, IoT, Server 2016, 2019, etc.
- `x86`: for 32-bit applications (typically found in `C:\Program Files (x86)\`)
- `x64`: for 64-bit applications (typically found in `C:\Program Files\`)

*For example, if you use 64-bit Windows Media Player 11 on 64-bit Windows 7, you will choose `mfstub_NT61_x64.dll`.*

Your chosen DLL is the **proxy mf.dll**. Remember this.

### 3. Locate the correct system `mf.dll`
You must find the correct version of the real, system `mf.dll` that matches your *target application*'s bitness.

- If you have a 64-bit version of Windows:
  - The 64-bit version of `mf.dll` is located at: `C:\Windows\System32\mf.dll`
  - The 32-bit version of `mf.dll` is located at: `C:\Windows\SysWOW64\mf.dll` *(yes, the names seem to be backwards - this is correct)*

- If you have a 32-bit version of Windows:
  - The 32-bit version of `mf.dll` is located at: `C:\Windows\System32\mf.dll`
  - There is no 64-bit version.

*For example, if you use 64-bit Windows Media Player 11 on 64-bit Windows 7, you will need `C:\Windows\System32\mf.dll`.*

Your chosen DLL is the **original mf.dll**. Remember this.

<hr/>

Once you have identified the correct **proxy DLL** and **original DLL**, you can either install MfStub per-application (recommended) or system-wide.

<hr/>

### 4. Install per-application (recommended)
MfStub will only affect the application you install it for. All other programs on your computer will remain unaffected.

1. Locate the program files directory of your target application.
2. Copy/hardlink/symlink the **original mf.dll** into the application directory.<br/>
  *Note: Copying works today but might cause unexpected problems for you tomorrow. It is much better to symlink the original mf.dll to its new location instead, using a tool like [HardLinkShellExt](https://schinagl.priv.at/nt/hardlinkshellext/hardlinkshellext.html#download) (see [how](https://schinagl.priv.at/nt/hardlinkshellext/hardlinkshellext.html#usinglinkshellextension)).*
3. Renamed the copied/linked `mf.dll` to `m_.dll`.
4. Copy the correct **proxy mf.dll** into the application directory.
5. Rename the copied MfStub proxy DLL to `mf.dll`.

You will now have two new DLLs in your application directory.
- `m_.dll` is the original, real, system `mf.dll` that the proxy DLL will use.
- `mf.dll` is the MfStub proxy DLL that the target application will use.

#### Example process with Windows Media Player 11 (64-bit) on 64-bit Windows 7
1. Windows Media Player 11 (64-bit) is typically installed at `C:\Program Files\Windows Media Player`
2. Copy/hardlink/symlink `C:\Windows\System32\mf.dll` to `C:\Program Files\Windows Media Player\mf.dll`
3. Rename `C:\Program Files\Windows Media Player\mf.dll` to `m_.dll`
4. Copy `mfstub_NT61_x64.dll` to `C:\Program Files\Windows Media Player\mfstub_NT61_x64.dll`
5. Rename `C:\Program Files\Windows Media Player\mfstub_NT61_x64.dll` to `mf.dll`

Completed install:<br/>
<img width="614" height="258" alt="Post install example with wmp 11" src="https://github.com/user-attachments/assets/c5fdc58a-4098-47f4-b6d2-ad34f4ccea3b" />

Now you can launch the target program (e.g. WMP 11) and play some media!

### 4. Install system-wide (not recommended)
MfStub will affect all applications of the target bitness (32-bit or 64-bit) on your computer.

1. Open the system folder containing the **original mf.dll** (`System32` or `SysWOW64`).
2. Rename the system `mf.dll` to `m_.dll`.<br/>
   *You will probably be unable to do this, since TrustedInstaller owns the file (and for good reason). Only proceed if you know how to safely manipulate and service this file yourself.*
3. Copy the correct MfStub **proxy mf.dll** into the folder.
4. Rename the MfStub DLL to `mf.dll`.

**This install method is for power users only.** Windows updates and servicing events will break this setup and potentially cause significant headaches for you later on.

## Config
The first time you run your target application with MfStub installed, it will try to write a `mfstub.ini` config file next to itself, inside the application directory. This config file contains a few options for controlling the behavior of MfStub. Feel free to modify them as you see fit.

This config file is not critical and can be ignored. MfStub will use default configuration if the `mfstub.ini` file does not exist.

### Troubleshoot mfstub.ini not being created:
- Many application directories are write-protected, so only applications launched with elevated permissions can write to them. This includes most programs installed under `Program Files` and `Program Files (x86)`, such as Windows Media Player. You will have to launch the application with administrator rights in order for it to create the `mfstub.ini` file.
- Some applications, including Windows Media Player, do not load Media Foundation (`mf.dll` and co) until the user requests media playback. If you still do not see `mfstub.ini`, try playing a media file to make sure that `mf.dll` gets loaded.

<br/>

# Building
This repo consists of a single Visual Studio 2017 solution that builds all release artifacts.

The solution contains x86 and x64 targets and platform-specific code for several versions of Windows. Each OS-specific MfStub DLL is built using the contemporary VS toolset for that OS and is statically linked with the accompanying MSVC runtime.
- `NT60_x86` and `NT60_x64` are configured to build with the v90 (VS 2008) toolset.
- `NT61_x86` and `NT61_x64` are configured to build with the v100 (VS 2010) toolset.
- `NT63_x86` and `NT63_x64` are configured to build with the v120 (VS 2013) toolset.
- `NTBC_x86` and `NTBC_x64` are configured to build with the v141 (VS 2017) toolset.

If you have all the appropriate VS toolsets installed, you can download and easily build the entire solution with a single click.

<br/>

# Credits
- MfStub development uses [DllProxyCreator](http://jacquelin.potier.free.fr/DllProxyCreator/) by Jacquelin Potier.
- MfStub binaries uses [inih](https://github.com/benhoyt/inih) by Ben Hoyt.

<br/>

# Licenses
### MfStub
```
Copyright (c) 2026 TiberiumFusion

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### inih
```
The "inih" library is distributed under the New BSD license:

Copyright (c) 2009, Ben Hoyt
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Ben Hoyt nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY BEN HOYT ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL BEN HOYT BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```