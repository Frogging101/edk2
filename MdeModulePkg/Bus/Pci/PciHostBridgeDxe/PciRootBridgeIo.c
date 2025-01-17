/** @file

  PCI Root Bridge Io Protocol code.

Copyright (c) 1999 - 2016, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "PciHostBridge.h"
#include "PciRootBridge.h"
#include "PciHostResource.h"

#define NO_MAPPING  (VOID *) -1

//
// Lookup table for increment values based on transfer widths
//
UINT8 mInStride[] = {
  1, // EfiPciWidthUint8
  2, // EfiPciWidthUint16
  4, // EfiPciWidthUint32
  8, // EfiPciWidthUint64
  0, // EfiPciWidthFifoUint8
  0, // EfiPciWidthFifoUint16
  0, // EfiPciWidthFifoUint32
  0, // EfiPciWidthFifoUint64
  1, // EfiPciWidthFillUint8
  2, // EfiPciWidthFillUint16
  4, // EfiPciWidthFillUint32
  8  // EfiPciWidthFillUint64
};

//
// Lookup table for increment values based on transfer widths
//
UINT8 mOutStride[] = {
  1, // EfiPciWidthUint8
  2, // EfiPciWidthUint16
  4, // EfiPciWidthUint32
  8, // EfiPciWidthUint64
  1, // EfiPciWidthFifoUint8
  2, // EfiPciWidthFifoUint16
  4, // EfiPciWidthFifoUint32
  8, // EfiPciWidthFifoUint64
  0, // EfiPciWidthFillUint8
  0, // EfiPciWidthFillUint16
  0, // EfiPciWidthFillUint32
  0  // EfiPciWidthFillUint64
};

/**
  Construct the Pci Root Bridge instance.

  @param Bridge            The root bridge instance.
  @param HostBridgeHandle  Handle to the HostBridge.

  @return The pointer to PCI_ROOT_BRIDGE_INSTANCE just created
          or NULL if creation fails.
**/
PCI_ROOT_BRIDGE_INSTANCE *
CreateRootBridge (
  IN PCI_ROOT_BRIDGE       *Bridge,
  IN EFI_HANDLE            HostBridgeHandle
  )
{
  PCI_ROOT_BRIDGE_INSTANCE *RootBridge;
  PCI_RESOURCE_TYPE        Index;
  CHAR16                   *DevicePathStr;

  DevicePathStr = NULL;

  DEBUG ((EFI_D_INFO, "RootBridge: "));
  DEBUG ((EFI_D_INFO, "%s\n", DevicePathStr = ConvertDevicePathToText (Bridge->DevicePath, FALSE, FALSE)));
  DEBUG ((EFI_D_INFO, "Support/Attr: %lx / %lx\n", Bridge->Supports, Bridge->Attributes));
  DEBUG ((EFI_D_INFO, "  DmaAbove4G: %s\n", Bridge->DmaAbove4G ? L"Yes" : L"No"));
  DEBUG ((EFI_D_INFO, "   AllocAttr: %lx (%s%s)\n", Bridge->AllocationAttributes,
          (Bridge->AllocationAttributes & EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM) != 0 ? L"CombineMemPMem " : L"",
          (Bridge->AllocationAttributes & EFI_PCI_HOST_BRIDGE_MEM64_DECODE) != 0 ? L"Mem64Decode" : L""
          ));
  DEBUG ((EFI_D_INFO, "         Bus: %lx - %lx\n", Bridge->Bus.Base, Bridge->Bus.Limit));
  DEBUG ((EFI_D_INFO, "          Io: %lx - %lx\n", Bridge->Io.Base, Bridge->Io.Limit));
  DEBUG ((EFI_D_INFO, "         Mem: %lx - %lx\n", Bridge->Mem.Base, Bridge->Mem.Limit));
  DEBUG ((EFI_D_INFO, "  MemAbove4G: %lx - %lx\n", Bridge->MemAbove4G.Base, Bridge->MemAbove4G.Limit));
  DEBUG ((EFI_D_INFO, "        PMem: %lx - %lx\n", Bridge->PMem.Base, Bridge->PMem.Limit));
  DEBUG ((EFI_D_INFO, " PMemAbove4G: %lx - %lx\n", Bridge->PMemAbove4G.Base, Bridge->PMemAbove4G.Limit));

  //
  // Make sure Mem and MemAbove4G apertures are valid
  //
  if (Bridge->Mem.Base < Bridge->Mem.Limit) {
    ASSERT (Bridge->Mem.Limit < SIZE_4GB);
    if (Bridge->Mem.Limit >= SIZE_4GB) {
      return NULL;
    }
  }
  if (Bridge->MemAbove4G.Base < Bridge->MemAbove4G.Limit) {
    ASSERT (Bridge->MemAbove4G.Base >= SIZE_4GB);
    if (Bridge->MemAbove4G.Base < SIZE_4GB) {
      return NULL;
    }
  }
  if (Bridge->PMem.Base < Bridge->PMem.Limit) {
    ASSERT (Bridge->PMem.Limit < SIZE_4GB);
    if (Bridge->PMem.Limit >= SIZE_4GB) {
      return NULL;
    }
  }
  if (Bridge->PMemAbove4G.Base < Bridge->PMemAbove4G.Limit) {
    ASSERT (Bridge->PMemAbove4G.Base >= SIZE_4GB);
    if (Bridge->PMemAbove4G.Base < SIZE_4GB) {
      return NULL;
    }
  }

  if ((Bridge->AllocationAttributes & EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM) != 0) {
    //
    // If this bit is set, then the PCI Root Bridge does not
    // support separate windows for Non-prefetchable and Prefetchable
    // memory.
    //
    ASSERT (Bridge->PMem.Base >= Bridge->PMem.Limit);
    ASSERT (Bridge->PMemAbove4G.Base >= Bridge->PMemAbove4G.Limit);
    if ((Bridge->PMem.Base < Bridge->PMem.Limit) ||
        (Bridge->PMemAbove4G.Base < Bridge->PMemAbove4G.Limit)
        ) {
      return NULL;
    }
  }

  if ((Bridge->AllocationAttributes & EFI_PCI_HOST_BRIDGE_MEM64_DECODE) == 0) {
    //
    // If this bit is not set, then the PCI Root Bridge does not support
    // 64 bit memory windows.
    //
    ASSERT (Bridge->MemAbove4G.Base >= Bridge->MemAbove4G.Limit);
    ASSERT (Bridge->PMemAbove4G.Base >= Bridge->PMemAbove4G.Limit);
    if ((Bridge->MemAbove4G.Base < Bridge->MemAbove4G.Limit) ||
        (Bridge->PMemAbove4G.Base < Bridge->PMemAbove4G.Limit)
        ) {
      return NULL;
    }
  }

  RootBridge = AllocateZeroPool (sizeof (PCI_ROOT_BRIDGE_INSTANCE));
  ASSERT (RootBridge != NULL);

  RootBridge->Signature = PCI_ROOT_BRIDGE_SIGNATURE;
  RootBridge->Supports = Bridge->Supports;
  RootBridge->Attributes = Bridge->Attributes;
  RootBridge->DmaAbove4G = Bridge->DmaAbove4G;
  RootBridge->AllocationAttributes = Bridge->AllocationAttributes;
  RootBridge->DevicePath = DuplicateDevicePath (Bridge->DevicePath);
  RootBridge->DevicePathStr = DevicePathStr;
  RootBridge->ConfigBuffer = AllocatePool (
    TypeMax * sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR)
    );
  ASSERT (RootBridge->ConfigBuffer != NULL);
  InitializeListHead (&RootBridge->Maps);

  CopyMem (&RootBridge->Bus, &Bridge->Bus, sizeof (PCI_ROOT_BRIDGE_APERTURE));
  CopyMem (&RootBridge->Io, &Bridge->Io, sizeof (PCI_ROOT_BRIDGE_APERTURE));
  CopyMem (&RootBridge->Mem, &Bridge->Mem, sizeof (PCI_ROOT_BRIDGE_APERTURE));
  CopyMem (&RootBridge->MemAbove4G, &Bridge->MemAbove4G, sizeof (PCI_ROOT_BRIDGE_APERTURE));


  for (Index = TypeIo; Index < TypeMax; Index++) {
    RootBridge->ResAllocNode[Index].Type   = Index;
    RootBridge->ResAllocNode[Index].Base   = 0;
    RootBridge->ResAllocNode[Index].Length = 0;
    RootBridge->ResAllocNode[Index].Status = ResNone;
  }

  RootBridge->RootBridgeIo.SegmentNumber  = Bridge->Segment;
  RootBridge->RootBridgeIo.ParentHandle   = HostBridgeHandle;
  RootBridge->RootBridgeIo.PollMem        = RootBridgeIoPollMem;
  RootBridge->RootBridgeIo.PollIo         = RootBridgeIoPollIo;
  RootBridge->RootBridgeIo.Mem.Read       = RootBridgeIoMemRead;
  RootBridge->RootBridgeIo.Mem.Write      = RootBridgeIoMemWrite;
  RootBridge->RootBridgeIo.Io.Read        = RootBridgeIoIoRead;
  RootBridge->RootBridgeIo.Io.Write       = RootBridgeIoIoWrite;
  RootBridge->RootBridgeIo.CopyMem        = RootBridgeIoCopyMem;
  RootBridge->RootBridgeIo.Pci.Read       = RootBridgeIoPciRead;
  RootBridge->RootBridgeIo.Pci.Write      = RootBridgeIoPciWrite;
  RootBridge->RootBridgeIo.Map            = RootBridgeIoMap;
  RootBridge->RootBridgeIo.Unmap          = RootBridgeIoUnmap;
  RootBridge->RootBridgeIo.AllocateBuffer = RootBridgeIoAllocateBuffer;
  RootBridge->RootBridgeIo.FreeBuffer     = RootBridgeIoFreeBuffer;
  RootBridge->RootBridgeIo.Flush          = RootBridgeIoFlush;
  RootBridge->RootBridgeIo.GetAttributes  = RootBridgeIoGetAttributes;
  RootBridge->RootBridgeIo.SetAttributes  = RootBridgeIoSetAttributes;
  RootBridge->RootBridgeIo.Configuration  = RootBridgeIoConfiguration;

  return RootBridge;
}

/**
  Check parameters for IO,MMIO,PCI read/write services of PCI Root Bridge IO.

  The I/O operations are carried out exactly as requested. The caller is
  responsible for satisfying any alignment and I/O width restrictions that a PI
  System on a platform might require. For example on some platforms, width
  requests of EfiCpuIoWidthUint64 do not work. Misaligned buffers, on the other
  hand, will be handled by the driver.

  @param[in] This           A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.

  @param[in] OperationType  I/O operation type: IO/MMIO/PCI.

  @param[in] Width          Signifies the width of the I/O or Memory operation.

  @param[in] Address        The base address of the I/O operation.

  @param[in] Count          The number of I/O operations to perform. The number
                            of bytes moved is Width size * Count, starting at
                            Address.

  @param[in] Buffer         For read operations, the destination buffer to
                            store the results. For write operations, the source
                            buffer from which to write data.

  @retval EFI_SUCCESS            The parameters for this request pass the
                                 checks.

  @retval EFI_INVALID_PARAMETER  Width is invalid for this PI system.

  @retval EFI_INVALID_PARAMETER  Buffer is NULL.

  @retval EFI_UNSUPPORTED        The Buffer is not aligned for the given Width.

  @retval EFI_UNSUPPORTED        The address range specified by Address, Width,
                                 and Count is not valid for this PI system.
**/
EFI_STATUS
RootBridgeIoCheckParameter (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN OPERATION_TYPE                         OperationType,
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN UINT64                                 Address,
  IN UINTN                                  Count,
  IN VOID                                   *Buffer
  )
{
  PCI_ROOT_BRIDGE_INSTANCE                     *RootBridge;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS  *PciRbAddr;
  UINT64                                       Base;
  UINT64                                       Limit;
  UINT32                                       Size;

  //
  // Check to see if Buffer is NULL
  //
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check to see if Width is in the valid range
  //
  if ((UINT32) Width >= EfiPciWidthMaximum) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // For FIFO type, the target address won't increase during the access,
  // so treat Count as 1
  //
  if (Width >= EfiPciWidthFifoUint8 && Width <= EfiPciWidthFifoUint64) {
    Count = 1;
  }

  Width = (EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH) (Width & 0x03);
  Size  = 1 << Width;

  //
  // Check to see if Address is aligned
  //
  if ((Address & (Size - 1)) != 0) {
    return EFI_UNSUPPORTED;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);

  //
  // Check to see if any address associated with this transfer exceeds the
  // maximum allowed address.  The maximum address implied by the parameters
  // passed in is Address + Size * Count.  If the following condition is met,
  // then the transfer is not supported.
  //
  //    Address + Size * Count > Limit + 1
  //
  // Since Limit can be the maximum integer value supported by the CPU and
  // Count can also be the maximum integer value supported by the CPU, this
  // range check must be adjusted to avoid all oveflow conditions.
  //
  if (OperationType == IoOperation) {
    //
    // Allow Legacy IO access
    //
    if (Address + MultU64x32 (Count, Size) <= 0x1000) {
      if ((RootBridge->Attributes & (
           EFI_PCI_ATTRIBUTE_ISA_IO | EFI_PCI_ATTRIBUTE_VGA_PALETTE_IO | EFI_PCI_ATTRIBUTE_VGA_IO |
           EFI_PCI_ATTRIBUTE_IDE_PRIMARY_IO | EFI_PCI_ATTRIBUTE_IDE_SECONDARY_IO |
           EFI_PCI_ATTRIBUTE_ISA_IO_16 | EFI_PCI_ATTRIBUTE_VGA_PALETTE_IO_16 | EFI_PCI_ATTRIBUTE_VGA_IO_16)) != 0) {
        return EFI_SUCCESS;
      }
    }
    Base = RootBridge->Io.Base;
    Limit = RootBridge->Io.Limit;
  } else if (OperationType == MemOperation) {
    //
    // Allow Legacy MMIO access
    //
    if ((Address >= 0xA0000) && (Address + MultU64x32 (Count, Size)) <= 0xC0000) {
      if ((RootBridge->Attributes & EFI_PCI_ATTRIBUTE_VGA_MEMORY) != 0) {
        return EFI_SUCCESS;
      }
    }
    //
    // By comparing the Address against Limit we know which range to be used
    // for checking
    //
    if (Address + MultU64x32 (Count, Size) <= RootBridge->Mem.Limit + 1) {
      Base = RootBridge->Mem.Base;
      Limit = RootBridge->Mem.Limit;
    } else {
      Base = RootBridge->MemAbove4G.Base;
      Limit = RootBridge->MemAbove4G.Limit;
    }
  } else {
    PciRbAddr = (EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS*) &Address;
    if (PciRbAddr->Bus < RootBridge->Bus.Base ||
        PciRbAddr->Bus > RootBridge->Bus.Limit) {
      return EFI_INVALID_PARAMETER;
    }

    if (PciRbAddr->Device > PCI_MAX_DEVICE ||
        PciRbAddr->Function > PCI_MAX_FUNC) {
      return EFI_INVALID_PARAMETER;
    }

    if (PciRbAddr->ExtendedRegister != 0) {
      Address = PciRbAddr->ExtendedRegister;
    } else {
      Address = PciRbAddr->Register;
    }
    Base = 0;
    Limit = 0xFFF;
  }

  if (Address < Base) {
      return EFI_INVALID_PARAMETER;
  }

  if (Address + MultU64x32 (Count, Size) > Limit + 1) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**
  Polls an address in memory mapped I/O space until an exit condition is met,
  or a timeout occurs.

  This function provides a standard way to poll a PCI memory location. A PCI
  memory read operation is performed at the PCI memory address specified by
  Address for the width specified by Width. The result of this PCI memory read
  operation is stored in Result. This PCI memory read operation is repeated
  until either a timeout of Delay 100 ns units has expired, or (Result & Mask)
  is equal to Value.

  @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in]   Width     Signifies the width of the memory operations.
  @param[in]   Address   The base address of the memory operations. The caller
                         is responsible for aligning Address if required.
  @param[in]   Mask      Mask used for the polling criteria. Bytes above Width
                         in Mask are ignored. The bits in the bytes below Width
                         which are zero in Mask are ignored when polling the
                         memory address.
  @param[in]   Value     The comparison value used for the polling exit
                         criteria.
  @param[in]   Delay     The number of 100 ns units to poll. Note that timer
                         available may be of poorer granularity.
  @param[out]  Result    Pointer to the last value read from the memory
                         location.

  @retval EFI_SUCCESS            The last data returned from the access matched
                                 the poll exit criteria.
  @retval EFI_INVALID_PARAMETER  Width is invalid.
  @retval EFI_INVALID_PARAMETER  Result is NULL.
  @retval EFI_TIMEOUT            Delay expired before a match occurred.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a
                                 lack of resources.
**/

EFI_STATUS
EFIAPI
RootBridgeIoPollMem (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN  UINT64                                 Address,
  IN  UINT64                                 Mask,
  IN  UINT64                                 Value,
  IN  UINT64                                 Delay,
  OUT UINT64                                 *Result
  )
{
  EFI_STATUS  Status;
  UINT64      NumberOfTicks;
  UINT32      Remainder;

  if (Result == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((UINT32)Width > EfiPciWidthUint64) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // No matter what, always do a single poll.
  //
  Status = This->Mem.Read (This, Width, Address, 1, Result);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((*Result & Mask) == Value) {
    return EFI_SUCCESS;
  }

  if (Delay == 0) {
    return EFI_SUCCESS;

  } else {

    //
    // Determine the proper # of metronome ticks to wait for polling the
    // location.  The nuber of ticks is Roundup (Delay /
    // mMetronome->TickPeriod)+1
    // The "+1" to account for the possibility of the first tick being short
    // because we started in the middle of a tick.
    //
    // BugBug: overriding mMetronome->TickPeriod with UINT32 until Metronome
    // protocol definition is updated.
    //
    NumberOfTicks = DivU64x32Remainder (Delay, (UINT32) mMetronome->TickPeriod,
                      &Remainder);
    if (Remainder != 0) {
      NumberOfTicks += 1;
    }
    NumberOfTicks += 1;

    while (NumberOfTicks != 0) {

      mMetronome->WaitForTick (mMetronome, 1);

      Status = This->Mem.Read (This, Width, Address, 1, Result);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if ((*Result & Mask) == Value) {
        return EFI_SUCCESS;
      }

      NumberOfTicks -= 1;
    }
  }
  return EFI_TIMEOUT;
}

/**
  Reads from the I/O space of a PCI Root Bridge. Returns when either the
  polling exit criteria is satisfied or after a defined duration.

  This function provides a standard way to poll a PCI I/O location. A PCI I/O
  read operation is performed at the PCI I/O address specified by Address for
  the width specified by Width.
  The result of this PCI I/O read operation is stored in Result. This PCI I/O
  read operation is repeated until either a timeout of Delay 100 ns units has
  expired, or (Result & Mask) is equal to Value.

  @param[in] This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in] Width     Signifies the width of the I/O operations.
  @param[in] Address   The base address of the I/O operations. The caller is
                       responsible for aligning Address if required.
  @param[in] Mask      Mask used for the polling criteria. Bytes above Width in
                       Mask are ignored. The bits in the bytes below Width
                       which are zero in Mask are ignored when polling the I/O
                       address.
  @param[in] Value     The comparison value used for the polling exit criteria.
  @param[in] Delay     The number of 100 ns units to poll. Note that timer
                       available may be of poorer granularity.
  @param[out] Result   Pointer to the last value read from the memory location.

  @retval EFI_SUCCESS            The last data returned from the access matched
                                 the poll exit criteria.
  @retval EFI_INVALID_PARAMETER  Width is invalid.
  @retval EFI_INVALID_PARAMETER  Result is NULL.
  @retval EFI_TIMEOUT            Delay expired before a match occurred.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a
                                 lack of resources.
**/
EFI_STATUS
EFIAPI
RootBridgeIoPollIo (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN  UINT64                                 Address,
  IN  UINT64                                 Mask,
  IN  UINT64                                 Value,
  IN  UINT64                                 Delay,
  OUT UINT64                                 *Result
  )
{
  EFI_STATUS  Status;
  UINT64      NumberOfTicks;
  UINT32      Remainder;

  //
  // No matter what, always do a single poll.
  //

  if (Result == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((UINT32)Width > EfiPciWidthUint64) {
    return EFI_INVALID_PARAMETER;
  }

  Status = This->Io.Read (This, Width, Address, 1, Result);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  if ((*Result & Mask) == Value) {
    return EFI_SUCCESS;
  }

  if (Delay == 0) {
    return EFI_SUCCESS;

  } else {

    //
    // Determine the proper # of metronome ticks to wait for polling the
    // location.  The number of ticks is Roundup (Delay /
    // mMetronome->TickPeriod)+1
    // The "+1" to account for the possibility of the first tick being short
    // because we started in the middle of a tick.
    //
    NumberOfTicks = DivU64x32Remainder (Delay, (UINT32)mMetronome->TickPeriod,
                      &Remainder);
    if (Remainder != 0) {
      NumberOfTicks += 1;
    }
    NumberOfTicks += 1;

    while (NumberOfTicks != 0) {

      mMetronome->WaitForTick (mMetronome, 1);

      Status = This->Io.Read (This, Width, Address, 1, Result);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if ((*Result & Mask) == Value) {
        return EFI_SUCCESS;
      }

      NumberOfTicks -= 1;
    }
  }
  return EFI_TIMEOUT;
}

/**
  Enables a PCI driver to access PCI controller registers in the PCI root
  bridge memory space.

  The Mem.Read(), and Mem.Write() functions enable a driver to access PCI
  controller registers in the PCI root bridge memory space.
  The memory operations are carried out exactly as requested. The caller is
  responsible for satisfying any alignment and memory width restrictions that a
  PCI Root Bridge on a platform might require.

  @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in]   Width     Signifies the width of the memory operation.
  @param[in]   Address   The base address of the memory operation. The caller
                         is responsible for aligning the Address if required.
  @param[in]   Count     The number of memory operations to perform. Bytes
                         moved is Width size * Count, starting at Address.
  @param[out]  Buffer    For read operations, the destination buffer to store
                         the results. For write operations, the source buffer
                         to write data from.

  @retval EFI_SUCCESS            The data was read from or written to the PCI
                                 root bridge.
  @retval EFI_INVALID_PARAMETER  Width is invalid for this PCI root bridge.
  @retval EFI_INVALID_PARAMETER  Buffer is NULL.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a
                                 lack of resources.
**/
EFI_STATUS
EFIAPI
RootBridgeIoMemRead (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN     UINT64                                 Address,
  IN     UINTN                                  Count,
  OUT    VOID                                   *Buffer
  )
{
  EFI_STATUS                             Status;

  Status = RootBridgeIoCheckParameter (This, MemOperation, Width, Address,
                                       Count, Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  return mCpuIo->Mem.Read (mCpuIo, (EFI_CPU_IO_PROTOCOL_WIDTH) Width, Address, Count, Buffer);
}

/**
  Enables a PCI driver to access PCI controller registers in the PCI root
  bridge memory space.

  The Mem.Read(), and Mem.Write() functions enable a driver to access PCI
  controller registers in the PCI root bridge memory space.
  The memory operations are carried out exactly as requested. The caller is
  responsible for satisfying any alignment and memory width restrictions that a
  PCI Root Bridge on a platform might require.

  @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in]   Width     Signifies the width of the memory operation.
  @param[in]   Address   The base address of the memory operation. The caller
                         is responsible for aligning the Address if required.
  @param[in]   Count     The number of memory operations to perform. Bytes
                         moved is Width size * Count, starting at Address.
  @param[in]   Buffer    For read operations, the destination buffer to store
                         the results. For write operations, the source buffer
                         to write data from.

  @retval EFI_SUCCESS            The data was read from or written to the PCI
                                 root bridge.
  @retval EFI_INVALID_PARAMETER  Width is invalid for this PCI root bridge.
  @retval EFI_INVALID_PARAMETER  Buffer is NULL.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a
                                 lack of resources.
**/
EFI_STATUS
EFIAPI
RootBridgeIoMemWrite (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN     UINT64                                 Address,
  IN     UINTN                                  Count,
  IN     VOID                                   *Buffer
  )
{
  EFI_STATUS                             Status;

  Status = RootBridgeIoCheckParameter (This, MemOperation, Width, Address,
                                       Count, Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  return mCpuIo->Mem.Write (mCpuIo, (EFI_CPU_IO_PROTOCOL_WIDTH) Width, Address, Count, Buffer);
}

/**
  Enables a PCI driver to access PCI controller registers in the PCI root
  bridge I/O space.

  @param[in]   This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in]   Width       Signifies the width of the memory operations.
  @param[in]   Address     The base address of the I/O operation. The caller is
                           responsible for aligning the Address if required.
  @param[in]   Count       The number of I/O operations to perform. Bytes moved
                           is Width size * Count, starting at Address.
  @param[out]  Buffer      For read operations, the destination buffer to store
                           the results. For write operations, the source buffer
                           to write data from.

  @retval EFI_SUCCESS              The data was read from or written to the PCI
                                   root bridge.
  @retval EFI_INVALID_PARAMETER    Width is invalid for this PCI root bridge.
  @retval EFI_INVALID_PARAMETER    Buffer is NULL.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a
                                   lack of resources.
**/
EFI_STATUS
EFIAPI
RootBridgeIoIoRead (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN     UINT64                                 Address,
  IN     UINTN                                  Count,
  OUT    VOID                                   *Buffer
  )
{
  EFI_STATUS                                    Status;
  Status = RootBridgeIoCheckParameter (
             This, IoOperation, Width,
             Address, Count, Buffer
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  return mCpuIo->Io.Read (mCpuIo, (EFI_CPU_IO_PROTOCOL_WIDTH) Width, Address, Count, Buffer);
}

/**
  Enables a PCI driver to access PCI controller registers in the PCI root
  bridge I/O space.

  @param[in]   This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in]   Width       Signifies the width of the memory operations.
  @param[in]   Address     The base address of the I/O operation. The caller is
                           responsible for aligning the Address if required.
  @param[in]   Count       The number of I/O operations to perform. Bytes moved
                           is Width size * Count, starting at Address.
  @param[in]   Buffer      For read operations, the destination buffer to store
                           the results. For write operations, the source buffer
                           to write data from.

  @retval EFI_SUCCESS              The data was read from or written to the PCI
                                   root bridge.
  @retval EFI_INVALID_PARAMETER    Width is invalid for this PCI root bridge.
  @retval EFI_INVALID_PARAMETER    Buffer is NULL.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a
                                   lack of resources.
**/
EFI_STATUS
EFIAPI
RootBridgeIoIoWrite (
  IN       EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL         *This,
  IN       EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH   Width,
  IN       UINT64                                  Address,
  IN       UINTN                                   Count,
  IN       VOID                                    *Buffer
  )
{
  EFI_STATUS                                    Status;
  Status = RootBridgeIoCheckParameter (
             This, IoOperation, Width,
             Address, Count, Buffer
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  return mCpuIo->Io.Write (mCpuIo, (EFI_CPU_IO_PROTOCOL_WIDTH) Width, Address, Count, Buffer);
}

/**
  Enables a PCI driver to copy one region of PCI root bridge memory space to
  another region of PCI root bridge memory space.

  The CopyMem() function enables a PCI driver to copy one region of PCI root
  bridge memory space to another region of PCI root bridge memory space. This
  is especially useful for video scroll operation on a memory mapped video
  buffer.
  The memory operations are carried out exactly as requested. The caller is
  responsible for satisfying any alignment and memory width restrictions that a
  PCI root bridge on a platform might require.

  @param[in] This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
                         instance.
  @param[in] Width       Signifies the width of the memory operations.
  @param[in] DestAddress The destination address of the memory operation. The
                         caller is responsible for aligning the DestAddress if
                         required.
  @param[in] SrcAddress  The source address of the memory operation. The caller
                         is responsible for aligning the SrcAddress if
                         required.
  @param[in] Count       The number of memory operations to perform. Bytes
                         moved is Width size * Count, starting at DestAddress
                         and SrcAddress.

  @retval  EFI_SUCCESS             The data was copied from one memory region
                                   to another memory region.
  @retval  EFI_INVALID_PARAMETER   Width is invalid for this PCI root bridge.
  @retval  EFI_OUT_OF_RESOURCES    The request could not be completed due to a
                                   lack of resources.
**/
EFI_STATUS
EFIAPI
RootBridgeIoCopyMem (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL              *This,
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH        Width,
  IN UINT64                                       DestAddress,
  IN UINT64                                       SrcAddress,
  IN UINTN                                        Count
  )
{
  EFI_STATUS  Status;
  BOOLEAN     Forward;
  UINTN       Stride;
  UINTN       Index;
  UINT64      Result;

  if ((UINT32) Width > EfiPciWidthUint64) {
    return EFI_INVALID_PARAMETER;
  }

  if (DestAddress == SrcAddress) {
    return EFI_SUCCESS;
  }

  Stride = (UINTN) (1 << Width);

  Forward = TRUE;
  if ((DestAddress > SrcAddress) &&
      (DestAddress < (SrcAddress + Count * Stride))) {
    Forward = FALSE;
    SrcAddress = SrcAddress + (Count - 1) * Stride;
    DestAddress = DestAddress + (Count - 1) * Stride;
  }

  for (Index = 0; Index < Count; Index++) {
    Status = RootBridgeIoMemRead (
               This,
               Width,
               SrcAddress,
               1,
               &Result
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
    Status = RootBridgeIoMemWrite (
               This,
               Width,
               DestAddress,
               1,
               &Result
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
    if (Forward) {
      SrcAddress += Stride;
      DestAddress += Stride;
    } else {
      SrcAddress -= Stride;
      DestAddress -= Stride;
    }
  }
  return EFI_SUCCESS;
}


/**
  PCI configuration space access.

  @param This     A pointer to EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
  @param Read     TRUE indicating it's a read operation.
  @param Width    Signifies the width of the memory operation.
  @param Address  The address within the PCI configuration space
                  for the PCI controller.
  @param Count    The number of PCI configuration operations
                  to perform.
  @param Buffer   The destination buffer to store the results.

  @retval EFI_SUCCESS            The data was read/written from/to the PCI root bridge.
  @retval EFI_INVALID_PARAMETER  Invalid parameters found.
**/
EFI_STATUS
EFIAPI
RootBridgeIoPciAccess (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN     BOOLEAN                               Read,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN     UINT64                                Address,
  IN     UINTN                                 Count,
  IN OUT VOID                                  *Buffer
  )
{
  EFI_STATUS                                   Status;
  PCI_ROOT_BRIDGE_INSTANCE                     *RootBridge;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS  PciAddress;
  UINT8                                        *Uint8Buffer;
  UINT8                                        InStride;
  UINT8                                        OutStride;
  UINTN                                        Size;

  Status = RootBridgeIoCheckParameter (This, PciOperation, Width, Address, Count, Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Read Pci configuration space
  //
  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  CopyMem (&PciAddress, &Address, sizeof (PciAddress));

  if (PciAddress.ExtendedRegister == 0) {
    PciAddress.ExtendedRegister = PciAddress.Register;
  }

  Address = PCI_SEGMENT_LIB_ADDRESS (
              RootBridge->RootBridgeIo.SegmentNumber,
              PciAddress.Bus,
              PciAddress.Device,
              PciAddress.Function,
              PciAddress.ExtendedRegister
              );

  //
  // Select loop based on the width of the transfer
  //
  InStride  = mInStride[Width];
  OutStride = mOutStride[Width];
  Size      = (UINTN) (1 << (Width & 0x03));
  for (Uint8Buffer = Buffer; Count > 0; Address += InStride, Uint8Buffer += OutStride, Count--) {
    if (Read) {
      PciSegmentReadBuffer (Address, Size, Uint8Buffer);
    } else {
      PciSegmentWriteBuffer (Address, Size, Uint8Buffer);
    }
  }
  return EFI_SUCCESS;
}

/**
  Allows read from PCI configuration space.

  @param This     A pointer to EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
  @param Width    Signifies the width of the memory operation.
  @param Address  The address within the PCI configuration space
                  for the PCI controller.
  @param Count    The number of PCI configuration operations
                  to perform.
  @param Buffer   The destination buffer to store the results.

  @retval EFI_SUCCESS           The data was read from the PCI root bridge.
  @retval EFI_INVALID_PARAMETER Invalid parameters found.
**/
EFI_STATUS
EFIAPI
RootBridgeIoPciRead (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN     UINT64                                Address,
  IN     UINTN                                 Count,
  IN OUT VOID                                  *Buffer
  )
{
  return RootBridgeIoPciAccess (This, TRUE, Width, Address, Count, Buffer);
}

/**
  Allows write to PCI configuration space.

  @param This     A pointer to EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
  @param Width    Signifies the width of the memory operation.
  @param Address  The address within the PCI configuration space
                  for the PCI controller.
  @param Count    The number of PCI configuration operations
                  to perform.
  @param Buffer   The source buffer to get the results.

  @retval EFI_SUCCESS            The data was written to the PCI root bridge.
  @retval EFI_INVALID_PARAMETER  Invalid parameters found.
**/
EFI_STATUS
EFIAPI
RootBridgeIoPciWrite (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN     UINT64                                Address,
  IN     UINTN                                 Count,
  IN OUT VOID                                  *Buffer
  )
{
  return RootBridgeIoPciAccess (This, FALSE, Width, Address, Count, Buffer);
}

/**

  Provides the PCI controller-specific address needed to access
  system memory for DMA.

  @param This           A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param Operation      Indicate if the bus master is going to read or write
                        to system memory.
  @param HostAddress    The system memory address to map on the PCI controller.
  @param NumberOfBytes  On input the number of bytes to map.
                        On output the number of bytes that were mapped.
  @param DeviceAddress  The resulting map address for the bus master PCI
                        controller to use to access the system memory's HostAddress.
  @param Mapping        The value to pass to Unmap() when the bus master DMA
                        operation is complete.

  @retval EFI_SUCCESS            Success.
  @retval EFI_INVALID_PARAMETER  Invalid parameters found.
  @retval EFI_UNSUPPORTED        The HostAddress cannot be mapped as a common buffer.
  @retval EFI_DEVICE_ERROR       The System hardware could not map the requested address.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to lack of resources.

**/
EFI_STATUS
EFIAPI
RootBridgeIoMap (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL            *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_OPERATION  Operation,
  IN     VOID                                       *HostAddress,
  IN OUT UINTN                                      *NumberOfBytes,
  OUT    EFI_PHYSICAL_ADDRESS                       *DeviceAddress,
  OUT    VOID                                       **Mapping
  )
{
  EFI_STATUS                                        Status;
  PCI_ROOT_BRIDGE_INSTANCE                          *RootBridge;
  EFI_PHYSICAL_ADDRESS                              PhysicalAddress;
  MAP_INFO                                          *MapInfo;

  if (HostAddress == NULL || NumberOfBytes == NULL || DeviceAddress == NULL ||
      Mapping == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Make sure that Operation is valid
  //
  if ((UINT32) Operation >= EfiPciOperationMaximum) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);

  PhysicalAddress = (EFI_PHYSICAL_ADDRESS) (UINTN) HostAddress;
  if (!RootBridge->DmaAbove4G && ((PhysicalAddress + *NumberOfBytes) > SIZE_4GB)) {
    //
    // If the root bridge can not handle performing DMA above 4GB but
    // any part of the DMA transfer being mapped is above 4GB, then
    // map the DMA transfer to a buffer below 4GB.
    //

    if (Operation == EfiPciOperationBusMasterCommonBuffer ||
        Operation == EfiPciOperationBusMasterCommonBuffer64) {
      //
      // Common Buffer operations can not be remapped.  If the common buffer
      // if above 4GB, then it is not possible to generate a mapping, so return
      // an error.
      //
      return EFI_UNSUPPORTED;
    }

    //
    // Allocate a MAP_INFO structure to remember the mapping when Unmap() is
    // called later.
    //
    MapInfo = AllocatePool (sizeof (MAP_INFO));
    if (MapInfo == NULL) {
      *NumberOfBytes = 0;
      return EFI_OUT_OF_RESOURCES;
    }

    //
    // Initialize the MAP_INFO structure
    //
    MapInfo->Signature         = MAP_INFO_SIGNATURE;
    MapInfo->Operation         = Operation;
    MapInfo->NumberOfBytes     = *NumberOfBytes;
    MapInfo->NumberOfPages     = EFI_SIZE_TO_PAGES (MapInfo->NumberOfBytes);
    MapInfo->HostAddress       = PhysicalAddress;
    MapInfo->MappedHostAddress = SIZE_4GB - 1;

    //
    // Allocate a buffer below 4GB to map the transfer to.
    //
    Status = gBS->AllocatePages (
                    AllocateMaxAddress,
                    EfiBootServicesData,
                    MapInfo->NumberOfPages,
                    &MapInfo->MappedHostAddress
                    );
    if (EFI_ERROR (Status)) {
      FreePool (MapInfo);
      *NumberOfBytes = 0;
      return Status;
    }

    //
    // If this is a read operation from the Bus Master's point of view,
    // then copy the contents of the real buffer into the mapped buffer
    // so the Bus Master can read the contents of the real buffer.
    //
    if (Operation == EfiPciOperationBusMasterRead ||
        Operation == EfiPciOperationBusMasterRead64) {
      CopyMem (
        (VOID *) (UINTN) MapInfo->MappedHostAddress,
        (VOID *) (UINTN) MapInfo->HostAddress,
        MapInfo->NumberOfBytes
        );
    }

    InsertTailList (&RootBridge->Maps, &MapInfo->Link);

    //
    // The DeviceAddress is the address of the maped buffer below 4GB
    //
    *DeviceAddress = MapInfo->MappedHostAddress;
    //
    // Return a pointer to the MAP_INFO structure in Mapping
    //
    *Mapping       = MapInfo;
  } else {
    //
    // If the root bridge CAN handle performing DMA above 4GB or
    // the transfer is below 4GB, so the DeviceAddress is simply the
    // HostAddress
    //
    *DeviceAddress = PhysicalAddress;
    *Mapping       = NO_MAPPING;
  }

  return EFI_SUCCESS;
}

/**
  Completes the Map() operation and releases any corresponding resources.

  The Unmap() function completes the Map() operation and releases any
  corresponding resources.
  If the operation was an EfiPciOperationBusMasterWrite or
  EfiPciOperationBusMasterWrite64, the data is committed to the target system
  memory.
  Any resources used for the mapping are freed.

  @param[in] This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in] Mapping   The mapping value returned from Map().

  @retval EFI_SUCCESS            The range was unmapped.
  @retval EFI_INVALID_PARAMETER  Mapping is not a value that was returned by Map().
  @retval EFI_DEVICE_ERROR       The data was not committed to the target system memory.
**/
EFI_STATUS
EFIAPI
RootBridgeIoUnmap (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  IN VOID                             *Mapping
  )
{
  MAP_INFO                 *MapInfo;
  LIST_ENTRY               *Link;
  PCI_ROOT_BRIDGE_INSTANCE *RootBridge;

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  //
  // See if the Map() operation associated with this Unmap() required a mapping
  // buffer. If a mapping buffer was not required, then this function simply
  // returns EFI_SUCCESS.
  //
  if (Mapping == NO_MAPPING) {
    return EFI_SUCCESS;
  }

  MapInfo = NO_MAPPING;
  for (Link = GetFirstNode (&RootBridge->Maps)
       ; !IsNull (&RootBridge->Maps, Link)
       ; Link = GetNextNode (&RootBridge->Maps, Link)
       ) {
    MapInfo = MAP_INFO_FROM_LINK (Link);
    if (MapInfo == Mapping) {
      break;
    }
  }
  //
  // Mapping is not a valid value returned by Map()
  //
  if (MapInfo != Mapping) {
    return EFI_INVALID_PARAMETER;
  }
  RemoveEntryList (&MapInfo->Link);

  //
  // If this is a write operation from the Bus Master's point of view,
  // then copy the contents of the mapped buffer into the real buffer
  // so the processor can read the contents of the real buffer.
  //
  if (MapInfo->Operation == EfiPciOperationBusMasterWrite ||
      MapInfo->Operation == EfiPciOperationBusMasterWrite64) {
    CopyMem (
      (VOID *) (UINTN) MapInfo->HostAddress,
      (VOID *) (UINTN) MapInfo->MappedHostAddress,
      MapInfo->NumberOfBytes
      );
  }

  //
  // Free the mapped buffer and the MAP_INFO structure.
  //
  gBS->FreePages (MapInfo->MappedHostAddress, MapInfo->NumberOfPages);
  FreePool (Mapping);
  return EFI_SUCCESS;
}

/**
  Allocates pages that are suitable for an EfiPciOperationBusMasterCommonBuffer
  or EfiPciOperationBusMasterCommonBuffer64 mapping.

  @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param Type        This parameter is not used and must be ignored.
  @param MemoryType  The type of memory to allocate, EfiBootServicesData or
                     EfiRuntimeServicesData.
  @param Pages       The number of pages to allocate.
  @param HostAddress A pointer to store the base system memory address of the
                     allocated range.
  @param Attributes  The requested bit mask of attributes for the allocated
                     range. Only the attributes
                     EFI_PCI_ATTRIBUTE_MEMORY_WRITE_COMBINE,
                     EFI_PCI_ATTRIBUTE_MEMORY_CACHED, and
                     EFI_PCI_ATTRIBUTE_DUAL_ADDRESS_CYCLE may be used with this
                     function.

  @retval EFI_SUCCESS            The requested memory pages were allocated.
  @retval EFI_INVALID_PARAMETER  MemoryType is invalid.
  @retval EFI_INVALID_PARAMETER  HostAddress is NULL.
  @retval EFI_UNSUPPORTED        Attributes is unsupported. The only legal
                                 attribute bits are MEMORY_WRITE_COMBINE,
                                 MEMORY_CACHED, and DUAL_ADDRESS_CYCLE.
  @retval EFI_OUT_OF_RESOURCES   The memory pages could not be allocated.
**/
EFI_STATUS
EFIAPI
RootBridgeIoAllocateBuffer (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  IN  EFI_ALLOCATE_TYPE                Type,
  IN  EFI_MEMORY_TYPE                  MemoryType,
  IN  UINTN                            Pages,
  OUT VOID                             **HostAddress,
  IN  UINT64                           Attributes
  )
{
  EFI_STATUS                Status;
  EFI_PHYSICAL_ADDRESS      PhysicalAddress;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  EFI_ALLOCATE_TYPE         AllocateType;

  //
  // Validate Attributes
  //
  if ((Attributes & EFI_PCI_ATTRIBUTE_INVALID_FOR_ALLOCATE_BUFFER) != 0) {
    return EFI_UNSUPPORTED;
  }

  //
  // Check for invalid inputs
  //
  if (HostAddress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // The only valid memory types are EfiBootServicesData and
  // EfiRuntimeServicesData
  //
  if (MemoryType != EfiBootServicesData &&
      MemoryType != EfiRuntimeServicesData) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);

  AllocateType = AllocateAnyPages;
  if (!RootBridge->DmaAbove4G) {
    //
    // Limit allocations to memory below 4GB
    //
    AllocateType    = AllocateMaxAddress;
    PhysicalAddress = (EFI_PHYSICAL_ADDRESS) (SIZE_4GB - 1);
  }
  Status = gBS->AllocatePages (
                  AllocateType,
                  MemoryType,
                  Pages,
                  &PhysicalAddress
                  );
  if (!EFI_ERROR (Status)) {
    *HostAddress = (VOID *) (UINTN) PhysicalAddress;
  }

  return Status;
}

/**
  Frees memory that was allocated with AllocateBuffer().

  The FreeBuffer() function frees memory that was allocated with
  AllocateBuffer().

  @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param Pages       The number of pages to free.
  @param HostAddress The base system memory address of the allocated range.

  @retval EFI_SUCCESS            The requested memory pages were freed.
  @retval EFI_INVALID_PARAMETER  The memory range specified by HostAddress and
                                 Pages was not allocated with AllocateBuffer().
**/
EFI_STATUS
EFIAPI
RootBridgeIoFreeBuffer (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  IN  UINTN                            Pages,
  OUT VOID                             *HostAddress
  )
{
  return gBS->FreePages ((EFI_PHYSICAL_ADDRESS) (UINTN) HostAddress, Pages);
}

/**
  Flushes all PCI posted write transactions from a PCI host bridge to system
  memory.

  The Flush() function flushes any PCI posted write transactions from a PCI
  host bridge to system memory. Posted write transactions are generated by PCI
  bus masters when they perform write transactions to target addresses in
  system memory.
  This function does not flush posted write transactions from any PCI bridges.
  A PCI controller specific action must be taken to guarantee that the posted
  write transactions have been flushed from the PCI controller and from all the
  PCI bridges into the PCI host bridge. This is typically done with a PCI read
  transaction from the PCI controller prior to calling Flush().

  @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.

  @retval EFI_SUCCESS        The PCI posted write transactions were flushed
                             from the PCI host bridge to system memory.
  @retval EFI_DEVICE_ERROR   The PCI posted write transactions were not flushed
                             from the PCI host bridge due to a hardware error.
**/
EFI_STATUS
EFIAPI
RootBridgeIoFlush (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL           *This
  )
{
  return EFI_SUCCESS;
}

/**
  Gets the attributes that a PCI root bridge supports setting with
  SetAttributes(), and the attributes that a PCI root bridge is currently
  using.

  The GetAttributes() function returns the mask of attributes that this PCI
  root bridge supports and the mask of attributes that the PCI root bridge is
  currently using.

  @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param Supported   A pointer to the mask of attributes that this PCI root
                     bridge supports setting with SetAttributes().
  @param Attributes  A pointer to the mask of attributes that this PCI root
                     bridge is currently using.

  @retval  EFI_SUCCESS           If Supports is not NULL, then the attributes
                                 that the PCI root bridge supports is returned
                                 in Supports. If Attributes is not NULL, then
                                 the attributes that the PCI root bridge is
                                 currently using is returned in Attributes.
  @retval  EFI_INVALID_PARAMETER Both Supports and Attributes are NULL.
**/
EFI_STATUS
EFIAPI
RootBridgeIoGetAttributes (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  OUT UINT64                           *Supported,
  OUT UINT64                           *Attributes
  )
{
  PCI_ROOT_BRIDGE_INSTANCE *RootBridge;

  if (Attributes == NULL && Supported == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  //
  // Set the return value for Supported and Attributes
  //
  if (Supported != NULL) {
    *Supported  = RootBridge->Supports;
  }

  if (Attributes != NULL) {
    *Attributes = RootBridge->Attributes;
  }

  return EFI_SUCCESS;
}

/**
  Sets attributes for a resource range on a PCI root bridge.

  The SetAttributes() function sets the attributes specified in Attributes for
  the PCI root bridge on the resource range specified by ResourceBase and
  ResourceLength. Since the granularity of setting these attributes may vary
  from resource type to resource type, and from platform to platform, the
  actual resource range and the one passed in by the caller may differ. As a
  result, this function may set the attributes specified by Attributes on a
  larger resource range than the caller requested. The actual range is returned
  in ResourceBase and ResourceLength. The caller is responsible for verifying
  that the actual range for which the attributes were set is acceptable.

  @param This            A pointer to the
                         EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param Attributes      The mask of attributes to set. If the
                         attribute bit MEMORY_WRITE_COMBINE,
                         MEMORY_CACHED, or MEMORY_DISABLE is set,
                         then the resource range is specified by
                         ResourceBase and ResourceLength. If
                         MEMORY_WRITE_COMBINE, MEMORY_CACHED, and
                         MEMORY_DISABLE are not set, then
                         ResourceBase and ResourceLength are ignored,
                         and may be NULL.
  @param ResourceBase    A pointer to the base address of the
                         resource range to be modified by the
                         attributes specified by Attributes.
  @param ResourceLength  A pointer to the length of the resource
                                   range to be modified by the attributes
                                   specified by Attributes.

  @retval  EFI_SUCCESS           The current configuration of this PCI root bridge
                                 was returned in Resources.
  @retval  EFI_UNSUPPORTED       The current configuration of this PCI root bridge
                                 could not be retrieved.
**/
EFI_STATUS
EFIAPI
RootBridgeIoSetAttributes (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  IN     UINT64                           Attributes,
  IN OUT UINT64                           *ResourceBase,
  IN OUT UINT64                           *ResourceLength
  )
{
  PCI_ROOT_BRIDGE_INSTANCE            *RootBridge;

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);

  if ((Attributes & (~RootBridge->Supports)) != 0) {
    return EFI_UNSUPPORTED;
  }

  RootBridge->Attributes = Attributes;
  return EFI_SUCCESS;
}

/**
  Retrieves the current resource settings of this PCI root bridge in the form
  of a set of ACPI 2.0 resource descriptors.

  There are only two resource descriptor types from the ACPI Specification that
  may be used to describe the current resources allocated to a PCI root bridge.
  These are the QWORD Address Space Descriptor (ACPI 2.0 Section 6.4.3.5.1),
  and the End Tag (ACPI 2.0 Section 6.4.2.8). The QWORD Address Space
  Descriptor can describe memory, I/O, and bus number ranges for dynamic or
  fixed resources. The configuration of a PCI root bridge is described with one
  or more QWORD Address Space Descriptors followed by an End Tag.

  @param[in]   This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[out]  Resources   A pointer to the ACPI 2.0 resource descriptors that
                           describe the current configuration of this PCI root
                           bridge. The storage for the ACPI 2.0 resource
                           descriptors is allocated by this function. The
                           caller must treat the return buffer as read-only
                           data, and the buffer must not be freed by the
                           caller.

  @retval  EFI_SUCCESS     The current configuration of this PCI root bridge
                           was returned in Resources.
  @retval  EFI_UNSUPPORTED The current configuration of this PCI root bridge
                           could not be retrieved.
**/
EFI_STATUS
EFIAPI
RootBridgeIoConfiguration (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL     *This,
  OUT VOID                                **Resources
  )
{
  PCI_RESOURCE_TYPE                 Index;
  PCI_ROOT_BRIDGE_INSTANCE          *RootBridge;
  PCI_RES_NODE                      *ResAllocNode;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Descriptor;
  EFI_ACPI_END_TAG_DESCRIPTOR       *End;

  //
  // Get this instance of the Root Bridge.
  //
  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  ZeroMem (
    RootBridge->ConfigBuffer,
    TypeMax * sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR)
    );
  Descriptor = RootBridge->ConfigBuffer;
  for (Index = TypeIo; Index < TypeMax; Index++) {

    ResAllocNode = &RootBridge->ResAllocNode[Index];

    if (ResAllocNode->Status != ResAllocated) {
      continue;
    }

    Descriptor->Desc = ACPI_ADDRESS_SPACE_DESCRIPTOR;
    Descriptor->Len  = sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) - 3;
    Descriptor->AddrRangeMin  = ResAllocNode->Base;
    Descriptor->AddrRangeMax  = ResAllocNode->Base + ResAllocNode->Length - 1;
    Descriptor->AddrLen       = ResAllocNode->Length;
    switch (ResAllocNode->Type) {

    case TypeIo:
      Descriptor->ResType       = ACPI_ADDRESS_SPACE_TYPE_IO;
      break;

    case TypePMem32:
      Descriptor->SpecificFlag = EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE;
    case TypeMem32:
      Descriptor->ResType               = ACPI_ADDRESS_SPACE_TYPE_MEM;
      Descriptor->AddrSpaceGranularity  = 32;
      break;

    case TypePMem64:
      Descriptor->SpecificFlag = EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE;
    case TypeMem64:
      Descriptor->ResType               = ACPI_ADDRESS_SPACE_TYPE_MEM;
      Descriptor->AddrSpaceGranularity  = 64;
      break;

    case TypeBus:
      Descriptor->ResType       = ACPI_ADDRESS_SPACE_TYPE_BUS;
      break;

    default:
      break;
    }

    Descriptor++;
  }
  //
  // Terminate the entries.
  //
  End = (EFI_ACPI_END_TAG_DESCRIPTOR *) Descriptor;
  End->Desc     = ACPI_END_TAG_DESCRIPTOR;
  End->Checksum = 0x0;

  *Resources = RootBridge->ConfigBuffer;
  return EFI_SUCCESS;
}
