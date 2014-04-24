/*
 * Copyright 2014, Jessica Hamilton, jessica.l.hamilton@gmail.com.
 * Distributed under the terms of the MIT License.
 */


#include "efi_platform.h"

#include <SupportDefs.h>
#include <arch/x86/arch_acpi.h>
#include <boot/platform.h>
#include <boot/stdio.h>

#include <string.h>


#define TRACE_ACPI
#ifdef TRACE_ACPI
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif


extern status_t
acpi_check_rsdt(acpi_rsdp* rsdp);

void
acpi_init()
{
	EFI_GUID acpi = ACPI_20_TABLE_GUID;
	EFI_CONFIGURATION_TABLE *table = kSystemTable->ConfigurationTable;
	UINTN entries = kSystemTable->NumberOfTableEntries;

	// Try to find the ACPI RSDP.
	for (uint32 i = 0; i < entries; i++) {
		acpi_rsdp *rsdp = NULL;

		EFI_GUID vendor = table[i].VendorGuid;

		if (vendor.Data1 == acpi.Data1
			&& vendor.Data2 == acpi.Data2
			&& vendor.Data3 == acpi.Data3
			&& strncmp((char *)vendor.Data4, (char *)acpi.Data4, 8) == 0) {
			rsdp = (acpi_rsdp *)(table[i].VendorTable);
			if (strncmp((char *)rsdp, ACPI_RSDP_SIGNATURE, 8) == 0)
				TRACE(("acpi_init: found ACPI RSDP signature at %p\n", rsdp));

			if (rsdp != NULL && acpi_check_rsdt(rsdp) == B_OK)
				break;
		}
	}
}
