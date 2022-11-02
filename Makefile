PROJ=ESP_DS18
LIN=$(shell grep '^platform =' platformio.ini)
PLATFORM=$(shell echo ${LIN} | cut -d "=" -f2- | tr -d '[:space:]' )
BOARD=d1_mini
HELTEC=heltec_wifi_lora_32_V2

DEST=pharmdata.ddns.net

all: push
# all: mkdir push upload monitor

mkdir:
	ssh ${DEST} "mkdir -p /var/www/html/firmware/d1_mini/${PROJ}"
	ssh ${DEST} "mkdir -p /var/www/html/firmware/esp32dev/${PROJ}"
	ssh ${DEST} "mkdir -p /var/www/html/firmware/${HELTEC}/${PROJ}"

push:
	echo "PROJ: ${PROJ}"
	echo "LIN: ${LIN}"
	echo "PLATFORM: ${PLATFORM}"
	pio run
	scp .pio/build/d1_mini/firmware.bin ${DEST}:/var/www/html/firmware/d1_mini/${PROJ}/firmware.bin
	scp .pio/build/esp32dev/firmware.bin ${DEST}:/var/www/html/firmware/esp32dev/${PROJ}/firmware.bin
	scp .pio/build/${HELTEC}/firmware.bin ${DEST}:/var/www/html/firmware/${HELTEC}/${PROJ}/firmware.bin

upload:
	pio run --target upload

monitor:
	pio device monitor -p /dev/ttyUSB0

clean:
	pio run --target clean

