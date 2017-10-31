//
// Copyright 2017 Garrett D'Amore <garrett@damore.org>
// Copyright 2017 Capitar IT Group BV <info@capitar.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include "convey.h"
#include "trantest.h"

#if defined(NNG_ENABLE_PAIR1)
#define PairConvey Convey
#include "protocol/pair1/pair.h"
#elif defined(NNG_ENABLE_PAIR0)
#define PairConvey Convey
#include "protocol/pair0/pair.h"
#else
#define PairConvey SkipConvey
#endif

// TCP tests.

#ifndef _WIN32
#include <arpa/inet.h>
#endif

static int
check_props_v4(nng_msg *msg, nng_listener l, nng_dialer d)
{
	nng_pipe p;
	size_t   z;
	p = nng_msg_get_pipe(msg);
	So(p > 0);

	Convey("Local address property works", {
		nng_sockaddr la;
		z = sizeof(nng_sockaddr);
		So(nng_pipe_getopt(p, NNG_OPT_LOCADDR, &la, &z) == 0);
		So(z == sizeof(la));
		So(la.s_un.s_family == NNG_AF_INET);
		So(la.s_un.s_in.sa_port == htons(trantest_port - 1));
		So(la.s_un.s_in.sa_port != 0);
		So(la.s_un.s_in.sa_addr == htonl(0x7f000001));
	});

	Convey("Remote address property works", {
		nng_sockaddr ra;
		z = sizeof(nng_sockaddr);
		So(nng_pipe_getopt(p, NNG_OPT_REMADDR, &ra, &z) == 0);
		So(z == sizeof(ra));
		So(ra.s_un.s_family == NNG_AF_INET);
		So(ra.s_un.s_in.sa_port != 0);
		So(ra.s_un.s_in.sa_addr == htonl(0x7f000001));
	});

	return (0);
}

TestMain("TCP Transport", {

	trantest_test_extended("tcp://127.0.0.1:%u", check_props_v4);

	PairConvey("We cannot connect to wild cards", {
		nng_socket s;
		char       addr[NNG_MAXADDRLEN];

		So(nng_pair_open(&s) == 0);
		Reset({ nng_close(s); });
		trantest_next_address(addr, "tcp://*:%u");
		So(nng_dial(s, addr, NULL, 0) == NNG_EADDRINVAL);
	});

	PairConvey("We can bind to wild card", {
		nng_socket s1;
		nng_socket s2;
		char       addr[NNG_MAXADDRLEN];

		So(nng_pair_open(&s1) == 0);
		So(nng_pair_open(&s2) == 0);
		Reset({
			nng_close(s2);
			nng_close(s1);
		});
		trantest_next_address(addr, "tcp://*:%u");
		So(nng_listen(s1, addr, NULL, 0) == 0);
		// reset port back one
		trantest_prev_address(addr, "tcp://127.0.0.1:%u");
		So(nng_dial(s2, addr, NULL, 0) == 0);
	});

	PairConvey("Malformed TCP addresses do not panic", {
		nng_socket s1;

		So(nng_pair_open(&s1) == 0);
		Reset({ nng_close(s1); });
		So(nng_dial(s1, "tcp://127.0.0.1", NULL, 0) == NNG_EADDRINVAL);
		So(nng_dial(s1, "tcp://127.0.0.1.32", NULL, 0) ==
		    NNG_EADDRINVAL);
		So(nng_dial(s1, "tcp://127.0.x.1.32", NULL, 0) ==
		    NNG_EADDRINVAL);
		So(nng_listen(s1, "tcp://127.0.0.1", NULL, 0) ==
		    NNG_EADDRINVAL);
		So(nng_listen(s1, "tcp://127.0.0.1.32", NULL, 0) ==
		    NNG_EADDRINVAL);
		So(nng_listen(s1, "tcp://127.0.x.1.32", NULL, 0) ==
		    NNG_EADDRINVAL);
	});

	nng_fini();
})
