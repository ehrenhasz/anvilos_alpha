





Q(/flash)
Q(/flash/lib)
Q(/sd)
Q(/sd/lib)


Q(/)

#if MICROPY_HW_ENABLE_USB

Q(VCP)
Q(MSC)
Q(VCP+MSC)
Q(VCP+HID)
Q(VCP+MSC+HID)
#if MICROPY_HW_USB_CDC_NUM >= 2
Q(2xVCP)
Q(2xVCP+MSC)
Q(2xVCP+MSC+HID)
#endif
#if MICROPY_HW_USB_CDC_NUM >= 3
Q(3xVCP)
Q(3xVCP+MSC)
Q(3xVCP+MSC+HID)
#endif
#endif
