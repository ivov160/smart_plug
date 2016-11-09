# Readme
# Настройка окружения

Для сборки окружения нужны следующие пакеты:
 - gcc
 - make
 - autoconf
 - automake
 - binutils
 - git
 - grep
 - sed
 - wget
 - help2man
 - bison
 - flex
 - gperf
 - libtool
 - texinfo
 - patch
 - возможно еще что-то, во время сборки будет говорить чего нет 

# Сборка toolchain

Для сборки toolchain (компилятор, отладчик, линковщик и т.д.) пока используется https://github.com/pfalcon/esp-open-sdk. 
@todo Думаю стоит заменить на https://github.com/jcmvbkbc/crosstool-NG так как кроме компилятора ничего больше от туда не беретьс

<code>
	mkdir ~/esp && cd ~/esp
	git clone https://github.com/pfalcon/esp-open-sdk.git && cd esp-open-sdk
	git submodule init && git submodule update
	make
</code>

Дальше если все пакеты для сборки стоят все будет ок. Если чего-то нет будет сообщение придеться доставлять

Надо создать symlink на toolchain:

<code>
	ln -s ~/esp/esp-open-sdk/xtensa-lx106-elf/xtensa-lx106-elf ~/esp/
	cp -rvf ./xtensa-lx106-elf/bin/* ~/esp/xtensa-lx106-elf/bin/
</code>

Путь надо прописать в bashrc или еще где. Либо перед каждой сборкой писать:

<code>
	export PATH=~/esp/xtensa-lx106-elf/bin:$PATH
</code>

# Сборка SDK

SDK стандарнтое на базе https://github.com/espressif/ESP8266_RTOS_SDK

<code>
	cd ~/esp
	git clone https://github.com/espressif/ESP8266_RTOS_SDK.git && cd ESP8266_RTOS_SDK 
	git checkout v1.4.0
</code>


# Сборка проекта

<code>
	cd ~/esp && git clone ssh://git\@bitbucket.org/look-in/plug.git 
	cd plug
	git submodule init && git submodule update
</code>

Для сборки проекта скриптам сборки нужны 2 директории SDK_PATH и BIN_PATH для этого придеться либо прописать в bashrc аля PATH
Либо перед вызовом сборки указывать эти переменные:

<code>
	SDK_PATH=~/esp/ESP8266_RTOS_SDK BIN_PATH=./bin ./gen_misc.sh
</code>

# Сборка документации

Для сборки документации должен быть установлен doxygen. Сама сборка происходит вызовом комманды doxygen в корне проекта. 
После чего в корне проекта должна появиться папка doc/html в которой нужно открыть index.html

# Прошивка

<code>
	esptool.py --port /dev/ttyUSB0 --baud 115200 write_flash 0x00000 ./bin/eagle.flash.bin  0x20000 ./bin/eagle.irom0text.bin 0x6C000 ./devices/plug.device_info
</code>

# Отладка

Отладка работает по средством: https://github.com/espressif/esp-gdbstub.git.
Сама заглушка добавляется в прошивку при сборе DEBUG версии в ином случае данный отладчик в сборку не добавляется 
и отладка будет не доступна.

Ограниченичя отладчика:
	- не умеет software breakpoint (gdb: br). Доступна лишь одна hardware breakpoint (gdb: hbr)
	- gdb: next отрабатывает как step из-за чего постоянно входит во все функции. Решением является gdb: delete и установка точки (gdb: hbr) после функции

<code>
	xtensa-lx106-elf-gdb -x ./gdbcmd
</code>
