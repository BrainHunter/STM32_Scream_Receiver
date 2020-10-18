# STM32 Scream Receiver
Use the STM32F4Discovery with BaseBoard (STM32F4DIS-BB) as a Scream Receiver

For more Informations about scream see:
https://github.com/duncanthrax/scream

## Status
This project should build easily with the STM32Cube IDE. 
The code is configured to use DHCP to get a IP Address, so it should be pretty much Plug&Play.

The project was tested with 2 Channels, 16bits/ch with 44100KHz & 48000KHz. Any other configuration probably does not work.

## How does it work
* There is one Thread of the RTOS which receives the scream multicast packages. These are fed into a Fifo buffer. 
The data from the fifo is transfered to the DAC using a DMA channel. - That's basicaly it. (Look for "StartAudioPlayback" and follow the data...)
* The USB is configured as CDC to emulate a UART for debug output
* 2 LEDs indicate a fifo overflow / underflow. If you see these blinking you know why the audio is bad ;-)





