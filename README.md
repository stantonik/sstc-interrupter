
# SSTC-Interrupter

> I provide a digital interrupter for driving SSTC/DRSSTC. Thanks to the microcontroller, it is easy to add and custom features.

The signal generated is supposed to be ANDed with your main Tesla drive signal.

## Features
- Manual control: generate PWM from 1Hz to 20KHz along with a minimal pulse width of 1us.
- Audio modulation: different modulations available (only PWM for now). The audio is sampled at 16kHz and the PWM carrier frequency is 30KHz by default, but you can adjust these value to fit your needs.
