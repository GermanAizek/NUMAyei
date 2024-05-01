# NUMAyei scheduler (non-NUMA app -> NUMA app)

<p align="center"><img src="https://github.com/GermanAizek/NUMAyei/assets/21138600/f3eae1b4-1044-4266-9236-53f411a7d057" width="500"></p>

## Attention:

- While this is very crude solution, in no case use it for multiplayer games, principle hooking functions is used as in cheats.

- Use only `Release` builded DLL for injection! Performance Debug configuration will spoil NUMAyei scheduler.

## Where did idea come from?

I watched one video `How Bad is This $10,000 PC from 10 Years Ago??` from YouTube Channel ([@LinusTechTips](https://www.youtube.com/@LinusTechTips)) and it became funny to me that they tried to compare launch Crysis 2 on server with x4 GPU SLI and did not see difference in fps metrics. But they did not realize that emphasis was on one CPU because game created threads for one processor, and not for both, so FPS did not change whether new 1x GPU or old x4 GPU SLI. Also, most likely game is made only for maximum 2x SLI, but no more, again, I actually have plans to fix this, in theory, I can make hack to unlock SLI scale GPUs. I hope that [@nharris-lmg](https://github.com/nharris-lmg) will be able to notify LTT team about this and re-test Crysis 2 on old NUMA server motherboard using this hack. Not to forget to enable NUMA feature in bios!

Moment from video:

![2 CPU full utilize](https://github.com/GermanAizek/NUMAyei/assets/21138600/b1faa010-9a7f-415c-8cb8-9703170b0f24)


## TODO:
- Hook and rewrite VirtualAlloc to VirtualAllocExNuma for each NUMA node
- Hook any OpenProcess and CreateProcess for migrate to other NUMA nodes
- Hook any method detect cores and threads
- Hook open process as double-click or context menu right-click for non-PRO users ~~not to run cmd.exe or powershell~~ (in future `numayei.exe ./binary_non_numa.exe`)

## Summary:

This will not make sense if running program does not initially separate WinAPI threads using `CreateThread()` and similar functions.

Mechanism load balancer is simple, most programs do not work and do not call functions specifying NUMA node or group processors, so by default very first one is selected (only NUMA 0 or only NUMA 1).

If we inject NUMAyei scheduler in app we can redefine all functions to assign to each NUMA node in the system and using NUMA allocator.

I strictly remind you that programs that are already optimized for NUMA will not give any strong performance profit. Examples that I tested: XMRig miner, some benchmarks like Corona, Blender, etc.

It is desirable to have some kind text database on Wiki Github where there info compatibility and it will increase performance as percentage.

## Why you should use NUMAyei sheduler?

No one forces you to do this, but you will notice how power consumption CPUs will decrease, as well as their performance will increase, since for one NUMA node to work. With my thread scheduler, you have fully load system on fully worked 100% !

Windows scheduler has to allocate the highest frequency to calculate task faster, while second node is idle, performing background tasks unrelated to main working desired process used.

## Requirements:
- Minimum version Windows 11 (I will try to make version lower)
- Any DLL Injector
- Builded NUMADLL.dll (once I set up Github CI, you can download latest binaries from ![here](https://github.com/GermanAizek/NUMAyei/releases))

## Tested on:
- Windows 11 Pro 23H2 [22631.3296]
- 2 NUMA nodes (dual socket)
- Xeon E5 v3-v4 family CPU

## How to Use?

1. Download any DLL Injector, I advise you to take an opensource. (im tested on Xenos64 Injector)
2. Select compiled `NUMADLL.dll` and inject in any process (any methods)
3. In future `NUMA.exe` it will be an injector and through its parameter you can specify path to running exe or active running process by PID.

Good example below in screenshot using NUMAyei with running binary not NUMA-aware adapting. 

![NUMA run binary optimization](https://github.com/GermanAizek/NUMAyei/assets/21138600/a9da1ef1-4aff-4fd1-bb10-a359c224f32f)

![NUMA full utilize](https://github.com/GermanAizek/NUMAyei/assets/21138600/c19e3519-8250-4423-94ff-32665b292fd7)

![NUMA Windows full implementation verified bencmark CPU-Z](https://github.com/GermanAizek/NUMAyei/assets/21138600/dee749bc-73ac-4bbf-b183-ee8e0ad861e2)

#### Updated with new NUMA allocation (beginning with commit [6c6fb5](https://github.com/GermanAizek/NUMAyei/commit/6c6fb5cbfd4606ada20970354797771f6ac6a338))

In new CPU-Z version:

![2 old server cpu equal modern cpu](https://github.com/GermanAizek/NUMAyei/assets/21138600/3b94d5fd-d66d-4797-8164-b591ae3776b6)

![Xeon E5-2699 v3 (dual socket) vs modern cpus](https://github.com/GermanAizek/NUMAyei/assets/21138600/ecdae967-8b6f-4e32-9351-d6be16f6ee57)

