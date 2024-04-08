Attention:
While this is very crude solution, in no case use it for multiplayer games, principle hooking functions is used as in cheats.

Use only Release build DLL for injection! Performance Debug configuration will spoil NUMAyei scheduler.

Todo:
- Hook and rewrite VirtualAlloc to VirtualAllocExNuma for each NUMA node

![hqdefault_out2](https://github.com/GermanAizek/NUMAyei/assets/21138600/f3eae1b4-1044-4266-9236-53f411a7d057)

Benchmark:

This will not make sense if running program does not initially separate WinAPI threads using `CreateThread()` and similar functions. Mechanism load balancer is simple, most programs do not work and do not call functions specifying NUMA node or group processors, so by default very first one is selected (only NUMA 0 or only NUMA 1), if we inject NUMAyei we can redefine all functions to assign to each NUMA node in the system and using allocator used specifically for NUMA systems.

Tested on:
- Windows 11 Pro 23H2 [22631.3296]

Good example using NUMAyei with running binary not NUMA-aware adapting.

![diff_](https://github.com/GermanAizek/NUMAyei/assets/21138600/a9da1ef1-4aff-4fd1-bb10-a359c224f32f)
