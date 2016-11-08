/*
** Copyright 2003, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
** Distributed under the terms of the OpenBeOS License.
*/
#ifndef KERNEL_BOOT_PLATFORM_AMIGA_KERNEL_ARGS_H
#define KERNEL_BOOT_PLATFORM_AMIGA_KERNEL_ARGS_H


#ifndef KERNEL_BOOT_KERNEL_ARGS_H
#	error This file is included from <boot/kernel_args.h> only
#endif

#define SMP_MAX_CPUS 1

#define MAX_PHYSICAL_MEMORY_RANGE 4
#define MAX_PHYSICAL_ALLOCATED_RANGE 8
#define MAX_VIRTUAL_ALLOCATED_RANGE 32


typedef struct {
	uint64 _reserved[8];
} platform_kernel_args, platform_kernel_args_legacy;


#endif	/* KERNEL_BOOT_PLATFORM_AMIGA_KERNEL_ARGS_H */
