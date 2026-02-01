
 

#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

#define DEVICE_N(vendor, IDS, nport)				\
static const struct usb_device_id vendor##_id_table[] = {	\
	IDS(),							\
	{ },							\
};								\
static struct usb_serial_driver vendor##_device = {		\
	.driver = {						\
		.owner =	THIS_MODULE,			\
		.name =		#vendor,			\
	},							\
	.id_table =		vendor##_id_table,		\
	.num_ports =		nport,				\
};

#define DEVICE(vendor, IDS)	DEVICE_N(vendor, IDS, 1)

 
#define CARELINK_IDS()			\
	{ USB_DEVICE(0x0a21, 0x8001) }	 
DEVICE(carelink, CARELINK_IDS);

 
#define FLASHLOADER_IDS()		\
	{ USB_DEVICE_INTERFACE_CLASS(0x058b, 0x0041, USB_CLASS_CDC_DATA) }, \
	{ USB_DEVICE(0x8087, 0x0716) }, \
	{ USB_DEVICE(0x8087, 0x0801) }
DEVICE(flashloader, FLASHLOADER_IDS);

 
#define FUNSOFT_IDS()			\
	{ USB_DEVICE(0x1404, 0xcddc) }
DEVICE(funsoft, FUNSOFT_IDS);

 
#define GOOGLE_IDS()						\
	{ USB_VENDOR_AND_INTERFACE_INFO(0x18d1,			\
					USB_CLASS_VENDOR_SPEC,	\
					0x50,			\
					0x01) }
DEVICE(google, GOOGLE_IDS);

 
#define HP4X_IDS()			\
	{ USB_DEVICE(0x03f0, 0x0121) }
DEVICE(hp4x, HP4X_IDS);

 
#define KAUFMANN_IDS()			\
	{ USB_DEVICE(0x16d0, 0x0870) }
DEVICE(kaufmann, KAUFMANN_IDS);

 
#define LIBTRANSISTOR_IDS()			\
	{ USB_DEVICE(0x1209, 0x8b00) }
DEVICE(libtransistor, LIBTRANSISTOR_IDS);

 
#define MOTO_IDS()			\
	{ USB_DEVICE(0x05c6, 0x3197) },	 	\
	{ USB_DEVICE(0x0c44, 0x0022) },	 	\
	{ USB_DEVICE(0x22b8, 0x2a64) },	 		\
	{ USB_DEVICE(0x22b8, 0x2c84) },	 	\
	{ USB_DEVICE(0x22b8, 0x2c64) }	 
DEVICE(moto_modem, MOTO_IDS);

 
#define MOTOROLA_TETRA_IDS()			\
	{ USB_DEVICE(0x0cad, 0x9011) },	  \
	{ USB_DEVICE(0x0cad, 0x9012) },	  \
	{ USB_DEVICE(0x0cad, 0x9013) },	  \
	{ USB_DEVICE(0x0cad, 0x9015) },	  \
	{ USB_DEVICE(0x0cad, 0x9016) }	 
DEVICE(motorola_tetra, MOTOROLA_TETRA_IDS);

 
#define NOKIA_IDS()			\
	{ USB_DEVICE(0x0421, 0x069a) }	 
DEVICE(nokia, NOKIA_IDS);

 
#define NOVATEL_IDS()			\
	{ USB_DEVICE(0x09d7, 0x0100) }	 
DEVICE_N(novatel_gps, NOVATEL_IDS, 3);

 
#define SIEMENS_IDS()			\
	{ USB_DEVICE(0x908, 0x0004) }
DEVICE(siemens_mpi, SIEMENS_IDS);

 
#define SUUNTO_IDS()			\
	{ USB_DEVICE(0x0fcf, 0x1008) },	\
	{ USB_DEVICE(0x0fcf, 0x1009) }  
DEVICE(suunto, SUUNTO_IDS);

 
#define VIVOPAY_IDS()			\
	{ USB_DEVICE(0x1d5f, 0x1004) }	 
DEVICE(vivopay, VIVOPAY_IDS);

 
#define ZIO_IDS()			\
	{ USB_DEVICE(0x1CBE, 0x0103) }
DEVICE(zio, ZIO_IDS);

 
static struct usb_serial_driver * const serial_drivers[] = {
	&carelink_device,
	&flashloader_device,
	&funsoft_device,
	&google_device,
	&hp4x_device,
	&kaufmann_device,
	&libtransistor_device,
	&moto_modem_device,
	&motorola_tetra_device,
	&nokia_device,
	&novatel_gps_device,
	&siemens_mpi_device,
	&suunto_device,
	&vivopay_device,
	&zio_device,
	NULL
};

static const struct usb_device_id id_table[] = {
	CARELINK_IDS(),
	FLASHLOADER_IDS(),
	FUNSOFT_IDS(),
	GOOGLE_IDS(),
	HP4X_IDS(),
	KAUFMANN_IDS(),
	LIBTRANSISTOR_IDS(),
	MOTO_IDS(),
	MOTOROLA_TETRA_IDS(),
	NOKIA_IDS(),
	NOVATEL_IDS(),
	SIEMENS_IDS(),
	SUUNTO_IDS(),
	VIVOPAY_IDS(),
	ZIO_IDS(),
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

module_usb_serial_driver(serial_drivers, id_table);
MODULE_LICENSE("GPL v2");
