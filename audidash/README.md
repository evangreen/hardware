= Audidash =

Audidash is a project I started in 2017 to connect the instrument cluster of a 1998 Audi A4 to my PC. The various indicators and gauges correspond to stats of my machine. For instance, the speed gauge correllates to my words per minute of typing, and the RPM gauge corresponds to my mouse distance. It's actually the second instrument cluster I've done this to, though the interface turned out to be completely different from the first instrument cluster I wired up.

I got the instrument cluster on eBay for something like twenty or thirty bucks. This particular make/model has significance to me because it's the car I drove in high school (and still do at the time of this writing).

== Interface ==

The Audi instrument cluster I got interfaces with the rest of the car via two big 32-pin connectors on the back. The signals are mostly digital and want either ground or +12VDC. The speed and RPM gauges measure the duration between +12V pulses, as you might expect. A couple of the gauges (fuel, temp, and oil) are analog. I was able to approximate these signals using PWM and a random capacitor to smooth things out.

== Board ==

The electronics I'm adding consist of an STM32F103 board (available on eBay for just a couple of bucks), and an ESP8266 Wifi board to connect to the local network. The STM32 MCU runs at 3.3V, but the instrument cluster needs +12V. I connected the I/O pins to a ULN2003A for those pins that wanted ground to activate, and other I/O pins to a TD62783 as a high side switch. I'm then using jumpers to connect the board to the instrument cluster.

Rather than send off to have boards made (since I only need 1), I decided to solder everything together manually on a protoboard. Many of the chips can line up side by side, so no jumpers are needed.

== App ==
On the PC side, I cannibalized my last PcDash project as the starting point. The app runs on Windows (only), and installs low level keyboard and mouse hooks to measure mousing rates and WPM. It also monitors network traffic rates, hard disk I/O rates, and CPU usage to fire off indicators if some part of the PC is experiencing heavy use.

The app itself is a command line app that runs in the background. It takes the IP address of the AudiDash device on the command line, and sends UDP packets blindly to it to change its state.

== Quirks ==
It turns out the Audi instrument cluster is not a dumb device at all, and requires a fair amount of manipulation to display arbitrary data.

The gauges act in a non-linear manner, which is probably consistent with faking an analog signal using PWM and a capacitor. The temperature gauge is manipulated, so there is a large range of actual values that correspond to the "straight in the middle" temperature reading.