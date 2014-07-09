#ifndef PETTYCOIN_TAL_PACKET_APPEND_PROOF_H
#define PETTYCOIN_TAL_PACKET_APPEND_PROOF_H
/* Generate wants packet.o, but this function pulls in too much. */
#include "block.h"

#define tal_packet_append_proof(ppkt, block, shardnum, txidx, proof, tx, refs) \
	tal_packet_append_proof_(ptr_to_ptr(ppkt), (block), (shardnum), \
				 (txidx), (proof), (tx), (refs))

void tal_packet_append_proof_(void *ppkt, const struct block *block,
			      u16 shardnum, u8 txoff,
			      const struct protocol_proof *proof,
			      const union protocol_tx *tx,
			      const struct protocol_input_ref *refs);
#endif /* PETTYCOIN_TAL_PACKET_APPEND_PROOF_H */