/*
 * parameters.c
 * Brandon Azad
 */
#define PARAMETERS_EXTERN
#include "parameters.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>

#include "log.h"
#include "platform.h"
#include "platform_match.h"

// ---- Initialization routines -------------------------------------------------------------------

// A struct describing an initialization.
struct initialization {
	const char *devices;
	const char *builds;
	void (*init)(void);
};

// Run initializations matching this platform.
static size_t
run_initializations(struct initialization *inits, size_t count) {
	size_t match_count = 0;
	for (size_t i = 0; i < count; i++) {
		struct initialization *init = &inits[i];
		if (platform_matches(init->devices, init->builds)) {
			init->init();
			match_count++;
		}
	}
	return match_count;
}

// A helper macro to get the number of elements in a static array.
#define ARRAY_COUNT(x)	(sizeof(x) / sizeof((x)[0]))

// ---- General system parameters -----------------------------------------------------------------

// Initialization for general system parameters.
static void
init__system_parameters() {
	STATIC_ADDRESS(kernel_base) = 0xFFFFFFF007004000;
	kernel_slide_step = 0x200000;
	message_size_for_kmsg_zone = 76;
	kmsg_zone_size = 256;
	max_ool_ports_per_message = 16382;
	gc_step = 2 * MB;
}

// A list of general system parameter initializations by platform.
static struct initialization system_parameters[] = {
	{ "*", "*", init__system_parameters },
};

// ---- Offset initialization ---------------------------------------------------------------------

// Initialization for iPhone11,8 16C50 (and similar devices).
static void
offsets__iphone11_8__16C50() {
	SIZE(ipc_entry)               = 0x18;
	OFFSET(ipc_entry, ie_object)  =  0;
	OFFSET(ipc_entry, ie_bits)    =  8;
	OFFSET(ipc_entry, ie_request) = 16;

	SIZE(ipc_port)                  =   0xa8;
	BLOCK_SIZE(ipc_port)            = 0x4000;
	OFFSET(ipc_port, ip_bits)       =   0;
	OFFSET(ipc_port, ip_references) =   4;
	OFFSET(ipc_port, waitq_flags)   =  24;
	OFFSET(ipc_port, imq_messages)  =  64;
	OFFSET(ipc_port, imq_msgcount)  =  80;
	OFFSET(ipc_port, imq_qlimit)    =  82;
	OFFSET(ipc_port, ip_receiver)   =  96;
	OFFSET(ipc_port, ip_kobject)    = 104;
	OFFSET(ipc_port, ip_nsrequest)  = 112;
	OFFSET(ipc_port, ip_requests)   = 128;
	OFFSET(ipc_port, ip_mscount)    = 156;
	OFFSET(ipc_port, ip_srights)    = 160;

	SIZE(ipc_port_request)                = 0x10;
	OFFSET(ipc_port_request, ipr_soright) = 0;

	OFFSET(ipc_space, is_table_size) = 0x14;
	OFFSET(ipc_space, is_table)      = 0x20;

	SIZE(ipc_voucher)       =   0x50;
	BLOCK_SIZE(ipc_voucher) = 0x4000;

	OFFSET(proc, p_pid)   = 0x60;
	OFFSET(proc, p_ucred) = 0xf8;

	SIZE(sysctl_oid)                = 0x50;
	OFFSET(sysctl_oid, oid_parent)  =  0x0;
	OFFSET(sysctl_oid, oid_link)    =  0x8;
	OFFSET(sysctl_oid, oid_kind)    = 0x14;
	OFFSET(sysctl_oid, oid_handler) = 0x30;
	OFFSET(sysctl_oid, oid_version) = 0x48;
	OFFSET(sysctl_oid, oid_refcnt)  = 0x4c;

	OFFSET(task, lck_mtx_type) =   0xb;
	OFFSET(task, ref_count)    =  0x10;
	OFFSET(task, active)       =  0x14;
	OFFSET(task, map)          =  0x20;
	OFFSET(task, itk_space)    = 0x300;
	OFFSET(task, bsd_info)     = 0x368;
}

// Initialization for iPhone10,1 16B92 (and similar devices).
static void
offsets__iphone10_1__16B92() {
	offsets__iphone11_8__16C50();

	OFFSET(task, bsd_info) = 0x358;
}

// Initialize offset parameters whose values are computed from other parameters.
static void
initialize_computed_offsets() {
	COUNT_PER_BLOCK(ipc_port) = BLOCK_SIZE(ipc_port) / SIZE(ipc_port);
	COUNT_PER_BLOCK(ipc_voucher) = BLOCK_SIZE(ipc_voucher) / SIZE(ipc_voucher);
}

// A list of offset initializations by platform.
static struct initialization offsets[] = {
    { "", "", NULL },
	{ "*", "*", initialize_computed_offsets },
};

// The minimum number of offsets that must match in order to declare a platform initialized.
static const size_t min_offsets = 2;

// ---- Public API --------------------------------------------------------------------------------

bool
parameters_init() {
	platform_init();
    char *initf = "offsets__iphone10_1__16B92";
    struct initialization offs = { platform.machine, platform.osversion, offsets__iphone10_1__16B92 };
    const char *eleven_eight[] =  { "iPhone11," };
    for (int i = 0; i < ARRAY_COUNT(eleven_eight); i++) {
        if (strstr(platform.machine, eleven_eight[i]) != NULL) {
            offs.init = offsets__iphone11_8__16C50;
            initf = "offsets__iphone11_8__16C50";
            break;
        }
    }
    offsets[0] = offs;
    printf("using offsets: { \"%s\", \"%s\", %s }\nif this doesn't work, add \"%s\" to the eleven_eight char array and make a new issue on github.com/alticha/voucher_swap\n", platform.machine, platform.osversion, initf, platform.machine);
	run_initializations(system_parameters, ARRAY_COUNT(system_parameters));
	size_t count = run_initializations(offsets, ARRAY_COUNT(offsets));
	if (count < min_offsets) {
		ERROR("somehow, there are no offsets for %s %s", platform.machine, platform.osversion);
		return false;
	}
	return true;
}
