#include <iostream>
#include <Windows.h>
#include <stdio.h>

// Next time I'll use a shared project...
#include "../../../../../../Users//sames//source//repos//PriorityBooster//PriorityBooster//communication.h"

int main( int argc, const char *argv[] )
{
	if ( argc < 3 )
	{
		printf( "Invalid usage.\nUsage: comm.exe <threadid> <priority>\n" );
		return -1;
	}

	// Open a handle to our device.
	// This call will reach the driver in its IRP_MJ_CREATE dispatch routine (the PriorityBoosterCreateClose function)
	HANDLE hDevice = CreateFile(
		L"\\\\.\\PriorityBooster",
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr
	);

	if ( hDevice == INVALID_HANDLE_VALUE )
	{
		printf( "Failed to obtain handle to driver.\n" );
		int err = GetLastError( );
		if ( err == 2 )
		{
			printf( "Driver not found...\n" );
			return -1;
		}

		printf( "GetLastError() -> %d", GetLastError( ) );
		return -1;
	}

	ThreadData data; // the structure passed to DeviceIoControl (and to our driver)

	// Convert the string arguments into integers
	data.ThreadID = atoi( argv[1] );
	data.Priority = atoi( argv[2] );

	// Communicate with the driver.
	DWORD returned;
	BOOL success = DeviceIoControl(
		hDevice,
		IOCTL_SET_PRIORITY,
		&data, sizeof( data ),
		nullptr, 0,
		&returned, nullptr
	);

	// There should be some more checks here... I'll look into the returned argument for the IOCTL call
	if ( success )
	{
		printf( "Success!\n" );
		return 0;
	}

	printf( "Failed to communicate with driver.\n" );
	CloseHandle( hDevice );
	return -1;

}