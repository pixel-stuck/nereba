# nereba
A warmboot bootrom exploit for the Nintendo Switch

### Compiling
Compilation requires devkitA64, devkitARM and libnx to be installed. Once those are installed, simply run "make" to compile.

### Where does the name come from?
The name "nereba" comes from a conjugation of the Japanese verb neru, "to sleep", meaning roughly "if I sleep, then..." This name was suggested by a friend and is partially intended to poke fun at the Atmosphère CFW project using French names.

### How does it work?
The core of this exploit is not a Horizon OS vulnerability, but a vulnerability in the bootrom of the Tegra X1. During normal operation, when the Switch enters sleep mode, the main processor shuts down all but a few essential components of itself. The OS stores some state in RAM, which is kept on and its contents preserved. On wakeup, it re-executes the bootrom, and uses the state stored in RAM and in one of the regions kept on in the main processor. The bootrom takes a special codepath called the warmboot path, based on the presence of certain data (which can also be simulated on a reboot), and allows the console to wake up from sleep. One of the wakeup tasks is to get the RAM ready for usage, as it was put in a special mode before sleep. There are a set of parameters used to configure the RAM that are used during boot and wakeup. The bootrom assumes that these parameters do not change, as they are signed during a "coldboot" (power on reset), but Nvidia forgot to verify them during warmboot. This means they are able to be changed and thus the bootrom will use them to perform arbitrary writes. We can use these writes to take control of the bootrom using the built in ipatch system. Exploition on 1.0 is simple, as the region where the RAM parameters are stored is accessible easily with the nspwn exploit. This changed on later firmware versions; using this on firmware versions higher than 1.0 requires more complex exploits to achieve the same results.

