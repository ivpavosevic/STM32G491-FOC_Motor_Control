%This script is supposed to read UART data made in packets from STM32G491
%board for purpose of logging and plotting data to be visible in real-time
s = serialport("COM3","115200");

% \n terminator
configureTerminator(s, "LF"); 

% read data from serial

% process data to plot