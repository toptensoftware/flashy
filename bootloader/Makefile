include raspi.mk


bootloader.zip:
	@$(MAKE) aarch CONFIG=release
	zip ../flashy/bootloader.zip -r -j . -i *.img
