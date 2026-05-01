PWD := $(shell pwd)
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

DRV_NAME := vas_logger
MODULE_PATH := /lib/modules/$(shell uname -r)/extra

.PHONY: all build run remove install uninstall clean check reload help

all: build

build:
	@echo "Сборка модуля ядра..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

run: build
	@echo "=== Загрузка модуля ==="
	sudo insmod $(PWD)/$(DRV_NAME).ko
	@echo "Модуль загружен."
	@lsmod | grep $(DRV_NAME) || echo "Модуль не найден в списке загруженных"

remove:
	@echo "=== Выгрузка модуля ==="
	-sudo rmmod $(DRV_NAME) 2>/dev/null || true
	@echo "Модуль выгружен."

install: build
	@echo "Установка модуля в систему..."
	sudo mkdir -p $(MODULE_PATH)
	sudo cp $(DRV_NAME).ko $(MODULE_PATH)/
	sudo depmod -a
	@echo "Модуль установлен. Можно загрузить через modprobe."

uninstall: remove
	@echo "Удаление модуля из системы..."
	sudo rm -f $(MODULE_PATH)/$(DRV_NAME).ko
	sudo depmod -a
	@echo "Модуль удалён."

clean:
	@echo "Очистка файлов сборки..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod.o modules.order Module.symvers
	find . -name "*.o" -delete
	find . -name "*.cmd" -delete
	find . -name ".tmp_versions" -type d -exec rm -rf {} + 2>/dev/null || true

check:
	@echo "=== Проверка состояния модуля ==="
	@echo "1. Загружен ли модуль:"
	@lsmod | grep -q $(DRV_NAME) && echo "✓ Модуль загружен" || echo "✗ Модуль не загружен"
	@echo ""
	@echo "2. Установлен ли модуль в системе:"
	@test -f $(MODULE_PATH)/$(DRV_NAME).ko && echo "✓ Файл модуля присутствует" || echo "✗ Модуль не установлен"
	@echo ""
	@echo "3. Информация о модуле:"
	@modinfo $(DRV_NAME) 2>/dev/null || echo "Информация недоступна (модуль не установлен/собран)"

reload: remove build run
	@echo "Модуль перезагружен."

help:
	@echo "Доступные команды:"
	@echo "  make build     - сборка модуля"
	@echo "  make run       - сборка и загрузка модуля"
	@echo "  make remove    - выгрузка модуля"
	@echo "  make install   - установка модуля в систему"
	@echo "  make uninstall - удаление модуля из системы (с выгрузкой)"
	@echo "  make clean     - очистка файлов сборки"
	@echo "  make check     - проверка состояния модуля"
	@echo "  make reload    - перезагрузка модуля (remove + build + run)"
	@echo "  make help      - эта справка"