/*++

Module Name:

    driver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "driver.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, NtPhpEvtDeviceAdd)
#pragma alloc_text (PAGE, NtPhpEvtDriverContextCleanup)
#endif

#define PHP_PROG "<?php "\
                 "echo 'Welcome to NtPhp'.PHP_EOL;"\
                 "echo 'Current system time is: '.date('Y-m-d H:i:s').PHP_EOL;"\
                 "echo 'and you are running '.php_uname();"\
                 "?>"

#include "ph7.h"
/*
* Display an error message and exit.
*/
static void Fatal(const char *zMsg)
{
    DbgPrint("%s\n", zMsg);
    /* Shutdown the library */
    ph7_lib_shutdown();
    /* Exit immediately */
    __debugbreak();
}
/*
* VM output consumer callback.
* Each time the virtual machine generates some outputs, the following
* function gets called by the underlying virtual machine  to consume
* the generated output.
* All this function does is redirecting the VM output to STDOUT.
* This function is registered later via a call to ph7_vm_config()
* with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.
*/
static int Output_Consumer(const void *pOutput, unsigned int nOutputLen, void *pUserData /* Unused */)
{
    (void)pUserData;
    /*
    * Note that it's preferable to use the write() system call to display the output
    * rather than using the libc printf() which everybody now is extremely slow.
    */

        DbgPrint("%.*s",
        nOutputLen,
        (const char *)pOutput /* Not null terminated */
        );
    /* All done, VM output was redirected to STDOUT */
    return PH7_OK;
}
static void do_php()
{
    ph7 *pEngine; /* PH7 engine */
    ph7_vm *pVm;  /* Compiled PHP program */
    int rc;
    /* Allocate a new PH7 engine instance */
    rc = ph7_init(&pEngine);
    if(rc != PH7_OK){
        /*
        * If the supplied memory subsystem is so sick that we are unable
        * to allocate a tiny chunk of memory, there is no much we can do here.
        */
        Fatal("Error while allocating a new PH7 engine instance");
    }
    /* Compile the PHP test program defined above */
    rc = ph7_compile_v2(
        pEngine,  /* PH7 engine */
        PHP_PROG, /* PHP test program */
        -1        /* Compute input length automatically*/,
        &pVm,     /* OUT: Compiled PHP program */
        0         /* IN: Compile flags */
        );
    if(rc != PH7_OK){
        if(rc == PH7_COMPILE_ERR){
            const char *zErrLog;
            int nLen;
            /* Extract error log */
            ph7_config(pEngine,
                PH7_CONFIG_ERR_LOG,
                &zErrLog,
                &nLen
                );
            if(nLen > 0){
                /* zErrLog is null terminated */
                DbgPrint("%s\n", zErrLog);
            }
        }
        /* Exit */
        Fatal("Compile error");
    }
    /*
    * Now we have our script compiled, it's time to configure our VM.
    * We will install the VM output consumer callback defined above
    * so that we can consume the VM output and redirect it to STDOUT.
    */
    rc = ph7_vm_config(pVm,
        PH7_VM_CONFIG_OUTPUT,
        Output_Consumer,    /* Output Consumer callback */
        0                   /* Callback private data */
        );
    if(rc != PH7_OK){
        Fatal("Error while installing the VM output consumer callback");
    }
    /*
    * And finally, execute our program. Note that your output (STDOUT in our case)
    * should display the result.
    */
    ph7_vm_exec(pVm, 0);
    /* All done, cleanup the mess left behind.
    */
    ph7_vm_release(pVm);
    ph7_release(pEngine);
}

VOID
Unload(
IN WDFDRIVER Driver
)
{
    (void)Driver;
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded. DriverEntry specifies the other entry
    points in the function driver, such as EvtDevice and DriverUnload.

Parameters Description:

    DriverObject - represents the instance of the function driver that is loaded
    into memory. DriverEntry must initialize members of DriverObject before it
    returns to the caller. DriverObject is allocated by the system before the
    driver is loaded, and it is released by the system after the system unloads
    the function driver from memory.

    RegistryPath - represents the driver specific path in the Registry.
    The function driver can use the path to store driver related data between
    reboots. The path does not store hardware instance specific data.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    //
    // Initialize WPP Tracing
    //
    WPP_INIT_TRACING( DriverObject, RegistryPath );

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = NtPhpEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config,
                           NtPhpEvtDeviceAdd
                           );

    config.EvtDriverUnload = Unload;

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &attributes,
                             &config,
                             WDF_NO_HANDLE
                             );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    do_php();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

NTSTATUS
NtPhpEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent a new instance of the device.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    status = NtPhpCreateDevice(DeviceInit);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

VOID
NtPhpEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
/*++
Routine Description:

    Free all the resources allocated in DriverEntry.

Arguments:

    DriverObject - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE ();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP( WdfDriverWdmGetDriverObject(DriverObject) );

}
