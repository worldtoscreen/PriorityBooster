#include <ntifs.h>
#include <ntddk.h>
#include "functions.h"       // declared functions
#include "communication.h"   // communication structures

// Note: if DriverEntry does not return STATUS_SUCCESS, the DriverUnload routine is not called. That means we
// have to clean up everything done inside of the function.
extern "C" NTSTATUS
DriverEntry( _In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING )
{
	DriverObject->DriverUnload = PriorityBoosterUnload;

	/* Setup Dispatch Routines */
	// All drivers must support IRL_MJ_CREATE & IRP_MJ_CLOSE so handles to the driver can be opened and closed...
	// These functions are called when a usermode process attempts to open or close a handle to the driver.
	// The Create & Close functions are the same for this driver, but may be different for others
	DriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;

	// Our DeviceIoControl callback function
	// Called when a usermode process uses DeviceIoControl to communicate with the driver.
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;

/*
	NTSTATUS
		IoCreateDevice(
		[in]           PDRIVER_OBJECT  DriverObject,
		[in]           ULONG           DeviceExtensionSize,
		[in, optional] PUNICODE_STRING DeviceName,
		[in]           DEVICE_TYPE     DeviceType,
		[in]           ULONG           DeviceCharacteristics,
		[in]           BOOLEAN         Exclusive,
		[out]          PDEVICE_OBJECT  *DeviceObject
	);
*/

	// A string to hold our internal device name
	// The devie name can be anything but idealy is under the \\Device directory
	UNICODE_STRING InternalDeviceName = RTL_CONSTANT_STRING( L"\\Device\\PriorityBooster" );

	// Create the device object. The IO manager will automatically assign DriverObject->DeviceObject to the result, if the function succeeded.
	PDEVICE_OBJECT DeviceObject;
	NTSTATUS status = IoCreateDevice(
		DriverObject,                    // Our DriverObject,
		0,				 // no need for extra bytes
		&InternalDeviceName,			 // Our device name
		FILE_DEVICE_UNKNOWN,	 // Device type, usually unknown for software drivers
		0,				 // charactaristic flags, not needed in this case
		FALSE,                  // can not be opened by multiple clients
		&DeviceObject                    // Our device object
	);

	if ( !NT_SUCCESS( status ) )
	{
		KdPrint( ( "Failed to create device object (0x%08x)\n", status ) );
		return status;
	}

	// Now we have a pointer to our DeviceObject. We can now make this object accessible to user mode callers
	// by providing something called a symbolic link. This is essentially just a name that usermode can reference.
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING( L"\\??\\PriorityBooster" );

	// Have the IO manager link our internal device name with our symbolic name
	status = IoCreateSymbolicLink( &SymbolicLinkName, &InternalDeviceName );

	if ( !NT_SUCCESS( status ) )
	{
		KdPrint( ( "Failed to create symbolic link (0x%08x)\n", status ) );
		IoDeleteDevice( DeviceObject ); // Delete our device object to prevent memory leaks & other issues
		return status;
	}



	return STATUS_SUCCESS;
}

void PriorityBoosterUnload( _In_ PDRIVER_OBJECT DriverObject )
{
	// Our symbolic name
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING( L"\\??\\PriorityBooster" );

	// Delete the link between the symbolic name and the internal device name by providing the symbolic name
	IoDeleteSymbolicLink( &SymbolicLinkName );

	// Finally delete the device
	IoDeleteDevice( DriverObject->DeviceObject );
}

// The dispatch routine for IRP_MJ_CREATE and IRP_MJ_CLOSE
// Every dispatch routine accepts a PDEVICE_OBJECT for the device and an PIRP / IRP* for the request (I/O Request Packet)
// Everything is wrapped in an IRP - read, write, create, close, etc.
// The purpose of the driver is to handle the IRP
_Use_decl_annotations_
NTSTATUS
PriorityBoosterCreateClose( PDEVICE_OBJECT DeviceObject, PIRP Irp )
{
	// We only have one (the driver is not exclusive) so we don't care about which DeviceObject is being passed
	UNREFERENCED_PARAMETER( DeviceObject );

	// Setup our packet for the return back to usermode.
	// Set our status to STATUS_SUCCESS
	Irp->IoStatus.Status = STATUS_SUCCESS;

	// Information is a polymorphic (pointer meant to be cast) depending on the operation.
	Irp->IoStatus.Information = 0;

	// Complete the request.
	IoCompleteRequest( Irp, IO_NO_INCREMENT );

	// We need to return the same status that was placed in our IRP's IoStatus.Status member
	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS PriorityBoosterDeviceControl( PDEVICE_OBJECT, PIRP Irp )
{
	// Get the stack location of the IRP so that we can access it's members.
	// We only have one IO_STACK_LOCATION but reguardless this is how it should be done.
	IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation( Irp );
	NTSTATUS status = STATUS_SUCCESS;
	ThreadData *data;

	switch ( stack->Parameters.DeviceIoControl.IoControlCode )
	{
		// Our defined IOCTL code for priority setting
		case IOCTL_SET_PRIORITY:
			/* Actually changing a threads priority. */

			// Check if the structure provided by the call was the correct size.
			// If that buffer was less than the size of ThreadData, interepreting it as a ThreadData structure and accessing members would cause an access violation
			if ( stack->Parameters.DeviceIoControl.InputBufferLength < sizeof( ThreadData ) )
			{
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			data = (ThreadData *)stack->Parameters.DeviceIoControl.Type3InputBuffer;
			// We cast it to a pointer (while the UM client provided a struct by reference)
			// because Type3InputBuffer is a PVOID

			// Make sure that pointer is valid
			if ( !data )
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			// Make sure that the Priority is within the valid range
			if ( data->Priority < 1 || data->Priority > 31 )
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			/*
			KPRIORITY
			KeSetPriorityThread(
				[in, out] PKTHREAD  Thread,
				[in]      KPRIORITY Priority
			);
			*/

			PETHREAD TargetThread;

			// data->ThreadID is a ULONG, but really it's a handle.
			// Essentially ULongToHandle casts the ULong to a HANDLE
			// Removed for clarity

			/*
			NTSTATUS
			PsLookupThreadByThreadId(
				[in]  HANDLE   ThreadId,
				[out] PETHREAD *Thread
			);
			*/

			status = PsLookupThreadByThreadId(
				(HANDLE)data->ThreadID,
				&TargetThread
			);

			if ( !NT_SUCCESS( status ) )
				break;
			/*
				KPRIORITY KeSetPriorityThread(
				[in, out] PKTHREAD  Thread,
				[in]      KPRIORITY Priority
				);
			*/
			KeSetPriorityThread( (PKTHREAD)TargetThread, (KPRIORITY)data->Priority );

			break;

		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
	}

	// Tell the I/O Request Packet that we're done, return a status.
	// The usermode client will recieve this information.
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );
	return status;
}