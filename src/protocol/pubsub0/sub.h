//
// Copyright 2017 Garrett D'Amore <garrett@damore.org>
// Copyright 2017 Capitar IT Group BV <info@capitar.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#ifndef NNG_PROTOCOL_PUBSUB_SUB_H
#define NNG_PROTOCOL_PUBSUB_SUB_H

extern int nng_sub0_open(nng_socket *);

#ifndef nng_sub_open
#define nng_sub_open nng_sub0_open
#endif

#define NNG_OPT_SUB_SUBSCRIBE "sub:subscribe"
#define NNG_OPT_SUB_UNSUBSCRIBE "sub:unsubscribe"

#endif // NNG_PROTOCOL_PUBSUB_SUB_H
