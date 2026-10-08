/* Disk and partition answers from the host (patches/sg/1629): what
 * \\.\C: says about the disk under the prefix. mountmgr made these up (a
 * disk of 10000 cylinders, no seek penalty, no maker, zeroed extents) or did
 * not answer (partition information, length). The gate compares the
 * answers with sysfs. Prints "key value" lines. */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>

#ifndef IOCTL_DISK_GET_LENGTH_INFO
#define IOCTL_DISK_GET_LENGTH_INFO CTL_CODE(IOCTL_DISK_BASE, 0x0017, METHOD_BUFFERED, FILE_READ_ACCESS)
#endif

int main(int argc, char **argv)
{
    const char *drive = argc > 1 ? argv[1] : "\\\\.\\C:";
    HANDLE h = CreateFileA(drive, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    STORAGE_PROPERTY_QUERY q;
    DEVICE_SEEK_PENALTY_DESCRIPTOR seek;
    STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR align;
    GET_LENGTH_INFORMATION length;
    PARTITION_INFORMATION_EX part;
    VOLUME_DISK_EXTENTS extents;
    DISK_GEOMETRY_EX geo;
    BYTE buf[1024];
    STORAGE_DEVICE_DESCRIPTOR *dev = (void *)buf;
    DWORD got;

    if (h == INVALID_HANDLE_VALUE) { printf("open failed %lu\n", GetLastError()); return 1; }

    memset(&q, 0, sizeof(q));
    q.PropertyId = StorageDeviceSeekPenaltyProperty;
    q.QueryType = PropertyStandardQuery;
    if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), &seek, sizeof(seek), &got, NULL))
        printf("seekpenalty %d\n", seek.IncursSeekPenalty);
    q.PropertyId = StorageAccessAlignmentProperty;
    if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), &align, sizeof(align), &got, NULL))
        printf("logicalsector %lu\nphysicalsector %lu\n", align.BytesPerLogicalSector, align.BytesPerPhysicalSector);
    q.PropertyId = StorageDeviceProperty;
    if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), buf, sizeof(buf), &got, NULL))
    {
        printf("bustype %u\nremovable %d\n", dev->BusType, dev->RemovableMedia);
        printf("product %s\n", dev->ProductIdOffset ? (char *)buf + dev->ProductIdOffset : "(none)");
        printf("vendor %s\n", dev->VendorIdOffset ? (char *)buf + dev->VendorIdOffset : "(none)");
    }
    {
        /* the length needs read access */
        HANDLE r = CreateFileA(drive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (r != INVALID_HANDLE_VALUE &&
            DeviceIoControl(r, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &length, sizeof(length), &got, NULL))
            printf("length %llu\n", (unsigned long long)length.Length.QuadPart);
        else printf("length failed %lu\n", GetLastError());
        if (r != INVALID_HANDLE_VALUE) CloseHandle(r);
    }
    if (DeviceIoControl(h, IOCTL_DISK_GET_PARTITION_INFO_EX, NULL, 0, &part, sizeof(part), &got, NULL))
        printf("partstyle %d\npartstart %llu\npartlength %llu\npartnumber %lu\n", part.PartitionStyle,
               (unsigned long long)part.StartingOffset.QuadPart, (unsigned long long)part.PartitionLength.QuadPart,
               part.PartitionNumber);
    else printf("partinfo failed %lu\n", GetLastError());
    if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &extents, sizeof(extents), &got, NULL))
        printf("extents %lu\nextentstart %llu\nextentlength %llu\n", extents.NumberOfDiskExtents,
               (unsigned long long)extents.Extents[0].StartingOffset.QuadPart,
               (unsigned long long)extents.Extents[0].ExtentLength.QuadPart);
    if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geo, sizeof(geo), &got, NULL))
        printf("disksize %llu\nbytespersector %lu\n", (unsigned long long)geo.DiskSize.QuadPart, geo.Geometry.BytesPerSector);
    CloseHandle(h);
    return 0;
}
