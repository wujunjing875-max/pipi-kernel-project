/*
 * JianShangAim.sys — 最小只读/读写内核通道
 *
 * 作用：向用户态暴露 \\.\JianShangAim，通过 IOCTL 直接对目标进程做
 *       MmCopyVirtualMemory，绕开 PPL / ObRegisterCallbacks / 句柄最小权限限制。
 *
 * 构建（WDK + VS，x64 Free）：
 *   1. 新建 "Empty WDM Driver" 工程，加入本文件
 *   2. 目标平台 x64，配置 Release
 *   3. 关闭"驱动签名强制"或用测试签名：bcdedit /set testsigning on && 重启
 *   4. 产物 JianShangAim.sys 丢到 js-aim/driver/ 下
 *
 * 加载：
 *   sc create JianShangAim type= kernel start= demand binPath= C:\path\JianShangAim.sys
 *   sc start JianShangAim
 *   之后 core/kernel.py 的 probe() 会返回 mode=kernel
 */
#include <ntifs.h>
#include <ntddk.h>

#define JS_DEVICE_NAME   L"\\Device\\JianShangAim"
#define JS_SYMLINK_NAME  L"\\DosDevices\\JianShangAim"

#define IOCTL_JS_READ   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JS_WRITE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _JS_REQ {
    ULONG  Pid;
    PVOID  Address;
    ULONG  Size;
    UCHAR  Data[1];
} JS_REQ, *PJS_REQ;

static NTSTATUS JsCopy(BOOLEAN ToTarget, PJS_REQ req, PULONG_PTR info)
{
    PEPROCESS src = NULL, dst = NULL;
    NTSTATUS st;
    SIZE_T done = 0;

    st = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)req->Pid, &dst);
    if (!NT_SUCCESS(st)) return st;

    if (ToTarget) {
        st = MmCopyVirtualMemory(PsGetCurrentProcess(), req->Data,
                                 dst, req->Address, req->Size,
                                 KernelMode, &done);
    } else {
        st = MmCopyVirtualMemory(dst, req->Address,
                                 PsGetCurrentProcess(), req->Data, req->Size,
                                 KernelMode, &done);
    }
    ObDereferenceObject(dst);
    *info = done;
    return st;
}

static NTSTATUS JsDispatch(PDEVICE_OBJECT dev, PIRP irp)
{
    UNREFERENCED_PARAMETER(dev);
    PIO_STACK_LOCATION sp = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS st = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR info = 0;
    PVOID buf = irp->AssociatedIrp.SystemBuffer;
    ULONG inlen = sp->Parameters.DeviceIoControl.InputBufferLength;

    if (sp->MajorFunction == IRP_MJ_CREATE || sp->MajorFunction == IRP_MJ_CLOSE) {
        st = STATUS_SUCCESS;
    } else if (sp->MajorFunction == IRP_MJ_DEVICE_CONTROL &&
               inlen >= FIELD_OFFSET(JS_REQ, Data) &&
               (sp->Parameters.DeviceIoControl.IoControlCode == IOCTL_JS_READ ||
                sp->Parameters.DeviceIoControl.IoControlCode == IOCTL_JS_WRITE)) {
        PJS_REQ req = (PJS_REQ)buf;
        if (req->Size <= inlen - FIELD_OFFSET(JS_REQ, Data)) {
            st = JsCopy(sp->Parameters.DeviceIoControl.IoControlCode == IOCTL_JS_WRITE, req, &info);
        } else {
            st = STATUS_BUFFER_TOO_SMALL;
        }
    }

    irp->IoStatus.Status = st;
    irp->IoStatus.Information = info;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return st;
}

static VOID JsUnload(PDRIVER_OBJECT drv)
{
    UNICODE_STRING sym;
    RtlInitUnicodeString(&sym, JS_SYMLINK_NAME);
    IoDeleteSymbolicLink(&sym);
    if (drv->DeviceObject) IoDeleteDevice(drv->DeviceObject);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING reg)
{
    UNREFERENCED_PARAMETER(reg);
    PDEVICE_OBJECT dev = NULL;
    UNICODE_STRING name, sym;
    NTSTATUS st;

    RtlInitUnicodeString(&name, JS_DEVICE_NAME);
    st = IoCreateDevice(drv, 0, &name, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &dev);
    if (!NT_SUCCESS(st)) return st;

    RtlInitUnicodeString(&sym, JS_SYMLINK_NAME);
    st = IoCreateSymbolicLink(&sym, &name);
    if (!NT_SUCCESS(st)) { IoDeleteDevice(dev); return st; }

    drv->MajorFunction[IRP_MJ_CREATE] = JsDispatch;
    drv->MajorFunction[IRP_MJ_CLOSE] = JsDispatch;
    drv->MajorFunction[IRP_MJ_DEVICE_CONTROL] = JsDispatch;
    drv->DriverUnload = JsUnload;

    dev->Flags |= DO_BUFFERED_IO;
    dev->Flags &= ~DO_DEVICE_INITIALIZING;
    DbgPrint("[JianShangAim] loaded\n");
    return STATUS_SUCCESS;
}
