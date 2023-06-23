Payloads for J-Link v10/v11
===========================

How to compile
--------------
1 Install MDK with LPC4300 DFP.

2 Create a project, select LPC4322:CortexM4 as device.

3 Enable CMSIS Core in package manager.

4 add one payload source in to project.

5 add after build step to save bin file: $K\ARM\ARMCC\bin\fromelf.exe --bin --output=Objects\@L.bin !L

6 (optional) change IROM to 0x20000050:0x800 and IRAM to 0x10000000:0x8000

How to write payloads
---------------------
your payload binary will copied to sram 0x20000050 and execute on Cortex-M4.

by change m4rxret template in EDUReviver, you can place your code in to 0x10000048, too.

you can use one of source file in this directory as template.

the entry of your code is __main and will place on begining of genareted binary, and execute at first. return from __main will continue execute J-Link's firmware.
you can do reset in payloads too, if you think your payloads destoryed firmware's execution.

you can call InitUSART3 and redirect stdout to it, to debug your payload by printf.