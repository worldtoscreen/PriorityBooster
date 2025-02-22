#pragma once
/* Prototypes / Declarations (simmilar) */

// Unload Routine
void PriorityBoosterUnload( _In_ PDRIVER_OBJECT DriverObject );

// All this function does is approve the request.
// If there is no function provided for the MajorFunction array (by index something) the kernel will assume the driver does not support the operation
// PIRP - Pointer to an I/O Request Packet
NTSTATUS PriorityBoosterCreateClose( _In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp );

// The driver's routine for DeviceIoControl callbacks (?)
NTSTATUS PriorityBoosterDeviceControl( _In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp );