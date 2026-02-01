
 

#include <linux/crc32.h>

#include "rxe.h"
#include "rxe_loc.h"

 
int rxe_icrc_init(struct rxe_dev *rxe)
{
	struct crypto_shash *tfm;

	tfm = crypto_alloc_shash("crc32", 0, 0);
	if (IS_ERR(tfm)) {
		rxe_dbg_dev(rxe, "failed to init crc32 algorithm err: %ld\n",
			       PTR_ERR(tfm));
		return PTR_ERR(tfm);
	}

	rxe->tfm = tfm;

	return 0;
}

 
static __be32 rxe_crc32(struct rxe_dev *rxe, __be32 crc, void *next, size_t len)
{
	__be32 icrc;
	int err;

	SHASH_DESC_ON_STACK(shash, rxe->tfm);

	shash->tfm = rxe->tfm;
	*(__be32 *)shash_desc_ctx(shash) = crc;
	err = crypto_shash_update(shash, next, len);
	if (unlikely(err)) {
		rxe_dbg_dev(rxe, "failed crc calculation, err: %d\n", err);
		return (__force __be32)crc32_le((__force u32)crc, next, len);
	}

	icrc = *(__be32 *)shash_desc_ctx(shash);
	barrier_data(shash_desc_ctx(shash));

	return icrc;
}

 
static __be32 rxe_icrc_hdr(struct sk_buff *skb, struct rxe_pkt_info *pkt)
{
	unsigned int bth_offset = 0;
	struct iphdr *ip4h = NULL;
	struct ipv6hdr *ip6h = NULL;
	struct udphdr *udph;
	struct rxe_bth *bth;
	__be32 crc;
	int length;
	int hdr_size = sizeof(struct udphdr) +
		(skb->protocol == htons(ETH_P_IP) ?
		sizeof(struct iphdr) : sizeof(struct ipv6hdr));
	 
	u8 pshdr[sizeof(struct udphdr) +
		sizeof(struct ipv6hdr) +
		RXE_BTH_BYTES];

	 
	crc = (__force __be32)0xdebb20e3;

	if (skb->protocol == htons(ETH_P_IP)) {  
		memcpy(pshdr, ip_hdr(skb), hdr_size);
		ip4h = (struct iphdr *)pshdr;
		udph = (struct udphdr *)(ip4h + 1);

		ip4h->ttl = 0xff;
		ip4h->check = CSUM_MANGLED_0;
		ip4h->tos = 0xff;
	} else {				 
		memcpy(pshdr, ipv6_hdr(skb), hdr_size);
		ip6h = (struct ipv6hdr *)pshdr;
		udph = (struct udphdr *)(ip6h + 1);

		memset(ip6h->flow_lbl, 0xff, sizeof(ip6h->flow_lbl));
		ip6h->priority = 0xf;
		ip6h->hop_limit = 0xff;
	}
	udph->check = CSUM_MANGLED_0;

	bth_offset += hdr_size;

	memcpy(&pshdr[bth_offset], pkt->hdr, RXE_BTH_BYTES);
	bth = (struct rxe_bth *)&pshdr[bth_offset];

	 
	bth->qpn |= cpu_to_be32(~BTH_QPN_MASK);

	length = hdr_size + RXE_BTH_BYTES;
	crc = rxe_crc32(pkt->rxe, crc, pshdr, length);

	 
	crc = rxe_crc32(pkt->rxe, crc, pkt->hdr + RXE_BTH_BYTES,
			rxe_opcode[pkt->opcode].length - RXE_BTH_BYTES);
	return crc;
}

 
int rxe_icrc_check(struct sk_buff *skb, struct rxe_pkt_info *pkt)
{
	__be32 *icrcp;
	__be32 pkt_icrc;
	__be32 icrc;

	icrcp = (__be32 *)(pkt->hdr + pkt->paylen - RXE_ICRC_SIZE);
	pkt_icrc = *icrcp;

	icrc = rxe_icrc_hdr(skb, pkt);
	icrc = rxe_crc32(pkt->rxe, icrc, (u8 *)payload_addr(pkt),
				payload_size(pkt) + bth_pad(pkt));
	icrc = ~icrc;

	if (unlikely(icrc != pkt_icrc))
		return -EINVAL;

	return 0;
}

 
void rxe_icrc_generate(struct sk_buff *skb, struct rxe_pkt_info *pkt)
{
	__be32 *icrcp;
	__be32 icrc;

	icrcp = (__be32 *)(pkt->hdr + pkt->paylen - RXE_ICRC_SIZE);
	icrc = rxe_icrc_hdr(skb, pkt);
	icrc = rxe_crc32(pkt->rxe, icrc, (u8 *)payload_addr(pkt),
				payload_size(pkt) + bth_pad(pkt));
	*icrcp = ~icrc;
}
