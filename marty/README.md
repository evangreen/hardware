# Marty McFly

The Marty McFly was a set of boards I created in late 2015 for my girlfriend as a gift. It's an LED countdown clock, in the style of the console from "Back to the Future". The three displays track the programmable "destination time" on the top row, the current time on the bottom row, and the "estimated time to arrival" in the middle row. The destination time and current time can be programmed using the keypad. The ETA counts up if the current time is after the destination time.

### Motivation
Because it's fun. I was hoping to give this to my girlfriend in time for the Back to the Future anniversary, but failed to complete it in time. She still appreciated the gift, and she ended up making the cool backing and framing for it.

I also wanted a chance to play with the HT16K33 chip, which drives a matrix of LEDs with constant current, and can simultaneously scan in a matrix keypad. It interfaces with a microcontroller via I2C.

### Sourcing
I wanted attractive LEDs and a retro keypad for this project. It seems like most of the cheap 7-segment displays being sold now have rectangular bars for the top and bottom segments. I prefer the more symmetric face where all segments have the pointy ends that join with each other. This led me toward the older HDSP LEDs that HP used to make. I also needed a consistent look across three colors: red, yellow and green. It took some doing, but I eventually found sets of LEDs in each color with the front face I wanted. I also stumbled across somebody selling old touchtone phone keypads on eBay, which seemed perfect for this project. Once I had my LEDs, I could start designing the board.

### Hardware Design
I used Seeedstudio to make my boards. They are incredibly cheap, but it takes awhile for the boards to get back to you, and you must buy quantities of at least 5. Rather than making one very large board (which would have been very expensive), or two different types of small boards, I wanted to send away for a single PCB design that could serve multiple purposes, depending on which parts were populated.

Each row in the display is actually two boards side by side, with holes for LEDs in a 2-2-4 digit combination. For the date version on the left, all LEDs are populated, and for the time on the right, the last two digits of the year are left unpopulated.

Most of the boards are simply display boards. Pins on the left pipe I2C commands directly into the HT16K33, and pins on the right forward those commands out to the next board. One board also has an ATMega on it, this is the master board that controls all the other display boards. Of course, the design is the same, so all boards have the traces and pads for the ATMega, but only one has it populated. One board also has the keypad connected to it (though again, the keypad pins are there on every board). I used solder bridge "parts" in the PCB design, then blobbed solder bridges in unique binary combinations to give each board a different I2C ID.

### Firwmare Design
The firmware on the ATMega needs to keep track of the current date and time, store a destination time, calculate the delta between them, and display all three of these values by sending them out to the displays via I2C. It also needs to poll for changes on the keypad, and allow for programming of either the current time or the destination time.

Tracking time is always tricky business. Computing the difference between two calendar dates can also be very tricky, especially when considering changes like Daylight Saving transitions and computing a delta in terms of months and days.

Internally I store time as a tuple of Year, Day (of year), and Second (of day). Storing it this way means I can add and subtract time easily, and only worry about things like Daylight Saving when it's time to display something. Advancing the time is done by adding to the seconds, then calling a normalize function to make sure seconds, days, and year are all within a valid range. The number of seconds per day is always constant (since we adjust for Daylight Saving later), and the number of days in a year is either 365 or 366 depending on the year.

Converting a DATE into a full calendar date involves dealing with Daylight Saving, then splitting out the fields. We compute the Day-of-Year on which the Daylight Saving transitions occur for the given year, and see whether or not our day and time fall within that range. If they do, we add another hour to the seconds and re-normalize. Splitting Day-of-Year into Month and Day is a short loop of subtracting the number of days for each month while possible. Splitting the seconds into hours, minutes, and seconds is simple division.

Computing the difference between dates reuses a lot of the same logic. The delta itself is computed by subtracting each field from its counterpart (ie LeftSeconds - RightSeconds, LeftDays - RightDays, LeftYears - RightYears), and then calling the same normalize and display routines as before. The benefit of this is that the ETA counter will always tick down consistently, even across Daylight Saving transitions. The slight downside is it means the first "month" difference between two dates is always the number of days in January, even if two dates in question are something February 1st and March 1st. This seemed like a decent tradeoff, since it allowed a lot of code reuse and kept the countdown consistent, at the cost of intuitive but sometimes ambiguous month delta results.

### Results
Great success! The project came together smoothly, and my girlfriend did an amazing job with the fit and finish of the display. I was overall quite happy with the HT16K33 chips, they made controlling a large number of LEDs and interfacing with the keypad very simple. The green 7-segment HDSP displays I got on eBay are weaker than I'd like. They are a bit dim, and I can see a ripple from the asymmetric multiplexing of the HT16K33 (since some time slots go to the keypad). I blame the LEDs for this more than the HT16K33, since the symptoms appear isolated to the green LEDs. The yellow LEDs, which are actually newer and came from Lite-On, are my favorite, as they're clear and bright.

I would use the HT16K33 again in future projects. It might be a bit expensive in production, but for small hobby projects like mine it does a great job freeing up the MCU and cutting down on PCB traces. I wish I had used it in the Airlight project.
