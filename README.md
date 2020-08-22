# XLLN - xlivelessness
Games for Windows LiveLessNess. A complete Games For Windows - LIVE<sup>TM</sup> (GFWL) rewrite.

## Purpose
With GFWL being slowly phased out and many online features disappearing across the range of titles using that library, the idea is to rewrite or stub all aspects of it. Doing so also improves one's ability to reverse engineer (and improve) the titles themselves due to the anti-debugging measures present within the original module. Title specific modifications however do not belong here and instead should be implemented as an [XLLN-Module](https://gitlab.com/GlitchyScripts/xlln-modules). Additionally this library should be as modular and minimalistic as possible with entire features capable to be disabled (for example network functionality) or implemented as separate XLLN-Module (online features and voice chat).

## XLLN-Modules
The purpose of an XLiveLessNess-Module is to enhance some aspect of one or many GFWL Games/Titles or XLiveLessNess itself. On process attach, XLiveLessNess (or an [XLLN-Wrapper](https://gitlab.com/GlitchyScripts/xlln-wrappers) library) will codecave/hook the entry point (where AddressOfEntryPoint points to in the PE header) of the Title and invoke all XLLN-Modules from there. The benefits of this are:
1. XLiveLessNess (xlive.dll) and all other dynamically linked libraries (DLLs) have definitely been internally initialised at that point so we can call additional XLLN logging / setup functions from them etc. (via GetModuleHandle, GetProcAddress and the ordinal/export identifier).
2. If xlive.dll is not XLLN then other [XLLN-Wrapper](https://gitlab.com/GlitchyScripts/xlln-wrappers) libraries can load the XLLN-Modules via the same AddressOfEntryPoint codecave idea.
All XLLN-Modules installed in the "./xlln/modules/" folder from the working directory of the Title will be invoked in an arbitrary order. Use the XLLNModulePostInit@41101 and XLLNModulePreUninit@41102 exports in the case that access to other XLLN-Modules is required on startup and shutdown.

## Contributions
You may make contributions via pull requests. However it is best to check or add to the [issues board](https://gitlab.com/GlitchyScripts/xlivelessness/-/issues) before starting any work to ensure that the work isn't already being covered by someone else or is being planned out differently.
