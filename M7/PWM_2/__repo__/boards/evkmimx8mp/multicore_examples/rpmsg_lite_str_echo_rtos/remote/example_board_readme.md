Hardware requirements
=====================
- Micro USB cable
- EVK-MIMX8M Plus board
- J-Link Debug Probe
- 12V power supply
- Personal Computer

Board settings
============
No special is needed.



Prepare the Demo
===============
1.  Connect 12V power supply and J-Link Debug Probe to the board, switch SW3 to power on the board
2.  Connect a USB cable between the host PC and the J23 USB port on the target board.
3.  Open two serial terminals for A53 core and M7 core with the following settings:
    - 115200 baud rate
    - 8 data bits
    - No parity
    - One stop bit
    - No flow control
4.  Using U-Boot command to run the demo.bin file. For details, please refer to Getting Started with MCUXpresso SDK for EVK-MIMX8MP.pdf
5.  After running the demo.bin, using the "boot" command to boot the kernel on the A core terminal;
6.  After the kernel is boot, using "root" to login.
7.  After login, make sure imx_rpmsg_tty kernel module is inserted (lsmod) or insert it (modprobe imx_rpmsg_tty).

Running the demo
===============
After the boot process succeeds, the ARM Cortex-M7 terminal displays the following information:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
RPMSG String Echo FreeRTOS RTOS API Demo...

Nameservice sent, ready for incoming messages...
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
After the Linux RPMsg tty module was installed, the ARM Cortex-M7 terminal displays the following
information:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Get Messgae From Master Side : "hello world!" [len : 12]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The user can then input an arbitrary string to the virtual RPMsg tty using the following echo command on
Cortex-A terminal:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
echo test > /dev/ttyRPMSG30 log below shows the output of the RPMsg-Lite str echo demo in the terminal window:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
On the M7 terminal, the received string content and its length is output, as shown in the log.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Get Message From Master Side : "test" [len : 4]
Get New Line From Master Side
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Customization options
=====================