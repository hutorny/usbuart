## USBUART
 A cross-platform library for reading/wring data via USB-UART adapters

### Introduction

`USBUART` is a LIBUSB based library that implements a relay from USB-UART 
converter’s endpoints to a pair of I/O resources, either appointed by given 
file descriptors, or created inside the library with pipe[(2)](http://linux.die.net/man/2/pipe).

User application may then use standard I/O operations for reading and writing data.
USBUART Library provides API for three languages - C++, C and Java.

See C/C++ API description in [usbuart.h](usbuart_8h.html) and Android API in 

### Usage with C++

	// Instantiate a context
	context ctx;
	
	// Attach USB via a pipe channel
	channel chnl;
	ctx.pipe(device_id{0x067b,0x0609},chnl,_115200_8N1n);
	
	//Run loop in one thread
	while(ctx.loop(10) >= -error_t::no_channel);
	
	//Read/write data in other thread(s)
	char buff[256];
	read(chnl.fd_read, buff, sizeof(buff));
	
	//or use non-blocking I/O in the loop body

### Usage on Android

	// Instantiate a context
	ctx = new UsbUartContext();
	
	// Start a thread running event loop 
	new Thread(ctx).start();
	
	// Obtain permission to the USB device
	UsbManager usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
	usbManager.requestPermission(device, PendingIntent.getBroadcast(...));
	
	// Open device
	UsbDeviceConnection connection = usbManager.openDevice(device);
	
	// Create pipe channel
	Channel channel = ctx.pipe(connection, 0, EIA_TIA_232_Info._115200_8N1n());
	
	// Open streams
	InputStream input = channel.getInputStream();	
	OutputStream output = channel.getOutputStream();	
	
	// Perfrom I/O operations with the streams
	
### Building 

1. Get USBUART library sources

		git clone https://github.com/hutorny/usbuart.git

2. Get libusb sources

		cd usbuart
		git clone https://github.com/libusb/libusb.git

3. Change directory to libusb

		cd libusb

4. Configure and build libusb

		./autogen.sh --disable-udev
		make

5. Change directory to usbuart 

		cd ..

6. Make USBUART

		make

### Building for Android	

1. Get USBUART library sources

		git clone https://github.com/hutorny/usbuart.git

2. Get Android-tuned libusb fork 

		cd usbuart
		git clone https://github.com/hutorny/libusb.git

3. Download the latest NDK from:
   http://developer.android.com/tools/sdk/ndk/index.html

4. Extract the NDK.

5. Open a shell and make sure there exist an NDK global variable
   set to the directory where you extracted the NDK.

6. Change directory to usbuart/libusb

7. Configure libusb 

		./android/autogen.sh --enable-system_log

8. Change directory to usbuart

		cd ..

9. Build libary and modules 

		ant debug
