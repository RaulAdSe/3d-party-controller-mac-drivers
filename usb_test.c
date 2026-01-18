// usb_test.c - Test program to enumerate and read from Bigben controller

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#define BIGBEN_VID 0x146b
#define BIGBEN_PID_PC 0x0603
#define BIGBEN_PID_PS4 0x0d05

void print_device_info(libusb_device *dev) {
    struct libusb_device_descriptor desc;
    int r = libusb_get_device_descriptor(dev, &desc);
    if (r < 0) {
        fprintf(stderr, "Failed to get device descriptor\n");
        return;
    }

    printf("  VID:PID = %04x:%04x\n", desc.idVendor, desc.idProduct);
    printf("  Device Class: %02x, SubClass: %02x, Protocol: %02x\n",
           desc.bDeviceClass, desc.bDeviceSubClass, desc.bDeviceProtocol);
    printf("  Num Configurations: %d\n", desc.bNumConfigurations);
}

void print_config_info(libusb_device *dev) {
    struct libusb_config_descriptor *config;
    int r = libusb_get_config_descriptor(dev, 0, &config);
    if (r < 0) {
        fprintf(stderr, "Failed to get config descriptor\n");
        return;
    }

    printf("  Configuration %d:\n", config->bConfigurationValue);
    printf("    Num Interfaces: %d\n", config->bNumInterfaces);

    for (int i = 0; i < config->bNumInterfaces; i++) {
        const struct libusb_interface *iface = &config->interface[i];
        for (int j = 0; j < iface->num_altsetting; j++) {
            const struct libusb_interface_descriptor *altsetting = &iface->altsetting[j];
            printf("    Interface %d (alt %d):\n", altsetting->bInterfaceNumber, altsetting->bAlternateSetting);
            printf("      Class: %02x, SubClass: %02x, Protocol: %02x\n",
                   altsetting->bInterfaceClass, altsetting->bInterfaceSubClass, altsetting->bInterfaceProtocol);
            printf("      Num Endpoints: %d\n", altsetting->bNumEndpoints);

            for (int k = 0; k < altsetting->bNumEndpoints; k++) {
                const struct libusb_endpoint_descriptor *endpoint = &altsetting->endpoint[k];
                printf("        Endpoint 0x%02x: ", endpoint->bEndpointAddress);
                if (endpoint->bEndpointAddress & 0x80) {
                    printf("IN ");
                } else {
                    printf("OUT ");
                }
                switch (endpoint->bmAttributes & 0x03) {
                    case 0: printf("Control"); break;
                    case 1: printf("Isochronous"); break;
                    case 2: printf("Bulk"); break;
                    case 3: printf("Interrupt"); break;
                }
                printf(", Max Packet Size: %d\n", endpoint->wMaxPacketSize);
            }
        }
    }

    libusb_free_config_descriptor(config);
}

int try_read_controller(libusb_device_handle *handle, int interface_num, uint8_t endpoint) {
    printf("\nAttempting to read from controller (interface %d, endpoint 0x%02x)...\n", interface_num, endpoint);

    // Try to claim the interface
    int r = libusb_claim_interface(handle, interface_num);
    if (r < 0) {
        fprintf(stderr, "Failed to claim interface: %s\n", libusb_strerror(r));
        return -1;
    }

    printf("Interface claimed successfully!\n");

    // Read data
    unsigned char data[64];
    int transferred;

    printf("Reading data (press buttons on controller)...\n");
    for (int i = 0; i < 20; i++) {
        r = libusb_interrupt_transfer(handle, endpoint, data, sizeof(data), &transferred, 100);
        if (r == 0) {
            printf("Read %d bytes: ", transferred);
            for (int j = 0; j < transferred && j < 20; j++) {
                printf("%02x ", data[j]);
            }
            printf("\n");
        } else if (r == LIBUSB_ERROR_TIMEOUT) {
            printf(".");
            fflush(stdout);
        } else {
            fprintf(stderr, "\nRead error: %s\n", libusb_strerror(r));
        }
    }
    printf("\n");

    libusb_release_interface(handle, interface_num);
    return 0;
}

int main() {
    libusb_context *ctx = NULL;
    libusb_device **devs;
    libusb_device *bigben_dev = NULL;
    ssize_t cnt;
    int r;

    printf("Bigben Controller USB Test\n");
    printf("==========================\n\n");

    // Initialize libusb
    r = libusb_init(&ctx);
    if (r < 0) {
        fprintf(stderr, "Failed to init libusb: %s\n", libusb_strerror(r));
        return 1;
    }

    // Get device list
    cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        fprintf(stderr, "Failed to get device list\n");
        libusb_exit(ctx);
        return 1;
    }

    printf("Scanning for Bigben controller (VID: 0x%04x)...\n\n", BIGBEN_VID);

    // Find Bigben controller
    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        r = libusb_get_device_descriptor(devs[i], &desc);
        if (r < 0) continue;

        if (desc.idVendor == BIGBEN_VID) {
            printf("Found Bigben device!\n");
            print_device_info(devs[i]);
            print_config_info(devs[i]);
            bigben_dev = devs[i];
            break;
        }
    }

    if (!bigben_dev) {
        printf("Bigben controller not found!\n");
        printf("\nListing all USB devices:\n");
        for (ssize_t i = 0; i < cnt; i++) {
            struct libusb_device_descriptor desc;
            r = libusb_get_device_descriptor(devs[i], &desc);
            if (r < 0) continue;
            printf("  %04x:%04x (Class: %02x)\n", desc.idVendor, desc.idProduct, desc.bDeviceClass);
        }
        libusb_free_device_list(devs, 1);
        libusb_exit(ctx);
        return 1;
    }

    // Try to open the device
    libusb_device_handle *handle;
    r = libusb_open(bigben_dev, &handle);
    if (r < 0) {
        fprintf(stderr, "\nFailed to open device: %s\n", libusb_strerror(r));
        libusb_free_device_list(devs, 1);
        libusb_exit(ctx);
        return 1;
    }

    printf("\nDevice opened successfully!\n");

    // Check if kernel driver is attached
    if (libusb_kernel_driver_active(handle, 0) == 1) {
        printf("Kernel driver is active, detaching...\n");
        r = libusb_detach_kernel_driver(handle, 0);
        if (r < 0) {
            fprintf(stderr, "Failed to detach kernel driver: %s\n", libusb_strerror(r));
        }
    }

    // Get configuration info to find endpoints
    struct libusb_config_descriptor *config;
    r = libusb_get_config_descriptor(bigben_dev, 0, &config);
    if (r == 0 && config->bNumInterfaces > 0) {
        const struct libusb_interface *iface = &config->interface[0];
        if (iface->num_altsetting > 0) {
            const struct libusb_interface_descriptor *altsetting = &iface->altsetting[0];
            for (int k = 0; k < altsetting->bNumEndpoints; k++) {
                const struct libusb_endpoint_descriptor *endpoint = &altsetting->endpoint[k];
                // Find IN endpoint (interrupt)
                if ((endpoint->bEndpointAddress & 0x80) &&
                    ((endpoint->bmAttributes & 0x03) == 3)) { // Interrupt IN
                    try_read_controller(handle, altsetting->bInterfaceNumber, endpoint->bEndpointAddress);
                    break;
                }
            }
        }
        libusb_free_config_descriptor(config);
    }

    libusb_close(handle);
    libusb_free_device_list(devs, 1);
    libusb_exit(ctx);

    return 0;
}
