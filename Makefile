PWD := $(shell pwd)
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

DRV_NAME := vas_logger
MODULE_PATH := /lib/modules/$(shell uname -r)/extra

MOUNT_POINT := /vas_log
FS_TYPE := vas_log

.PHONY: all build run remove install uninstall clean check reload help

all: build

build:
	@echo "Сборка модуля ядра..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

run: build
	@echo "=== Загрузка модуля и монтирование ФС ==="
	@# Создаём точку монтирования, если её нет
	@if [ ! -d $(MOUNT_POINT) ]; then \
		echo "Создание точки монтирования $(MOUNT_POINT)..."; \
		sudo mkdir -p $(MOUNT_POINT); \
	fi
	@# Загружаем модуль
	sudo insmod $(PWD)/$(DRV_NAME).ko
	@# Даём ядру время обработать регистрацию модуля
	@sleep 0.5
	@# Монтируем ФС, если она ещё не смонтирована
	@if ! mount | grep -q "on $(MOUNT_POINT) type $(FS_TYPE)"; then \
		echo "Монтирование $(FS_TYPE) в $(MOUNT_POINT)..."; \
		sudo mount -t $(FS_TYPE) none $(MOUNT_POINT); \
	else \
		echo "ФС уже смонтирована в $(MOUNT_POINT)."; \
	fi
	@echo "Модуль загружен и ФС готова к использованию."
	@echo "Проверка:"
	@lsmod | grep $(DRV_NAME) || echo "Модуль не найден в списке загруженных"
	@mount | grep $(MOUNT_POINT) || echo "Точка монтирования не найдена"

remove:
	@echo "=== Размонтирование ФС и выгрузка модуля ==="
	@# Размонтируем ФС, если она смонтирована
	@if mount | grep -q "on $(MOUNT_POINT) type $(FS_TYPE)"; then \
		echo "Размонтирование $(MOUNT_POINT)..."; \
		sudo umount $(MOUNT_POINT); \
	else \
		echo "ФС не смонтирована, пропускаем размонтирование."; \
	fi
	@# Выгружаем модуль (игнорируем ошибку, если уже выгружен)
	-sudo rmmod $(DRV_NAME) 2>/dev/null || true
	@echo "Модуль выгружен."
	@# Проверка, что точка не осталась занятой
	@if mount | grep -q "on $(MOUNT_POINT) type $(FS_TYPE)"; then \
		echo "ВНИМАНИЕ: Точка $(MOUNT_POINT) всё ещё смонтирована!"; \
	fi

install: build
	@echo "Установка модуля в систему..."
	sudo mkdir -p $(MODULE_PATH)
	sudo cp $(DRV_NAME).ko $(MODULE_PATH)/
	sudo depmod -a
	@echo "Модуль установлен. Теперь можно загрузить его через modprobe или настроить автозагрузку."

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
	@echo "3. Точка монтирования $(MOUNT_POINT):"
	@if [ -d $(MOUNT_POINT) ]; then \
		if mount | grep -q "on $(MOUNT_POINT) type $(FS_TYPE)"; then \
			echo "✓ ФС смонтирована и доступна"; \
		else \
			echo "✗ Директория существует, но ФС не смонтирована"; \
		fi; \
	else \
		echo "✗ Директория $(MOUNT_POINT) отсутствует"; \
	fi
	@echo ""
	@echo "4. Информация о модуле:"
	@modinfo $(DRV_NAME) 2>/dev/null || echo "Информация недоступна (модуль не установлен/собран)"

reload: remove build run
	@echo "Модуль перезагружен и ФС перемонтирована."

help:
	@echo "Доступные команды:"
	@echo "  make build     - сборка модуля"
	@echo "  make run       - сборка, загрузка модуля и автоматическое монтирование в $(MOUNT_POINT)"
	@echo "  make remove    - размонтирование $(MOUNT_POINT) и выгрузка модуля"
	@echo "  make install   - установка модуля в систему (требует модуль в собранном виде)"
	@echo "  make uninstall - полное удаление модуля (с размонтированием и выгрузкой)"
	@echo "  make clean     - очистка файлов сборки"
	@echo "  make check     - проверка состояния модуля и точки монтирования"
	@echo "  make reload    - перезагрузка модуля (remove + build + run)"
	@echo "  make help      - эта справка"