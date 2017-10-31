//
// Copyright 2017 Garrett D'Amore <garrett@damore.org>
// Copyright 2017 Capitar IT Group BV <info@capitar.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#ifndef NNG_PROTOCOL_BUS0_BUS_H
#define NNG_PROTOCOL_BUS0_BUS_H

extern int nng_bus0_open(nng_socket *);

#ifndef nng_bus_open
#define nng_bus_open nng_bus0_open
#endif

#endif // NNG_PROTOCOL_BUS0_BUS_H
