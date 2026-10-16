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
$(BUILD)/apps/%.native.o: apps/native/%.c apps/native/oxp.h apps/native/ui_widgets.h apps/native/ui_pointer.h apps/native/ui_atlas.h apps/native/font_engine.h apps/native/game_logic.h apps/native/edit_session.h apps/native/paint_history.h apps/native/file_workflow.h apps/native/file_picker.h kernel/gui/editor_keycodes.h Makefile
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
$(BUILD)/apps/native_scene.o: apps/native/native_scene.c apps/native/native_scene.h apps/native/shell_raster.h apps/native/skin.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/apps/skin.o: apps/native/skin.c apps/native/skin.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/skin/aurora-widgets.oxua $(BUILD)/skin/aurora-icons.oxua $(BUILD)/skin/aurora-chrome.oxua $(BUILD)/skin/aurora-menu.oxua $(BUILD)/skin/aurora-toolbar.oxua &: tools/pack_ui_atlases.py tools/pack_skin.py assets/ui/aurora/widgets-atlas.png assets/ui/aurora/icons-32.png assets/ui/aurora/chrome-strip.png assets/ui/aurora/menu-atlas.png assets/ui/aurora/toolbar-atlas.png
	@mkdir -p $(BUILD)/skin
	python3 tools/pack_ui_atlases.py --source assets/ui/aurora --output $(BUILD)/skin
$(BUILD)/skin/aurora-font.oxft: tools/pack_aurora_font.py tools/pack_skin.py assets/ui/aurora/fonts/aurora-font.png
	@mkdir -p $(dir $@)
	python3 tools/pack_aurora_font.py assets/ui/aurora/fonts/aurora-font.png $@
$(BUILD)/skin/aurora.oxs: tools/pack_skin.py $(wildcard assets/ui/aurora/*.png)
	@mkdir -p $(dir $@)
	python3 tools/pack_skin.py $@ --source assets/ui/aurora
# Theme variants are original editable PNG artwork, not generated at build.
# The Blue pack retains its existing aurora.* filenames for compatibility.
$(BUILD)/skin/olive-widgets.oxua $(BUILD)/skin/olive-icons.oxua $(BUILD)/skin/olive-chrome.oxua $(BUILD)/skin/olive-menu.oxua $(BUILD)/skin/olive-toolbar.oxua &: tools/pack_ui_atlases.py tools/pack_skin.py assets/ui/aurora/olive/widgets-atlas.png assets/ui/aurora/olive/icons-32.png assets/ui/aurora/olive/chrome-strip.png assets/ui/aurora/olive/menu-atlas.png assets/ui/aurora/olive/toolbar-atlas.png
	@mkdir -p $(BUILD)/skin
	python3 tools/pack_ui_atlases.py --source assets/ui/aurora/olive --output $(BUILD)/skin --prefix olive
$(BUILD)/skin/midnight-widgets.oxua $(BUILD)/skin/midnight-icons.oxua $(BUILD)/skin/midnight-chrome.oxua $(BUILD)/skin/midnight-menu.oxua $(BUILD)/skin/midnight-toolbar.oxua &: tools/pack_ui_atlases.py tools/pack_skin.py assets/ui/aurora/midnight/widgets-atlas.png assets/ui/aurora/midnight/icons-32.png assets/ui/aurora/midnight/chrome-strip.png assets/ui/aurora/midnight/menu-atlas.png assets/ui/aurora/midnight/toolbar-atlas.png
	@mkdir -p $(BUILD)/skin
	python3 tools/pack_ui_atlases.py --source assets/ui/aurora/midnight --output $(BUILD)/skin --prefix midnight
$(BUILD)/skin/olive.oxs: tools/pack_skin.py $(wildcard assets/ui/aurora/olive/*.png)
	@mkdir -p $(dir $@)
	python3 tools/pack_skin.py $@ --source assets/ui/aurora/olive
$(BUILD)/skin/midnight.oxs: tools/pack_skin.py $(wildcard assets/ui/aurora/midnight/*.png)
	@mkdir -p $(dir $@)
	python3 tools/pack_skin.py $@ --source assets/ui/aurora/midnight
$(BUILD)/apps/wallpaper.o: apps/native/wallpaper.c apps/native/wallpaper.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/apps/shell_icons.o: apps/native/shell_icons.c apps/native/shell_icons.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/apps/shell_menu_art.o: apps/native/shell_menu_art.c apps/native/shell_menu_art.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/apps/windowd.native.o: apps/native/shell_menu_art.h apps/native/theme.h
$(BUILD)/apps/windowd.native.o: apps/native/shell_layout.h apps/native/shell_icons.h apps/native/shell_interaction.h
$(BUILD)/apps/windowd.native.o: apps/native/native_scene.h
$(BUILD)/apps/windowd.native.o: kernel/user/wm_display.h
$(BUILD)/apps/shell_raster.o: apps/native/shell_raster.c apps/native/shell_raster.h apps/native/font_engine.h kernel/user/shell_abi.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -Ikernel -c $< -o $@
$(BUILD)/apps/wm_protocol.o: apps/native/wm_protocol.c apps/native/wm_protocol.h Makefile
	@mkdir -p $(dir $@)
	$(CC) -target x86_64-unknown-none-elf -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fpie $(RING3_CPU_FLAGS) -Wall -Wextra -Werror -c $< -o $@
$(BUILD)/apps/windowd.bin: $(BUILD)/apps/windowd.native.o $(BUILD)/apps/native_start.o $(BUILD)/apps/native_runtime.o $(BUILD)/apps/wm_protocol.o $(BUILD)/apps/shell_raster.o $(BUILD)/apps/native_scene.o $(BUILD)/apps/wallpaper.o $(BUILD)/apps/skin.o $(BUILD)/apps/shell_icons.o $(BUILD)/apps/shell_menu_art.o apps/native/app.ld tools/check_windowd_pic.py tools/check_windowd_bss.py
	python3 tools/check_windowd_pic.py $(BUILD)/apps/windowd.native.o $(BUILD)/apps/native_runtime.o $(BUILD)/apps/wm_protocol.o $(BUILD)/apps/shell_raster.o $(BUILD)/apps/native_scene.o $(BUILD)/apps/wallpaper.o $(BUILD)/apps/skin.o $(BUILD)/apps/shell_icons.o $(BUILD)/apps/shell_menu_art.o
	$(LD) -m elf_x86_64 --oformat binary -T apps/native/app.ld $(BUILD)/apps/native_start.o $(BUILD)/apps/windowd.native.o $(BUILD)/apps/native_runtime.o $(BUILD)/apps/wm_protocol.o $(BUILD)/apps/shell_raster.o $(BUILD)/apps/native_scene.o $(BUILD)/apps/wallpaper.o $(BUILD)/apps/skin.o $(BUILD)/apps/shell_icons.o $(BUILD)/apps/shell_menu_art.o -Map $(BUILD)/apps/windowd.map -o $@
	python3 tools/check_windowd_bss.py $(BUILD)/apps/windowd.map
# Inspect the *actual compiled object code*; changes to toolchain defaults
# must not silently reintroduce unsupported SIMD/FPU instructions.
# The stamp is an initrd prerequisite, so every image build is protected.
$(BUILD)/apps/.ring3-isa-ok: $(NATIVE_BINS) $(BUILD)/apps/hello.o $(BUILD)/apps/fault.o tools/check_ring3_isa.py
	python3 tools/check_ring3_isa.py $(BUILD)/apps
	@touch $@
$(BUILD)/initrd.img: $(BUILD)/apps/hello.bin $(BUILD)/apps/fault.bin $(NATIVE_BINS) $(BUILD)/apps/.ring3-isa-ok tools/mkinitrd.py $(wildcard assets/wallpapers/*.bmp) $(BUILD)/skin/aurora.oxs $(BUILD)/skin/aurora-widgets.oxua $(BUILD)/skin/aurora-icons.oxua $(BUILD)/skin/olive.oxs $(BUILD)/skin/olive-widgets.oxua $(BUILD)/skin/olive-icons.oxua $(BUILD)/skin/midnight.oxs $(BUILD)/skin/midnight-widgets.oxua $(BUILD)/skin/midnight-icons.oxua $(BUILD)/skin/aurora-chrome.oxua $(BUILD)/skin/olive-chrome.oxua $(BUILD)/skin/midnight-chrome.oxua $(BUILD)/skin/midnight-menu.oxua $(BUILD)/skin/aurora-menu.oxua $(BUILD)/skin/olive-menu.oxua $(BUILD)/skin/aurora-toolbar.oxua $(BUILD)/skin/olive-toolbar.oxua $(BUILD)/skin/midnight-toolbar.oxua $(BUILD)/skin/aurora-font.oxft
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
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_GAMES tests/suite_apps.c -o build/tests/native_games_test
	build/tests/native_games_test

test-native-games-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_GAMES tests/suite_apps.c -o build/tests/native_games_sanitized
	build/tests/native_games_sanitized

test-shell-bridge:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_SHELL_HOST_TEST -Ikernel -DOPENXP_TEST_SHELL_BRIDGE tests/suite_shell.c kernel/gui/shell_bridge.c kernel/video/fb.c -o build/tests/shell_bridge_test
	build/tests/shell_bridge_test

test-shell-bridge-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_SHELL_HOST_TEST -Ikernel -DOPENXP_TEST_SHELL_BRIDGE tests/suite_shell.c kernel/gui/shell_bridge.c kernel/video/fb.c -o build/tests/shell_bridge_sanitized
	build/tests/shell_bridge_sanitized

test-capipc:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_CAPIPC_HOST_TEST -Ikernel -DOPENXP_TEST_CAPIPC tests/suite_process.c kernel/user/capipc.c -o build/tests/capipc_test
	build/tests/capipc_test

test-capipc-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_CAPIPC_HOST_TEST -Ikernel -DOPENXP_TEST_CAPIPC tests/suite_process.c kernel/user/capipc.c -o build/tests/capipc_sanitized
	build/tests/capipc_sanitized

test-window-protocol:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_WM_PROTOCOL tests/suite_process.c apps/native/wm_protocol.c -o build/tests/wm_protocol_test
	build/tests/wm_protocol_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_CAPIPC_HOST_TEST -Ikernel -DOPENXP_TEST_WM_CAPIPC tests/suite_process.c apps/native/wm_protocol.c kernel/user/capipc.c -o build/tests/wm_capipc_test
	build/tests/wm_capipc_test

test-window-protocol-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_WM_PROTOCOL tests/suite_process.c apps/native/wm_protocol.c -o build/tests/wm_protocol_sanitized
	build/tests/wm_protocol_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_CAPIPC_HOST_TEST -Ikernel -DOPENXP_TEST_WM_CAPIPC tests/suite_process.c apps/native/wm_protocol.c kernel/user/capipc.c -o build/tests/wm_capipc_sanitized
	build/tests/wm_capipc_sanitized

test-unit: test-aurora17 test-aurora16 test-aurora15 test-aurora14 test-aurora13 test-advanced-input test-native-pointer test-shell-interaction test-font-fit test-font-engine test-aurora-font-png test-shell-menu-art test-calculator-layout test-aurora-themes test-shell-icons test-ui-atlas test-native-widgets test-native-scene-skin test-skin test-windowd-pic test-native-scroll test-native-text-layout test-native-layout test-wallpaper test-native-scene test-shell-raster test-compositor-integration test-native-games test-shell-bridge test-window-protocol test-desktop test-capipc $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -DOPENXP_NATIVE_WAIT_HOST_TEST -Ikernel -DOPENXP_TEST_NATIVE_WAIT tests/suite_apps.c kernel/gui/native.c kernel/video/fb.c -o build/tests/native_wait_test
	build/tests/native_wait_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_VFS tests/suite_filesystem.c kernel/fs/vfs.c kernel/fs/initrd.c kernel/fs/ramfs.c -o build/tests/vfs_test
	build/tests/vfs_test $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_IRQ_HOST_TEST -Ikernel -DOPENXP_TEST_APP_LAUNCH tests/suite_process.c kernel/user/app_launch.c kernel/arch/x86_64/irq_state.c -o build/tests/app_launch_test
	build/tests/app_launch_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_PCI_HOST_TEST -Ikernel -DOPENXP_TEST_PCI tests/suite_kernel.c kernel/drivers/pci.c -o build/tests/pci_test
	build/tests/pci_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_PERSIST_HOST_TEST -Ikernel -DOPENXP_TEST_PERSIST tests/suite_filesystem.c kernel/fs/persist.c kernel/fs/ramfs.c -o build/tests/persist_test
	build/tests/persist_test
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -Ikernel -DOPENXP_TEST_NATIVE_WINDOW tests/suite_apps.c kernel/gui/native.c kernel/video/fb.c -o build/tests/native_window_test
	build/tests/native_window_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_RAMFS tests/suite_filesystem.c kernel/fs/ramfs.c -o build/tests/ramfs_test
	build/tests/ramfs_test
	@mkdir -p build/tests
	python3 -m unittest discover -s tests -p 'test_*.py'
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_INITRD tests/suite_filesystem.c kernel/fs/initrd.c -o build/tests/initrd_test
	build/tests/initrd_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_INITRD_IMAGE tests/suite_filesystem.c kernel/fs/initrd.c -o build/tests/initrd_image_test
	build/tests/initrd_image_test $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_PROCESS_HOST_TEST -DOPENXP_CAPIPC_HOST_TEST -Ikernel -DOPENXP_TEST_PROCESS_MANAGER tests/suite_process.c kernel/user/process.c kernel/user/capipc.c -o build/tests/process_manager_test
	build/tests/process_manager_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -DOPENXP_CAPIPC_HOST_TEST -DOPENXP_SHELL_HOST_TEST -DOPENXP_IRQ_HOST_TEST -DOPENXP_PERSIST_HOST_TEST -Ikernel -DOPENXP_TEST_SYSCALL tests/suite_process.c kernel/user/syscall.c kernel/user/app_launch.c kernel/arch/x86_64/irq_state.c kernel/user/capipc.c kernel/gui/native.c kernel/gui/shell_bridge.c kernel/video/fb.c kernel/fs/ramfs.c kernel/fs/vfs.c kernel/fs/initrd.c kernel/fs/persist.c -o build/tests/syscall_test
	build/tests/syscall_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_VMM_HOST_TEST -Ikernel -DOPENXP_TEST_PROCESS tests/suite_process.c kernel/mm/vmm.c kernel/mm/address_space.c kernel/user/elf.c -o build/tests/process_test
	build/tests/process_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_PMM tests/suite_kernel.c kernel/mm/pmm.c -o build/tests/pmm_test
	build/tests/pmm_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_VMM_HOST_TEST -Ikernel -DOPENXP_TEST_VMM tests/suite_kernel.c kernel/mm/vmm.c -o build/tests/vmm_test
	build/tests/vmm_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_HEAP tests/suite_kernel.c kernel/mm/heap.c -o build/tests/heap_test
	build/tests/heap_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_HEAP_STRESS tests/suite_kernel.c kernel/mm/heap.c -o build/tests/heap_stress_test
	build/tests/heap_stress_test
	$(HOST_CC) -std=c11 -O2 -g -fno-builtin -Wall -Wextra -Werror -DOPENXP_TEST_STRING tests/suite_kernel.c kernel/lib/string.c -o build/tests/string_test
	build/tests/string_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel -DOPENXP_TEST_SCHEDULER tests/suite_kernel.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/scheduler_test
	build/tests/scheduler_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_CONTEXT_SWITCH tests/suite_kernel.c kernel/sched/switch.S -o build/tests/context_switch_test
	build/tests/context_switch_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TIMER_HOST_TEST -Ikernel -DOPENXP_TEST_TIMER tests/suite_kernel.c kernel/arch/x86_64/timer.c -o build/tests/timer_test
	build/tests/timer_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel -DOPENXP_TEST_PREEMPT tests/suite_kernel.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/preempt_test
	build/tests/preempt_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel -DOPENXP_TEST_SLEEP tests/suite_kernel.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/sleep_test
	build/tests/sleep_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_IRQ_HOST_TEST -Ikernel -DOPENXP_TEST_IRQ_STATE tests/suite_kernel.c kernel/arch/x86_64/irq_state.c -o build/tests/irq_state_test
	build/tests/irq_state_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_CRASH tests/suite_kernel.c kernel/diag/crash.c -o build/tests/crash_test
	build/tests/crash_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -Ikernel -DOPENXP_TEST_SYNC tests/suite_kernel.c kernel/sync/wait_queue.c kernel/sync/event.c kernel/sync/mutex.c kernel/sync/ipc.c -o build/tests/sync_test
	build/tests/sync_test

test-sanitize: test-aurora17-sanitize test-aurora16-sanitize test-aurora15-sanitize test-aurora14-sanitize test-aurora13-sanitize test-advanced-input-sanitize test-native-pointer-sanitize test-shell-interaction-sanitize test-font-engine-sanitize test-shell-menu-art-sanitize test-calculator-layout-sanitize test-aurora-themes test-shell-icons-sanitize test-ui-atlas test-native-widgets-sanitize test-native-scene-skin-sanitize test-skin-sanitize test-native-scroll-sanitize test-native-text-layout-sanitize test-native-layout-sanitize test-wallpaper-sanitize test-native-scene-sanitize test-shell-raster-sanitize test-compositor-integration-sanitize test-native-games-sanitize test-shell-bridge-sanitize test-window-protocol-sanitize test-desktop-sanitize test-capipc-sanitize $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -DOPENXP_NATIVE_WAIT_HOST_TEST -Ikernel -DOPENXP_TEST_NATIVE_WAIT tests/suite_apps.c kernel/gui/native.c kernel/video/fb.c -o build/tests/native_wait_sanitized
	build/tests/native_wait_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_VFS tests/suite_filesystem.c kernel/fs/vfs.c kernel/fs/initrd.c kernel/fs/ramfs.c -o build/tests/vfs_sanitized
	build/tests/vfs_sanitized $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_IRQ_HOST_TEST -Ikernel -DOPENXP_TEST_APP_LAUNCH tests/suite_process.c kernel/user/app_launch.c kernel/arch/x86_64/irq_state.c -o build/tests/app_launch_sanitized
	build/tests/app_launch_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_PCI_HOST_TEST -Ikernel -DOPENXP_TEST_PCI tests/suite_kernel.c kernel/drivers/pci.c -o build/tests/pci_sanitized
	build/tests/pci_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_PERSIST_HOST_TEST -Ikernel -DOPENXP_TEST_PERSIST tests/suite_filesystem.c kernel/fs/persist.c kernel/fs/ramfs.c -o build/tests/persist_sanitized
	build/tests/persist_sanitized
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -Ikernel -DOPENXP_TEST_NATIVE_WINDOW tests/suite_apps.c kernel/gui/native.c kernel/video/fb.c -o build/tests/native_window_sanitized
	build/tests/native_window_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_RAMFS tests/suite_filesystem.c kernel/fs/ramfs.c -o build/tests/ramfs_sanitized
	build/tests/ramfs_sanitized
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_INITRD tests/suite_filesystem.c kernel/fs/initrd.c -o build/tests/initrd_sanitized
	build/tests/initrd_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_PROCESS_HOST_TEST -DOPENXP_CAPIPC_HOST_TEST -Ikernel -DOPENXP_TEST_PROCESS_MANAGER tests/suite_process.c kernel/user/process.c kernel/user/capipc.c -o build/tests/process_manager_sanitized
	build/tests/process_manager_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -DOPENXP_CAPIPC_HOST_TEST -DOPENXP_SHELL_HOST_TEST -DOPENXP_IRQ_HOST_TEST -DOPENXP_PERSIST_HOST_TEST -Ikernel -DOPENXP_TEST_SYSCALL tests/suite_process.c kernel/user/syscall.c kernel/user/app_launch.c kernel/arch/x86_64/irq_state.c kernel/user/capipc.c kernel/gui/native.c kernel/gui/shell_bridge.c kernel/video/fb.c kernel/fs/ramfs.c kernel/fs/vfs.c kernel/fs/initrd.c kernel/fs/persist.c -o build/tests/syscall_sanitized
	build/tests/syscall_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_VMM_HOST_TEST -Ikernel -DOPENXP_TEST_PROCESS tests/suite_process.c kernel/mm/vmm.c kernel/mm/address_space.c kernel/user/elf.c -o build/tests/process_sanitized
	build/tests/process_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_PMM tests/suite_kernel.c kernel/mm/pmm.c -o build/tests/pmm_sanitized
	build/tests/pmm_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_VMM_HOST_TEST -Ikernel -DOPENXP_TEST_VMM tests/suite_kernel.c kernel/mm/vmm.c -o build/tests/vmm_sanitized
	build/tests/vmm_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_HEAP tests/suite_kernel.c kernel/mm/heap.c -o build/tests/heap_sanitized
	build/tests/heap_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_HEAP_STRESS tests/suite_kernel.c kernel/mm/heap.c -o build/tests/heap_stress_sanitized
	build/tests/heap_stress_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fno-builtin -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_STRING tests/suite_kernel.c kernel/lib/string.c -o build/tests/string_sanitized
	build/tests/string_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel -DOPENXP_TEST_SCHEDULER tests/suite_kernel.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/scheduler_sanitized
	build/tests/scheduler_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TIMER_HOST_TEST -Ikernel -DOPENXP_TEST_TIMER tests/suite_kernel.c kernel/arch/x86_64/timer.c -o build/tests/timer_sanitized
	build/tests/timer_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel -DOPENXP_TEST_PREEMPT tests/suite_kernel.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/preempt_sanitized
	build/tests/preempt_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_SCHED_HOST_TEST -DOPENXP_IRQ_HOST_TEST -Ikernel -DOPENXP_TEST_SLEEP tests/suite_kernel.c kernel/sched/scheduler.c kernel/arch/x86_64/irq_state.c -o build/tests/sleep_sanitized
	build/tests/sleep_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_IRQ_HOST_TEST -Ikernel -DOPENXP_TEST_IRQ_STATE tests/suite_kernel.c kernel/arch/x86_64/irq_state.c -o build/tests/irq_state_sanitized
	build/tests/irq_state_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_CRASH tests/suite_kernel.c kernel/diag/crash.c -o build/tests/crash_sanitized
	build/tests/crash_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -pthread -Ikernel -DOPENXP_TEST_SYNC tests/suite_kernel.c kernel/sync/wait_queue.c kernel/sync/event.c kernel/sync/mutex.c kernel/sync/ipc.c -o build/tests/sync_sanitized
	build/tests/sync_sanitized
test-desktop:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_SHELL tests/suite_shell.c kernel/shell/shell.c -o build/tests/shell_test
	build/tests/shell_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_FB tests/suite_graphics.c kernel/video/fb.c -o build/tests/fb_test
	build/tests/fb_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_INPUT_HOST_TEST -Ikernel -DOPENXP_TEST_INPUT tests/suite_kernel.c kernel/input/input.c -o build/tests/input_test
	build/tests/input_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_WM_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel -DOPENXP_TEST_DESKTOP tests/suite_shell.c kernel/gui/desktop.c kernel/gui/native.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o build/tests/desktop_test
	build/tests/desktop_test

test-desktop-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_SHELL tests/suite_shell.c kernel/shell/shell.c -o build/tests/shell_sanitized
	build/tests/shell_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_FB tests/suite_graphics.c kernel/video/fb.c -o build/tests/fb_sanitized
	build/tests/fb_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_INPUT_HOST_TEST -Ikernel -DOPENXP_TEST_INPUT tests/suite_kernel.c kernel/input/input.c -o build/tests/input_sanitized
	build/tests/input_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_WM_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel -DOPENXP_TEST_DESKTOP tests/suite_shell.c kernel/gui/desktop.c kernel/gui/native.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o build/tests/desktop_sanitized
	build/tests/desktop_sanitized

clean:
	rm -rf $(BUILD) build/deep build/process build/preempt build/user build/initrd build/exception build/guard build/timer build/sync build/user-fault build/user-protection build/tests

.PHONY: test-native-paint
# Native source codec test (no guest needed).
test-native-paint:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_PAINT_CODEC tests/suite_aurora.c -o build/tests/native_paint_codec_test
	build/tests/native_paint_codec_test

.PHONY: test-native-edit
test-native-edit:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_TEXT_EDIT tests/suite_aurora.c -o build/tests/native_text_edit_test
	build/tests/native_text_edit_test

.PHONY: test-native-theme
test-native-theme:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_THEME tests/suite_graphics.c -o build/tests/native_theme_test
	build/tests/native_theme_test

.PHONY: test-native-sanitize
test-native-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_PAINT_CODEC tests/suite_aurora.c -o build/tests/native_paint_sanitized
	build/tests/native_paint_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_TEXT_EDIT tests/suite_aurora.c -o build/tests/native_text_edit_sanitized
	build/tests/native_text_edit_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_THEME tests/suite_graphics.c -o build/tests/native_theme_sanitized
	build/tests/native_theme_sanitized

# Pixel-level integration through the actual desktop renderer, not only a
# synthetic command-list unit test. Hardware input/VM runtime still separate.
.PHONY: test-compositor-integration test-compositor-integration-sanitize
test-compositor-integration:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_SHELL_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel -DOPENXP_TEST_COMPOSITOR_INTEGRATION tests/suite_graphics.c kernel/gui/desktop.c kernel/gui/native.c kernel/gui/shell_bridge.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o $(BUILD)/tests/compositor_integration_test
	$(BUILD)/tests/compositor_integration_test
test-compositor-integration-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_SHELL_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel -DOPENXP_TEST_COMPOSITOR_INTEGRATION tests/suite_graphics.c kernel/gui/desktop.c kernel/gui/native.c kernel/gui/shell_bridge.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o $(BUILD)/tests/compositor_integration_sanitized
	$(BUILD)/tests/compositor_integration_sanitized

.PHONY: test-shell-raster test-shell-raster-sanitize
test-shell-raster:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_SHELL_RASTER tests/suite_graphics.c apps/native/shell_raster.c -o $(BUILD)/tests/shell_raster_test
	$(BUILD)/tests/shell_raster_test
test-shell-raster-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_SHELL_RASTER tests/suite_graphics.c apps/native/shell_raster.c -o $(BUILD)/tests/shell_raster_sanitized
	$(BUILD)/tests/shell_raster_sanitized

.PHONY: test-native-scene test-native-scene-sanitize
test-native-scene:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_SCENE tests/suite_graphics.c apps/native/native_scene.c apps/native/shell_raster.c apps/native/skin.c -o $(BUILD)/tests/native_scene_test
	$(BUILD)/tests/native_scene_test
test-native-scene-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_SCENE tests/suite_graphics.c apps/native/native_scene.c apps/native/shell_raster.c apps/native/skin.c -o $(BUILD)/tests/native_scene_sanitized
	$(BUILD)/tests/native_scene_sanitized

# Regression: incremental desktop regions must exactly match a full composed
# frame, including cursor underlay, window movement and taskbar updates.
.PHONY: test-present test-present-sanitize
test-present:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_WM_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel -DOPENXP_TEST_DESKTOP_PRESENT tests/suite_shell.c kernel/gui/desktop.c kernel/gui/native.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o $(BUILD)/tests/desktop_present_test
	$(BUILD)/tests/desktop_present_test
test-present-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_WM_HOST_TEST -DOPENXP_NATIVE_HOST_TEST -Ikernel -DOPENXP_TEST_DESKTOP_PRESENT tests/suite_shell.c kernel/gui/desktop.c kernel/gui/native.c kernel/shell/shell.c kernel/video/fb.c kernel/fs/ramfs.c -o $(BUILD)/tests/desktop_present_sanitized
	$(BUILD)/tests/desktop_present_sanitized

.PHONY: test-wallpaper test-wallpaper-sanitize
test-wallpaper:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -DOPENXP_TEST_WALLPAPER tests/suite_graphics.c apps/native/wallpaper.c -o build/tests/wallpaper_test
	build/tests/wallpaper_test
test-wallpaper-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_WALLPAPER tests/suite_graphics.c apps/native/wallpaper.c -o build/tests/wallpaper_sanitized
	build/tests/wallpaper_sanitized

.PHONY: test-native-layout test-native-layout-sanitize
test-native-layout:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_LAYOUT tests/suite_apps.c -o $(BUILD)/tests/native_layout_test
	$(BUILD)/tests/native_layout_test
test-native-layout-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_LAYOUT tests/suite_apps.c -o $(BUILD)/tests/native_layout_sanitized
	$(BUILD)/tests/native_layout_sanitized

.PHONY: test-native-text-layout test-native-text-layout-sanitize
test-native-text-layout:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_TEXT_LAYOUT tests/suite_apps.c -o $(BUILD)/tests/native_text_layout_test
	$(BUILD)/tests/native_text_layout_test
test-native-text-layout-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_TEXT_LAYOUT tests/suite_apps.c -o $(BUILD)/tests/native_text_layout_sanitized
	$(BUILD)/tests/native_text_layout_sanitized

.PHONY: test-windowd-pic
test-windowd-pic:
	python3 -m unittest discover -s tests -p "windowd_pic_test.py" -v

.PHONY: test-native-scroll test-native-scroll-sanitize
test-native-scroll:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_SCROLL tests/suite_apps.c -o $(BUILD)/tests/native_scroll_test
	$(BUILD)/tests/native_scroll_test
test-native-scroll-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_SCROLL tests/suite_apps.c -o $(BUILD)/tests/native_scroll_sanitized
	$(BUILD)/tests/native_scroll_sanitized

.PHONY: test-skin test-skin-sanitize
test-skin: $(BUILD)/skin/aurora.oxs
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_SKIN tests/suite_graphics.c apps/native/skin.c -o $(BUILD)/tests/skin_test
	$(BUILD)/tests/skin_test $(BUILD)/skin/aurora.oxs
test-skin-sanitize: $(BUILD)/skin/aurora.oxs
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_SKIN tests/suite_graphics.c apps/native/skin.c -o $(BUILD)/tests/skin_sanitized
	$(BUILD)/tests/skin_sanitized $(BUILD)/skin/aurora.oxs

.PHONY: test-native-scene-skin test-native-scene-skin-sanitize
test-native-scene-skin: $(BUILD)/skin/aurora.oxs
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_SCENE_SKIN tests/suite_graphics.c apps/native/native_scene.c apps/native/shell_raster.c apps/native/skin.c -o $(BUILD)/tests/native_scene_skin_test
	$(BUILD)/tests/native_scene_skin_test $(BUILD)/skin/aurora.oxs
test-native-scene-skin-sanitize: $(BUILD)/skin/aurora.oxs
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_SCENE_SKIN tests/suite_graphics.c apps/native/native_scene.c apps/native/shell_raster.c apps/native/skin.c -o $(BUILD)/tests/native_scene_skin_sanitized
	$(BUILD)/tests/native_scene_skin_sanitized $(BUILD)/skin/aurora.oxs

.PHONY: test-calculator-layout test-calculator-layout-sanitize
test-calculator-layout:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_CALCULATOR_LAYOUT tests/suite_apps.c -o $(BUILD)/tests/calculator_layout_test
	$(BUILD)/tests/calculator_layout_test
test-calculator-layout-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_CALCULATOR_LAYOUT tests/suite_apps.c -o $(BUILD)/tests/calculator_layout_sanitized
	$(BUILD)/tests/calculator_layout_sanitized

.PHONY: test-native-widgets test-native-widgets-sanitize
test-native-widgets:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_WIDGETS tests/suite_apps.c -o $(BUILD)/tests/native_widgets_test
	$(BUILD)/tests/native_widgets_test
test-native-widgets-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_WIDGETS tests/suite_apps.c -o $(BUILD)/tests/native_widgets_sanitized
	$(BUILD)/tests/native_widgets_sanitized

.PHONY: test-ui-atlas
test-ui-atlas: $(BUILD)/skin/aurora-widgets.oxua $(BUILD)/skin/aurora-icons.oxua $(BUILD)/skin/aurora-chrome.oxua $(BUILD)/skin/aurora-menu.oxua $(BUILD)/skin/olive-menu.oxua $(BUILD)/skin/midnight-menu.oxua
	python3 -m unittest discover -s tests -p 'test_ui_atlases.py' -v

# Aurora 7 menu skin parser/9-slice tests.
.PHONY: test-shell-menu-art test-shell-menu-art-sanitize
test-shell-menu-art: $(BUILD)/skin/aurora-menu.oxua
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_SHELL_MENU_ART tests/suite_graphics.c apps/native/shell_menu_art.c -o $(BUILD)/tests/shell_menu_art_test
	$(BUILD)/tests/shell_menu_art_test $(BUILD)/skin/aurora-menu.oxua
test-shell-menu-art-sanitize: $(BUILD)/skin/aurora-menu.oxua
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_SHELL_MENU_ART tests/suite_graphics.c apps/native/shell_menu_art.c -o $(BUILD)/tests/shell_menu_art_sanitized
	$(BUILD)/tests/shell_menu_art_sanitized $(BUILD)/skin/aurora-menu.oxua
.PHONY: test-shell-icons test-shell-icons-sanitize
# Pure tests run without a VM, using the exact native-format atlas parser.
test-shell-icons:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_SHELL_ICONS tests/suite_graphics.c apps/native/shell_icons.c -o build/tests/shell_icons_test
	build/tests/shell_icons_test
test-shell-icons-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_SHELL_ICONS tests/suite_graphics.c apps/native/shell_icons.c -o build/tests/shell_icons_sanitized
	build/tests/shell_icons_sanitized

.PHONY: test-aurora-themes
test-aurora-themes: $(BUILD)/initrd.img $(BUILD)/skin/aurora.oxs $(BUILD)/skin/olive.oxs $(BUILD)/skin/midnight.oxs $(BUILD)/skin/aurora-widgets.oxua $(BUILD)/skin/olive-widgets.oxua $(BUILD)/skin/midnight-widgets.oxua $(BUILD)/skin/aurora-icons.oxua $(BUILD)/skin/olive-icons.oxua $(BUILD)/skin/midnight-icons.oxua $(BUILD)/skin/aurora-chrome.oxua $(BUILD)/skin/olive-chrome.oxua $(BUILD)/skin/midnight-chrome.oxua $(BUILD)/skin/midnight-menu.oxua $(BUILD)/skin/aurora-menu.oxua $(BUILD)/skin/olive-menu.oxua $(BUILD)/skin/aurora-toolbar.oxua $(BUILD)/skin/olive-toolbar.oxua $(BUILD)/skin/midnight-toolbar.oxua $(BUILD)/skin/aurora-font.oxft
	python3 -m unittest discover -s tests -p 'test_aurora_themes.py' -v

# Aurora 8 font loading and proportional glyph tests.
.PHONY: test-font-engine test-font-engine-sanitize
$(BUILD)/tests/font_engine_test: tests/suite_graphics.c apps/native/font_engine.h apps/native/shell_raster.c
	@mkdir -p $(dir $@)
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_FONT_ENGINE tests/suite_graphics.c apps/native/shell_raster.c -o $@
test-font-engine: $(BUILD)/skin/aurora-font.oxft $(BUILD)/tests/font_engine_test
	$(BUILD)/tests/font_engine_test $(BUILD)/skin/aurora-font.oxft
test-font-engine-sanitize: $(BUILD)/skin/aurora-font.oxft
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_FONT_ENGINE tests/suite_graphics.c apps/native/shell_raster.c -o $(BUILD)/tests/font_engine_sanitized
	$(BUILD)/tests/font_engine_sanitized $(BUILD)/skin/aurora-font.oxft

.PHONY: test-aurora-font-png
test-aurora-font-png: $(BUILD)/skin/aurora-font.oxft
	python3 -m unittest discover -s tests -p 'test_aurora_font.py' -v

# Aurora 9: bounded pixel-accurate label layout regression.
.PHONY: test-font-fit test-font-fit-sanitize
test-font-fit:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_FONT_FIT tests/suite_graphics.c -o $(BUILD)/tests/font_fit_test
	$(BUILD)/tests/font_fit_test

test-font-fit-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_FONT_FIT tests/suite_graphics.c -o $(BUILD)/tests/font_fit_sanitized
	$(BUILD)/tests/font_fit_sanitized

# Aurora 10 release-action regression: cancel when pointer leaves an item.
.PHONY: test-shell-interaction test-shell-interaction-sanitize
test-shell-interaction:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_SHELL_INTERACTION tests/suite_shell.c -o $(BUILD)/tests/shell_interaction_test
	$(BUILD)/tests/shell_interaction_test

test-shell-interaction-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_SHELL_INTERACTION tests/suite_shell.c -o $(BUILD)/tests/shell_interaction_sanitized
	$(BUILD)/tests/shell_interaction_sanitized

# Aurora 11 native pointer ABI and gesture state machine (host + ASan/UBSan).
.PHONY: test-native-pointer test-native-pointer-sanitize
test-native-pointer:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_POINTER tests/suite_aurora.c -o build/tests/native_pointer_test
	build/tests/native_pointer_test
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -Ikernel -DOPENXP_TEST_NATIVE_POINTER_KERNEL tests/suite_aurora.c kernel/gui/native.c kernel/video/fb.c -o build/tests/native_pointer_kernel_test
	build/tests/native_pointer_kernel_test

test-native-pointer-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_NATIVE_POINTER tests/suite_aurora.c -o build/tests/native_pointer_sanitized
	build/tests/native_pointer_sanitized
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_NATIVE_HOST_TEST -Ikernel -DOPENXP_TEST_NATIVE_POINTER_KERNEL tests/suite_aurora.c kernel/gui/native.c kernel/video/fb.c -o build/tests/native_pointer_kernel_sanitized
	build/tests/native_pointer_kernel_sanitized

# Aurora 12: pure gesture, continuous stroke, selection and thumb math.
.PHONY: test-advanced-input test-advanced-input-sanitize
test-advanced-input:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_TEST_ADVANCED_INPUT tests/suite_aurora.c -o $(BUILD)/tests/advanced_input_test
	$(BUILD)/tests/advanced_input_test
test-advanced-input-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_TEST_ADVANCED_INPUT tests/suite_aurora.c -o $(BUILD)/tests/advanced_input_sanitized
	$(BUILD)/tests/advanced_input_sanitized

# Aurora 13: bounded, cross-application clipboard + editing regression suite.
.PHONY: test-aurora13 test-aurora13-sanitize
test-aurora13:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_AURORA13_EDIT tests/suite_aurora.c -o $(BUILD)/tests/aurora13_edit_test
	$(BUILD)/tests/aurora13_edit_test
test-aurora13-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_AURORA13_EDIT tests/suite_aurora.c -o $(BUILD)/tests/aurora13_edit_sanitized
	$(BUILD)/tests/aurora13_edit_sanitized

# Aurora 14: document associations, tagged clipboard, double-click and
# Open/Save-As picker validation. Pure helpers run under host sanitizers.
.PHONY: test-aurora14 test-aurora14-sanitize
test-aurora14:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_AURORA14_WORKFLOW tests/suite_aurora.c -o $(BUILD)/tests/aurora14_workflow_test
	$(BUILD)/tests/aurora14_workflow_test
test-aurora14-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_AURORA14_WORKFLOW tests/suite_aurora.c -o $(BUILD)/tests/aurora14_workflow_sanitized
	$(BUILD)/tests/aurora14_workflow_sanitized

# Aurora 15: per-launch data arguments, picker focus, drag/drop and storage
.PHONY: test-aurora15 test-aurora15-sanitize
test-aurora15:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_AURORA15_WORKFLOW tests/suite_aurora.c -o $(BUILD)/tests/aurora15_workflow_test
	$(BUILD)/tests/aurora15_workflow_test

test-aurora15-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_AURORA15_WORKFLOW tests/suite_aurora.c -o $(BUILD)/tests/aurora15_workflow_sanitized
	$(BUILD)/tests/aurora15_workflow_sanitized

# Aurora 16: nested-folder file destinations, editor drag targets and IPC ABI.
.PHONY: test-aurora16 test-aurora16-sanitize
test-aurora16:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_AURORA16_WORKFLOW tests/suite_aurora.c -o $(BUILD)/tests/aurora16_workflow_test
	$(BUILD)/tests/aurora16_workflow_test
test-aurora16-sanitize:
	@mkdir -p $(BUILD)/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_AURORA16_WORKFLOW tests/suite_aurora.c -o $(BUILD)/tests/aurora16_workflow_sanitized
	$(BUILD)/tests/aurora16_workflow_sanitized

# Aurora 17: increased RAMFS quota, transactional subtree changes, v1/v2
# journal compatibility and full-capacity persistent roundtrip.
.PHONY: test-aurora17 test-aurora17-sanitize
test-aurora17: $(BUILD)/initrd.img
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_RAMFS tests/suite_filesystem.c kernel/fs/ramfs.c -o build/tests/aurora17_ramfs
	build/tests/aurora17_ramfs
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_VFS tests/suite_filesystem.c kernel/fs/vfs.c kernel/fs/initrd.c kernel/fs/ramfs.c -o build/tests/aurora17_vfs
	build/tests/aurora17_vfs $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -DOPENXP_PERSIST_HOST_TEST -Ikernel -DOPENXP_TEST_PERSIST tests/suite_filesystem.c kernel/fs/persist.c kernel/fs/ramfs.c -o build/tests/aurora17_persist
	build/tests/aurora17_persist
	python3 -m unittest tests/test_storage_format.py

test-aurora17-sanitize: $(BUILD)/initrd.img
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_RAMFS tests/suite_filesystem.c kernel/fs/ramfs.c -o build/tests/aurora17_ramfs_san
	build/tests/aurora17_ramfs_san
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ikernel -DOPENXP_TEST_VFS tests/suite_filesystem.c kernel/fs/vfs.c kernel/fs/initrd.c kernel/fs/ramfs.c -o build/tests/aurora17_vfs_san
	build/tests/aurora17_vfs_san $(BUILD)/initrd.img
	$(HOST_CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -DOPENXP_PERSIST_HOST_TEST -Ikernel -DOPENXP_TEST_PERSIST tests/suite_filesystem.c kernel/fs/persist.c kernel/fs/ramfs.c -o build/tests/aurora17_persist_san
	build/tests/aurora17_persist_san
