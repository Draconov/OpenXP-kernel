SHELL := /bin/bash
CC := clang
LD := ld.lld
HOST_CC ?= cc
BUILD ?= build
BOOTLOADER_DIR ?= build/limine
LIMINE_BRANCH ?= v10.x-binary
EXTRA_CFLAGS ?=
# Ring 3 currently does not preserve XMM/MMX/x87 state during scheduling.
# Treat all userspace C objects alike (including the rasterizer and runtime).
RING3_CPU_FLAGS := -mno-red-zone -mgeneral-regs-only -mno-sse -mno-sse2 -mno-mmx -mno-80387
CFLAGS := -target x86_64-unknown-none-elf -std=c11 -O2 -g -ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check -fno-pic -fno-pie -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mcmodel=kernel -Wall -Wextra -Werror -Ikernel -MMD -MP $(EXTRA_CFLAGS)
LDFLAGS := -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 -T kernel/linker.ld
C_SOURCES := kernel/main.c kernel/serial.c kernel/arch/x86_64/interrupts.c kernel/arch/x86_64/gdt.c kernel/arch/x86_64/timer.c kernel/arch/x86_64/irq_state.c kernel/arch/x86_64/rtc.c kernel/diag/crash.c kernel/mm/pmm.c kernel/mm/vmm.c kernel/mm/address_space.c kernel/mm/heap.c kernel/sched/scheduler.c kernel/sync/wait_queue.c kernel/sync/event.c kernel/sync/mutex.c kernel/sync/ipc.c kernel/fs/initrd.c kernel/fs/ramfs.c kernel/fs/vfs.c kernel/fs/persist.c kernel/drivers/ata_ide.c kernel/drivers/pci.c kernel/shell/shell.c kernel/video/fb.c kernel/input/input.c kernel/gui/desktop.c kernel/gui/native.c kernel/gui/shell_bridge.c kernel/user/userspace.c kernel/user/process.c kernel/user/capipc.c kernel/user/app_launch.c kernel/user/elf.c kernel/user/syscall.c kernel/lib/string.c
ASM_SOURCES := kernel/arch/x86_64/isr.S kernel/arch/x86_64/irq.S kernel/sched/switch.S kernel/user/entry.S kernel/user/demo.S kernel/fs/initrd_data.S
OBJECTS := $(patsubst kernel/%.c,$(BUILD)/%.o,$(C_SOURCES)) $(patsubst kernel/%.S,$(BUILD)/%.o,$(ASM_SOURCES))
-include $(OBJECTS:.o=.d)
ISO := $(BUILD)/openxp.iso

.PHONY: smoke-native-compose smoke-raster smoke-compositor test-native-games test-native-games-sanitize smoke-shell test-shell-bridge test-shell-bridge-sanitize test-window-protocol test-window-protocol-sanitize test-capipc test-capipc-sanitize smoke-window-ipc kernel-storage iso-storage run-storage smoke-native smoke-native-wait smoke-deep test-desktop test-desktop-sanitize all kernel iso run smoke smoke-initrd smoke-exception smoke-guard smoke-timer smoke-sync smoke-user smoke-process smoke-user-preempt smoke-user-fault smoke-user-protection test-unit test-sanitize clean
all: iso
kernel: $(BUILD)/openxp.elf
$(BUILD)/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: kernel/%.S
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -DOPENXP_INITRD_PATH=\"$(BUILD)/initrd.img\" -c $< -o $@
$(BUILD)/apps/%.o: apps/%.S
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -c $< -o $@
$(BUILD)/apps/%.bin: $(BUILD)/apps/%.o
	$(LD) -m elf_x86_64 --oformat binary -Ttext 0x100000 $< -o $@
NATIVE_APPS := calculator clock notepad probe waitprobe canvas windowd winclient calendar snake mines monitor about paint wordpad terminal filemanager run settings
NATIVE_BINS := $(addprefix $(BUILD)/apps/,$(addsuffix .bin,$(NATIVE_APPS)))
# Keep intermediate native objects so the whole Ring 3 ISA audit can be
# rerun without recompiling or silently skipping application entry points.
.SECONDARY: $(addprefix $(BUILD)/apps/,$(addsuffix .native.o,$(NATIVE_APPS)))
$(BUILD)/apps/%.native.o: apps/native/%.c apps/native/oxp.h apps/native/game_logic.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/apps/native_runtime.o: apps/native/runtime.c Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/apps/native_start.o: apps/native/start.S
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -c $< -o $@
$(BUILD)/apps/%.bin: $(BUILD)/apps/%.native.o $(BUILD)/apps/native_start.o $(BUILD)/apps/native_runtime.o apps/native/app.ld
	$(LD) -m elf_x86_64 --oformat binary -T apps/native/app.ld $(BUILD)/apps/native_start.o $< $(BUILD)/apps/native_runtime.o -o $@
$(BUILD)/apps/windowd.native.o $(BUILD)/apps/winclient.native.o: apps/native/wm_protocol.h
$(BUILD)/apps/native_scene.o: apps/native/native_scene.c apps/native/native_scene.h apps/native/shell_raster.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/apps/wallpaper.o: apps/native/wallpaper.c apps/native/wallpaper.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/apps/windowd.native.o: apps/native/native_scene.h
$(BUILD)/apps/windowd.native.o: kernel/user/wm_display.h
$(BUILD)/apps/shell_raster.o: apps/native/shell_raster.c apps/native/shell_raster.h kernel/user/shell_abi.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -Ikernel -c $< -o $@
$(BUILD)/apps/wm_protocol.o: apps/native/wm_protocol.c apps/native/wm_protocol.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/apps/windowd.bin: $(BUILD)/apps/windowd.native.o $(BUILD)/apps/native_start.o $(BUILD)/apps/native_runtime.o $(BUILD)/apps/wm_protocol.o $(BUILD)/apps/shell_raster.o $(BUILD)/apps/native_scene.o $(BUILD)/apps/wallpaper.o apps/native/app.ld
	$(LD) -m elf_x86_64 --oformat binary -T apps/native/app.ld $(BUILD)/apps/native_start.o $(BUILD)/apps/windowd.native.o $(BUILD)/apps/native_runtime.o $(BUILD)/apps/wm_protocol.o $(BUILD)/apps/shell_raster.o $(BUILD)/apps/native_scene.o $(BUILD)/apps/wallpaper.o -Map $(BUILD)/apps/windowd.map -o $@
	python3 tools/check_windowd_bss.py $(BUILD)/apps/windowd.map
# Inspect the *actual compiled object code*; changes to toolchain defaults
# must not silently reintroduce unsupported SIMD/FPU instructions.
# The stamp is an initrd prerequisite, so every image build is protected.
$(BUILD)/apps/.ring3-isa-ok: $(NATIVE_BINS) $(BUILD)/apps/hello.o $(BUILD)/apps/fault.o tools/check_ring3_isa.py
	python3 tools/check_ring3_isa.py $(BUILD)/apps
	@touch $@
$(BUILD)/initrd.img: $(BUILD)/apps/hello.bin $(BUILD)/apps/fault.bin $(NATIVE_BINS) $(BUILD)/apps/.ring3-isa-ok tools/mkinitrd.py $(wildcard assets/wallpapers/*.bmp)
	python3 tools/mkinitrd.py $@ $(BUILD)/apps/hello.bin $(BUILD)/apps/fault.bin $(NATIVE_BINS)
$(BUILD)/fs/initrd_data.o: $(BUILD)/initrd.img
$(BUILD)/openxp.elf: $(OBJECTS) kernel/linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@
$(BOOTLOADER_DIR)/limine-bios.sys:
	@mkdir -p $(dir $(BOOTLOADER_DIR))
	git clone --depth=1 --branch $(LIMINE_BRANCH) https://github.com/limine-bootloader/limine.git $(BOOTLOADER_DIR)
$(ISO): $(BUILD)/openxp.elf $(BOOTLOADER_DIR)/limine-bios.sys limine.conf
	@command -v xorriso >/dev/null || { echo 'Install xorriso'; exit 1; }
	@mkdir -p $(BUILD)/iso/boot/limine $(BUILD)/iso/EFI/BOOT
	cp $(BUILD)/openxp.elf $(BUILD)/iso/boot/openxp.elf
	cp limine.conf $(BUILD)/iso/boot/limine/limine.conf
	cp $(BOOTLOADER_DIR)/limine-bios.sys $(BOOTLOADER_DIR)/limine-bios-cd.bin $(BOOTLOADER_DIR)/limine-uefi-cd.bin $(BUILD)/iso/boot/limine/
	cp $(BOOTLOADER_DIR)/BOOTX64.EFI $(BUILD)/iso/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label $(BUILD)/iso -o $(ISO)
	$(MAKE) -C $(BOOTLOADER_DIR)
	$(BOOTLOADER_DIR)/limine bios-install $(ISO)
iso: $(ISO)
run: iso
	qemu-system-x86_64 -m 512M -cdrom $(ISO) -serial stdio -no-reboot
# Separate opt-in storage build. NEVER attach important host or VM disks.
kernel-storage:
	$(MAKE) BUILD=build/storage EXTRA_CFLAGS=-DOPENXP_ENABLE_EXPERIMENTAL_IDE_STORAGE kernel
iso-storage:
	$(MAKE) BUILD=build/storage EXTRA_CFLAGS=-DOPENXP_ENABLE_EXPERIMENTAL_IDE_STORAGE iso
run-storage: iso-storage
	@test -f build/openxp-data.img || python3 tools/mkstorage.py build/openxp-data.img
	qemu-system-x86_64 -m 512M -cdrom build/storage/openxp.iso -serial stdio -no-reboot -drive if=ide,index=2,format=raw,file=build/openxp-data.img
smoke: iso
	bash scripts/smoke.sh $(ISO) OPENXP_BOOT_OK
smoke-native:
	$(MAKE) BUILD=build/native EXTRA_CFLAGS=-DOPENXP_NATIVE_SELFTEST iso
	bash scripts/smoke.sh build/native/openxp.iso OPENXP_NATIVE_ABI_OK
smoke-window-ipc:
	$(MAKE) BUILD=build/window-ipc EXTRA_CFLAGS=-DOPENXP_CAPIPC_SELFTEST iso
	bash scripts/smoke.sh build/window-ipc/openxp.iso OPENXP_CAPIPC_OK
smoke-native-wait:
	$(MAKE) BUILD=build/native-wait EXTRA_CFLAGS=-DOPENXP_NATIVE_WAIT_SELFTEST iso
	bash scripts/smoke.sh build/native-wait/openxp.iso OPENXP_NATIVE_WAIT_OK
smoke-shell: iso
	bash scripts/smoke.sh $(ISO) OPENXP_RING3_SHELL_READY
smoke-compositor: iso
	bash scripts/smoke.sh $(ISO) OPENXP_RING3_COMPOSE_READY
smoke-raster: iso
	bash scripts/smoke.sh $(ISO) OPENXP_RING3_RASTER_READY
smoke-native-compose:
	$(MAKE) BUILD=build/native-composition EXTRA_CFLAGS=-DOPENXP_NATIVE_COMPOSE_SELFTEST iso
	bash scripts/smoke.sh build/native-composition/openxp.iso OPENXP_RING3_NATIVE_PIXELS_READY
smoke-deep:
	$(MAKE) BUILD=build/deep EXTRA_CFLAGS=-DOPENXP_DEEP_SELFTEST iso
	bash scripts/smoke.sh build/deep/openxp.iso OPENXP_DEEP_OK
smoke-exception:
	$(MAKE) BUILD=build/exception EXTRA_CFLAGS="-DOPENXP_DEEP_SELFTEST -DOPENXP_EXCEPTION_SELFTEST" iso
	bash scripts/smoke.sh build/exception/openxp.iso OPENXP_EXCEPTION_OK
smoke-timer:
	$(MAKE) BUILD=build/timer EXTRA_CFLAGS="-DOPENXP_DEEP_SELFTEST -DOPENXP_TIMER_SELFTEST" iso
	bash scripts/smoke.sh build/timer/openxp.iso OPENXP_TIMER_OK
smoke-sync:
	$(MAKE) BUILD=build/sync EXTRA_CFLAGS="-DOPENXP_DEEP_SELFTEST -DOPENXP_SYNC_SELFTEST" iso
	bash scripts/smoke.sh build/sync/openxp.iso OPENXP_SYNC_TEST_OK
smoke-user:
	$(MAKE) BUILD=build/user EXTRA_CFLAGS=-DOPENXP_DEEP_SELFTEST iso
	bash scripts/smoke.sh build/user/openxp.iso OPENXP_USERSPACE_OK
smoke-process:
	$(MAKE) BUILD=build/process EXTRA_CFLAGS=-DOPENXP_DEEP_SELFTEST iso
	bash scripts/smoke.sh build/process/openxp.iso OPENXP_PROCESS_REAP_OK
smoke-user-preempt:
	$(MAKE) BUILD=build/preempt EXTRA_CFLAGS=-DOPENXP_DEEP_SELFTEST iso
	bash scripts/smoke.sh build/preempt/openxp.iso OPENXP_USER_PREEMPT_OK
smoke-initrd:
	$(MAKE) BUILD=build/initrd EXTRA_CFLAGS=-DOPENXP_DEEP_SELFTEST iso
	bash scripts/smoke.sh build/initrd/openxp.iso OPENXP_INITRD_OK
smoke-user-protection:
	$(MAKE) BUILD=build/user-protection EXTRA_CFLAGS="-DOPENXP_DEEP_SELFTEST -DOPENXP_USER_PROTECTION_SELFTEST" iso
	bash scripts/smoke.sh build/user-protection/openxp.iso OPENXP_USER_PROTECTION_OK
smoke-user-fault:
	$(MAKE) BUILD=build/user-fault EXTRA_CFLAGS="-DOPENXP_DEEP_SELFTEST -DOPENXP_USER_FAULT_SELFTEST" iso
	bash scripts/smoke.sh build/user-fault/openxp.iso OPENXP_USER_FAULT_OK
smoke-guard:
	$(MAKE) BUILD=build/guard EXTRA_CFLAGS="-DOPENXP_DEEP_SELFTEST -DOPENXP_GUARD_SELFTEST" iso
	bash scripts/smoke.sh build/guard/openxp.iso OPENXP_GUARD_OK

test-native-games:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror tests/native_games_test.c -o build/tests/native_games_test
	build/tests/native_games_test

test-native-games-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/native_games_test.c -o build/tests/native_games_sanitized
	build/tests/native_games_sanitized

test-shell-bridge:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_SHELL_HOST_TEST -Ikernel tests/shell_bridge_test.c kernel/gui/shell_bridge.c kernel/video/fb.c -o build/tests/shell_bridge_test
	build/tests/shell_bridge_test

test-shell-bridge-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_SHELL_HOST_TEST -Ikernel tests/shell_bridge_test.c kernel/gui/shell_bridge.c kernel/video/fb.c -o build/tests/shell_bridge_sanitized
	build/tests/shell_bridge_sanitized

test-capipc:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_CAPIPC_HOST_TEST -Ikernel tests/capipc_test.c kernel/user/capipc.c -o build/tests/capipc_test
	build/tests/capipc_test

test-capipc-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_CAPIPC_HOST_TEST -Ikernel tests/capipc_test.c kernel/user/capipc.c -o build/tests/capipc_sanitized
	build/tests/capipc_sanitized

test-window-protocol:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror tests/wm_protocol_test.c apps/native/wm_protocol.c -o build/tests/wm_protocol_test
	build/tests/wm_protocol_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_CAPIPC_HOST_TEST -Ikernel tests/wm_capipc_test.c apps/native/wm_protocol.c kernel/user/capipc.c -o build/tests/wm_capipc_test
	build/tests/wm_capipc_test

test-window-protocol-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/wm_protocol_test.c apps/native/wm_protocol.c -o build/tests/wm_protocol_sanitized
	build/tests/wm_protocol_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_CAPIPC_HOST_TEST -Ikernel tests/wm_capipc_test.c apps/native/wm_protocol.c kernel/user/capipc.c -o build/tests/wm_capipc_sanitized
	build/tests/wm_capipc_sanitized

test-unit: test-native-text-layout test-native-layout test-wallpaper test-native-scene test-shell-raster test-compositor-integration test-native-games test-shell-bridge test-window-protocol test-desktop test-capipc $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -DOPENXP_NATIVE_WAIT_HOST_TEST -Ikernel tests/native_wait_test.c kernel/gui/native.c kernel/video/fb.c -o build/tests/native_wait_test
	build/tests/native_wait_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel tests/vfs_test.c kernel/fs/vfs.c kernel/fs/initrd.c kernel/fs/ramfs.c -o build/tests/vfs_test
	build/tests/vfs_test $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_IRQ_HOST_TEST -Ikernel tests/app_launch_test.c kernel/user/app_launch.c kernel/arch/x86_64/irq_state.c -o build/tests/app_launch_test
	build/tests/app_launch_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_PCI_HOST_TEST -Ikernel tests/pci_test.c kernel/drivers/pci.c -o build/tests/pci_test
	build/tests/pci_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_PERSIST_HOST_TEST -Ikernel tests/persist_test.c kernel/fs/persist.c kernel/fs/ramfs.c -o build/tests/persist_test
	build/tests/persist_test
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -Ikernel tests/native_window_test.c kernel/gui/native.c kernel/video/fb.c -o build/tests/native_window_test
	build/tests/native_window_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel tests/ramfs_test.c kernel/fs/ramfs.c -o build/tests/ramfs_test
	build/tests/ramfs_test
	@mkdir -p build/tests
	python3 -m unittest discover -s tests -p 'test_*.py'
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel tests/initrd_test.c kernel/fs/initrd.c -o build/tests/initrd_test
	build/tests/initrd_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel tests/initrd_image_test.c kernel/fs/initrd.c -o build/tests/initrd_image_test
	build/tests/initrd_image_test $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_PROCESS_HOST_TEST -DOPENXP_CAPIPC_HOST_TEST -Ikernel tests/process_manager_test.c kernel/user/process.c kernel/user/capipc.c -o build/tests/process_manager_test
	build/tests/process_manager_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -DOPENXP_CAPIPC_HOST_TEST -DOPENXP_SHELL_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel tests/syscall_test.c kernel/user/syscall.c kernel/user/app_launch.c kernel/arch/x86_64/irq_state.c kernel/user/capipc.c kernel/gui/native.c kernel/gui/shell_bridge.c kernel/video/fb.c kernel/fs/ramfs.c kernel/fs/vfs.c kernel/fs/initrd.c -o build/tests/syscall_test
	build/tests/syscall_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_VMM_HOST_TEST -Ikernel tests/process_test.c kernel/mm/vmm.c kernel/mm/address_space.c kernel/user/elf.c -o build/tests/process_test
	build/tests/process_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel tests/pmm_test.c kernel/mm/pmm.c -o build/tests/pmm_test
	build/tests/pmm_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_VMM_HOST_TEST -Ikernel tests/vmm_test.c kernel/mm/vmm.c -o build/tests/vmm_test
	build/tests/vmm_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel tests/heap_test.c kernel/mm/heap.c -o build/tests/heap_test
	build/tests/heap_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel tests/heap_stress_test.c kernel/mm/heap.c -o build/tests/heap_stress_test
	build/tests/heap_stress_test
	$(HOST_CC) -std=c11 -O2 -g -fno-builtin -Wall -Wextra -Werror tests/string_test.c kernel/lib/string.c -o build/tests/string_test
	build/tests/string_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel tests/scheduler_test.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/scheduler_test
	build/tests/scheduler_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror tests/context_switch_test.c kernel/sched/switch.S -o build/tests/context_switch_test
	build/tests/context_switch_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TIMER_HOST_TEST -Ikernel tests/timer_test.c kernel/arch/x86_64/timer.c -o build/tests/timer_test
	build/tests/timer_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel tests/preempt_test.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/preempt_test
	build/tests/preempt_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel tests/sleep_test.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/sleep_test
	build/tests/sleep_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_IRQ_HOST_TEST -Ikernel tests/irq_state_test.c kernel/arch/x86_64/irq_state.c -o build/tests/irq_state_test
	build/tests/irq_state_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel tests/crash_test.c kernel/diag/crash.c -o build/tests/crash_test
	build/tests/crash_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -Ikernel tests/sync_test.c kernel/sync/wait_queue.c kernel/sync/event.c kernel/sync/mutex.c kernel/sync/ipc.c -o build/tests/sync_test
	build/tests/sync_test

test-sanitize: test-native-text-layout-sanitize test-native-layout-sanitize test-wallpaper-sanitize test-native-scene-sanitize test-shell-raster-sanitize test-compositor-integration-sanitize test-native-games-sanitize test-shell-bridge-sanitize test-window-protocol-sanitize test-desktop-sanitize test-capipc-sanitize $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -DOPENXP_NATIVE_WAIT_HOST_TEST -Ikernel tests/native_wait_test.c kernel/gui/native.c kernel/video/fb.c -o build/tests/native_wait_sanitized
	build/tests/native_wait_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel tests/vfs_test.c kernel/fs/vfs.c kernel/fs/initrd.c kernel/fs/ramfs.c -o build/tests/vfs_sanitized
	build/tests/vfs_sanitized $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_IRQ_HOST_TEST -Ikernel tests/app_launch_test.c kernel/user/app_launch.c kernel/arch/x86_64/irq_state.c -o build/tests/app_launch_sanitized
	build/tests/app_launch_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_PCI_HOST_TEST -Ikernel tests/pci_test.c kernel/drivers/pci.c -o build/tests/pci_sanitized
	build/tests/pci_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_PERSIST_HOST_TEST -Ikernel tests/persist_test.c kernel/fs/persist.c kernel/fs/ramfs.c -o build/tests/persist_sanitized
	build/tests/persist_sanitized
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -Ikernel tests/native_window_test.c kernel/gui/native.c kernel/video/fb.c -o build/tests/native_window_sanitized
	build/tests/native_window_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel tests/ramfs_test.c kernel/fs/ramfs.c -o build/tests/ramfs_sanitized
	build/tests/ramfs_sanitized
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel tests/initrd_test.c kernel/fs/initrd.c -o build/tests/initrd_sanitized
	build/tests/initrd_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_PROCESS_HOST_TEST -DOPENXP_CAPIPC_HOST_TEST -Ikernel tests/process_manager_test.c kernel/user/process.c kernel/user/capipc.c -o build/tests/process_manager_sanitized
	build/tests/process_manager_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -DOPENXP_CAPIPC_HOST_TEST -DOPENXP_SHELL_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel tests/syscall_test.c kernel/user/syscall.c kernel/user/app_launch.c kernel/arch/x86_64/irq_state.c kernel/user/capipc.c kernel/gui/native.c kernel/gui/shell_bridge.c kernel/video/fb.c kernel/fs/ramfs.c kernel/fs/vfs.c kernel/fs/initrd.c -o build/tests/syscall_sanitized
	build/tests/syscall_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_VMM_HOST_TEST -Ikernel tests/process_test.c kernel/mm/vmm.c kernel/mm/address_space.c kernel/user/elf.c -o build/tests/process_sanitized
	build/tests/process_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel tests/pmm_test.c kernel/mm/pmm.c -o build/tests/pmm_sanitized
	build/tests/pmm_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_VMM_HOST_TEST -Ikernel tests/vmm_test.c kernel/mm/vmm.c -o build/tests/vmm_sanitized
	build/tests/vmm_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel tests/heap_test.c kernel/mm/heap.c -o build/tests/heap_sanitized
	build/tests/heap_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel tests/heap_stress_test.c kernel/mm/heap.c -o build/tests/heap_stress_sanitized
	build/tests/heap_stress_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fno-builtin -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/string_test.c kernel/lib/string.c -o build/tests/string_sanitized
	build/tests/string_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel tests/scheduler_test.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/scheduler_sanitized
	build/tests/scheduler_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TIMER_HOST_TEST -Ikernel tests/timer_test.c kernel/arch/x86_64/timer.c -o build/tests/timer_sanitized
	build/tests/timer_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel tests/preempt_test.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/preempt_sanitized
	build/tests/preempt_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel tests/sleep_test.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/sleep_sanitized
	build/tests/sleep_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_IRQ_HOST_TEST -Ikernel tests/irq_state_test.c kernel/arch/x86_64/irq_state.c -o build/tests/irq_state_sanitized
	build/tests/irq_state_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel tests/crash_test.c kernel/diag/crash.c -o build/tests/crash_sanitized
	build/tests/crash_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -pthread -Ikernel tests/sync_test.c kernel/sync/wait_queue.c kernel/sync/event.c kernel/sync/mutex.c kernel/sync/ipc.c -o build/tests/sync_sanitized
	build/tests/sync_sanitized
test-desktop:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel tests/shell_test.c kernel/shell/shell.c -o build/tests/shell_test
	build/tests/shell_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel tests/fb_test.c kernel/video/fb.c -o build/tests/fb_test
	build/tests/fb_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_INPUT_HOST_TEST -Ikernel tests/input_test.c kernel/input/input.c -o build/tests/input_test
	build/tests/input_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_WM_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel tests/desktop_test.c kernel/gui/desktop.c kernel/gui/native.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o build/tests/desktop_test
	build/tests/desktop_test

test-desktop-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel tests/shell_test.c kernel/shell/shell.c -o build/tests/shell_sanitized
	build/tests/shell_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel tests/fb_test.c kernel/video/fb.c -o build/tests/fb_sanitized
	build/tests/fb_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_INPUT_HOST_TEST -Ikernel tests/input_test.c kernel/input/input.c -o build/tests/input_sanitized
	build/tests/input_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_WM_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel tests/desktop_test.c kernel/gui/desktop.c kernel/gui/native.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o build/tests/desktop_sanitized
	build/tests/desktop_sanitized

clean:
	rm -rf $(BUILD) build/deep build/process build/preempt build/user build/initrd build/exception build/guard build/timer build/sync build/user-fault build/user-protection build/tests

.PHONY: test-native-paint
# Native source codec test (no guest needed).
test-native-paint:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tests/native_paint_codec_test.c -o build/tests/native_paint_codec_test
	build/tests/native_paint_codec_test

.PHONY: test-native-edit
test-native-edit:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tests/native_text_edit_test.c -o build/tests/native_text_edit_test
	build/tests/native_text_edit_test

.PHONY: test-native-theme
test-native-theme:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tests/native_theme_test.c -o build/tests/native_theme_test
	build/tests/native_theme_test

.PHONY: test-native-sanitize
test-native-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/native_paint_codec_test.c -o build/tests/native_paint_sanitized
	build/tests/native_paint_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/native_text_edit_test.c -o build/tests/native_text_edit_sanitized
	build/tests/native_text_edit_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/native_theme_test.c -o build/tests/native_theme_sanitized
	build/tests/native_theme_sanitized

# Pixel-level integration through the actual desktop renderer, not only a
# synthetic command-list unit test. Hardware input/VM runtime still separate.
.PHONY: test-compositor-integration test-compositor-integration-sanitize
test-compositor-integration:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_SHELL_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel tests/compositor_integration_test.c kernel/gui/desktop.c kernel/gui/native.c kernel/gui/shell_bridge.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o $(BUILD)/tests/compositor_integration_test
	$(BUILD)/tests/compositor_integration_test
test-compositor-integration-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_SHELL_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel tests/compositor_integration_test.c kernel/gui/desktop.c kernel/gui/native.c kernel/gui/shell_bridge.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o $(BUILD)/tests/compositor_integration_sanitized
	$(BUILD)/tests/compositor_integration_sanitized

.PHONY: test-shell-raster test-shell-raster-sanitize
test-shell-raster:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror tests/shell_raster_test.c apps/native/shell_raster.c -o $(BUILD)/tests/shell_raster_test
	$(BUILD)/tests/shell_raster_test
test-shell-raster-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/shell_raster_test.c apps/native/shell_raster.c -o $(BUILD)/tests/shell_raster_sanitized
	$(BUILD)/tests/shell_raster_sanitized

.PHONY: test-native-scene test-native-scene-sanitize
test-native-scene:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror tests/native_scene_test.c apps/native/native_scene.c apps/native/shell_raster.c -o $(BUILD)/tests/native_scene_test
	$(BUILD)/tests/native_scene_test
test-native-scene-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/native_scene_test.c apps/native/native_scene.c apps/native/shell_raster.c -o $(BUILD)/tests/native_scene_sanitized
	$(BUILD)/tests/native_scene_sanitized

# Regression: incremental desktop regions must exactly match a full composed
# frame, including cursor underlay, window movement and taskbar updates.
.PHONY: test-present test-present-sanitize
test-present:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_WM_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel tests/desktop_present_test.c kernel/gui/desktop.c kernel/gui/native.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o $(BUILD)/tests/desktop_present_test
	$(BUILD)/tests/desktop_present_test
test-present-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_WM_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel tests/desktop_present_test.c kernel/gui/desktop.c kernel/gui/native.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o $(BUILD)/tests/desktop_present_sanitized
	$(BUILD)/tests/desktop_present_sanitized

.PHONY: test-wallpaper test-wallpaper-sanitize
test-wallpaper:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror tests/wallpaper_test.c apps/native/wallpaper.c -o build/tests/wallpaper_test
	build/tests/wallpaper_test
test-wallpaper-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/wallpaper_test.c apps/native/wallpaper.c -o build/tests/wallpaper_sanitized
	build/tests/wallpaper_sanitized

.PHONY: test-native-layout test-native-layout-sanitize
test-native-layout:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror tests/native_layout_test.c -o $(BUILD)/tests/native_layout_test
	$(BUILD)/tests/native_layout_test
test-native-layout-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/native_layout_test.c -o $(BUILD)/tests/native_layout_sanitized
	$(BUILD)/tests/native_layout_sanitized

.PHONY: test-native-text-layout test-native-text-layout-sanitize
test-native-text-layout:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror tests/native_text_layout_test.c -o $(BUILD)/tests/native_text_layout_test
	$(BUILD)/tests/native_text_layout_test
test-native-text-layout-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror tests/native_text_layout_test.c -o $(BUILD)/tests/native_text_layout_sanitized
	$(BUILD)/tests/native_text_layout_sanitized
