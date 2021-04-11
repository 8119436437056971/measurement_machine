# measurement_machine

see https://www.raspberrypi.org/forums/viewtopic.php?f=32&t=308469;

## Pulse Generation
- data     gpio out 20 pin 26 
- sync     gpio out 19 pin 25 
- pulse    gpio out 18 pin 24  debug output for PIO commands 
- trigger  gpio out 15 pin 20  pulse prox 270us at start of sequence 

## Measurement
- data     gpio in  21 pin 27 
- sync     gpio in  22 pin 29 

## Other
- LED      blinking @2.5Hz 400ms 

# UART commands

- h: help 
- r: report  Report data generation 
- s: slow    Slow generation, 800 ms, fast generation 8 ms
- c: compare Compare generated pattern with measured pattern
- t: trace   Write a trace of signals (us resolution)

# Usage

## Measurement only

Connect input signals (3.3V level) to GPIO 21, 22

## Data generation
Data generation is running always on core 1.

- data and sync are on GPIO 20, 19. 
- trigger pulse is generated few us before first digit. Output on GPIO 15. Helps to monitor signals on oscilloscope or logic analyzer

## Test
Connect data pins and sync pins.

The controller will record the generated data. With 'r' on uart, switch on/off reporting for generated data to compare results.

With 'c', enable/disable comparison of data.



