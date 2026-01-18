// BigbenUSB.c - Bigben controller USB communication implementation

#include "include/BigbenUSB.h"
#include <libusb-1.0/libusb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ENDPOINT_IN  0x81
#define ENDPOINT_OUT 0x02
#define INTERFACE_NUM 0
#define READ_TIMEOUT_MS 100

struct BigbenController {
    libusb_device_handle* handle;
    pthread_t read_thread;
    volatile bool running;
    volatile bool connected;

    BigbenInputCallback input_callback;
    void* input_context;
    BigbenConnectionCallback connection_callback;
    void* connection_context;
};

static libusb_context* usb_ctx = NULL;
static int init_count = 0;

int bigben_init(void) {
    if (init_count++ > 0) {
        return 0; // Already initialized
    }

    int r = libusb_init(&usb_ctx);
    if (r < 0) {
        fprintf(stderr, "bigben_init: Failed to init libusb: %s\n", libusb_strerror(r));
        init_count--;
        return r;
    }

    return 0;
}

void bigben_cleanup(void) {
    if (--init_count > 0) {
        return;
    }

    if (usb_ctx) {
        libusb_exit(usb_ctx);
        usb_ctx = NULL;
    }
    init_count = 0;
}

BigbenController* bigben_create(void) {
    BigbenController* controller = calloc(1, sizeof(BigbenController));
    if (!controller) {
        return NULL;
    }
    return controller;
}

void bigben_destroy(BigbenController* controller) {
    if (!controller) return;

    bigben_stop_reading(controller);
    bigben_close(controller);
    free(controller);
}

void bigben_set_input_callback(BigbenController* controller, BigbenInputCallback callback, void* context) {
    if (!controller) return;
    controller->input_callback = callback;
    controller->input_context = context;
}

void bigben_set_connection_callback(BigbenController* controller, BigbenConnectionCallback callback, void* context) {
    if (!controller) return;
    controller->connection_callback = callback;
    controller->connection_context = context;
}

static libusb_device* find_bigben_device(void) {
    libusb_device** devs;
    ssize_t cnt = libusb_get_device_list(usb_ctx, &devs);
    if (cnt < 0) {
        return NULL;
    }

    libusb_device* found = NULL;
    for (ssize_t i = 0; i < cnt && !found; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0) {
            if (desc.idVendor == BIGBEN_VID &&
                (desc.idProduct == BIGBEN_PID_PC || desc.idProduct == BIGBEN_PID_PS4)) {
                found = devs[i];
                libusb_ref_device(found);
            }
        }
    }

    libusb_free_device_list(devs, 1);
    return found;
}

int bigben_open(BigbenController* controller) {
    if (!controller || !usb_ctx) {
        return -1;
    }

    if (controller->handle) {
        return 0; // Already open
    }

    libusb_device* dev = find_bigben_device();
    if (!dev) {
        fprintf(stderr, "bigben_open: Controller not found\n");
        return -1;
    }

    int r = libusb_open(dev, &controller->handle);
    libusb_unref_device(dev);

    if (r < 0) {
        fprintf(stderr, "bigben_open: Failed to open device: %s\n", libusb_strerror(r));
        return r;
    }

    // Detach kernel driver if needed
    if (libusb_kernel_driver_active(controller->handle, INTERFACE_NUM) == 1) {
        r = libusb_detach_kernel_driver(controller->handle, INTERFACE_NUM);
        if (r < 0) {
            fprintf(stderr, "bigben_open: Warning - could not detach kernel driver: %s\n", libusb_strerror(r));
        }
    }

    // Claim interface
    r = libusb_claim_interface(controller->handle, INTERFACE_NUM);
    if (r < 0) {
        fprintf(stderr, "bigben_open: Failed to claim interface: %s\n", libusb_strerror(r));
        libusb_close(controller->handle);
        controller->handle = NULL;
        return r;
    }

    controller->connected = true;

    if (controller->connection_callback) {
        controller->connection_callback(true, controller->connection_context);
    }

    return 0;
}

void bigben_close(BigbenController* controller) {
    if (!controller) return;

    bigben_stop_reading(controller);

    if (controller->handle) {
        libusb_release_interface(controller->handle, INTERFACE_NUM);
        libusb_close(controller->handle);
        controller->handle = NULL;
    }

    if (controller->connected) {
        controller->connected = false;
        if (controller->connection_callback) {
            controller->connection_callback(false, controller->connection_context);
        }
    }
}

bool bigben_is_connected(BigbenController* controller) {
    return controller && controller->connected;
}

int bigben_poll(BigbenController* controller, BigbenInputReport* report, int timeout_ms) {
    if (!controller || !controller->handle || !report) {
        return -1;
    }

    unsigned char data[64];
    int transferred;

    int r = libusb_interrupt_transfer(controller->handle, ENDPOINT_IN,
                                       data, sizeof(data), &transferred, timeout_ms);

    if (r == LIBUSB_ERROR_TIMEOUT) {
        return r;
    }

    if (r < 0) {
        fprintf(stderr, "bigben_poll: Read error: %s\n", libusb_strerror(r));
        return r;
    }

    // Parse the data into the report structure
    // XInput format: bytes 0-1 are header, 2-3 buttons, 4-5 triggers, 6-13 sticks
    if (transferred >= 14) {
        report->report_id = data[0];
        report->report_size = data[1];
        report->buttons = data[2] | (data[3] << 8);
        report->left_trigger = data[4];
        report->right_trigger = data[5];
        report->left_stick_x = (int16_t)(data[6] | (data[7] << 8));
        report->left_stick_y = (int16_t)(data[8] | (data[9] << 8));
        report->right_stick_x = (int16_t)(data[10] | (data[11] << 8));
        report->right_stick_y = (int16_t)(data[12] | (data[13] << 8));
    }

    return 0;
}

static void* read_thread_func(void* arg) {
    BigbenController* controller = (BigbenController*)arg;
    BigbenInputReport report;

    while (controller->running) {
        int r = bigben_poll(controller, &report, READ_TIMEOUT_MS);

        if (r == LIBUSB_ERROR_NO_DEVICE || r == LIBUSB_ERROR_IO) {
            // Device disconnected
            controller->connected = false;
            if (controller->connection_callback) {
                controller->connection_callback(false, controller->connection_context);
            }
            break;
        }

        if (r == 0 && controller->input_callback) {
            controller->input_callback(&report, controller->input_context);
        }
    }

    return NULL;
}

int bigben_start_reading(BigbenController* controller) {
    if (!controller || !controller->handle) {
        return -1;
    }

    if (controller->running) {
        return 0; // Already running
    }

    controller->running = true;

    int r = pthread_create(&controller->read_thread, NULL, read_thread_func, controller);
    if (r != 0) {
        fprintf(stderr, "bigben_start_reading: Failed to create thread\n");
        controller->running = false;
        return -1;
    }

    return 0;
}

void bigben_stop_reading(BigbenController* controller) {
    if (!controller || !controller->running) {
        return;
    }

    controller->running = false;
    pthread_join(controller->read_thread, NULL);
}

int bigben_set_rumble(BigbenController* controller, uint8_t weak_motor, uint8_t strong_motor) {
    if (!controller || !controller->handle) {
        return -1;
    }

    // XInput rumble report format
    unsigned char data[8] = {
        0x00,         // Report ID
        0x08,         // Report size
        0x00,         // Reserved
        weak_motor,   // Weak motor intensity
        strong_motor, // Strong motor intensity
        0x00, 0x00, 0x00
    };

    int transferred;
    int r = libusb_interrupt_transfer(controller->handle, ENDPOINT_OUT,
                                       data, sizeof(data), &transferred, 100);

    if (r < 0) {
        fprintf(stderr, "bigben_set_rumble: Write error: %s\n", libusb_strerror(r));
        return r;
    }

    return 0;
}
