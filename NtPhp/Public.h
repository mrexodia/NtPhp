/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_NtPhp,
    0xc824d193,0x1ed0,0x47fc,0xb2,0x43,0xf4,0xb7,0x03,0x01,0x4b,0x55);
// {c824d193-1ed0-47fc-b243-f4b703014b55}
