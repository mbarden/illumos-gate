/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2013 (c) Joyent, Inc.  All rights reserved.
 * Copyright 2015 (c) Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 */

#ifndef _BCM2836_UART_H
#define	_BCM2836_UART_H

/*
 * Interface to the BCM2836's uart.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

void bcm2836_uart_init(void);
void bcm2836_uart_putc(uint8_t);
uint8_t bcm2836_uart_getc(void);
int bcm2836_uart_isc(void);

#ifdef __cplusplus
}
#endif

#endif /* _BCM2836_UART_H */
