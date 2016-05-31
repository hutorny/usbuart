/*
 * Copyright (C) 2016 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * info_usbuart_api_Context.cpp
 * 				- native methods implementation for info.usbuart.api.Context
 *
 * This file is part of USBUART Library. http://hutorny.in.ua/projects/usbuart
 *
 * The USBUART Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License v2
 * as published by the Free Software Foundation;
 *
 * The USBUART Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the USBUART Library; if not, see
 * <http://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "info_usbuart_api_UsbUartContext.h"
#include "usbuart.hpp"
using namespace usbuart;

extern "C"
int android_enumerate_device(struct libusb_context *ctx,
		uint8_t busnum, uint8_t devaddr, const char *sysfs_dir);

extern "C"
void libusb_set_debug(libusb_context *ctx, int level);

jlong JNICALL Java_info_usbuart_api_UsbUartContext_create
  (JNIEnv * jni, jclass) {
	log.d(__,"here");
//	freopen("/data/data/info.usbuart.testapp/usbuart.log","w",stderr); //FIXME drop
	try {
		context* ctx = new context();
		libusb_set_debug(ctx->native(), 3); 	//FIXME drop
		libusb_set_debug(nullptr, 3); 			//FIXME drop
		return reinterpret_cast<jlong>(ctx);
	} catch(error_t err) {
		log.e(__,"Error %d creating usbuart context", +err);
	} catch(std::runtime_error& err) {
		log.e(__,"Error %s creating usbuart context", err.what());
	} catch(...) {
		log.e(__,"Unknown error creating usbuart context");
	}
	jni->ThrowNew(jni->FindClass("java/lang/Exception"),
			"Error creating USBUART context");
}

/*
 * Class:     info_usbuart_api_UsbUartContext
 * Method:    loop
 * Signature: (JI)I
 */
jint JNICALL Java_info_usbuart_api_UsbUartContext_loop
  (JNIEnv *, jclass, jlong ctx, jint timeout) {
	return reinterpret_cast<context*>(ctx)->loop(timeout);
}


static channel channelJ(JNIEnv * jni, jobject jch) {
	channel ch;
	auto jclas 			= jni->GetObjectClass(jch);
	jfieldID fd_read	= jni->GetFieldID(jclas, "fd_read", "I");
	jfieldID fd_write	= jni->GetFieldID(jclas, "fd_write", "I");
	ch.fd_read			= jni->GetIntField(jch, fd_read);
	ch.fd_write			= jni->GetIntField(jch, fd_write);
	return ch;
}

static void channelJ(JNIEnv * jni, jobject jch, const channel& ch) {
	auto jclas 			= jni->GetObjectClass(jch);
	jfieldID fd_read	= jni->GetFieldID(jclas, "fd_read", "I");
	jfieldID fd_write	= jni->GetFieldID(jclas, "fd_write", "I");
	jni->SetIntField(jch, fd_read, ch.fd_read);
	jni->SetIntField(jch, fd_write, ch.fd_write);
}


/**
 * Converts value of a Java Enum object to integer via its ordinal()
 */
template<typename T>
static T ordinal(JNIEnv * jni, jobject jenum) {
	auto jclas = jni->GetObjectClass(jenum);
//	log.d(__,"ordinal");
	auto method = jni->GetMethodID(jclas,"ordinal","()I");
	return static_cast<T>(jni->CallIntMethod(jenum, method));
}
static eia_tia_232_info protocolJ(JNIEnv * jni, jobject jobj) {
	auto jclas = jni->GetObjectClass(jobj);
/*
 *  $JAVA_HOME/bin/javap -s  -classpath ../out/production/usbuart/ info.usbuart.api.EIA_TIA_232_Info
 */
	jfieldID baudrate	= jni->GetFieldID(jclas, "baudrate", "I");
	jfieldID databits	= jni->GetFieldID(jclas, "databits", "C");
	jfieldID parity		= jni->GetFieldID(jclas, "parity",
						  "Linfo/usbuart/api/EIA_TIA_232_Info$parity_t;");
	if( jni->ExceptionOccurred() ) return eia_tia_232_info {0};
	jfieldID stopbits	= jni->GetFieldID(jclas, "stopbits",
						  "Linfo/usbuart/api/EIA_TIA_232_Info$stop_bits_t;");
	jfieldID flowcontrol= jni->GetFieldID(jclas, "flowcontrol",
						  "Linfo/usbuart/api/EIA_TIA_232_Info$flow_control_t;");
	return eia_tia_232_info {
		static_cast<baudrate_t>(jni->GetIntField(jobj,baudrate)),
		static_cast<uint8_t>(jni->GetCharField(jobj,databits)),
		ordinal<parity_t>(jni, jni->GetObjectField(jobj,parity)),
		ordinal<stop_bits_t>(jni, jni->GetObjectField(jobj,stopbits)),
		ordinal<flow_control_t>(jni, jni->GetObjectField(jobj,flowcontrol))
	};
}

static int parse_tail(const char* tag, char* file) {
	char* dlm = strrchr(file,'/');
	if( dlm == nullptr ) {
		log.d(tag,"failed to understand USB link '%s'", file);
		return 0;
	}
	*dlm++ = 0;
	return strtol(dlm,nullptr,10);
}


static device_addr from_fd(int fd) {
	char file[128];
	char proc[128];

	snprintf(proc, sizeof(proc), "/proc/self/fd/%d", fd);
	memset(file, 0, sizeof(file));
	readlink(proc, file, sizeof(file)-1);
	uint8_t dev = parse_tail(__, file);
	uint8_t bus = parse_tail(__, file);
	return { bus, dev };
}

jint JNICALL Java_info_usbuart_api_UsbUartContext_attach
  (JNIEnv * jni, jclass, jlong ctx, jint fd, jint ifc, jobject jch, jobject jpi) {
	channel ch 			= channelJ(jni, jch);
	eia_tia_232_info pi	= protocolJ(jni, jpi);
	if( jni->ExceptionOccurred() ) return -error_t::jni_error;
	device_addr da		= from_fd(fd);
	da.ifc = ifc;
	log.d(__,"fd=%d, da=%03d/%03d", fd, da.busid, da.devid);
	return reinterpret_cast<context*>(ctx)->attach(da, ch, pi);
}

jint JNICALL Java_info_usbuart_api_UsbUartContext_pipe
  (JNIEnv * jni, jclass, jlong ctx, jint fd, jint ifc, jobject jch, jobject jpi) {
	log.d(__,"fd=%d", fd);
	eia_tia_232_info pi	= protocolJ(jni, jpi);
	if( jni->ExceptionOccurred() ) return -error_t::jni_error;
	device_addr da		= from_fd(fd);
	da.ifc = ifc;
	log.d(__,"fd=%d, da=%03d/%03d", fd, da.busid, da.devid);
	channel ch {-1,-1};
	jint res = reinterpret_cast<context*>(ctx)->pipe(da, ch, pi);
	log.d(__,"res=%d", res);
	if( res == 0 ) channelJ(jni, jch, ch);
	return 0;//res;
}

jint JNICALL Java_info_usbuart_api_UsbUartContext_sendbreak
  (JNIEnv * jni, jclass, jlong ctx, jobject jch) {
	channel ch 			= channelJ(jni, jch);
	return reinterpret_cast<context*>(ctx)->sendbreak(ch);
}

jint JNICALL Java_info_usbuart_api_UsbUartContext_status
  (JNIEnv * jni, jclass, jlong ctx, jobject jch) {
	channel ch 			= channelJ(jni, jch);
	return reinterpret_cast<context*>(ctx)->status(ch);
}

jint JNICALL Java_info_usbuart_api_UsbUartContext_reset
  (JNIEnv * jni, jclass, jlong ctx, jobject jch) {
	channel ch 			= channelJ(jni, jch);
	return reinterpret_cast<context*>(ctx)->reset(ch);
}

void JNICALL Java_info_usbuart_api_UsbUartContext_close
  (JNIEnv * jni, jclass, jlong ctx, jobject jch) {
	channel ch 			= channelJ(jni, jch);
	reinterpret_cast<context*>(ctx)->close(ch);
}

static int sysfs_for(int fd, char* dst, unsigned n) {
	char path[64];
	struct stat st;
	if( fstat(fd, &st) < 0 ) {
		log.w(__,"Error %d accessing fd %d: %s", errno, fd, strerror(errno));
		return -error_t::io_error;
	}
	snprintf(path,sizeof(path), "/sys/dev/char/%d:%d",
			major(st.st_rdev),minor(st.st_rdev));
	log.d(__,"inspecting dev=%d:%d '%s'", major(st.st_rdev), minor(st.st_rdev), path);
	char * sysfs;
	if( ! (sysfs = realpath(path, nullptr)) ) {
		log.w(__,"Error %d accessing %s: %s", errno, path, strerror(errno));
		return -error_t::io_error;
	}
	char* p = strrchr(sysfs, '/');
	strncpy(dst, p+1, n);
	free(sysfs);
	return +error_t::success;
}

void JNICALL Java_info_usbuart_api_UsbUartContext_hotplug
  (JNIEnv * jni, jclass, jlong ctx, jint fd) {
	char sys_dir[512];
	device_addr da		= from_fd(fd);
	if( sysfs_for(fd, sys_dir, sizeof(sys_dir)) ) sys_dir[0] = 0;
	int res = android_enumerate_device(
		reinterpret_cast<context*>(ctx)->native(), da.busid, da.devid, sys_dir);
	log.d(__,"(%03d/%03d %s)->%d", da.busid, da.devid, sys_dir, res);
}
