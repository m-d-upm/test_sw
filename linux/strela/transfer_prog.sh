#!/bin/bash
stty -F /dev/ttyUSB0 38400
sx prog.elf < /dev/ttyUSB0 > /dev/ttyUSB0
