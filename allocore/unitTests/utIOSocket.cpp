#include "utAllocore.h"

#include "catch.hpp"

TEST_CASE( "IOSocket", "[socket]" ) {

	{
		SocketClient s;
		SocketServer r;
	}

	int numTrials = 20000;
	unsigned port = 4110;
	int dropped = 0;

	const char dataSend[] = "On the other hand, we denounce with righteous indignation and dislike men who are so beguiled and demoralized by the charms of pleasure of the moment, so blinded by desire, that they cannot foresee the pain and trouble that are bound to ensue; and equal blame belongs to those who fail in their duty through weakness of will, which is the same as saying through shrinking from toil and pain. These cases are perfectly simple and easy to distinguish. In a free hour, when our power of choice is untrammelled and when nothing prevents our being able to do what we like best, every pleasure is to be welcomed and every pain avoided. But in certain circumstances and owing to the claims of duty or the obligations of business it will frequently occur that pleasures have to be repudiated and annoyances accepted. The wise man therefore always holds in these matters to this principle of selection: he rejects pleasures to secure other greater pleasures, or else he endures pains to avoid worse pains.";
	char dataRecv[sizeof dataSend];

	SocketClient c(port, "localhost");
	SocketServer s(port, "", 0.1);

	REQUIRE(c.port() == port);

	// Make receiver block forever until a packet is received.
	// All packets should be received.
	s.timeout(-1);

	for(int i=0; i<numTrials; ++i){
		dataRecv[0] = '\0';
		int ns = c.send(dataSend, sizeof dataSend);
		int nr = s.recv(dataRecv, sizeof dataRecv);
		REQUIRE(ns == sizeof dataSend);
		REQUIRE(nr == sizeof dataSend);
		REQUIRE(0 == strcmp(dataSend, dataRecv));
	}


	// Make receiver return immediately after checking for packets.
	// Most packets will likely not be received.
	s.timeout(0);

	for(int i=0; i<numTrials; ++i){
		dataRecv[0] = '\0';
		c.send(dataSend, sizeof dataSend);
		s.recv(dataRecv, sizeof dataRecv);
		if(!strcmp(dataSend, dataRecv)) ++dropped;
	}
	//printf("%d\n", dropped);
	REQUIRE(dropped);


	// Make receiver block for a short duration waiting for incoming packets.
	// All packets should be received.
	s.timeout(0.02);

	for(int i=0; i<numTrials; ++i){
		dataRecv[0] = '\0';
		int ns = c.send(dataSend, sizeof dataSend);
		int nr = s.recv(dataRecv, sizeof dataRecv);
		REQUIRE(ns == sizeof dataSend);
		REQUIRE(nr == sizeof dataSend);
		REQUIRE(0 == strcmp(dataSend, dataRecv));
	}

	// make sure timeout works:
	for(int i=0; i<20; ++i){
		s.recv(dataRecv, sizeof dataRecv);
		//printf("r %d\n", i);
	}

	// Empirical tests
	{
//		printf("%s\n", Socket::hostName().c_str());
//		printf("%s\n", Socket::hostIP().c_str());
	}
}
