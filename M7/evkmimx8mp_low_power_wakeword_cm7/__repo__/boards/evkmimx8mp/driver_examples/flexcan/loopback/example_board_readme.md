Hardware requirements
=====================
- Micro USB cable
- EVK-MIMX8M Plus board
- J-Link Debug Probe
- 12V power supply
- Personal Computer

Board settings
============
No special settings are required.

Prepare the Demo
===============
1.  Connect 12V power supply and J-Link Debug Probe to the board, switch SW3 to power on the board
2.  Connect a USB cable between the host PC and the J23 USB port on the target board.
3.  Open a serial terminal with the following settings:
    - 115200 baud rate
    - 8 data bits
    - No parity
    - One stop bit
    - No flow control
4.  Download the program to the target board.
5.  Launch the debugger in your IDE to begin running the demo.

Running the demo
================
When the example runs successfully, following information can be seen on the terminal:

~~~~~~~~~~~~~~~~~~~~~~

==FlexCAN loopback functional example -- Start.==

Send message from MB8 to MB9
tx word0 = 0x11223344
tx word1 = 0x55667788

Received message from MB9
rx word0 = 0x11223344
rx word1 = 0x55667788

==FlexCAN loopback functional example -- Finish.==
~~~~~~~~~~~~~~~~~~~~~~
