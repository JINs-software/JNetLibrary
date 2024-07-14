#include "JNetCore.h"

void jnet::Encode(BYTE symmetricKey, BYTE randKey, USHORT payloadLen, BYTE& checkSum, BYTE* payloads) {
	BYTE payloadSum = 0;
	for (USHORT i = 0; i < payloadLen; i++) {
		payloadSum += payloads[i];
		payloadSum %= 256;
	}
	BYTE Pb = payloadSum ^ (randKey + 1);
	BYTE Eb = Pb ^ (symmetricKey + 1);
	checkSum = Eb;

	for (USHORT i = 1; i <= payloadLen; i++) {
		//BYTE Pn = payloads[i - 1] ^ (Pb + randKey + (BYTE)(i + 1));
		//BYTE En = Pn ^ (Eb + dfPACKET_KEY + (BYTE)(i + 1));
		BYTE Pn = payloads[i - 1] ^ (Pb + randKey + i + 1);
		BYTE En = Pn ^ (Eb + symmetricKey + i + 1);

		payloads[i - 1] = En;

		Pb = Pn;
		Eb = En;
	}
}
bool jnet::Decode(BYTE symmetricKey, BYTE randKey, USHORT payloadLen, BYTE checkSum, BYTE* payloads) {
	BYTE Pb = checkSum ^ (symmetricKey + 1);
	BYTE payloadSum = Pb ^ (randKey + 1);
	BYTE Eb = checkSum;
	BYTE Pn;
	BYTE Dn;
	BYTE payloadSumCmp = 0;

	for (USHORT i = 1; i <= payloadLen; i++) {
		//Pn = payloads[i - 1] ^ (Eb + dfPACKET_KEY + (BYTE)(i + 1));
		//Dn = Pn ^ (Pb + randKey + (BYTE)(i + 1));
		Pn = payloads[i - 1] ^ (Eb + symmetricKey + i + 1);
		Dn = Pn ^ (Pb + randKey + i + 1);

		Pb = Pn;
		Eb = payloads[i - 1];
		payloads[i - 1] = Dn;
		payloadSumCmp += payloads[i - 1];
		payloadSumCmp %= 256;
	}

	if (payloadSum != payloadSumCmp) {
#if defined(ASSERT)
		DebugBreak();
#endif
		return false;
	}

	return true;
}
bool jnet::Decode(BYTE symmetricKey, BYTE randKey, USHORT payloadLen, BYTE checkSum, JBuffer& ringPayloads) {
	if (ringPayloads.GetDirectDequeueSize() >= payloadLen) {
		return Decode(symmetricKey,randKey, payloadLen, checkSum, ringPayloads.GetDequeueBufferPtr());
	}
	else {
		BYTE Pb = checkSum ^ (symmetricKey + 1);
		BYTE payloadSum = Pb ^ (randKey + 1);
		BYTE Eb = checkSum;
		BYTE Pn, Dn;
		BYTE payloadSumCmp = 0;

		UINT offset = ringPayloads.GetDeqOffset();
		BYTE* bytepayloads = ringPayloads.GetBeginBufferPtr();
		for (USHORT i = 1; i <= payloadLen; i++, offset++) {
			offset = offset % (ringPayloads.GetBufferSize() + 1);
			//Pn = bytepayloads[offset] ^ (Eb + dfPACKET_KEY + (BYTE)(i + 1));
			//Dn = Pn ^ (Pb + randKey + (BYTE)(i + 1));
			Pn = bytepayloads[offset] ^ (Eb + symmetricKey + i + 1);
			Dn = Pn ^ (Pb + randKey + i + 1);

			Pb = Pn;
			Eb = bytepayloads[offset];
			bytepayloads[offset] = Dn;
			payloadSumCmp += bytepayloads[offset];
			payloadSumCmp %= 256;
		}

		if (payloadSum != payloadSumCmp) {
#if defined(ASSERT)
			DebugBreak();
#endif
			return false;
		}

		return true;
	}
}