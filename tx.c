#include "addr.h"
#include "tx.h"
#include <ccan/structeq/structeq.h>

bool find_output(const union protocol_tx *tx, u16 output_num,
		 struct protocol_address *addr, u32 *amount)
{
	const struct protocol_gateway_payment *out;

	switch (tx_type(tx)) {
	case TX_FROM_GATEWAY:
		if (output_num > le16_to_cpu(tx->from_gateway.num_outputs))
			return false;
		out = get_from_gateway_outputs(&tx->from_gateway);
		*addr = out[output_num].output_addr;
		*amount = le32_to_cpu(out[output_num].send_amount);
		return true;
	case TX_NORMAL:
		if (output_num == 0) {
			/* Spending the send_amount. */
			*addr = tx->normal.output_addr;
			*amount = le32_to_cpu(tx->normal.send_amount);
			return true;
		} else if (output_num == 1) {
			/* Spending the change. */
			get_tx_input_address(tx, addr);
			*amount = le32_to_cpu(tx->normal.change_amount);
			return true;
		}
		return false;
	case TX_CLAIM:
		if (output_num == 0) {
			get_tx_input_address(tx, addr);
			*amount = le32_to_cpu(tx->claim.amount);
			return true;
		}
		return false;
	case TX_TO_GATEWAY:
		return false;
	}
	abort();
}

u32 find_matching_input(const union protocol_tx *tx,
			const struct protocol_input *inp)
{
	unsigned int i;

	/* Figure out which input of other did the spend. */
	for (i = 0; i < num_inputs(tx); i++) {
		if (structeq(&tx_input(tx, i)->input, &inp->input)
		    && tx_input(tx, i)->output == inp->output)
			return i;
	}
	abort();
}

u32 tx_amount_sent(const union protocol_tx *tx)
{
	switch (tx_type(tx)) {
	case TX_NORMAL:
		return le32_to_cpu(tx->normal.send_amount)
			+ le32_to_cpu(tx->normal.change_amount);
	case TX_FROM_GATEWAY: {
		u32 i, total = 0;
		for (i = 0; i < le16_to_cpu(tx->from_gateway.num_outputs); i++){
			le32 amount;
			amount = get_from_gateway_outputs(&tx->from_gateway)
				[i].send_amount;
			total += le32_to_cpu(amount);
		}
		return total;
	}
	case TX_TO_GATEWAY:
		return le32_to_cpu(tx->to_gateway.send_amount)
			+ le32_to_cpu(tx->to_gateway.change_amount);
	case TX_CLAIM:
		return le32_to_cpu(tx->claim.amount);
	}
	abort();
}
