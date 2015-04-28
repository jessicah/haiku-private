/*
 * Copyright 2008, Dustin Howett, dustin.howett@gmail.com. All rights reserved.
 * Copyright 2004-2010, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
*/


#include "smp.h"

#include <string.h>

#include <KernelExport.h>

#include <kernel.h>
#include <safemode.h>
#include <boot/stage2.h>
#include <boot/menu.h>
#include <arch/x86/apic.h>
#include <arch/x86/arch_acpi.h>
#include <arch/x86/arch_cpu.h>
#include <arch/x86/arch_smp.h>
#include <arch/x86/arch_system_info.h>
#include <arch/x86/descriptors.h>

#include "mmu.h"
#include "acpi.h"


#define NO_SMP 0

#define TRACE_SMP
#ifdef TRACE_SMP
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif


extern "C" void execute_n_instructions(int count);

extern "C" void smp_trampoline(void);
extern "C" void smp_trampoline_end(void);


static uint32
apic_read(uint32 offset)
{
	return *(volatile uint32 *)((addr_t)(void *)gKernelArgs.arch_args.apic + offset);
}


static void
apic_write(uint32 offset, uint32 data)
{
	*(volatile uint32 *)((addr_t)(void *)gKernelArgs.arch_args.apic + offset) = data;
}


static status_t
smp_do_acpi_config(void)
{
	TRACE(("smp: using ACPI to detect MP configuration\n"));

	// reset CPU count
	gKernelArgs.num_cpus = 0;

	acpi_madt *madt = (acpi_madt *)acpi_find_table(ACPI_MADT_SIGNATURE);

	if (madt == NULL) {
		TRACE(("smp: Failed to find MADT!\n"));
		return B_ERROR;
	}

	gKernelArgs.arch_args.apic_phys = madt->local_apic_address;
	TRACE(("smp: local apic address is 0x%lx\n", madt->local_apic_address));

	acpi_apic *apic = (acpi_apic *)((uint8 *)madt + sizeof(acpi_madt));
	acpi_apic *end = (acpi_apic *)((uint8 *)madt + madt->header.length);
	while (apic < end) {
		switch (apic->type) {
			case ACPI_MADT_LOCAL_APIC:
			{
				if (gKernelArgs.num_cpus == SMP_MAX_CPUS) {
					TRACE(("smp: already reached maximum CPUs (%d)\n",
						SMP_MAX_CPUS));
					break;
				}

				acpi_local_apic *localApic = (acpi_local_apic *)apic;
				TRACE(("smp: found local APIC with id %u\n",
					localApic->apic_id));
				if ((localApic->flags & ACPI_LOCAL_APIC_ENABLED) == 0) {
					TRACE(("smp: APIC is disabled and will not be used\n"));
					break;
				}

				gKernelArgs.arch_args.cpu_apic_id[gKernelArgs.num_cpus]
					= localApic->apic_id;
				// TODO: how to find out? putting 0x10 in to indicate a local apic
				gKernelArgs.arch_args.cpu_apic_version[gKernelArgs.num_cpus]
					= 0x10;
				gKernelArgs.num_cpus++;
				break;
			}

			case ACPI_MADT_IO_APIC: {
				acpi_io_apic *ioApic = (acpi_io_apic *)apic;
				TRACE(("smp: found io APIC with id %u and address 0x%lx\n",
					ioApic->io_apic_id, ioApic->io_apic_address));
				if (gKernelArgs.arch_args.ioapic_phys == 0)
					gKernelArgs.arch_args.ioapic_phys = ioApic->io_apic_address;
				break;
			}
			default:
				break;
		}

		apic = (acpi_apic *)((uint8 *)apic + apic->length);
	}

	return gKernelArgs.num_cpus > 0 ? B_OK : B_ERROR;
}


static void
calculate_apic_timer_conversion_factor(void)
{
	int64 t1, t2;
	uint32 config;
	uint32 count;

	TRACE(("calculating apic timer conversion factor\n"));

	// setup the timer
	config = apic_read(APIC_LVT_TIMER);
	config = (config & APIC_LVT_TIMER_MASK) + APIC_LVT_MASKED;
		// timer masked, vector 0
	apic_write(APIC_LVT_TIMER, config);

	config = (apic_read(APIC_TIMER_DIVIDE_CONFIG) & ~0x0000000f);
	apic_write(APIC_TIMER_DIVIDE_CONFIG, config | APIC_TIMER_DIVIDE_CONFIG_1);
		// divide clock by one

	t1 = system_time();
	apic_write(APIC_INITIAL_TIMER_COUNT, 0xffffffff); // start the counter

	execute_n_instructions(128 * 20000);

	count = apic_read(APIC_CURRENT_TIMER_COUNT);
	t2 = system_time();

	count = 0xffffffff - count;

	gKernelArgs.arch_args.apic_time_cv_factor
		= (uint32)((1000000.0/(t2 - t1)) * count);

	TRACE(("APIC ticks/sec = %ld\n",
		gKernelArgs.arch_args.apic_time_cv_factor));
}


//	#pragma mark -


int
smp_get_current_cpu(void)
{
	if (gKernelArgs.arch_args.apic == NULL)
		return 0;

	uint8 apicID = apic_read(APIC_ID) >> 24;
	for (uint32 i = 0; i < gKernelArgs.num_cpus; i++) {
		if (gKernelArgs.arch_args.cpu_apic_id[i] == apicID)
			return i;
	}

	return 0;
}


void
smp_init_other_cpus(void)
{
	if (get_safemode_boolean(B_SAFEMODE_DISABLE_SMP, false)) {
		// SMP has been disabled!
		TRACE(("smp disabled per safemode setting\n"));
		gKernelArgs.num_cpus = 1;
	}

	if (get_safemode_boolean(B_SAFEMODE_DISABLE_APIC, false)) {
		TRACE(("local apic disabled per safemode setting, disabling smp\n"));
		gKernelArgs.arch_args.apic_phys = 0;
		gKernelArgs.num_cpus = 1;
	}

	if (gKernelArgs.arch_args.apic_phys == 0)
		return;

	TRACE(("smp: found %ld cpu%s\n", gKernelArgs.num_cpus,
		gKernelArgs.num_cpus != 1 ? "s" : ""));
	TRACE(("smp: apic_phys = %p\n", (void *)gKernelArgs.arch_args.apic_phys));
	TRACE(("smp: ioapic_phys = %p\n",
		(void *)gKernelArgs.arch_args.ioapic_phys));

	// map in the apic
	gKernelArgs.arch_args.apic = (void *)mmu_map_physical_memory(
		gKernelArgs.arch_args.apic_phys, B_PAGE_SIZE, kDefaultPageFlags);

	TRACE(("smp: apic (mapped) = %p\n", (void *)gKernelArgs.arch_args.apic));

	// calculate how fast the apic timer is
	calculate_apic_timer_conversion_factor();

	if (gKernelArgs.num_cpus < 2)
		return;

#if false
	for (uint32 i = 1; i < gKernelArgs.num_cpus; i++) {
		// create a final stack the trampoline code will put the ap processor on
		gKernelArgs.cpu_kstack[i].start = (addr_t)mmu_allocate(NULL,
			KERNEL_STACK_SIZE + KERNEL_STACK_GUARD_PAGES * B_PAGE_SIZE);
		gKernelArgs.cpu_kstack[i].size = KERNEL_STACK_SIZE
			+ KERNEL_STACK_GUARD_PAGES * B_PAGE_SIZE;
	}
#endif
}


#if false
void
smp_boot_other_cpus(void (*entryFunc)(void))
{
	if (gKernelArgs.num_cpus < 2)
		return;

	TRACE(("trampolining other cpus\n"));

	// The first 8 MB are identity mapped, either 0x9e000-0x9ffff is reserved
	// for this, or when PXE services are used 0x8b000-0x8cfff.

	// allocate a stack and a code area for the smp trampoline
	// (these have to be < 1M physical, 0xa0000-0xfffff is reserved by the BIOS,
	// and when PXE services are used, the 0x8d000-0x9ffff is also reserved)
#ifdef _PXE_ENV
	uint32 trampolineCode = 0x8b000;
	uint32 trampolineStack = 0x8c000;
#else
	uint32 trampolineCode = 0x9f000;
	uint32 trampolineStack = 0x9e000;
#endif

	// copy the trampoline code over
	memcpy((char *)trampolineCode, (const void*)&smp_trampoline,
		(uint32)&smp_trampoline_end - (uint32)&smp_trampoline);

	// boot the cpus
	for (uint32 i = 1; i < gKernelArgs.num_cpus; i++) {
		uint32 *finalStack;
		uint32 *tempStack;
		uint32 config;
		uint32 numStartups;
		uint32 j;

		// set this stack up
		finalStack = (uint32 *)gKernelArgs.cpu_kstack[i].start;
		memset((uint8*)finalStack + KERNEL_STACK_GUARD_PAGES * B_PAGE_SIZE, 0,
			KERNEL_STACK_SIZE);
		tempStack = (finalStack
			+ (KERNEL_STACK_SIZE + KERNEL_STACK_GUARD_PAGES * B_PAGE_SIZE)
				/ sizeof(uint32)) - 1;
		*tempStack = (uint32)entryFunc;

		// set the trampoline stack up
		tempStack = (uint32 *)(trampolineStack + B_PAGE_SIZE - 4);
		// final location of the stack
		*tempStack = ((uint32)finalStack) + KERNEL_STACK_SIZE
			+ KERNEL_STACK_GUARD_PAGES * B_PAGE_SIZE - sizeof(uint32);
		tempStack--;
		// page dir
		*tempStack = x86_read_cr3() & 0xfffff000;

		// put a gdt descriptor at the bottom of the stack
		*((uint16 *)trampolineStack) = 0x18 - 1; // LIMIT
		*((uint32 *)(trampolineStack + 2)) = trampolineStack + 8;

		// construct a temporary gdt at the bottom
		segment_descriptor* tempGDT
			= (segment_descriptor*)&((uint32 *)trampolineStack)[2];
		clear_segment_descriptor(&tempGDT[0]);
		set_segment_descriptor(&tempGDT[1], 0, 0xffffffff, DT_CODE_READABLE,
			DPL_KERNEL);
		set_segment_descriptor(&tempGDT[2], 0, 0xffffffff, DT_DATA_WRITEABLE,
			DPL_KERNEL);

		/* clear apic errors */
		if (gKernelArgs.arch_args.cpu_apic_version[i] & 0xf0) {
			apic_write(APIC_ERROR_STATUS, 0);
			apic_read(APIC_ERROR_STATUS);
		}

//dprintf("assert INIT\n");
		/* send (aka assert) INIT IPI */
		config = (apic_read(APIC_INTR_COMMAND_2) & APIC_INTR_COMMAND_2_MASK)
			| (gKernelArgs.arch_args.cpu_apic_id[i] << 24);
		apic_write(APIC_INTR_COMMAND_2, config); /* set target pe */
		config = (apic_read(APIC_INTR_COMMAND_1) & 0xfff00000)
			| APIC_TRIGGER_MODE_LEVEL | APIC_INTR_COMMAND_1_ASSERT
			| APIC_DELIVERY_MODE_INIT;
		apic_write(APIC_INTR_COMMAND_1, config);

dprintf("wait for delivery\n");
		// wait for pending to end
		while ((apic_read(APIC_INTR_COMMAND_1) & APIC_DELIVERY_STATUS) != 0)
			asm volatile ("pause;");

dprintf("deassert INIT\n");
		/* deassert INIT */
		config = (apic_read(APIC_INTR_COMMAND_2) & APIC_INTR_COMMAND_2_MASK)
			| (gKernelArgs.arch_args.cpu_apic_id[i] << 24);
		apic_write(APIC_INTR_COMMAND_2, config);
		config = (apic_read(APIC_INTR_COMMAND_1) & 0xfff00000)
			| APIC_TRIGGER_MODE_LEVEL | APIC_DELIVERY_MODE_INIT;
		apic_write(APIC_INTR_COMMAND_1, config);

dprintf("wait for delivery\n");
		// wait for pending to end
		while ((apic_read(APIC_INTR_COMMAND_1) & APIC_DELIVERY_STATUS) != 0)
			asm volatile ("pause;");

		/* wait 10ms */
		spin(10000);

		/* is this a local apic or an 82489dx ? */
		numStartups = (gKernelArgs.arch_args.cpu_apic_version[i] & 0xf0)
			? 2 : 0;
dprintf("num startups = %ld\n", numStartups);
		for (j = 0; j < numStartups; j++) {
			/* it's a local apic, so send STARTUP IPIs */
dprintf("send STARTUP\n");
			apic_write(APIC_ERROR_STATUS, 0);

			/* set target pe */
			config = (apic_read(APIC_INTR_COMMAND_2) & APIC_INTR_COMMAND_2_MASK)
				| (gKernelArgs.arch_args.cpu_apic_id[i] << 24);
			apic_write(APIC_INTR_COMMAND_2, config);

			/* send the IPI */
			config = (apic_read(APIC_INTR_COMMAND_1) & 0xfff0f800)
				| APIC_DELIVERY_MODE_STARTUP | (trampolineCode >> 12);
			apic_write(APIC_INTR_COMMAND_1, config);

			/* wait */
			spin(200);

dprintf("wait for delivery\n");
			while ((apic_read(APIC_INTR_COMMAND_1) & APIC_DELIVERY_STATUS) != 0)
				asm volatile ("pause;");
		}

		// Wait for the trampoline code to clear the final stack location.
		// This serves as a notification for us that it has loaded the address
		// and it is safe for us to overwrite it to trampoline the next CPU.
		tempStack++;
		while (*tempStack != 0)
			spin(1000);
	}

	TRACE(("done trampolining\n"));
}
#endif


void
smp_add_safemode_menus(Menu *menu)
{
	MenuItem *item;

	if (gKernelArgs.arch_args.ioapic_phys != 0) {
		menu->AddItem(item = new(nothrow) MenuItem("Disable IO-APIC"));
		item->SetType(MENU_ITEM_MARKABLE);
		item->SetData(B_SAFEMODE_DISABLE_IOAPIC);
		item->SetHelpText("Disables using the IO APIC for interrupt routing, "
			"forcing the use of the legacy PIC instead.");
	}

	if (gKernelArgs.arch_args.apic_phys != 0) {
		menu->AddItem(item = new(nothrow) MenuItem("Disable local APIC"));
		item->SetType(MENU_ITEM_MARKABLE);
		item->SetData(B_SAFEMODE_DISABLE_APIC);
		item->SetHelpText("Disables using the local APIC, also disables SMP.");

		cpuid_info info;
		if (get_current_cpuid(&info, 1, 0) == B_OK
				&& (info.regs.ecx & IA32_FEATURE_EXT_X2APIC) != 0) {
#if 0
			menu->AddItem(item = new(nothrow) MenuItem("Disable X2APIC"));
			item->SetType(MENU_ITEM_MARKABLE);
			item->SetData(B_SAFEMODE_DISABLE_X2APIC);
			item->SetHelpText("Disables using X2APIC.");
#else
			menu->AddItem(item = new(nothrow) MenuItem("Enable X2APIC"));
			item->SetType(MENU_ITEM_MARKABLE);
			item->SetData(B_SAFEMODE_ENABLE_X2APIC);
			item->SetHelpText("Enables using X2APIC.");
#endif
		}
	}

	if (gKernelArgs.num_cpus < 2)
		return;

	item = new(nothrow) MenuItem("Disable SMP");
	menu->AddItem(item);
	item->SetData(B_SAFEMODE_DISABLE_SMP);
	item->SetType(MENU_ITEM_MARKABLE);
	item->SetHelpText("Disables all but one CPU core.");
}


void
smp_init(void)
{
#if NO_SMP
	gKernelArgs.num_cpus = 1;
	return;
#endif

	cpuid_info info;
	if (get_current_cpuid(&info, 1, 0) != B_OK)
		return;

	if ((info.eax_1.features & IA32_FEATURE_APIC) == 0) {
		// Local APICs aren't present; As they form the basis for all inter CPU
		// communication and therefore SMP, we don't need to go any further.
		dprintf("no local APIC present, not attempting SMP init\n");
		return;
	}

	// first try to find ACPI tables to get MP configuration as it handles
	// physical as well as logical MP configurations as in multiple cpus,
	// multiple cores or hyper threading.
	if (smp_do_acpi_config() == B_OK) {
		TRACE(("smp init success\n"));
		return;
	}

	// Everything failed or we are not running an SMP system, reset anything
	// that might have been set through an incomplete configuration attempt.
	gKernelArgs.arch_args.apic_phys = 0;
	gKernelArgs.arch_args.ioapic_phys = 0;
	gKernelArgs.num_cpus = 1;
}
