# TODO : prendre la valeur du port dans le platformio.ini
arduino_push:
	pio run -e uno -t upload --upload-port /dev/ttyACM0
