compile:
	python xxd.py > static.h
	arduino-cli compile --fqbn esp32:esp32:nodemcu-32s .
	rm static.h

upload:
	arduino-cli upload --fqbn esp32:esp32:nodemcu-32s -p /dev/ttyUSB0 . 

monitor:
	picocom -b 115200 /dev/ttyUSB0
