# NUMAyei scheduler

<p align="center"><img src="https://github.com/GermanAizek/NUMAyei/assets/21138600/f3eae1b4-1044-4266-9236-53f411a7d057" width="500"></p>

## Attention:

- While this is very crude solution, in no case use it for multiplayer games, principle hooking functions is used as in cheats.

- Use only `Release` builded DLL for injection! Performance Debug configuration will spoil NUMAyei scheduler.

## TODO:
- Hook and rewrite VirtualAlloc to VirtualAllocExNuma for each NUMA node

## Summary:

This will not make sense if running program does not initially separate WinAPI threads using `CreateThread()` and similar functions.

Mechanism load balancer is simple, most programs do not work and do not call functions specifying NUMA node or group processors, so by default very first one is selected (only NUMA 0 or only NUMA 1).

If we inject NUMAyei scheduler in app we can redefine all functions to assign to each NUMA node in the system and using NUMA allocator.

## Why should I use NUMAyei sheduler?

No one forces you to do this, but you will notice how power consumption CPUs will decrease, as well as their performance will increase, since for one NUMA node to work.

Windows scheduler has to allocate the highest frequency to calculate task faster, while second node is idle, performing background tasks unrelated to main working desired process used.

## Tested on:
- Windows 11 Pro 23H2 [22631.3296]
- another not tested




Good example below in screenshot using NUMAyei with running binary not NUMA-aware adapting. 

![](https://github.com/GermanAizek/NUMAyei/assets/21138600/a9da1ef1-4aff-4fd1-bb10-a359c224f32f)
