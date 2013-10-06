IRproxy
=======

Proxies IR signal from receiver to sending IR LED

The sketch reads in IR code from IR remote keypress
- It will convert IR code(s) from (several) remote(s) to a suitable code for a target IR receiver
- It will send out the proxy code

In this version the target is a Pioneer VSX527. The following codes are relevant:
- common = "IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S.";    // 35 pulses (this is a common code for all of them: full code must be : common - function - pause - common - function)
- volUP = "S.L.S.L.S.S.S.S.L.S.L.S.L.L.L.L.";       // 32 pulses  (including separating pauses) 
- volDOWN = "L.L.S.L.S.S.S.S.S.S.L.S.L.L.L.L.";       // 32 pulses
- mute = "S.L.S.S.L.S.S.S.L.S.L.L.S.L.L.L.";       // 32 pulses
- OnOff = "S.S.L.L.L.S.S.S.L.L.S.S.S.L.L.L.";
- DVD = "L.S.L.S.S.S.S.L.S.L.S.L.L.L.L.S.";




