#pragma once

#define ULONG unsigned long

// This data structure will be shared between the client and the driver.
// The client will tell the driver what to do and the driver will do that thing.
// So, this is the info that the driver expects the client to provide it with.
struct ThreadData
{
	ULONG ThreadID;
	int Priority;
};

// The predefined IOCTL code for a software driver (hardware drivers use different values for this)
#define SOFTWARE_DRIVER_DEVICE_TYPE 0x8000

// The IOCTL code for setting the priority of a thread
#define IOCTL_SET_PRIORITY CTL_CODE(SOFTWARE_DRIVER_DEVICE_TYPE, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)