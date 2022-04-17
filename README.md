# ModularBCI system
This repository contains code, data and tools which were used to implement and test the ModularBCI board into an EEG system.

## Microcontroller
The [microcontroller folder](/Final Version/microcontroller) contains the code writen for an STM32L475 IOT dev. board to control and set up the ModularBCI board.

## OpenViBE
An OpenViBE driver to monitor and record data over and USB-UART connection of the microcontroller.

## Matlab
Matlab skripts used to analys the recorded data.
There are also some debugging MATLAB tools for simply reading out the data send over UART-USB from the microcontroller.