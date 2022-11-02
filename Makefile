ifdef BRANCH_NAME
BNAME=${BRANCH_NAME}
else
BNAME=$(shell basename ${PWD})
endif

PROJ=ESP_DS18
BOARDS=d1_mini esp32dev heltec_wifi_lora_32_V2

DEST=pharmdata.ddns.net

all: mkdir push
# all: mkdir push upload monitor

mkdir:
	mkRemoteDirs ${DEST} $(BNAME) ${PROJ} ${BOARDS}

push:
	echo "PROJ: ${PROJ}"
	echo "LIN: ${LIN}"
	echo "PLATFORM: ${PLATFORM}"
	pio run
	scp .pio/build/d1_mini/firmware.bin ${DEST}:/var/www/html/firmware/$(BNAME)/d1_mini/${PROJ}/firmware.bin
	scp .pio/build/esp32dev/firmware.bin ${DEST}:/var/www/html/firmware/$(BNAME)/esp32dev/${PROJ}/firmware.bin
	scp .pio/build/heltec_wifi_lora_32_V2/firmware.bin ${DEST}:/var/www/html/firmware/$(BNAME)/heltec_wifi_lora_32_V2/${PROJ}/firmware.bin

upload:
	pio run --target upload

monitor:
	pio device monitor -p /dev/ttyUSB0

clean:
	pio run --target clean

