compile:
	xxd -i static/HTML_HEADER_MOLD > static.h
	xxd -i static/HTML_FOOTER_MOLD >> static.h
	xxd -i static/GET_PLANT_BODY_MOLD >> static.h
	xxd -i static/MANUAL_PLANT_DIV_MOLD >> static.h
	xxd -i static/AUTOMATIC_PLANT_DIV_MOLD >> static.h
	sed -i "s/static_//g; s/unsigned/static const/g" static.h
	arduino-cli compile --fqbn esp32:esp32:nodemcu-32s .
	rm static.h

upload:
	arduino-cli upload --fqbn esp32:esp32:nodemcu-32s -p /dev/ttyUSB0 . 

monitor:
	picocom -b 115200 /dev/ttyUSB0
